// Package link implements uplink selection for netagent.
//
// Interfaces are ranked by kind (eth > wifi > lte). Each interface is probed
// periodically; an interface only becomes healthy after RiseThreshold
// consecutive successful probes and only becomes unhealthy after
// FallThreshold consecutive failures. Because a higher-priority link must
// prove itself RiseThreshold times before it is used again, failback is
// hysteretic and a flapping Ethernet cable cannot cause route churn.
//
// Metered links (LTE) are subject to an upload policy: only high-priority
// incidents may be uploaded over them.
package link

import (
	"context"
	"errors"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"
)

// Kind is the physical type of an uplink. Lower values are preferred.
type Kind int

// Supported link kinds, in order of preference.
const (
	Ethernet Kind = iota
	WiFi
	LTE
)

// String returns the short name used in flags and logs.
func (k Kind) String() string {
	switch k {
	case Ethernet:
		return "eth"
	case WiFi:
		return "wifi"
	case LTE:
		return "lte"
	default:
		return fmt.Sprintf("Kind(%d)", int(k))
	}
}

// ParseKind converts "eth", "wifi" or "lte" into a Kind.
func ParseKind(s string) (Kind, error) {
	switch strings.ToLower(strings.TrimSpace(s)) {
	case "eth", "ethernet":
		return Ethernet, nil
	case "wifi", "wlan":
		return WiFi, nil
	case "lte", "wwan", "cellular":
		return LTE, nil
	default:
		return 0, fmt.Errorf("link: unknown kind %q", s)
	}
}

// Interface describes one uplink.
type Interface struct {
	Name    string
	Kind    Kind
	Metered bool
}

// Priority is the business priority of an upload.
type Priority int

// Upload priorities.
const (
	PriorityNormal Priority = iota
	PriorityHigh
)

// ProbeResult is the outcome of one health probe.
type ProbeResult struct {
	OK  bool
	RTT time.Duration
}

// Prober checks the health of a single interface. Implementations must
// honour ctx and must not block indefinitely.
type Prober interface {
	Probe(ctx context.Context, iface Interface) ProbeResult
}

// ProberFunc adapts a function to the Prober interface.
type ProberFunc func(ctx context.Context, iface Interface) ProbeResult

// Probe implements Prober.
func (f ProberFunc) Probe(ctx context.Context, iface Interface) ProbeResult {
	return f(ctx, iface)
}

// Config configures a Manager.
type Config struct {
	Interfaces []Interface
	// RiseThreshold is the number of consecutive successful probes required
	// before an interface is considered healthy (failback hysteresis).
	RiseThreshold int
	// FallThreshold is the number of consecutive failed probes required
	// before a healthy interface is considered unhealthy.
	FallThreshold int
	// SlowRTT is the probe RTT above which the health score is penalised.
	SlowRTT time.Duration
}

// Default thresholds.
const (
	DefaultRiseThreshold = 3
	DefaultFallThreshold = 2
	DefaultSlowRTT       = 300 * time.Millisecond
)

// ErrUnknownInterface is returned when an observation names an interface
// that is not managed.
var ErrUnknownInterface = errors.New("link: unknown interface")

// Change describes a switch of the active uplink. An empty name means
// "no uplink".
type Change struct {
	From string
	To   string
}

// Status is a telemetry snapshot of one interface.
type Status struct {
	Interface Interface
	Healthy   bool
	Score     int
	Active    bool
}

type ifaceState struct {
	iface      Interface
	healthy    bool
	okStreak   int
	failStreak int
	lastRTT    time.Duration
}

// Manager tracks interface health and selects the active uplink.
// It is safe for concurrent use.
type Manager struct {
	mu     sync.Mutex
	cfg    Config
	states []*ifaceState // sorted by preference
	active int          // index into states, -1 if none
}

// NewManager validates cfg and returns a Manager with all interfaces
// initially unhealthy.
func NewManager(cfg Config) (*Manager, error) {
	if len(cfg.Interfaces) == 0 {
		return nil, errors.New("link: no interfaces configured")
	}
	if cfg.RiseThreshold <= 0 {
		cfg.RiseThreshold = DefaultRiseThreshold
	}
	if cfg.FallThreshold <= 0 {
		cfg.FallThreshold = DefaultFallThreshold
	}
	if cfg.SlowRTT <= 0 {
		cfg.SlowRTT = DefaultSlowRTT
	}
	seen := make(map[string]bool, len(cfg.Interfaces))
	states := make([]*ifaceState, 0, len(cfg.Interfaces))
	for _, in := range cfg.Interfaces {
		if in.Name == "" {
			return nil, errors.New("link: interface with empty name")
		}
		if seen[in.Name] {
			return nil, fmt.Errorf("link: duplicate interface %q", in.Name)
		}
		seen[in.Name] = true
		states = append(states, &ifaceState{iface: in})
	}
	sort.SliceStable(states, func(i, j int) bool {
		return states[i].iface.Kind < states[j].iface.Kind
	})
	return &Manager{cfg: cfg, states: states, active: -1}, nil
}

