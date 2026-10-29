package link

import (
	"context"
	"errors"
	"testing"
	"time"
)

func TestParseKind(t *testing.T) {
	cases := []struct {
		in      string
		want    Kind
		wantErr bool
	}{
		{"eth", Ethernet, false},
		{"Ethernet", Ethernet, false},
		{"wifi", WiFi, false},
		{"wlan", WiFi, false},
		{"lte", LTE, false},
		{"cellular", LTE, false},
		{"satellite", 0, true},
	}
	for _, tc := range cases {
		got, err := ParseKind(tc.in)
		if (err != nil) != tc.wantErr {
			t.Fatalf("ParseKind(%q) err=%v, wantErr=%v", tc.in, err, tc.wantErr)
		}
		if !tc.wantErr && got != tc.want {
			t.Fatalf("ParseKind(%q)=%v, want %v", tc.in, got, tc.want)
		}
	}
	if Ethernet.String() != "eth" || WiFi.String() != "wifi" || LTE.String() != "lte" {
		t.Fatalf("unexpected Kind.String values")
	}
}

func TestNewManagerValidation(t *testing.T) {
	if _, err := NewManager(Config{}); err == nil {
		t.Fatal("expected error for empty interfaces")
	}
	if _, err := NewManager(Config{Interfaces: []Interface{{Name: ""}}}); err == nil {
		t.Fatal("expected error for unnamed interface")
	}
	if _, err := NewManager(Config{Interfaces: []Interface{
		{Name: "eth0", Kind: Ethernet},
		{Name: "eth0", Kind: WiFi},
	}}); err == nil {
		t.Fatal("expected error for duplicate interface")
	}
}

func TestFailoverAndHystereticFailback(t *testing.T) {
	mgr, err := NewManager(Config{
		Interfaces: []Interface{
			{Name: "wwan0", Kind: LTE, Metered: true},
			{Name: "wlan0", Kind: WiFi},
			{Name: "eth0", Kind: Ethernet},
		},
		RiseThreshold: 3,
		FallThreshold: 2,
	})
	if err != nil {
		t.Fatalf("NewManager: %v", err)
	}

	if _, ok := mgr.Active(); ok {
		t.Fatal("expected no active interface initially")
	}

	// Bring wlan0 up (3 consecutive successes).
	for i := 0; i < 2; i++ {
		if _, changed, _ := mgr.Observe("wlan0", ProbeResult{OK: true, RTT: 20 * time.Millisecond}); changed {
			t.Fatalf("wlan0 became active prematurely at probe %d", i+1)
		}
	}
	ch, changed, err := mgr.Observe("wlan0", ProbeResult{OK: true, RTT: 20 * time.Millisecond})
	if err != nil || !changed || ch.From != "" || ch.To != "wlan0" {
		t.Fatalf("expected switch to wlan0, got ch=%+v changed=%v err=%v", ch, changed, err)
	}

	// eth0 flaps: 2 successes then 1 failure must NOT preempt wlan0.
	_, _, _ = mgr.Observe("eth0", ProbeResult{OK: true, RTT: 5 * time.Millisecond})
	_, _, _ = mgr.Observe("eth0", ProbeResult{OK: true, RTT: 5 * time.Millisecond})
	_, changed, _ = mgr.Observe("eth0", ProbeResult{OK: false})
	if changed {
		t.Fatal("flapping eth0 should not have taken over")
	}

	// Now eth0 succeeds 3 times in a row -> failback to eth0.
	_, _, _ = mgr.Observe("eth0", ProbeResult{OK: true, RTT: 5 * time.Millisecond})
	_, _, _ = mgr.Observe("eth0", ProbeResult{OK: true, RTT: 5 * time.Millisecond})
	ch, changed, _ = mgr.Observe("eth0", ProbeResult{OK: true, RTT: 5 * time.Millisecond})
	if !changed || ch.From != "wlan0" || ch.To != "eth0" {
		t.Fatalf("expected failback wlan0->eth0, got %+v (changed=%v)", ch, changed)
	}

	// 1 failure on eth0 is below FallThreshold=2 -> stays on eth0.
	if _, changed, _ := mgr.Observe("eth0", ProbeResult{OK: false}); changed {
		t.Fatal("single failure should not trigger failover")
	}
	// 2nd consecutive failure -> failover to still-healthy wlan0.
	ch, changed, _ = mgr.Observe("eth0", ProbeResult{OK: false})
	if !changed || ch.From != "eth0" || ch.To != "wlan0" {
		t.Fatalf("expected failover eth0->wlan0, got %+v (changed=%v)", ch, changed)
	}
}

func TestMeteredUploadPolicy(t *testing.T) {
	mgr, err := NewManager(Config{
		Interfaces: []Interface{
			{Name: "wlan0", Kind: WiFi, Metered: false},
			{Name: "wwan0", Kind: LTE, Metered: true},
		},
		RiseThreshold: 1,
		FallThreshold: 1,
	})
	if err != nil {
		t.Fatal(err)
	}

	// No link up -> no upload.
	if mgr.AllowUpload(PriorityHigh) {
		t.Fatal("AllowUpload should be false when no link is active")
	}

	// Only LTE up -> normal blocked, high allowed.
	_, _, _ = mgr.Observe("wwan0", ProbeResult{OK: true})
	if mgr.AllowUpload(PriorityNormal) {
		t.Fatal("normal priority must be blocked on metered LTE")
	}
	if !mgr.AllowUpload(PriorityHigh) {
		t.Fatal("high priority must be allowed on metered LTE")
	}

	// WiFi comes up -> normal allowed.
	_, _, _ = mgr.Observe("wlan0", ProbeResult{OK: true})
	if !mgr.AllowUpload(PriorityNormal) {
		t.Fatal("normal priority must be allowed on unmetered WiFi")
	}
}

func TestProbeAllAndScoring(t *testing.T) {
	mgr, err := NewManager(Config{
		Interfaces: []Interface{
			{Name: "eth0", Kind: Ethernet},
			{Name: "wlan0", Kind: WiFi},
		},
		RiseThreshold: 1,
		FallThreshold: 1,
		SlowRTT:       100 * time.Millisecond,
	})
	if err != nil {
		t.Fatal(err)
	}

	prober := ProberFunc(func(_ context.Context, iface Interface) ProbeResult {
		if iface.Name == "eth0" {
			return ProbeResult{OK: true, RTT: 250 * time.Millisecond} // slow
		}
		return ProbeResult{OK: false}
	})

	ch, changed := mgr.ProbeAll(context.Background(), prober)
	if !changed || ch.To != "eth0" {
		t.Fatalf("ProbeAll expected switch to eth0, got %+v changed=%v", ch, changed)
	}

	score, err := mgr.Score("eth0")
	if err != nil || score != 80 {
		t.Fatalf("expected slow healthy score 80, got %d err=%v", score, err)
	}
	if _, err := mgr.Score("nonexistent"); !errors.Is(err, ErrUnknownInterface) {
		t.Fatalf("expected ErrUnknownInterface, got %v", err)
	}

	snap := mgr.Snapshot()
	if len(snap) != 2 || !snap[0].Active || snap[0].Interface.Name != "eth0" {
		t.Fatalf("unexpected snapshot: %+v", snap)
	}
}
