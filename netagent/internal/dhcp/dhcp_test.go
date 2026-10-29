package dhcp

import (
	"strings"
	"testing"
	"time"
)

func TestParseLeaseAndStateMachine(t *testing.T) {
	raw := `
# udhcpc lease file
interface=eth0
ip=192.168.10.42
mask=24
router=192.168.10.1
dns="10.0.0.1, 1.1.1.1"
server_id=192.168.10.1
obtained_unix=1700000000
lease_seconds=1600
`
	lease, err := ParseLease(strings.NewReader(raw))
	if err != nil {
		t.Fatalf("ParseLease: %v", err)
	}
	if lease.Interface != "eth0" || lease.IP.String() != "192.168.10.42" || len(lease.DNS) != 2 {
		t.Fatalf("unexpected parsed lease: %+v", lease)
	}

	t0 := time.Unix(1700000000, 0).UTC()
	cases := []struct {
		offset time.Duration
		want   State
	}{
		{0, StateBound},
		{799 * time.Second, StateBound},
		{800 * time.Second, StateRenewing}, // T1 = 50% of 1600s = 800s
		{1399 * time.Second, StateRenewing},
		{1400 * time.Second, StateRebinding}, // T2 = 87.5% of 1600s = 1400s
		{1599 * time.Second, StateRebinding},
		{1600 * time.Second, StateInit}, // Expired
	}
	for _, tc := range cases {
		got := lease.StateAt(t0.Add(tc.offset))
		if got != tc.want {
			t.Fatalf("at +%v: got %s, want %s", tc.offset, got, tc.want)
		}
	}
}

func TestParseLeaseErrors(t *testing.T) {
	badInputs := []string{
		"",
		"ip=not-an-ip\nlease_seconds=600\n",
		"ip=10.0.0.2\nlease_seconds=-5\n",
	}
	for _, in := range badInputs {
		if _, err := ParseLease(strings.NewReader(in)); err == nil {
			t.Fatalf("expected error for input %q", in)
		}
	}
}