// Observe records one probe result for the named interface and re-runs
// selection. It reports whether the active uplink changed.
func (m *Manager) Observe(name string, res ProbeResult) (Change, bool, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	st := m.find(name)
	if st == nil {
		return Change{}, false, fmt.Errorf("%w: %q", ErrUnknownInterface, name)
	}
	if res.OK {
		st.okStreak++
		st.failStreak = 0
		st.lastRTT = res.RTT
		if !st.healthy && st.okStreak >= m.cfg.RiseThreshold {
			st.healthy = true
		}
	} else {
		st.failStreak++
		st.okStreak = 0
		if st.healthy && st.failStreak >= m.cfg.FallThreshold {
			st.healthy = false
		}
	}
	ch, changed := m.reselect()
	return ch, changed, nil
}

// ProbeAll probes every interface once (without holding the lock while
// probing) and reports the net change of the active uplink.
func (m *Manager) ProbeAll(ctx context.Context, p Prober) (Change, bool) {
	m.mu.Lock()
	ifaces := make([]Interface, len(m.states))
	for i, st := range m.states {
		ifaces[i] = st.iface
	}
	before := m.activeName()
	m.mu.Unlock()

	for _, in := range ifaces {
		if ctx.Err() != nil {
			break
		}
		res := p.Probe(ctx, in)
		// Names come from our own list, so ErrUnknownInterface is impossible.
		_, _, _ = m.Observe(in.Name, res)
	}

	m.mu.Lock()
	after := m.activeName()
	m.mu.Unlock()
	if before == after {
		return Change{}, false
	}
	return Change{From: before, To: after}, true
}

// Active returns the currently selected uplink.
func (m *Manager) Active() (Interface, bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.active < 0 {
		return Interface{}, false
	}
	return m.states[m.active].iface, true
}

// AllowUpload applies the metered-link policy: nothing is uploaded without
// an uplink, and only high-priority incidents are uploaded over a metered
// uplink.
func (m *Manager) AllowUpload(p Priority) bool {
	in, ok := m.Active()
	if !ok {
		return false
	}
	if in.Metered {
		return p >= PriorityHigh
	}
	return true
}

// Snapshot returns per-interface telemetry in preference order.
func (m *Manager) Snapshot() []Status {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]Status, len(m.states))
	for i, st := range m.states {
		out[i] = Status{
			Interface: st.iface,
			Healthy:   st.healthy,
			Score:     m.score(st),
			Active:    i == m.active,
		}
	}
	return out
}

// Score returns the 0..100 health score of the named interface.
func (m *Manager) Score(name string) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	st := m.find(name)
	if st == nil {
		return 0, fmt.Errorf("%w: %q", ErrUnknownInterface, name)
	}
	return m.score(st), nil
}

// score is a telemetry signal; selection uses the healthy flag only so the
// policy stays easy to reason about.
func (m *Manager) score(st *ifaceState) int {
	s := 0
	if st.healthy {
		s = 100
	} else {
		s = 10 * st.okStreak
		if s > 50 {
			s = 50
		}
	}
	s -= 25 * st.failStreak
	if st.okStreak > 0 && st.lastRTT > m.cfg.SlowRTT {
		s -= 20
	}
	if s < 0 {
		s = 0
	}
	if s > 100 {
		s = 100
	}
	return s
}

func (m *Manager) find(name string) *ifaceState {
	for _, st := range m.states {
		if st.iface.Name == name {
			return st
		}
	}
	return nil
}

func (m *Manager) activeName() string {
	if m.active < 0 {
		return ""
	}
	return m.states[m.active].iface.Name
}

// reselect must be called with m.mu held.
func (m *Manager) reselect() (Change, bool) {
	best := -1
	for i, st := range m.states {
		if st.healthy {
			best = i
			break
		}
	}
	if best == m.active {
		return Change{}, false
	}
	ch := Change{From: m.activeName()}
	m.active = best
	ch.To = m.activeName()
	return ch, true
}
