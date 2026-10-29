package dnscheck

import (
	"context"
	"errors"
	"net/netip"
	"testing"
	"time"
)

func TestResolveFailoverAndCache(t *testing.T) {
	now := time.Unix(1_700_000_000, 0)
	calls := 0
	primaryUp := false

	r := ResolverFunc(func(_ context.Context, srv, host string) ([]netip.Addr, error) {
		calls++
		if srv == "10.0.0.1:53" && !primaryUp {
			return nil, errors.New("timeout")
		}
		return []netip.Addr{netip.MustParseAddr("198.51.100.10")}, nil
	})

	chk, err := New(Config{
		Primary:   "10.0.0.1:53",
		Secondary: "1.1.1.1:53",
		CacheTTL:  10 * time.Second,
		Now:       func() time.Time { return now },
	}, r)
	if err != nil {
		t.Fatal(err)
	}

	// Primary fails -> falls back to secondary (2 calls).
	addrs, used, err := chk.Resolve(context.Background(), "ingest.aegis.example")
	if err != nil || used != "1.1.1.1:53" || len(addrs) != 1 {
		t.Fatalf("unexpected resolve result: addrs=%v used=%s err=%v", addrs, used, err)
	}
	if calls != 2 {
		t.Fatalf("expected 2 resolver calls, got %d", calls)
	}

	// Cached lookup within TTL -> 0 new calls.
	now = now.Add(5 * time.Second)
	_, _, err = chk.Resolve(context.Background(), "ingest.aegis.example")
	if err != nil || calls != 2 {
		t.Fatalf("expected cache hit, calls=%d err=%v", calls, err)
	}

	// Advance past TTL and restore primary -> hits primary.
	now = now.Add(10 * time.Second)
	primaryUp = true
	_, used, err = chk.Resolve(context.Background(), "ingest.aegis.example")
	if err != nil || used != "10.0.0.1:53" {
		t.Fatalf("expected primary after TTL expiry, got used=%s err=%v", used, err)
	}
}

func TestCanaryHijackDetection(t *testing.T) {
	tests := []struct {
		name    string
		retAddr string
		wantErr error
	}{
		{"legit", "203.0.113.42", nil},
		{"captive_portal", "192.168.1.1", ErrHijackDetected},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			r := ResolverFunc(func(_ context.Context, _, _ string) ([]netip.Addr, error) {
				return []netip.Addr{netip.MustParseAddr(tc.retAddr)}, nil
			})
			chk, err := New(Config{
				Primary:        "10.0.0.1:53",
				CanaryHost:     "canary.aegis.example",
				ExpectedCanary: []netip.Prefix{netip.MustParsePrefix("203.0.113.0/24")},
			}, r)
			if err != nil {
				t.Fatal(err)
			}
			err = chk.CheckCanary(context.Background())
			if tc.wantErr == nil && err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if tc.wantErr != nil && !errors.Is(err, tc.wantErr) {
				t.Fatalf("got err=%v, want %v", err, tc.wantErr)
			}
		})
	}
}
