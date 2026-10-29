# AegisCam `netagent`

`netagent` is the Go 1.22 connectivity, VPN, and resumable evidence-upload daemon for AegisCam. It runs alongside the real-time C++17 `aegisd` core daemon in an isolated process so that network stack latency, TLS handshakes, and cloud retries never contend with the real-time capture and evidence-sealing threads.

## Architecture & Packages

All packages use the Go standard library only (`go.mod` has zero external dependencies) and inject every I/O boundary for deterministic, network-free unit tests.

| Package | Responsibility |
|---------|----------------|
| `internal/link` | Multi-uplink manager (`eth > wifi > lte`) with probe-streak hysteresis (`RiseThreshold`, `FallThreshold`), RTT-penalised health scoring, and metered-link upload policy (LTE restricted to `PriorityHigh`). |
| `internal/dnscheck` | Primary/secondary DNS failover, TTL answer cache, and canary-prefix verification to detect captive portals or DNS hijacking. |
| `internal/dhcp` | RFC 2131 lease timer model (`T1 = 50%`, `T2 = 87.5%`, expiry), state machine (`BOUND -> RENEWING -> REBINDING -> INIT`), and `udhcpc`/`dhclient` lease parser. |
| `internal/vpn` | `wg-quick` configuration renderer and WireGuard tunnel supervisor (handshake age check, exponential backoff restarts, and upload kill-switch). |
| `internal/uploader` | Resumable chunked HTTP `PATCH` uploader (4 MiB default chunks, `HEAD` offset discovery, per-chunk `X-Chunk-SHA256`, exponential backoff with full jitter, max in-flight semaphore, and token-bucket bandwidth cap). |
| `internal/ipc` | Client for the `aegisd` ASCII line protocol (`NET up\|down`, `UPLOAD_ACK <incident> <index>`, and asynchronous `EVT {...}` stream). |
| `cmd/netagent` | Daemon entrypoint with `SIGTERM`/`SIGINT` graceful shutdown. |

## Running Tests

```bash
cd netagent
go test -race -v ./...
```
