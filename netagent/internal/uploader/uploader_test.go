package uploader

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"io"
	"net/http"
	"strconv"
	"sync"
	"testing"
	"time"
)

type fakeServer struct {
	mu         sync.Mutex
	buf        []byte
	failNext   int
	patchCalls int
}

func (s *fakeServer) RoundTrip(req *http.Request) (*http.Response, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	switch req.Method {
	case http.MethodHead:
		hdr := make(http.Header)
		hdr.Set("Upload-Offset", strconv.Itoa(len(s.buf)))
		return &http.Response{
			StatusCode: http.StatusOK,
			Header:     hdr,
			Body:       io.NopCloser(bytes.NewReader(nil)),
		}, nil

	case http.MethodPatch:
		s.patchCalls++
		body, _ := io.ReadAll(req.Body)
		_ = req.Body.Close()
		if s.failNext > 0 {
			s.failNext--
			return &http.Response{
				StatusCode: http.StatusServiceUnavailable,
				Body:       io.NopCloser(bytes.NewReader(nil)),
			}, nil
		}
		sum := sha256.Sum256(body)
		if req.Header.Get("X-Chunk-SHA256") != hex.EncodeToString(sum[:]) {
			return &http.Response{
				StatusCode: http.StatusBadRequest,
				Body:       io.NopCloser(bytes.NewReader(nil)),
			}, nil
		}
		off, _ := strconv.Atoi(req.Header.Get("Upload-Offset"))
		if off != len(s.buf) {
			return &http.Response{
				StatusCode: http.StatusConflict,
				Body:       io.NopCloser(bytes.NewReader(nil)),
			}, nil
		}
		s.buf = append(s.buf, body...)
		return &http.Response{
			StatusCode: http.StatusNoContent,
			Body:       io.NopCloser(bytes.NewReader(nil)),
		}, nil
	}
	return &http.Response{StatusCode: http.StatusMethodNotAllowed, Body: io.NopCloser(bytes.NewReader(nil))}, nil
}

func TestResumableChunkedUploadWithRetry(t *testing.T) {
	payload := make([]byte, 2500)
	for i := range payload {
		payload[i] = byte((i * 31) & 0xff)
	}

	// Pre-populate first 1024 bytes on the server to test resume via HEAD.
	srv := &fakeServer{
		buf:      append([]byte(nil), payload[:1024]...),
		failNext: 2, // first 2 PATCH attempts fail with 503
	}

	var sleeps []time.Duration
	up, err := New(Config{
		BaseURL:      "https://ingest.aegis.example",
		ChunkSize:    1024,
		MaxRetries:   4,
		BaseBackoff:  10 * time.Millisecond,
		MaxBackoff:   100 * time.Millisecond,
		BandwidthBps: 1_000_000,
		Transport:    srv,
		Sleep: func(_ context.Context, d time.Duration) error {
			sleeps = append(sleeps, d)
			return nil
		},
		RandFloat64: func() float64 { return 0.5 },
	})
	if err != nil {
		t.Fatal(err)
	}

	if err := up.UploadSegment(context.Background(), "INC-AEG-0001-100", 1, payload); err != nil {
		t.Fatalf("UploadSegment: %v", err)
	}

	if !bytes.Equal(srv.buf, payload) {
		t.Fatalf("uploaded bytes mismatch: got %d bytes, want %d", len(srv.buf), len(payload))
	}
	// Remaining 1476 bytes = 2 chunks (1024 + 452) + 2 transient 503s = 4 PATCH calls.
	if srv.patchCalls != 4 {
		t.Fatalf("expected 4 PATCH calls, got %d", srv.patchCalls)
	}
	if len(sleeps) < 2 {
		t.Fatalf("expected backoff and bandwidth sleeps, got %v", sleeps)
	}
}
