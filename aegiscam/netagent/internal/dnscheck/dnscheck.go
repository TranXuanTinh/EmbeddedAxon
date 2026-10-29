// Package dnscheck monitors DNS resolver health, fails over between primary
// and secondary resolvers, caches answers with a bounded TTL, and detects
// captive portals or DNS hijacking using a canary hostname with expected IP
// prefixes/addresses.
package dnscheck

import (
	"context"
	"errors"
	"fmt"
	"net/netip"
	"sync"
	"time"
)

// ErrNoHealthyResolver is returned when both primary and secondary resolvers fail.
var ErrNoHealthyResolver = errors.New("dnscheck: all resolvers failed")

// ErrHijackDetected is returned when the canary query resolves to an unexpected IP.
var ErrHijackDetected = errors.New("dnscheck: captive portal or DNS hijack detected")

// Resolver abstracts DNS A/AAAA lookups for testability.
type Resolver interface {
	LookupIP(ctx context.Context, resolverAddr string, host string) ([]netip.Addr, error)
}

// ResolverFunc adapts a function to Resolver.
type ResolverFunc func(ctx context.Context, resolverAddr string, host string) ([]netip.Addr, error)

// LookupIP implements Resolver.
func (f ResolverFunc) LookupIP(ctx context.Context, resolverAddr string, host string) ([]netip.Addr, error) {
	return f(ctx, resolverAddr, host)
}

// Config configures the Checker.
type Config struct {
	Primary        string
	Secondary      string
	CacheTTL       time.Duration
	CanaryHost     string
	ExpectedCanary []netip.Prefix
	Now            func() time.Time
}

type cacheEntry struct {
	addrs   []netip.Addr
	expires time.Time
}

// Checker performs cached DNS lookups with resolver failover and canary checks.
type Checker struct {
	mu       sync.Mutex
	cfg      Config
	resolver Resolver
	cache    map[string]cacheEntry
	useSec   bool
}

// New creates a Checker.
func New(cfg Config, r Resolver) (*Checker, error) {
	if cfg.Primary == "" {
		return nil, errors.New("dnscheck: primary resolver required")
	}
	if r == nil {
		return nil, errors.New("dnscheck: resolver implementation required")
	}
	if cfg.CacheTTL <= 0 {
		cfg.CacheTTL = 30 * time.Second
	}
	if cfg.Now == nil {
		cfg.Now = time.Now
	}
	return &Checker{
		cfg:      cfg,
		resolver: r,
		cache:    make(map[string]cacheEntry),
	}, nil
}

// Resolve resolves host using the cache if fresh, otherwise querying primary
// and falling back to secondary.
func (c *Checker) Resolve(ctx context.Context, host string) ([]netip.Addr, string, error) {
	now := c.cfg.Now()
	c.mu.Lock()
	if ent, ok := c.cache[host]; ok && now.Before(ent.expires) {
		out := append([]netip.Addr(nil), ent.addrs...)
		active := c.activeResolverLocked()
		c.mu.Unlock()
		return out, active, nil
	}
	c.mu.Unlock()

	order := []string{c.cfg.Primary}
	if c.cfg.Secondary != "" {
		order = append(order, c.cfg.Secondary)
	}

	var lastErr error
	for i, srv := range order {
		addrs, err := c.resolver.LookupIP(ctx, srv, host)
		if err == nil && len(addrs) > 0 {
			c.mu.Lock()
			c.useSec = (i == 1)
			c.cache[host] = cacheEntry{
				addrs:   append([]netip.Addr(nil), addrs...),
				expires: c.cfg.Now().Add(c.cfg.CacheTTL),
			}
			c.mu.Unlock()
			return addrs, srv, nil
		}
		if err != nil {
			lastErr = err
		} else {
			lastErr = errors.New("empty answer")
		}
	}
	return nil, "", fmt.Errorf("%w: %v", ErrNoHealthyResolver, lastErr)
}

// CheckCanary resolves the configured CanaryHost (bypassing cache) and verifies
// that every returned address falls within at least one ExpectedCanary prefix.
func (c *Checker) CheckCanary(ctx context.Context) error {
	if c.cfg.CanaryHost == "" || len(c.cfg.ExpectedCanary) == 0 {
		return nil
	}
	c.mu.Lock()
	delete(c.cache, c.cfg.CanaryHost)
	c.mu.Unlock()

	addrs, _, err := c.Resolve(ctx, c.cfg.CanaryHost)
	if err != nil {
		return err
	}
	for _, a := range addrs {
		matched := false
		for _, pfx := range c.cfg.ExpectedCanary {
			if pfx.Contains(a) {
				matched = true
				break
			}
		}
		if !matched {
			return fmt.Errorf("%w: %s resolved to %s", ErrHijackDetected, c.cfg.CanaryHost, a)
		}
	}
	return nil
}

func (c *Checker) activeResolverLocked() string {
	if c.useSec && c.cfg.Secondary != "" {
		return c.cfg.Secondary
	}
	return c.cfg.Primary
}
