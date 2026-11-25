#!/usr/bin/env python3
"""Lowest macOS of a FINISHED package: a release .dmg or a deployed .app.

macos_min_version.py (ctest MacOsMinVersion.*) measures the build tree. What a
user gets is the disk image after macdeployqt and signing, with Qt frameworks,
plugins and the helper inside — this tool measures exactly that:

* every OWN Mach-O carries exactly the expected minos in every slice, and
  exactly the expected architectures (default arm64 + x86_64: the family's Mac
  packages are universal). Own = everything under Contents/MacOS/ and
  Contents/Helpers/ of the app, plus each --own <path relative to the .app>;
* every FOREIGN Mach-O (Qt frameworks and plugins, PCBUSB, …) asks for at most
  the expected version — the highest minos of all loaded images decides where
  the app starts, so one foreign file above the floor breaks the promise;
* LSMinimumSystemVersion of the app's Info.plist (and of every nested .app,
  .appex or .xpc) equals the expected version;
* Contents/Helpers/update-swap exists (the self-installation needs it), unless
  --no-swap-tool is given for a package that has no self-installation.

  dmg_minos.py <package.dmg | App.app> --expect 13.0 [--archs arm64,x86_64]
               [--own Contents/PlugIns/X.appex/Contents/MacOS/X]... [--no-swap-tool]
  dmg_minos.py --selftest

A .dmg is attached read-only and hidden through platform/macos/dmg-image.sh
(the family's one way to touch disk images; never hdiutil here) at an own empty
mount point, and ejected again in every case, also after an error. The image
must hold exactly one .app at its root.

Output contract (like the other CI guards): one line "DMG-MINOS OK: …" (exit 0),
"DMG-MINOS FAIL: …" (exit 1: a finding about the package), or
"DMG-MINOS ERROR: …" (exit 3: the image could not be attached, a file could
not be read, vtool is missing, or there was nothing to measure) — a broken
tool never looks like a good package. The lines before it list each finding.

--selftest builds throw-away bundles (folders, no disk image) with clang and
checks each verdict; it does no image work.
"""
from __future__ import annotations

import argparse
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TAG = "DMG-MINOS"
REPO = Path(__file__).resolve().parents[2]
DMG_IMAGE = REPO / "platform" / "macos" / "dmg-image.sh"
OWN_DIRS = ("Contents/MacOS/", "Contents/Helpers/")
NESTED_BUNDLES = (".app", ".appex", ".xpc")

