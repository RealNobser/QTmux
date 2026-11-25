#!/usr/bin/env python3
"""Lowest macOS a built Mach-O runs on (ctest MacOsMinVersion.*).

Every slice of the file must carry exactly the expected `minos` (LC_BUILD_VERSION,
read with `vtool -show-build`), and the bundle's Info.plist, when given, must
promise the same LSMinimumSystemVersion. Until 2026-11-21 the deployment target
in CMakeLists.txt never reached the compiler, and every release binary carried
the SDK of the build Mac instead (0.7.0: minos 27.2 while Info.plist said 12.0):
a green build says nothing about this, only the built file does.

  macos_min_version.py <mach-o> --expect 13.0 [--plist <Info.plist>]
  macos_min_version.py --selftest

Equal, not "at most": a lower minos than wanted means the value did not come
from the one place either.

Output contract (like the other CI guards): one line "MACOS-MIN OK: …" (exit 0),
"MACOS-MIN FAIL: …" (exit 1: a slice or the plist differs), or
"MACOS-MIN ERROR: …" (exit 3: the file, a slice or the plist could not be read)
- a broken tool never looks like a good binary.

--selftest builds throw-away binaries with clang and checks each verdict.
"""
from __future__ import annotations

import argparse
import plistlib
import subprocess
import sys
import tempfile
from pathlib import Path

TAG = "MACOS-MIN"


class ReadError(Exception):
    pass


def norm(v: str) -> tuple[int, ...]:
    try:
        parts = [int(p) for p in v.strip().split(".")]
    except ValueError as e:
        raise ReadError(f"not a version: {v!r}") from e
    while len(parts) > 1 and parts[-1] == 0:
        parts.pop()
    return tuple(parts)


def run(cmd: list[str]) -> str:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired) as e:
        raise ReadError(f"{cmd[0]}: {e}") from e
    if r.returncode != 0:
        raise ReadError(f"{' '.join(cmd)}: rc {r.returncode}: {r.stderr.strip()[:200]}")
    return r.stdout


def slices(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise ReadError(f"{path}: not a file")
    archs = run(["lipo", "-archs", str(path)]).split()
    if not archs:
        raise ReadError(f"{path}: no architectures")
    out: dict[str, str] = {}
    for a in archs:
        minos = None
        for line in run(["vtool", "-arch", a, "-show-build", str(path)]).splitlines():
            f = line.split()
            if len(f) >= 2 and f[0] == "minos":
                minos = f[1]
        if minos is None:
            raise ReadError(f"{path} ({a}): no minos in LC_BUILD_VERSION")
        out[a] = minos
    return out


def check(path: Path, expect: str, plist: Path | None) -> tuple[int, str]:
    try:
        want = norm(expect)
        got = slices(path)
        bad = [f"{a}={m}" for a, m in got.items() if norm(m) != want]
        plist_v = None
        if plist is not None:
            try:
                with plist.open("rb") as fh:
                    plist_v = str(plistlib.load(fh).get("LSMinimumSystemVersion", ""))
            except (OSError, plistlib.InvalidFileException) as e:
                raise ReadError(f"{plist}: {e}") from e
            if not plist_v or norm(plist_v) != want:
                bad.append(f"LSMinimumSystemVersion={plist_v or '(missing)'}")
    except ReadError as e:
        return 3, f"{TAG} ERROR: {e}"
    have = " ".join(f"{a}={m}" for a, m in got.items())
    if plist_v is not None:
        have += f" plist={plist_v}"
    if bad:
        return 1, f"{TAG} FAIL: {path.name} want {expect}, differs: {' '.join(bad)} (all: {have})"
    return 0, f"{TAG} OK: {path.name} {have}"


def selftest() -> int:
    src = "int main(void){return 0;}\n"
    cases = 0
    wrong: list[str] = []
    with tempfile.TemporaryDirectory(prefix="macos-min-") as td:
        d = Path(td)
        (d / "m.c").write_text(src)

        def build(name: str, minv: str, archs: list[str]) -> Path:
            out = d / name
            cmd = ["xcrun", "clang", f"-mmacosx-version-min={minv}", "-o", str(out), str(d / "m.c")]
            for a in archs:
                cmd[2:2] = ["-arch", a]
            run(cmd)
            return out

        def plist(name: str, v: str) -> Path:
            p = d / name
            with p.open("wb") as fh:
                plistlib.dump({"LSMinimumSystemVersion": v}, fh)
            return p

        try:
            u13 = build("u13", "13.0", ["arm64", "x86_64"])
            u14 = build("u14", "14.0", ["arm64", "x86_64"])
            a13 = build("a13", "13.0", ["arm64"])
            x14 = build("x14", "14.0", ["x86_64"])
            mixed = d / "mixed"
            run(["lipo", "-create", str(a13), str(x14), "-output", str(mixed)])
        except ReadError as e:
            print(f"{TAG} ERROR: selftest setup: {e}")
            return 3
        expectations = [
            ("universal 13.0, want 13.0, plist 13.0", u13, "13.0", plist("p13", "13.0"), 0),
            ("universal 14.0, want 13.0 (higher)", u14, "13.0", None, 1),
            ("universal 13.0, want 14.0 (lower)", u13, "14.0", None, 1),
            ("one slice differs (arm64 13.0, x86_64 14.0)", mixed, "13.0", None, 1),
            ("plist promises 12.0", u13, "13.0", plist("p12", "12.0"), 1),
            ("missing file", d / "nope", "13.0", None, 3),
        ]
        for label, f, want, pl, rc_want in expectations:
            cases += 1
            rc, line = check(f, want, pl)
            if rc != rc_want:
                wrong.append(f"{label}: rc {rc}, want {rc_want} ({line})")
    if wrong:
        for w in wrong:
            print(f"{TAG} SELFTEST WRONG: {w}")
        return 1
    print(f"{TAG} SELFTEST OK: {cases}/{cases} cases")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(allow_abbrev=False)
    ap.add_argument("binary", nargs="?")
    ap.add_argument("--expect")
    ap.add_argument("--plist")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.binary or not a.expect:
        ap.error("<binary> and --expect are required")
    rc, line = check(Path(a.binary), a.expect, Path(a.plist) if a.plist else None)
    print(line)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
