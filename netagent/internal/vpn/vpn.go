// Package vpn renders WireGuard wg-quick configuration files and supervises
// tunnel health via handshake age monitoring, bounded exponential-backoff
// restarts, and an upload kill-switch when the tunnel is down.
package vpn

import (
	"context"
	"errors"
	"fmt"
	"net/netip"
	"strings"
	"sync"
	"time"
)

// PeerConfig describes the WireGuard ingest peer.
type PeerConfig struct {
	PublicKey           string
	Endpoint            string
	AllowedIPs          []netip.Prefix
	PersistentKeepalive int // seconds
}

// TunnelConfig describes a device WireGuard interface.
type TunnelConfig struct {
	Address       netip.Prefix
	PrivateKeyRef string // path or PKCS#11 URI; never logged
	DNS           []netip.Addr
	MTU           int
	Peer          PeerConfig
}

// RenderWGQuick formats cfg as a minimal wg-quick(8) configuration.
func RenderWGQuick(cfg TunnelConfig) (string, error) {
	if !cfg.Address.IsValid() {
		return "", errors.New("vpn: invalid interface address")
	}
	if strings.TrimSpace(cfg.PrivateKeyRef) == "" {
		return "", errors.New("vpn: missing private key reference")
	}
	if strings.TrimSpace(cfg.Peer.PublicKey) == "" || strings.TrimSpace(cfg.Peer.Endpoint) == "" {
		return "", errors.New("vpn: peer public key and endpoint are required")
	}
	if len(cfg.Peer.AllowedIPs) == 0 {
		return "", errors.New("vpn: at least one AllowedIPs prefix required")
	}

	var b strings.Builder
	b.WriteString("[Interface]\n")
	fmt.Fprintf(&b, "Address = %s\n", cfg.Address.String())
	fmt.Fprintf(&b, "PrivateKey = %s\n", cfg.PrivateKeyRef)
	if len(cfg.DNS) > 0 {
		dnsStrs := make([]string, len(cfg.DNS))
		for i, a := range cfg.DNS {
			dnsStrs[i] = a.String()
		}
		fmt.Fprintf(&b, "DNS = %s\n", strings.Join(dnsStrs, ", "))
	}
	if cfg.MTU > 0 {
		fmt.Fprintf(&b, "MTU = %d\n", cfg.MTU)
	}
	b.WriteString("\n[Peer]\n")
	fmt.Fprintf(&b, "PublicKey = %s\n", cfg.Peer.PublicKey)
	fmt.Fprintf(&b, "Endpoint = %s\n", cfg.Peer.Endpoint)
	allowed := make([]string, len(cfg.Peer.AllowedIPs))
	for i, p := range cfg.Peer.AllowedIPs {
		allowed[i] = p.String()
	}
	fmt.Fprintf(&b, "AllowedIPs = %s\n", strings.Join(allowed, ", "))
	if cfg.Peer.PersistentKeepalive > 0 {
		fmt.Fprintf(&b, "PersistentKeepalive = %d\n", cfg.Peer.PersistentKeepalive)
	}
	return b.String(), nil
}

// Driver abstracts WireGuard kernel/userspace inspection and tunnel restart.
type Driver interface {
	LastHandshake(ctx context.Context) (time.Time, error)
	Restart(ctx context.Context) error
}

// SupervisorConfig configures tunnel health supervision.
type SupervisorConfig struct {
	MaxHandshakeAge time.Duration
	BaseBackoff     time.Duration
	MaxBackoff      time.Duration
	KillSwitch      bool
	Now             func() time.Time
}

// Supervisor monitors a WireGuard tunnel and enforces the kill-switch policy.
type Supervisor struct {
	mu          sync.Mutex
	cfg         SupervisorConfig
	drv         Driver
	healthy     bool
	restarts    int
	nextRestart time.Time
}

// NewSupervisor creates a Supervisor.
func NewSupervisor(cfg SupervisorConfig, drv Driver) (*Supervisor, error) {
	if drv == nil {
		return nil, errors.New("vpn: driver required")
	}
	if cfg.MaxHandshakeAge <= 0 {
		cfg.MaxHandshakeAge = 3 * time.Minute
	}
	if cfg.BaseBackoff <= 0 {
		cfg.BaseBackoff = 2 * time.Second
	}
	if cfg.MaxBackoff <= 0 {
		cfg.MaxBackoff = 60 * time.Second
	}
	if cfg.Now == nil {
		cfg.Now = time.Now
	}
	return &Supervisor{cfg: cfg, drv: drv}, nil
}

// Check evaluates tunnel health and triggers a restart with exponential backoff
// when the latest handshake is older than MaxHandshakeAge.
func (s *Supervisor) Check(ctx context.Context) (healthy bool, restarted bool, err error) {
	now := s.cfg.Now()
	hs, hsErr := s.drv.LastHandshake(ctx)

	s.mu.Lock()
	defer s.mu.Unlock()

	if hsErr == nil && !hs.IsZero() && now.Sub(hs) <= s.cfg.MaxHandshakeAge {
		s.healthy = true
		s.restarts = 0
		s.nextRestart = time.Time{}
		return true, false, nil
	}

	s.healthy = false
	if !s.nextRestart.IsZero() && now.Before(s.nextRestart) {
		return false, false, hsErr
	}

	rErr := s.drv.Restart(ctx)
	backoff := s.cfg.BaseBackoff << minInt(s.restarts, 10)
	if backoff > s.cfg.MaxBackoff {
		backoff = s.cfg.MaxBackoff
	}
	s.restarts++
	s.nextRestart = now.Add(backoff)
	if rErr != nil {
		return false, true, rErr
	}
	return false, true, hsErr
}

// AllowTraffic returns whether evidence upload traffic is permitted. When
// KillSwitch is enabled, traffic is blocked unless the tunnel is healthy.
func (s *Supervisor) AllowTraffic() bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	if !s.cfg.KillSwitch {
		return true
	}
	return s.healthy
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}
