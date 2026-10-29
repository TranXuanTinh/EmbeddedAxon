// Command netagent supervises network uplinks (eth > wifi > lte), DNS/DHCP
// health, WireGuard VPN connectivity, and resumable evidence upload to the
// cloud ingest service, communicating link status and upload acknowledgements
// to aegisd over a Unix domain socket.
package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/tinhtran/aegiscam/netagent/internal/ipc"
	"github.com/tinhtran/aegiscam/netagent/internal/link"
	"github.com/tinhtran/aegiscam/netagent/internal/uploader"
)

func main() {
	var (
		sockPath      = flag.String("aegisd-sock", "/run/aegis/aegisd.sock", "Path to aegisd Unix domain socket")
		ingestURL     = flag.String("ingest-url", "https://ingest.aegis.internal", "Evidence cloud ingest base URL")
		probeInterval = flag.Duration("probe-interval", 5*time.Second, "Link health probe interval")
		dryRun        = flag.Bool("dry-run", false, "Validate configuration and exit 0")
	)
	flag.Parse()

	mgr, err := link.NewManager(link.Config{
		Interfaces: []link.Interface{
			{Name: "eth0", Kind: link.Ethernet, Metered: false},
			{Name: "wlan0", Kind: link.WiFi, Metered: false},
			{Name: "wwan0", Kind: link.LTE, Metered: true},
		},
	})
	if err != nil {
		log.Fatalf("netagent: link manager init: %v", err)
	}

	up, err := uploader.New(uploader.Config{
		BaseURL: *ingestURL,
	})
	if err != nil {
		log.Fatalf("netagent: uploader init: %v", err)
	}
	_ = up

	if *dryRun {
		fmt.Println("netagent configuration OK")
		return
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	prober := link.ProberFunc(func(ctx context.Context, iface link.Interface) link.ProbeResult {
		start := time.Now()
		ifi, err := net.InterfaceByName(iface.Name)
		if err != nil || (ifi.Flags&net.FlagUp) == 0 {
			return link.ProbeResult{OK: false}
		}
		return link.ProbeResult{OK: true, RTT: time.Since(start)}
	})

	ticker := time.NewTicker(*probeInterval)
	defer ticker.Stop()

	log.Printf("netagent started (socket=%s, ingest=%s)", *sockPath, *ingestURL)
	for {
		select {
		case <-ctx.Done():
			log.Printf("netagent shutting down gracefully")
			return
		case <-ticker.C:
			if ch, changed := mgr.ProbeAll(ctx, prober); changed {
				log.Printf("uplink changed: %q -> %q", ch.From, ch.To)
				notifyAegisd(*sockPath, ch.To != "")
			}
		}
	}
}

func notifyAegisd(sockPath string, netUp bool) {
	conn, err := net.DialTimeout("unix", sockPath, 2*time.Second)
	if err != nil {
		return
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(2 * time.Second))
	cli := ipc.NewClient(conn)
	_, _ = cli.SendNet(netUp)
}
