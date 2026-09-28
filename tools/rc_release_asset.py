#!/usr/bin/env python3
"""rc_release_asset.py — hängt das CI-gebaute AppImage an das GitHub-Release
des Tags (CI-Seite von QTMUX-136).

Herkunft: 1:1 aus EmbyStudio `tools/release/rc_release_asset.py` (EMB-34,
Commits 3fea49e/67cb27d), angepasst sind nur dieser Text, der User-Agent und der
Draft-Text. Logik und Exit-Codes sind unverändert — Fehler dort UND hier beheben.

Warum ein Release-Asset und kein Actions-Artefakt: Die Artefakt-Speicherquota ist
ACCOUNT-weit (alle Repos von RealNobser teilen sie). Am 2026-09-28 war sie voll;
EmbyStudios 0.0.4-Tag-Lauf scheiterte dreimal allein am Upload-Schritt, frei wurde
sie u. a. durch Löschen alter QTmux-AppImage-Artefakte. Release-Assets zählen
nicht gegen diese Quota.

Läuft im CI-Job `linux-release` (nur Tag-Läufe, der einzige Job mit
`contents: write`) mit dem GITHUB_TOKEN des Jobs — kein PAT:

    rc_release_asset.py upload --tag v1.9.6 --commit <sha> DATEI…

  * sucht das Release des Tags unter ALLEN Releases (auch Drafts — der
    by-tag-Endpunkt liefert keine Drafts) und legt ein DRAFT an, wenn es keines
    gibt; DMG/MSI/ZIP, Text und das Veröffentlichen folgen im Release-Rezept;
  * verweigert ein VERÖFFENTLICHTES Release (ein stiller Asset-Tausch unter einem
    öffentlichen Release ist genau das, was nicht passieren darf);
  * verweigert, wenn der Tag nicht auf --commit zeigt (das Image behauptete
    sonst einen Commit, aus dem es nicht gebaut wurde);
  * ersetzt ein gleichnamiges Asset (clobber: Wiederholung desselben Tags) und
    liest das Release danach zurück: jede Datei mit Größe und — wo GitHub einen
    liefert — sha256-Digest.

Nur Standardbibliothek. Selbsttest: tools/test_rc_release_asset.py.
Exit: 0 OK · 1 Rücklesen stimmt nicht / Release verweigert · 2 Aufruf ·
3 Werkzeug- oder HTTP-Fehler. Ausgabezeilen beginnen mit RELEASE-ASSET OK/FAIL/ERROR.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

API = "https://api.github.com"


class ToolError(Exception):
    """Network/HTTP/tool trouble — never a verdict about the release."""


class Refused(Exception):
    """The release is not ours to change, or the read-back disagrees."""


class Api:
    """Tiny GitHub REST client; `transport` is replaceable for the self-test."""

    def __init__(self, repo: str, token: str, transport=None):
        self.repo, self.token = repo, token
        self.transport = transport or self._urllib

    def _urllib(self, method: str, url: str, body: bytes | None, headers: dict) -> tuple[int, bytes]:
        req = urllib.request.Request(url, data=body, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=600) as r:
                return r.status, r.read()
        except urllib.error.HTTPError as e:
            return e.code, e.read()
        except (urllib.error.URLError, OSError) as e:
            raise ToolError(f"{method} {url}: {e}") from e

    def call(self, method: str, path_or_url: str, body=None, ctype="application/json", ok=(200, 201, 204)):
        url = path_or_url if path_or_url.startswith("https://") else f"{API}/repos/{self.repo}{path_or_url}"
        data = None
        if body is not None:
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
        headers = {"Authorization": f"Bearer {self.token}", "Accept": "application/vnd.github+json",
                   "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "qtmux-release-asset"}
        if data is not None:
            headers["Content-Type"] = ctype
        status, raw = self.transport(method, url, data, headers)
        if status not in ok:
            raise ToolError(f"{method} {url} → HTTP {status}: {raw[:300].decode('utf-8', 'replace')}")
        return json.loads(raw) if raw.strip() else None


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_release(api: Api, tag: str) -> dict | None:
    """The release (draft or not) whose tag_name is <tag>. Paginated list —
    GET /releases/tags/<tag> does not see drafts."""
    page = 1
    while True:
        rels = api.call("GET", f"/releases?per_page=100&page={page}")
        for r in rels:
            if r.get("tag_name") == tag:
                return r
        if len(rels) < 100:
            return None
        page += 1


def tag_commit(api: Api, tag: str) -> str:
    """Commit sha the tag points at (annotated tags dereferenced by /commits)."""
    return api.call("GET", f"/commits/{urllib.parse.quote(tag, safe='')}")["sha"]


def upload(api: Api, tag: str, commit: str, files: list[Path], log=print) -> dict:
    sha = tag_commit(api, tag)
    if not sha.startswith(commit.lower()) and not commit.lower().startswith(sha):
        raise Refused(f"Tag {tag} zeigt auf {sha[:7]}, gebaut wurde {commit[:7]}")
    rel = find_release(api, tag)
    if rel is None:
        rel = api.call("POST", "/releases", {"tag_name": tag, "name": tag, "draft": True,
                                            "body": "Draft — AppImage aus der CI; DMG/MSI/ZIP und Text folgen im Release-Rezept."})
        log(f"RELEASE-ASSET: Draft-Release {rel['id']} für {tag} angelegt")
    elif not rel.get("draft"):
        raise Refused(f"Release {tag} ist bereits veröffentlicht — Assets ändert nur der Release-Worker")
    else:
        log(f"RELEASE-ASSET: vorhandenes Draft-Release {rel['id']} für {tag}")
    rid = rel["id"]
    upload_base = rel["upload_url"].split("{", 1)[0]
    existing = {a["name"]: a for a in api.call("GET", f"/releases/{rid}/assets?per_page=100")}
    want = {}
    for f in files:
        if f.name in existing:
            api.call("DELETE", f"/releases/assets/{existing[f.name]['id']}")
            log(f"RELEASE-ASSET: altes Asset {f.name} ersetzt (clobber)")
        api.call("POST", f"{upload_base}?name={urllib.parse.quote(f.name)}", f.read_bytes(),
                 ctype="application/octet-stream")
        want[f.name] = (f.stat().st_size, sha256_file(f))
    # Read back — never trust the upload's own answer.
    got = {a["name"]: a for a in api.call("GET", f"/releases/{rid}/assets?per_page=100")}
    problems = []
    for name, (size, digest) in want.items():
        a = got.get(name)
        if a is None:
            problems.append(f"{name} fehlt nach dem Upload")
            continue
        if a.get("size") != size:
            problems.append(f"{name}: Größe {a.get('size')} ≠ lokal {size}")
        d = a.get("digest") or ""
        if d.startswith("sha256:") and d.split(":", 1)[1] != digest:
            problems.append(f"{name}: Digest {d[7:23]}… ≠ lokal {digest[:16]}…")
        if a.get("state") not in (None, "uploaded"):
            problems.append(f"{name}: Zustand {a.get('state')}")
    if problems:
        raise Refused("; ".join(problems))
    return {"release_id": rid, "draft": True, "assets": {n: {"size": s, "sha256": d} for n, (s, d) in want.items()}}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(prog="rc_release_asset.py", allow_abbrev=False,
                                 description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("upload", allow_abbrev=False, help="Dateien an das (Draft-)Release des Tags hängen")
    p.add_argument("--tag", required=True)
    p.add_argument("--commit", required=True, help="Commit, aus dem die Dateien gebaut wurden (GITHUB_SHA)")
    p.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""))
    p.add_argument("files", nargs="+")
    a = ap.parse_args(argv)
    token = os.environ.get("GITHUB_TOKEN", "")
    if not token or not a.repo:
        print("RELEASE-ASSET ERROR: GITHUB_TOKEN und --repo/GITHUB_REPOSITORY nötig")
        return 3
    files = [Path(f) for f in a.files]
    missing = [str(f) for f in files if not f.is_file()]
    if missing:
        print(f"RELEASE-ASSET ERROR: keine Datei: {missing}")
        return 3
    try:
        res = upload(Api(a.repo, token), a.tag, a.commit, files)
    except Refused as e:
        print(f"RELEASE-ASSET FAIL: {e}")
        return 1
    except ToolError as e:
        print(f"RELEASE-ASSET ERROR: {e}")
        return 3
    for n, x in res["assets"].items():
        print(f"RELEASE-ASSET OK: {n} ({x['size']} B, sha256 {x['sha256'][:16]}…) an Draft-Release "
              f"{res['release_id']} ({a.tag}), zurückgelesen")
    return 0


if __name__ == "__main__":
    sys.exit(main())
