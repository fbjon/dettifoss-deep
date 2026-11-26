#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The emulator as one HTML file that opens from the disk.

    tools/pack_emu.py build/emu      (web/emu/build.sh runs it)

Stamps build/emu/index.html with the build (name, branch, commit, date) and writes
build/emu/dettifoss-deep-emu-BRANCH-COMMIT.html: the page with omni.wasm, worklet.js and the fonts
inside it. A page opened from file:// can't fetch its wasm or load a worklet module from a file, so
the packed page carries them (base64) and its switchOn() takes them from
window.OMNI_INLINE. The manual's links still point at pages beside it; they need the folder.
"""
import base64
import datetime
import os
import re
import subprocess
import sys

NAME = "Dettifoss Deep"


def git(*args):
    try:
        return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def build_id():
    """(branch, commit) of the tree being built; commit ends in + when the tree has changes."""
    branch = (os.environ.get("OM_BRANCH") or os.environ.get("GITHUB_HEAD_REF")
              or git("symbolic-ref", "--short", "-q", "HEAD") or "detached")
    commit = git("rev-parse", "--short=7", "HEAD") or "nogit"
    if git("status", "--porcelain", "--untracked-files=no"):
        commit += "+"
    return branch, commit


def replace_once(s, old, new):
    if s.count(old) != 1:
        sys.exit(f"pack_emu: expected one {old[:60]!r} in index.html, found {s.count(old)}")
    return s.replace(old, new)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__.strip().splitlines()[2].strip())
    d = sys.argv[1]
    branch, commit = build_id()
    date = datetime.date.today().isoformat()
    stamp = f"{NAME} · {branch} · {commit} · {date}"

    page_path = os.path.join(d, "index.html")
    with open(page_path, encoding="utf-8") as f:
        page = f.read()
    page = replace_once(page, "<!--BUILD-->", stamp)
    with open(page_path, "w", encoding="utf-8") as f:
        f.write(page)

    # the one-file page: fonts as data URLs, wasm and worklet in a script before the page's own
    one = re.sub(r'<link rel="preload" href="fonts/[^"]+"[^>]*>\n?', "", page)
    # the tab says which build: several saved pages open side by side
    one = re.sub(r"<title>[^<]*</title>", lambda m: f"<title>{NAME} {branch} {commit}</title>", one, count=1)

    def font(m):
        with open(os.path.join(d, "fonts", m.group(1)), "rb") as f:
            return 'url("data:font/woff2;base64,' + base64.b64encode(f.read()).decode() + '")'
    one, n = re.subn(r'url\("fonts/([^"]+\.woff2)"\)', font, one)
    if n == 0:
        sys.exit("pack_emu: no fonts found in index.html")
    with open(os.path.join(d, "omni.wasm"), "rb") as f:
        wasm = base64.b64encode(f.read()).decode()
    with open(os.path.join(d, "worklet.js"), "rb") as f:
        worklet = base64.b64encode(f.read()).decode()
    inline = f'<script>window.OMNI_INLINE = {{ wasm: "{wasm}", worklet: "{worklet}" }};</script>\n<script>'
    one = replace_once(one, "\n<script>", "\n" + inline)

    safe = re.sub(r"[^A-Za-z0-9._-]+", "-", branch).strip("-")
    out = os.path.join(d, f"dettifoss-deep-emu-{safe}-{commit.replace('+', '-dirty')}.html")
    for old in os.listdir(d):   # one packed page in the folder: the newest
        if old.startswith("dettifoss-deep-emu-") and old.endswith(".html"):
            os.remove(os.path.join(d, old))
    with open(out, "w", encoding="utf-8") as f:
        f.write(one)
    print(f"{out} ({os.path.getsize(out)} B): {stamp}")


if __name__ == "__main__":
    main()
