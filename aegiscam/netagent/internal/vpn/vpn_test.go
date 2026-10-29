package vpn

import (
	"context"
	"net/netip"
	"strings"
	"testing"
	"time"
)

type fakeDriver struct {
	handshake time.Time
	hsErr     error
	restarts  int
}

func (f *fakeDriver) LastHandshake(_ context.Context) (time.Time, error) {
	return f.handshake, f.hsErr
}

func (f *fakeDriver) Restart(_ context.Context) error {
	f.restarts++
	return nil
}

func TestRenderWGQuick(t *testing.T) {
	cfg := TunnelConfig{
		Address:       netip.MustParsePrefix("10.88.0.12/32"),
		PrivateKeyRef: "REDACTED_FROM_TPM",
		DNS:           []netip.Addr{netip.MustParseAddr("10.88.0.1")},
		MTU:           1380,
		Peer: PeerConfig{
			PublicKey:           "PEER_PUBKEY_BASE64==",
			Endpoint:            "vpn.aegis.example:51820",
			AllowedIPs:          []netip.Prefix{netip.MustParsePrefix("10.88.0.0/16")},
			PersistentKeepalive: 25,
		},
	}
	out, err := RenderWGQuick(cfg)
	if err != nil {
		t.Fatalf("RenderWGQuick: %v", err)
	}
	for _, want := range []string{
		"Address = 10.88.0.12/32",
		"MTU = 1380",
		"Endpoint = vpn.aegis.example:51820",
		"AllowedIPs = 10.88.0.0/16",
		"PersistentKeepalive = 25",
	} {
		if !strings.Contains(out, want) {
			t.Fatalf("missing %q in rendered config:\n%s", want, out)
		}
	}
}

func TestSupervisorKillSwitchAndBackoff(t *testing.T) {
	now := time.Unix(1_700_000_000, 0)
	drv := &fakeDriver{handshake: now.Add(-10 * time.Minute)}

	sup, err := NewSupervisor(SupervisorConfig{
		MaxHandshakeAge: 3 * time.Minute,
		BaseBackoff:     5 * time.Second,
		MaxBackoff:      30 * time.Second,
		KillSwitch:      true,
		Now:             func() time.Time { return now },
	}, drv)
	if err != nil {
		t.Fatal(err)
	}

	if sup.AllowTraffic() {
		t.Fatal("kill-switch must block traffic before first healthy check")
	}

	// Stale handshake -> triggers restart #1, backoff = 5s.
	healthy, restarted, _ := sup.Check(context.Background())
	if healthy || !restarted || drv.restarts != 1 {
		t.Fatalf("expected restart #1, got healthy=%v restarted=%v count=%d", healthy, restarted, drv.restarts)
	}

	// Within 5s backoff -> no restart.
	now = now.Add(2 * time.Second)
	_, restarted, _ = sup.Check(context.Background())
	if restarted || drv.restarts != 1 {
		t.Fatal("should not restart before backoff expires")
	}

	// After 5s -> triggers restart #2, backoff = 10s.
	now = now.Add(4 * time.Second)
	_, restarted, _ = sup.Check(context.Background())
	if !restarted || drv.restarts != 2 {
		t.Fatalf("expected restart #2, got restarted=%v count=%d", restarted, drv.restarts)
	}

	// Fresh handshake -> tunnel becomes healthy and kill-switch opens.
	drv.handshake = now
	healthy, restarted, err = sup.Check(context.Background())
	if err != nil || !healthy || restarted || !sup.AllowTraffic() {
		t.Fatalf("expected healthy tunnel, got healthy=%v restarted=%v allow=%v err=%v",
			healthy, restarted, sup.AllowTraffic(), err)
	}
}