_MACHO = {b"\xfe\xed\xfa\xce", b"\xfe\xed\xfa\xcf", b"\xce\xfa\xed\xfe", b"\xcf\xfa\xed\xfe"}
_FAT = {b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"}


class ReadError(Exception):
    """The measurement failed — never a statement about the package."""


def norm(v: str) -> tuple[int, int, int]:
    try:
        p = [int(x) for x in str(v).strip().split(".")]
    except ValueError as e:
        raise ReadError(f"not a version: {v!r}") from e
    if not 1 <= len(p) <= 3:
        raise ReadError(f"not a version: {v!r}")
    p += [0] * (3 - len(p))
    return p[0], p[1], p[2]


def run(cmd: list[str], timeout: int = 120) -> subprocess.CompletedProcess:
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except FileNotFoundError as e:
        raise ReadError(f"{cmd[0]}: not found") from e
    except subprocess.TimeoutExpired as e:
        raise ReadError(f"{cmd[0]}: no answer after {e.timeout} s") from e


def is_macho(p: Path) -> bool:
    try:
        with p.open("rb") as f:
            head = f.read(8)
    except OSError as e:
        raise ReadError(f"{p}: {e}") from e
    if head[:4] in _MACHO:
        return True
    # Java class files share 0xcafebabe; a fat header has a small count.
    return head[:4] in _FAT and 0 < int.from_bytes(head[4:8], "big") < 32


def slices(p: Path) -> dict[str, str]:
    """{arch: minos} of every slice (lipo for the list, vtool per slice)."""
    r = run(["lipo", "-archs", str(p)])
    if r.returncode != 0 or not r.stdout.split():
        raise ReadError(f"lipo -archs {p}: rc {r.returncode}: {r.stderr.strip()[:200]}")
    out: dict[str, str] = {}
    for a in r.stdout.split():
        v = run(["vtool", "-arch", a, "-show-build", str(p)])
        if v.returncode != 0:
            raise ReadError(f"vtool -show-build {p} ({a}): rc {v.returncode}: {v.stderr.strip()[:200]}")
        minos = None
        for line in v.stdout.splitlines():
            f = line.split()
            # LC_BUILD_VERSION says "minos X"; the old LC_VERSION_MIN_MACOSX
            # says "version X" inside its own block.
            if len(f) >= 2 and f[0] == "minos":
                minos = f[1]
        if minos is None:
            blk = v.stdout
            if "LC_VERSION_MIN_MACOSX" in blk:
                for line in blk.split("LC_VERSION_MIN_MACOSX", 1)[1].splitlines():
                    f = line.split()
                    if len(f) >= 2 and f[0] == "version":
                        minos = f[1]
                        break
        if minos is None:
            raise ReadError(f"{p} ({a}): no macOS minimum in the load commands")
        out[a] = minos
    return out


def plist_min(p: Path):
    try:
        with p.open("rb") as f:
            return plistlib.load(f).get("LSMinimumSystemVersion")
    except (OSError, plistlib.InvalidFileException, ValueError) as e:
        raise ReadError(f"{p}: Info.plist not readable ({e})") from e


def judge_app(app: Path, expect: str, archs: set[str], own_extra: list[str],
              need_swap: bool) -> tuple[list[str], list[str]]:
    """(findings, notes) for one .app folder. ReadError when nothing can be said."""
    want = norm(expect)
    if not (app / "Contents" / "Info.plist").is_file():
        raise ReadError(f"{app}: no Contents/Info.plist — not an app bundle")
    findings: list[str] = []
    own: dict[str, dict[str, str]] = {}
    foreign: dict[str, dict[str, str]] = {}
    nested_plists: list[str] = []
    for root, dirs, files in os.walk(app, followlinks=False):
        rp = Path(root)
        for d in dirs:
            if d.endswith(NESTED_BUNDLES):
                nested_plists.append((rp / d).relative_to(app).as_posix() + "/Contents/Info.plist")
        for name in files:
            p = rp / name
            if p.is_symlink() or not p.is_file() or not is_macho(p):
                continue
            rel = p.relative_to(app).as_posix()
            is_own = rel.startswith(OWN_DIRS) and rel.count("/") == 2 or rel in own_extra
            (own if is_own else foreign)[rel] = slices(p)
    if not own:
        raise ReadError(f"{app}: no own Mach-O under Contents/MacOS/ — nothing to measure")
    for rel in own_extra:
        if rel not in own:
            findings.append(f"{rel}: missing (no Mach-O)")
    if need_swap and "Contents/Helpers/update-swap" not in own:
        findings.append("Contents/Helpers/update-swap: missing — the app cannot replace itself")
    for rel, m in sorted(own.items()):
        if set(m) != archs:
            findings.append(f"{rel}: architectures {sorted(m)}, want {sorted(archs)}")
        bad = [f"{a} {v}" for a, v in sorted(m.items()) if norm(v) != want]
        if bad:
            findings.append(f"{rel}: minos {', '.join(bad)} != {expect}")
    above = [f"{rel} {a} {v}" for rel, m in sorted(foreign.items())
             for a, v in sorted(m.items()) if norm(v) > want]
    if above:
        findings.append(f"{len(above)} foreign slice(s) ask for more than {expect}: "
                        + "; ".join(above[:10]) + (" …" if len(above) > 10 else ""))
    for rel in ["Contents/Info.plist"] + sorted(nested_plists):
        p = app / rel
        if not p.is_file():
            findings.append(f"{rel}: missing")
            continue
        v = plist_min(p)
        if v is None or not str(v).strip():
            findings.append(f"{rel}: LSMinimumSystemVersion {'missing' if v is None else 'empty'}")
            continue
        try:
            same = norm(str(v)) == want
        except ReadError:
            same = False
        if not same:
            findings.append(f"{rel}: LSMinimumSystemVersion {v} != {expect}")
    fv = sorted({v for m in foreign.values() for v in m.values()}, key=norm)
    notes = [f"{len(own)} own Mach-O ({', '.join(sorted(own))})",
             f"{len(foreign)} foreign Mach-O, minos {', '.join(fv) or '-'}"]
    return findings, notes


def single_app(root: Path) -> Path:
    apps = sorted(p for p in root.iterdir() if p.suffix == ".app" and p.is_dir() and not p.is_symlink())
    if len(apps) != 1:
        raise ReadError(f"{root}: {len(apps)} .app at the root of the image, want exactly 1")
    return apps[0]


def check(package: Path, expect: str, archs: set[str], own_extra: list[str],
          need_swap: bool) -> tuple[int, list[str]]:
    lines: list[str] = []
    mp = None
    try:
        norm(expect)
        if shutil.which("vtool") is None or shutil.which("lipo") is None:
            raise ReadError("vtool or lipo not found (Xcode command line tools)")
        if package.suffix == ".dmg":
            if not package.is_file():
                raise ReadError(f"{package}: not a file")
            mp = Path(tempfile.mkdtemp(prefix="dmg-minos-mnt."))
            r = run([str(DMG_IMAGE), "attach", str(package), str(mp)], timeout=300)
            if r.returncode != 0:
                raise ReadError(f"dmg-image.sh attach {package}: rc {r.returncode}: "
                                f"{(r.stderr or r.stdout).strip()[-300:]}")
            app = single_app(mp)
        elif package.suffix == ".app" and package.is_dir():
            app = package
        else:
            raise ReadError(f"{package}: neither a .dmg file nor an .app folder")
        findings, notes = judge_app(app, expect, archs, own_extra, need_swap)
    except ReadError as e:
        return 3, lines + [f"{TAG} ERROR: {e}"]
    finally:
        if mp is not None:
            r = run([str(DMG_IMAGE), "eject", str(mp)], timeout=300)
            if r.returncode != 0:
                # Never remove a folder something is still mounted on.
                lines.append(f"{TAG} ERROR: dmg-image.sh eject {mp}: rc {r.returncode} — "
                             f"left mounted: {(r.stderr or r.stdout).strip()[-200:]}")
            else:
                try:
                    mp.rmdir()
                except OSError:
                    pass
    if any(l.startswith(f"{TAG} ERROR") for l in lines):
        return 3, lines
    lines += [f"  note {n}" for n in notes] + [f"  FAIL {f}" for f in findings]
    if findings:
        return 1, lines + [f"{TAG} FAIL: {package.name} want {expect}: {len(findings)} finding(s), "
                           f"first: {findings[0]}"]
    return 0, lines + [f"{TAG} OK: {package.name} minos {expect} "
                       f"({', '.join(sorted(archs))}), {notes[0].split(' (')[0]}, {notes[1]}"]


# ---- self-test -----------------------------------------------------------------

def selftest() -> int:
    if shutil.which("xcrun") is None:
        print(f"{TAG} ERROR: selftest needs xcrun clang")
        return 3
    wrong: list[str] = []
    with tempfile.TemporaryDirectory(prefix="dmg-minos-st-") as td:
        d = Path(td)
        (d / "m.c").write_text("int main(void){return 0;}\n")
        (d / "l.c").write_text("int f(void){return 1;}\n")
        bins: dict[str, Path] = {}

        def cc(name: str, minv: str, archs: list[str], dylib: bool = False) -> Path:
            out = d / name
            cmd = ["xcrun", "clang", f"-mmacosx-version-min={minv}", "-o", str(out)]
            for a in archs:
                cmd += ["-arch", a]
            cmd += (["-dynamiclib", str(d / "l.c")] if dylib else [str(d / "m.c")])
            r = run(cmd)
            if r.returncode != 0:
                raise ReadError(f"selftest clang: {r.stderr.strip()[:200]}")
            return out

        try:
            U = ["arm64", "x86_64"]
            bins["exe13"] = cc("exe13", "13.0", U)
            bins["exe14"] = cc("exe14", "14.0", U)
            bins["exe13arm"] = cc("exe13arm", "13.0", ["arm64"])
            bins["lib12"] = cc("lib12.dylib", "12.0", U, dylib=True)
            bins["lib14"] = cc("lib14.dylib", "14.0", U, dylib=True)
        except ReadError as e:
            print(f"{TAG} ERROR: {e}")
            return 3

        def bundle(name: str, exe="exe13", swap="exe13", lib="lib12", plist="13.0",
                   nested_plist="13.0", nested=False) -> Path:
            app = d / name / "T.app"
            (app / "Contents" / "MacOS").mkdir(parents=True)
            shutil.copy(bins[exe], app / "Contents" / "MacOS" / "T")
            if swap:
                (app / "Contents" / "Helpers").mkdir()
                shutil.copy(bins[swap], app / "Contents" / "Helpers" / "update-swap")
            fw = app / "Contents" / "Frameworks" / "Q.framework" / "Versions" / "A"
            fw.mkdir(parents=True)
            shutil.copy(bins[lib], fw / "Q")
            os.symlink("A", fw.parent / "Current")       # must not count twice
            info: dict = {"CFBundleIdentifier": "t.t", "CFBundleExecutable": "T"}
            if plist is not None:
                info["LSMinimumSystemVersion"] = plist
            with (app / "Contents" / "Info.plist").open("wb") as f:
                plistlib.dump(info, f)
            if nested:
                ex = app / "Contents" / "PlugIns" / "E.appex" / "Contents"
                (ex / "MacOS").mkdir(parents=True)
                shutil.copy(bins["exe13"], ex / "MacOS" / "E")
                with (ex / "Info.plist").open("wb") as f:
                    plistlib.dump({"LSMinimumSystemVersion": nested_plist}, f)
            return app

        U2 = {"arm64", "x86_64"}
        cases = [
            ("good universal 13.0 bundle", bundle("a"), [], True, 0),
            ("own program 14.0", bundle("b", exe="exe14"), [], True, 1),
            ("swap tool 14.0", bundle("c", swap="exe14"), [], True, 1),
            ("swap tool arm64 only", bundle("d", swap="exe13arm"), [], True, 1),
            ("swap tool missing", bundle("e", swap=None), [], True, 1),
            ("swap tool missing, --no-swap-tool", bundle("f", swap=None), [], False, 0),
            ("foreign framework 14.0", bundle("g", lib="lib14"), [], True, 1),
            ("plist 12.0", bundle("h", plist="12.0"), [], True, 1),
            ("plist key missing", bundle("i", plist=None), [], True, 1),
            ("plist empty", bundle("j", plist=""), [], True, 1),
            ("nested appex plist 12.0", bundle("k", nested=True, nested_plist="12.0"), [], True, 1),
            ("--own names a missing file", bundle("l"), ["Contents/PlugIns/X.appex/Contents/MacOS/X"], True, 1),
            ("not a package", d / "nope.txt", [], True, 3),
            ("missing .dmg", d / "nope.dmg", [], True, 3),
        ]
        for label, pkg, own, swap, rc_want in cases:
            rc, out = check(pkg, "13.0", U2, own, swap)
            if rc != rc_want:
                wrong.append(f"{label}: rc {rc}, want {rc_want} ({out[-1]})")
    if wrong:
        for w in wrong:
            print(f"{TAG} SELFTEST WRONG: {w}")
        return 1
    print(f"{TAG} SELFTEST OK: {len(cases)}/{len(cases)} cases")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(allow_abbrev=False, description=__doc__.split("\n")[0])
    ap.add_argument("package", nargs="?", help="the .dmg or the deployed .app")
    ap.add_argument("--expect", help="the macOS floor, e.g. 13.0")
    ap.add_argument("--archs", default="arm64,x86_64",
                    help="architectures every own Mach-O carries (default arm64,x86_64)")
    ap.add_argument("--own", action="append", default=[], metavar="REL",
                    help="another own Mach-O, relative to the .app")
    ap.add_argument("--no-swap-tool", action="store_true",
                    help="the package has no self-installation (no Contents/Helpers/update-swap)")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.package or not a.expect:
        ap.error("<package> and --expect are required")
    archs = {x for x in a.archs.replace(";", ",").split(",") if x.strip()}
    if not archs:
        ap.error("--archs is empty")
    rc, lines = check(Path(a.package), a.expect, archs, a.own, not a.no_swap_tool)
    print("\n".join(lines))
    return rc


if __name__ == "__main__":
    sys.exit(main())
