#!/usr/bin/env python3
"""Selbsttest für tools/rc_release_asset.py (QTMUX-136) — ohne Netz.

Ersetzt den HTTP-Transport durch ein GitHub-Imitat und prüft die vier Fälle,
auf denen die Sicherheit des Tag-Jobs ruht. Fälle übernommen aus EmbyStudio
`tools/release/rc_selftest.py` (ra_*), ergänzt um den Fremd-Commit-Fall.

    python3 tools/test_rc_release_asset.py      # Exit 0 = alle Fälle grün
"""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True  # kein tools/__pycache__ im Arbeitsbaum
sys.path.insert(0, str(Path(__file__).resolve().parent))
import rc_release_asset as ra  # noqa: E402

TAG_SHA = "c0ffee1" + "0" * 33
UPLOAD_URL = "https://uploads.example/rel/7/assets{?name,label}"


def fake_github(releases=None, size_lie=False):
    state = {"releases": list(releases or []), "assets": {}, "next": 100, "calls": []}

    def transport(method, url, body, headers):
        path = url.split("?")[0]
        state["calls"].append((method, path))
        if method == "GET" and "/commits/" in path:
            return 200, json.dumps({"sha": TAG_SHA}).encode()
        if method == "GET" and path.endswith("/releases"):
            return 200, json.dumps(state["releases"]).encode()
        if method == "POST" and path.endswith("/releases"):
            rel = dict(json.loads(body), id=7, upload_url=UPLOAD_URL)
            state["releases"].append(rel)
            return 201, json.dumps(rel).encode()
        if method == "GET" and path.endswith("/assets"):
            return 200, json.dumps(list(state["assets"].values())).encode()
        if method == "DELETE" and "/releases/assets/" in path:
            aid = int(path.rsplit("/", 1)[1])
            state["assets"] = {n: a for n, a in state["assets"].items() if a["id"] != aid}
            return 204, b""
        if method == "POST" and path.startswith("https://uploads.example/"):
            name = url.split("name=", 1)[1]
            state["next"] += 1
            state["assets"][name] = {"id": state["next"], "name": name, "state": "uploaded",
                                     "size": len(body) + (1 if size_lie else 0),
                                     "digest": "sha256:" + hashlib.sha256(body).hexdigest()}
            return 201, json.dumps(state["assets"][name]).encode()
        return 404, b"{}"

    return ra.Api("o/r", "t", transport), state


def files(tmp: Path):
    f = tmp / "QTmux-9.9.9-x86_64.AppImage"
    f.write_bytes(b"IMAGE")
    return [f]


def quiet(*_):
    pass


def case_creates_draft_and_reads_back(tmp):
    api, st = fake_github()
    res = ra.upload(api, "appimage-probe-9", "c0ffee1", files(tmp), log=quiet)
    assert st["releases"][0]["draft"] is True and len(res["assets"]) == 1, (st, res)


def case_clobbers_asset_of_same_name(tmp):
    api, st = fake_github([{"id": 7, "tag_name": "v9.9.9", "draft": True, "upload_url": UPLOAD_URL}])
    st["assets"]["QTmux-9.9.9-x86_64.AppImage"] = {"id": 5, "name": "QTmux-9.9.9-x86_64.AppImage",
                                                   "size": 9, "state": "uploaded",
                                                   "digest": "sha256:" + "00" * 32}
    ra.upload(api, "v9.9.9", "c0ffee1", files(tmp), log=quiet)
    assert ("DELETE", "https://api.github.com/repos/o/r/releases/assets/5") in st["calls"], st["calls"]


def case_refuses_published_release(tmp):
    api, st = fake_github([{"id": 7, "tag_name": "v9.9.9", "draft": False, "upload_url": UPLOAD_URL}])
    try:
        ra.upload(api, "v9.9.9", "c0ffee1", files(tmp), log=quiet)
    except ra.Refused:
        assert not any(m in ("POST", "DELETE") for m, _ in st["calls"]), st["calls"]
        return
    raise AssertionError("veröffentlichtes Release wurde angefasst")


def case_refuses_tag_on_foreign_commit(tmp):
    api, st = fake_github()
    try:
        ra.upload(api, "v9.9.9", "deadbee", files(tmp), log=quiet)
    except ra.Refused:
        assert not any(m in ("POST", "DELETE") for m, _ in st["calls"]), st["calls"]
        return
    raise AssertionError("Tag auf fremdem Commit wurde angenommen")


def case_read_back_mismatch_is_refused(tmp):
    api, _ = fake_github(size_lie=True)
    try:
        ra.upload(api, "v9.9.9", "c0ffee1", files(tmp), log=quiet)
    except ra.Refused:
        return
    raise AssertionError("abweichende Größe beim Rücklesen blieb unbemerkt")


def main() -> int:
    cases = [v for k, v in globals().items() if k.startswith("case_")]
    failed = 0
    for c in cases:
        with tempfile.TemporaryDirectory() as d:
            try:
                c(Path(d))
                print(f"OK   {c.__name__}")
            except Exception as e:  # noqa: BLE001 — jeder Fehler ist ein roter Fall
                failed += 1
                print(f"FAIL {c.__name__}: {e!r}")
    print(f"RELEASE-ASSET-SELFTEST {'OK' if not failed else 'FAIL'}: {len(cases) - failed}/{len(cases)} Fälle")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
