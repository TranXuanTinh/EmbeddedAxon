"""Pytest fixtures for driving `aegisd --sim` over its stdin/stdout line protocol."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable, Dict, List, Tuple

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))


def _find_aegisd_binary() -> Path:
    env_bin = os.environ.get("AEGISD_BIN")
    if env_bin:
        p = Path(env_bin)
        if p.is_file():
            return p

    candidates = [
        REPO_ROOT / "build" / "dev" / "core" / "aegisd",
        REPO_ROOT / "build" / "dev" / "core" / "aegisd.exe",
        REPO_ROOT / "build" / "core" / "aegisd",
        REPO_ROOT / "build" / "core" / "aegisd.exe",
        REPO_ROOT / "build" / "aegisd",
        REPO_ROOT / "build" / "aegisd.exe",
    ]
    for c in candidates:
        if c.is_file():
            return c
    for found in REPO_ROOT.glob("build/**/aegisd*"):
        if found.is_file() and found.suffix in ("", ".exe"):
            return found
    pytest.skip("aegisd binary not found (set AEGISD_BIN or build core target first)")


class AegisSimProcess:
    """Interactive driver for `aegisd --sim` over stdin/stdout."""

    def __init__(self, binary: Path, store_dir: Path, extra_args: List[str] | None = None) -> None:
        self.binary = binary
        self.store_dir = store_dir
        cmd = [
            str(binary),
            "--sim",
            "--store",
            str(store_dir),
            "--device-id",
            "AEG-0001",
        ]
        if extra_args:
            cmd.extend(extra_args)
        self.proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self.events: List[Dict[str, Any]] = []

    def command(self, line: str) -> Tuple[str, Dict[str, Any] | str]:
        assert self.proc.stdin is not None and self.proc.stdout is not None
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()

        while True:
            resp = self.proc.stdout.readline()
            if resp == "":
                stderr_out = self.proc.stderr.read() if self.proc.stderr else ""
                raise RuntimeError(f"aegisd closed stdout unexpectedly after {line!r}: {stderr_out}")
            resp = resp.rstrip("\r\n")
            if resp.startswith("EVT "):
                self.events.append(json.loads(resp[4:]))
                continue
            if resp.startswith("OK "):
                return "OK", json.loads(resp[3:])
            if resp == "OK":
                return "OK", {}
            if resp.startswith("ERR "):
                return "ERR", resp[4:]
            raise AssertionError(f"Unexpected protocol line: {resp!r}")

    def cmd_ok(self, line: str) -> Dict[str, Any]:
        status, payload = self.command(line)
        assert status == "OK", f"Expected OK for {line!r}, got ERR {payload}"
        assert isinstance(payload, dict)
        return payload

    def cmd_err(self, line: str) -> str:
        status, payload = self.command(line)
        assert status == "ERR", f"Expected ERR for {line!r}, got OK {payload}"
        assert isinstance(payload, str)
        return payload

    def close(self) -> int:
        if self.proc.poll() is None:
            try:
                self.command("QUIT")
            except Exception:  # noqa: BLE001
                self.proc.terminate()
        return self.proc.wait(timeout=5)


@pytest.fixture
def aegisd_bin() -> Path:
    return _find_aegisd_binary()


@pytest.fixture
def sim_factory(aegisd_bin: Path, tmp_path: Path) -> Callable[..., AegisSimProcess]:
    procs: List[AegisSimProcess] = []

    def _spawn(store_subdir: str = "store", extra_args: List[str] | None = None) -> AegisSimProcess:
        store = tmp_path / store_subdir
        store.mkdir(parents=True, exist_ok=True)
        sim = AegisSimProcess(aegisd_bin, store, extra_args=extra_args)
        procs.append(sim)
        return sim

    yield _spawn

    for p in procs:
        p.close()
