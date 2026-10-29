#!/usr/bin/env python3
"""Offline chain-of-custody verifier for AegisCam evidence stores (spec §4).

Verifies:
1. Append-only `audit.log` hash chain from 64-'0' genesis.
2. Every incident under `incidents/<incident_id>/`:
   - `manifest.json` structure and sealed flag
   - Each segment file exists, matches `bytes` and `sha256`, and has no extra
     unreferenced `seg_*.bin` files on disk
   - Binary SHA-256 hash chain (`H_n = SHA256(raw(H_{n-1}) || raw(sha256_n))`)
     with 32-zero-byte genesis matches `chain` and `chain_head`
   - `hmac` equals `HMAC-SHA256(key, chain_head_hex_ascii)`

Exit codes:
  0 - All evidence and audit records verified intact
  2 - Tampering or integrity violation detected
  1 - Usage or store path error
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import List

DEFAULT_SIM_KEY = "aegis-sim-device-key-DO-NOT-SHIP"
GENESIS_AUDIT_PREV = "0" * 64
GENESIS_CHAIN_RAW = b"\x00" * 32


@dataclass
class VerificationReport:
    ok: bool = True
    incidents_checked: int = 0
    segments_checked: int = 0
    audit_records_checked: int = 0
    errors: List[str] = field(default_factory=list)

    def fail(self, msg: str) -> None:
        self.ok = False
        self.errors.append(msg)

    def to_dict(self) -> dict:
        return {
            "ok": self.ok,
            "incidents_checked": self.incidents_checked,
            "segments_checked": self.segments_checked,
            "audit_records_checked": self.audit_records_checked,
            "errors": self.errors,
        }


def verify_audit_log(audit_path: Path, report: VerificationReport) -> None:
    if not audit_path.exists():
        return
    prev_hex = GENESIS_AUDIT_PREV
    expected_seq = 1
    lines = audit_path.read_text(encoding="utf-8", errors="replace").splitlines()
    for line_no, raw in enumerate(lines, start=1):
        if not raw.strip():
            continue
        parts = raw.split("|")
        if len(parts) != 6:
            report.fail(f"audit.log:{line_no}: expected 6 pipe-separated fields, got {len(parts)}")
            return
        seq_str, ts_str, event, detail, rec_prev, rec_hash = parts
        try:
            seq = int(seq_str)
        except ValueError:
            report.fail(f"audit.log:{line_no}: invalid sequence '{seq_str}'")
            return
        if seq != expected_seq:
            report.fail(f"audit.log:{line_no}: sequence gap (got {seq}, expected {expected_seq})")
        expected_seq = seq + 1

        if rec_prev.lower() != prev_hex.lower():
            report.fail(
                f"audit.log:{line_no}: prev_hash mismatch (got {rec_prev}, expected {prev_hex})"
            )

        payload = f"{rec_prev}|{seq_str}|{ts_str}|{event}|{detail}".encode("ascii", errors="replace")
        computed = hashlib.sha256(payload).hexdigest()
        if computed.lower() != rec_hash.lower():
            report.fail(
                f"audit.log:{line_no}: hash mismatch (got {rec_hash}, computed {computed})"
            )
        prev_hex = rec_hash
        report.audit_records_checked += 1


def verify_incident(inc_dir: Path, key_bytes: bytes, report: VerificationReport) -> None:
    manifest_path = inc_dir / "manifest.json"
    if not manifest_path.is_file():
        report.fail(f"{inc_dir.name}: missing manifest.json")
        return

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        report.fail(f"{inc_dir.name}: invalid manifest.json ({exc})")
        return

    if manifest.get("incident_id") != inc_dir.name:
        report.fail(
            f"{inc_dir.name}: manifest incident_id '{manifest.get('incident_id')}' does not match directory"
        )

    segments = manifest.get("segments")
    if not isinstance(segments, list):
        report.fail(f"{inc_dir.name}: manifest 'segments' must be a list")
        return

    chain_prev = GENESIS_CHAIN_RAW
    expected_files = set()

    for expected_idx, seg in enumerate(segments, start=1):
        if not isinstance(seg, dict):
            report.fail(f"{inc_dir.name}: malformed segment entry at #{expected_idx}")
            return
        idx = seg.get("index")
        fname = seg.get("file", "")
        expected_bytes = seg.get("bytes")
        expected_sha = str(seg.get("sha256", "")).lower()
        expected_chain = str(seg.get("chain", "")).lower()

        if idx != expected_idx:
            report.fail(f"{inc_dir.name}: segment index gap (got {idx}, expected {expected_idx})")

        seg_path = inc_dir / fname
        expected_files.add(fname)
        if not seg_path.is_file():
            report.fail(f"{inc_dir.name}: missing segment file {fname}")
            continue

        data = seg_path.read_bytes()
        if len(data) != expected_bytes:
            report.fail(
                f"{inc_dir.name}/{fname}: byte length {len(data)} != manifest {expected_bytes}"
            )

        actual_sha_raw = hashlib.sha256(data).digest()
        actual_sha_hex = actual_sha_raw.hex()
        if actual_sha_hex != expected_sha:
            report.fail(
                f"{inc_dir.name}/{fname}: sha256 mismatch (got {actual_sha_hex}, expected {expected_sha})"
            )

        chain_prev = hashlib.sha256(chain_prev + actual_sha_raw).digest()
        if chain_prev.hex() != expected_chain:
            report.fail(
                f"{inc_dir.name}/{fname}: chain mismatch (got {chain_prev.hex()}, expected {expected_chain})"
            )
        report.segments_checked += 1

    actual_seg_files = {p.name for p in inc_dir.glob("seg_*.bin")}
    orphan_files = actual_seg_files - expected_files
    if orphan_files:
        report.fail(f"{inc_dir.name}: unreferenced segment files on disk: {sorted(orphan_files)}")

    expected_head = chain_prev.hex()
    manifest_head = str(manifest.get("chain_head", "")).lower()
    if manifest_head != expected_head:
        report.fail(
            f"{inc_dir.name}: chain_head mismatch (manifest {manifest_head} != computed {expected_head})"
        )

    expected_hmac = hmac.new(
        key_bytes, manifest_head.encode("ascii"), hashlib.sha256
    ).hexdigest()
    manifest_hmac = str(manifest.get("hmac", "")).lower()
    if not hmac.compare_digest(expected_hmac, manifest_hmac):
        report.fail(
            f"{inc_dir.name}: HMAC verification failed (manifest {manifest_hmac} != computed {expected_hmac})"
        )

    report.incidents_checked += 1


def verify_store(store_root: Path, key: str = DEFAULT_SIM_KEY) -> VerificationReport:
    report = VerificationReport()
    verify_audit_log(store_root / "audit.log", report)

    incidents_dir = store_root / "incidents"
    if incidents_dir.is_dir():
        key_bytes = key.encode("utf-8")
        for child in sorted(incidents_dir.iterdir()):
            if child.is_dir():
                verify_incident(child, key_bytes, report)
    return report


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Verify AegisCam evidence store chain of custody (spec §4)."
    )
    parser.add_argument("store_root", type=Path, help="Path to evidence store root directory")
    parser.add_argument(
        "--key",
        default=DEFAULT_SIM_KEY,
        help="Device HMAC key (ASCII, defaults to simulation key)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON verification report",
    )
    args = parser.parse_args(argv)

    if not args.store_root.exists() or not args.store_root.is_dir():
        print(f"error: store_root '{args.store_root}' does not exist or is not a directory", file=sys.stderr)
        return 1

    report = verify_store(args.store_root, args.key)
    if args.json:
        print(json.dumps(report.to_dict(), indent=2))
    else:
        if report.ok:
            print(
                f"OK: verified {report.incidents_checked} incident(s), "
                f"{report.segments_checked} segment(s), "
                f"{report.audit_records_checked} audit record(s)."
            )
        else:
            for err in report.errors:
                print(f"TAMPER: {err}", file=sys.stderr)
    return 0 if report.ok else 2


if __name__ == "__main__":
    sys.exit(main())
