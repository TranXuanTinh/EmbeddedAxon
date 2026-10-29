// Package dhcp models DHCPv4 lease timing (T1 = 50%, T2 = 87.5%, expiry),
// parses dhclient/udhcpc-style key=value or block lease files, and drives the
// BOUND -> RENEWING -> REBINDING -> INIT state machine.
package dhcp

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"net/netip"
	"strconv"
	"strings"
	"time"
)

// State is the RFC 2131 client state subset tracked by netagent.
type State int

// DHCP client states.
const (
	StateInit State = iota
	StateBound
	StateRenewing
	StateRebinding
)

// String returns the RFC 2131 state name.
func (s State) String() string {
	switch s {
	case StateInit:
		return "INIT"
	case StateBound:
		return "BOUND"
	case StateRenewing:
		return "RENEWING"
	case StateRebinding:
		return "REBINDING"
	default:
		return fmt.Sprintf("State(%d)", int(s))
	}
}

// Lease represents an acquired DHCPv4 lease.
type Lease struct {
	Interface string
	IP        netip.Addr
	MaskBits  int
	Router    netip.Addr
	DNS       []netip.Addr
	ServerID  netip.Addr
	Obtained  time.Time
	LeaseTime time.Duration
}

// T1 returns the renewal time (50% of LeaseTime).
func (l Lease) T1() time.Time {
	return l.Obtained.Add(l.LeaseTime / 2)
}

// T2 returns the rebinding time (87.5% of LeaseTime = 7/8).
func (l Lease) T2() time.Time {
	return l.Obtained.Add((l.LeaseTime * 7) / 8)
}

// Expiry returns the time at which the lease expires.
func (l Lease) Expiry() time.Time {
	return l.Obtained.Add(l.LeaseTime)
}

// StateAt evaluates the client state at timestamp now.
func (l Lease) StateAt(now time.Time) State {
	if l.LeaseTime <= 0 || !l.IP.IsValid() || !now.Before(l.Expiry()) {
		return StateInit
	}
	if !now.Before(l.T2()) {
		return StateRebinding
	}
	if !now.Before(l.T1()) {
		return StateRenewing
	}
	return StateBound
}

// ParseLease parses a key=value lease file (udhcpc environment dump or
// simplified dhclient summary) into a Lease.
//
// Supported keys: interface, ip/fixed-address, mask/subnet_mask_bits, router,
// dns (space/comma separated), server_id, obtained_unix, lease_seconds.
func ParseLease(r io.Reader) (Lease, error) {
	var l Lease
	sc := bufio.NewScanner(r)
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		line = strings.TrimSuffix(line, ";")
		k, v, ok := splitKeyVal(line)
		if !ok {
			continue
		}
		v = strings.Trim(v, `"' `)
		switch k {
		case "interface":
			l.Interface = v
		case "ip", "fixed-address":
			addr, err := netip.ParseAddr(v)
			if err != nil {
				return Lease{}, fmt.Errorf("dhcp: invalid ip %q: %w", v, err)
			}
			l.IP = addr
		case "mask", "subnet_mask_bits":
			bits, err := strconv.Atoi(v)
			if err != nil || bits < 0 || bits > 32 {
				return Lease{}, fmt.Errorf("dhcp: invalid mask %q", v)
			}
			l.MaskBits = bits
		case "router", "routers":
			addr, err := netip.ParseAddr(strings.Fields(v)[0])
			if err != nil {
				return Lease{}, fmt.Errorf("dhcp: invalid router %q: %w", v, err)
			}
			l.Router = addr
		case "dns", "domain-name-servers":
			for _, tok := range strings.FieldsFunc(v, func(r rune) bool { return r == ',' || r == ' ' }) {
				if tok == "" {
					continue
				}
				addr, err := netip.ParseAddr(tok)
				if err != nil {
					return Lease{}, fmt.Errorf("dhcp: invalid dns %q: %w", tok, err)
				}
				l.DNS = append(l.DNS, addr)
			}
		case "server_id", "dhcp-server-identifier":
			addr, err := netip.ParseAddr(v)
			if err != nil {
				return Lease{}, fmt.Errorf("dhcp: invalid server_id %q: %w", v, err)
			}
			l.ServerID = addr
		case "obtained_unix":
			sec, err := strconv.ParseInt(v, 10, 64)
			if err != nil {
				return Lease{}, fmt.Errorf("dhcp: invalid obtained_unix %q: %w", v, err)
			}
			l.Obtained = time.Unix(sec, 0).UTC()
		case "lease_seconds", "dhcp-lease-time":
			sec, err := strconv.ParseInt(v, 10, 64)
			if err != nil || sec <= 0 {
				return Lease{}, fmt.Errorf("dhcp: invalid lease_seconds %q", v)
			}
			l.LeaseTime = time.Duration(sec) * time.Second
		}
	}
	if err := sc.Err(); err != nil {
		return Lease{}, err
	}
	if !l.IP.IsValid() || l.LeaseTime <= 0 {
		return Lease{}, errors.New("dhcp: incomplete lease (missing ip or lease_seconds)")
	}
	return l, nil
}

func splitKeyVal(line string) (string, string, bool) {
	if idx := strings.IndexByte(line, '='); idx > 0 {
		return strings.ToLower(strings.TrimSpace(line[:idx])), strings.TrimSpace(line[idx+1:]), true
	}
	fields := strings.Fields(line)
	if len(fields) >= 2 {
		if fields[0] == "option" && len(fields) >= 3 {
			return strings.ToLower(fields[1]), strings.Join(fields[2:], " "), true
		}
		return strings.ToLower(fields[0]), strings.Join(fields[1:], " "), true
	}
	return "", "", false
}
