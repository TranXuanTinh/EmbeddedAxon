// Package uploader implements resumable chunked evidence uploading with
// per-chunk SHA-256 verification headers, server offset querying, exponential
// backoff with full jitter, concurrency limiting, and a token-bucket bandwidth
// limiter.
package uploader

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"strconv"
	"sync"
	"time"
)

// DefaultChunkSize is 4 MiB per spec §6.
const DefaultChunkSize = 4 * 1024 * 1024

// Config configures an Uploader.
type Config struct {
	BaseURL      string
	ChunkSize    int
	MaxRetries   int
	BaseBackoff  time.Duration
	MaxBackoff   time.Duration
	MaxInFlight  int
	BandwidthBps int64 // bytes per second; 0 = unlimited
	Transport    http.RoundTripper
	Sleep        func(ctx context.Context, d time.Duration) error
	RandFloat64  func() float64
}

// Uploader performs resumable chunked uploads.
type Uploader struct {
	cfg    Config
	client *http.Client
	sem    chan struct{}
	bucket *tokenBucket
}

// New creates an Uploader.
func New(cfg Config) (*Uploader, error) {
	if cfg.BaseURL == "" {
		return nil, errors.New("uploader: BaseURL required")
	}
	if cfg.ChunkSize <= 0 {
		cfg.ChunkSize = DefaultChunkSize
	}
	if cfg.MaxRetries <= 0 {
		cfg.MaxRetries = 5
	}
	if cfg.BaseBackoff <= 0 {
		cfg.BaseBackoff = 100 * time.Millisecond
	}
	if cfg.MaxBackoff <= 0 {
		cfg.MaxBackoff = 10 * time.Second
	}
	if cfg.MaxInFlight <= 0 {
		cfg.MaxInFlight = 2
	}
	if cfg.Sleep == nil {
		cfg.Sleep = defaultSleep
	}
	if cfg.RandFloat64 == nil {
		rng := rand.New(rand.NewSource(1))
		var mu sync.Mutex
		cfg.RandFloat64 = func() float64 {
			mu.Lock()
			defer mu.Unlock()
			return rng.Float64()
		}
	}
	rt := cfg.Transport
	if rt == nil {
		rt = http.DefaultTransport
	}
	var tb *tokenBucket
	if cfg.BandwidthBps > 0 {
		tb = newTokenBucket(cfg.BandwidthBps, cfg.Sleep)
	}
	return &Uploader{
		cfg:    cfg,
		client: &http.Client{Transport: rt},
		sem:    make(chan struct{}, cfg.MaxInFlight),
		bucket: tb,
	}, nil
}

// UploadSegment uploads data for (incidentID, segmentIndex), resuming from the
// byte offset reported by the server via HEAD.
func (u *Uploader) UploadSegment(ctx context.Context, incidentID string, segmentIndex int, data []byte) error {
	select {
	case u.sem <- struct{}{}:
		defer func() { <-u.sem }()
	case <-ctx.Done():
		return ctx.Err()
	}

	url := fmt.Sprintf("%s/v1/incidents/%s/segments/%d", u.cfg.BaseURL, incidentID, segmentIndex)
	offset, err := u.queryOffset(ctx, url)
	if err != nil {
		return err
	}
	if offset > len(data) {
		offset = 0
	}

	for offset < len(data) {
		end := offset + u.cfg.ChunkSize
		if end > len(data) {
			end = len(data)
		}
		chunk := data[offset:end]
		if u.bucket != nil {
			if err := u.bucket.Wait(ctx, int64(len(chunk))); err != nil {
				return err
			}
		}
		if err := u.uploadChunkWithRetry(ctx, url, offset, len(data), chunk); err != nil {
			return err
		}
		offset = end
	}
	return nil
}

func (u *Uploader) queryOffset(ctx context.Context, url string) (int, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodHead, url, nil)
	if err != nil {
		return 0, err
	}
	resp, err := u.client.Do(req)
	if err != nil {
		return 0, err
	}
	_, _ = io.Copy(io.Discard, resp.Body)
	_ = resp.Body.Close()

	if resp.StatusCode == http.StatusNotFound {
		return 0, nil
	}
	if resp.StatusCode >= 200 && resp.StatusCode < 300 {
		if hdr := resp.Header.Get("Upload-Offset"); hdr != "" {
			off, err := strconv.Atoi(hdr)
			if err == nil && off >= 0 {
				return off, nil
			}
		}
		return 0, nil
	}
	return 0, fmt.Errorf("uploader: HEAD %s status %d", url, resp.StatusCode)
}

func (u *Uploader) uploadChunkWithRetry(ctx context.Context, url string, offset, total int, chunk []byte) error {
	sum := sha256.Sum256(chunk)
	sumHex := hex.EncodeToString(sum[:])

	var lastErr error
	for attempt := 0; attempt <= u.cfg.MaxRetries; attempt++ {
		if attempt > 0 {
			backoff := u.fullJitterBackoff(attempt)
			if err := u.cfg.Sleep(ctx, backoff); err != nil {
				return err
			}
		}
		req, err := http.NewRequestWithContext(ctx, http.MethodPatch, url, bytes.NewReader(chunk))
		if err != nil {
			return err
		}
		req.Header.Set("Content-Type", "application/offset+octet-stream")
		req.Header.Set("Upload-Offset", strconv.Itoa(offset))
		req.Header.Set("Upload-Length", strconv.Itoa(total))
		req.Header.Set("X-Chunk-SHA256", sumHex)

		resp, err := u.client.Do(req)
		if err != nil {
			lastErr = err
			continue
		}
		_, _ = io.Copy(io.Discard, resp.Body)
		_ = resp.Body.Close()

		if resp.StatusCode >= 200 && resp.StatusCode < 300 {
			return nil
		}
		if resp.StatusCode >= 400 && resp.StatusCode < 500 && resp.StatusCode != http.StatusTooManyRequests {
			return fmt.Errorf("uploader: permanent client error %d", resp.StatusCode)
		}
		lastErr = fmt.Errorf("uploader: server status %d", resp.StatusCode)
	}
	return fmt.Errorf("uploader: max retries exceeded: %w", lastErr)
}

// fullJitterBackoff computes sleep = rand(0, min(cap, base * 2^(attempt-1))).
func (u *Uploader) fullJitterBackoff(attempt int) time.Duration {
	shift := attempt - 1
	if shift > 20 {
		shift = 20
	}
	ceil := u.cfg.BaseBackoff * time.Duration(1<<shift)
	if ceil > u.cfg.MaxBackoff || ceil <= 0 {
		ceil = u.cfg.MaxBackoff
	}
	return time.Duration(u.cfg.RandFloat64() * float64(ceil))
}

func defaultSleep(ctx context.Context, d time.Duration) error {
	t := time.NewTimer(d)
	defer t.Stop()
	select {
	case <-t.C:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

type tokenBucket struct {
	mu    sync.Mutex
	rate  int64 // bytes per second
	sleep func(ctx context.Context, d time.Duration) error
}

func newTokenBucket(rateBps int64, sleep func(ctx context.Context, d time.Duration) error) *tokenBucket {
	return &tokenBucket{rate: rateBps, sleep: sleep}
}

func (b *tokenBucket) Wait(ctx context.Context, n int64) error {
	b.mu.Lock()
	defer b.mu.Unlock()
	if b.rate <= 0 || n <= 0 {
		return nil
	}
	waitDur := time.Duration(float64(n) / float64(b.rate) * float64(time.Second))
	return b.sleep(ctx, waitDur)
}
