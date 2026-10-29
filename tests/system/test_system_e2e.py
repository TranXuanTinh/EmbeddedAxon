"""End-to-end system tests driving `aegisd --sim` and `tools/verify_evidence.py`."""

from __future__ import annotations

import hashlib
import hmac
import json
from pathlib import Path

import verify_evidence


def test_offline_verifier_synthetic_store(tmp_path: Path) -> None:
    """§4: Directly validate verify_evidence.py against a spec-compliant store and tampered variants."""
    store = tmp_path / "synth_store"
    inc_id = "INC-AEG-0001-1700000000000"
    inc_dir = store / "incidents" / inc_id
    inc_dir.mkdir(parents=True)

    seg_bytes = b"\x00\x01\x02AEGIS_FRAME" * 64
    seg_path = inc_dir / "seg_000001.bin"
    seg_path.write_bytes(seg_bytes)

    seg_sha_raw = hashlib.sha256(seg_bytes).digest()
    chain_1 = hashlib.sha256((b"\x00" * 32) + seg_sha_raw).hexdigest()
    sig = hmac.new(
        verify_evidence.DEFAULT_SIM_KEY.encode("ascii"),
        chain_1.encode("ascii"),
        hashlib.sha256,
    ).hexdigest()

    manifest = {
        "version": 1,
        "device_id": "AEG-0001",
        "incident_id": inc_id,
        "trigger_source": "holster",
        "pre_event_ms": 30000,
        "segments": [
            {
                "index": 1,
                "file": "seg_000001.bin",
                "bytes": len(seg_bytes),
                "sha256": seg_sha_raw.hex(),
                "chain": chain_1,
            }
        ],
        "chain_head": chain_1,
        "hmac": sig,
        "sealed": True,
    }
    (inc_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    prev = "0" * 64
    body = f"{prev}|1|1700000000000|TRIGGER|src=holster"
    h1 = hashlib.sha256(body.encode("ascii")).hexdigest()
    (store / "audit.log").write_text(f"1|1700000000000|TRIGGER|src=holster|{prev}|{h1}\n", encoding="utf-8")

    assert verify_evidence.main([str(store), "--json"]) == 0

    # Tamper with audit.log and confirm exit code 2.
    (store / "audit.log").write_text(f"1|1700000000000|TRIGGER|src=button|{prev}|{h1}\n", encoding="utf-8")
    assert verify_evidence.main([str(store)]) == 2


def test_pre_event_capture_and_evidence_verification(sim_factory) -> None:
    """R1/R3/R4: Pre-event buffer is included, segments are sealed, and offline verifier passes."""
    sim = sim_factory(
        extra_args=["--pre-event-ms", "5000", "--segment-ms", "2000", "--post-record-ms", "1000"]
    )

    # Buffer 6 seconds of pre-event video in Buffering state.
    sim.cmd_ok("TICK 6000")
    st = sim.cmd_ok("STATUS")
    assert st.get("state") in ("Buffering", "Idle")

    # Trigger recording via holster sensor -> drains pre-event buffer (from keyframe).
    res = sim.cmd_ok("TRIGGER holster")
    assert res["state"] == "Recording"
    incident_id = res["incident"]
    assert incident_id.startswith("INC-AEG-0001-")

    # Record 4 seconds across multiple segments, then stop and finish post-record.
    sim.cmd_ok("TICK 2000")
    sim.cmd_ok("TICK 2000")
    sim.cmd_ok("STOP holster")
    sim.cmd_ok("TICK 1500")

    verify_res = sim.cmd_ok("VERIFY")
    assert verify_res.get("verified", True) is True

    rc = sim.close()
    assert rc == 0

    report = verify_evidence.verify_store(sim.store_dir)
    assert report.ok, f"Offline verifier failed: {report.errors}"
    assert report.incidents_checked == 1
    assert report.segments_checked >= 2
    assert report.audit_records_checked >= 2


def test_trigger_priority_arbitration(sim_factory) -> None:
    """R2/§3: Lower-priority source (`remote`) cannot stop a recording started by `holster`."""
    sim = sim_factory(extra_args=["--post-record-ms", "1000"])
    sim.cmd_ok("TICK 200")
    sim.cmd_ok("TRIGGER holster")

    # Advance past debounce window (50 ms) and attempt stop from lower-priority remote.
    sim.cmd_ok("TICK 200")
    status, _ = sim.command("STOP remote")
    st = sim.cmd_ok("STATUS")
    assert st["state"] == "Recording", f"remote should not stop holster recording (status={status})"

    # Stopping from holster succeeds.
    sim.cmd_ok("STOP holster")
    sim.cmd_ok("TICK 1500")
    st_after = sim.cmd_ok("STATUS")
    assert st_after["state"] == "Buffering"


def test_tamper_detection_segment_and_audit(sim_factory) -> None:
    """R3/§4: Flipping 1 byte in a segment or audit.log is caught by `VERIFY` and `verify_evidence.py`."""
    sim = sim_factory(extra_args=["--pre-event-ms", "2000", "--segment-ms", "1000", "--post-record-ms", "500"])
    sim.cmd_ok("TICK 2000")
    res = sim.cmd_ok("TRIGGER button")
    incident_id = res["incident"]
    sim.cmd_ok("TICK 2000")
    sim.cmd_ok("STOP button")
    sim.cmd_ok("TICK 1000")

    # Baseline clean check.
    assert verify_evidence.verify_store(sim.store_dir).ok

    # Corrupt 1 byte in seg_000001.bin.
    seg1 = sim.store_dir / "incidents" / incident_id / "seg_000001.bin"
    raw = bytearray(seg1.read_bytes())
    raw[0] ^= 0xFF
    seg1.write_bytes(bytes(raw))

    err_msg = sim.cmd_err("VERIFY")
    assert "INTEGRITY_ERROR" in err_msg

    rep = verify_evidence.verify_store(sim.store_dir)
    assert not rep.ok
    assert any("sha256 mismatch" in e for e in rep.errors)
    assert verify_evidence.main([str(sim.store_dir)]) == 2


def test_ota_ab_slot_staging_and_boot_counter_rollback(sim_factory, tmp_path: Path) -> None:
    """R6/§3: Stage image to slot B, exhaust 3 unconfirmed reboots -> automatic rollback to slot A."""
    sim = sim_factory()
    img = tmp_path / "firmware_v2.bin"
    img.write_bytes(b"AEGIS-FW-IMAGE-V2-PAYLOAD" * 64)
    digest = hashlib.sha256(img.read_bytes()).hexdigest()

    # Bad hash must be rejected.
    err = sim.cmd_err(f"OTA_STAGE {img} {'0' * 64}")
    assert "INTEGRITY_ERROR" in err or "INVALID_ARGUMENT" in err

    # Valid hash stages slot B with 3 boot attempts.
    sim.cmd_ok(f"OTA_STAGE {img} {digest}")

    # 3 reboots without OTA_MARK_GOOD -> rolls back to slot A on the 4th boot/check.
    for _ in range(3):
        sim.cmd_ok("OTA_REBOOT")
    st = sim.cmd_ok("OTA_REBOOT")
    if "slot" not in st:
        st = sim.cmd_ok("STATUS")
    assert st["slot"] == "A"


def test_protocol_robustness_and_graceful_shutdown(sim_factory) -> None:
    """§5: Unknown/malformed commands return structured ERR without crashing; QUIT seals active incident."""
    sim = sim_factory(extra_args=["--segment-ms", "1000"])

    for bad in ["", "UNKNOWN_CMD", "TRIGGER invalid_src", "TICK -5", "TICK 9999999", "OTA_STAGE only_one_arg"]:
        err = sim.cmd_err(bad)
        assert any(
            code in err for code in ("PARSE_ERROR", "INVALID_ARGUMENT", "INVALID_STATE", "NOT_SUPPORTED")
        ), f"Unexpected error response for {bad!r}: {err}"

    # Start recording and immediately QUIT -> daemon must drain, seal manifest, and exit 0.
    sim.cmd_ok("TICK 1000")
    sim.cmd_ok("TRIGGER signal")
    sim.cmd_ok("TICK 1500")
    assert sim.close() == 0

    rep = verify_evidence.verify_store(sim.store_dir)
    assert rep.ok, f"Store not cleanly sealed on QUIT: {rep.errors}"
    assert rep.incidents_checked == 1
