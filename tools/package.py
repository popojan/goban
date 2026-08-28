#!/usr/bin/env python3
"""Assemble a runnable Goban folder and archive it.

    tools/package.py --binary build/goban --platform linux-x64
    tools/package.py --binary build/Release/goban.exe --platform windows-x64
    tools/package.py --binary build/goban --platform linux-x64 --with-engine engine/gnugo

The application resolves every asset relative to the working directory —
`./config`, `./engine`, `./games` — so a "bundle" is a folder you unpack and run
from. There is no install step and no prefix; see docs/building.md.

`--with-engine` is why this script exists in two forms rather than one. CI runs
it *without* engines and publishes that to GitHub Releases; the same script is
run locally *with* `--with-engine engine/gnugo` to build the hraj.si bundle. One
implementation, two outputs, so the bundle people actually download is not a
hand-assembled thing that drifts from the one that is tested.
"""

import argparse
import json
import os
import re
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Config assets are copied wholesale except these. NotoSansSC is a leftover: no
# shipped config names it, and it is 8.5 MB.
EXCLUDE_NAMES = {"NotoSansSC-Regular.otf"}
EXCLUDE_SUFFIXES = {".swp", ".bak", ".pyc"}

DOCS = ["LICENSE", "README.md", "RELEASE_NOTES.md", "CREDITS.md", "THIRD-PARTY.md"]

ENGINE_README = """\
Put GTP engines here.

Goban plays through external engines and ships with none of its own, so this
folder is empty on purpose. The shipped configuration expects GNU Go:

    engine/gnugo/gnugo            (or gnugo.exe on Windows)

If `gnugo` is already on your PATH there is nothing to do — the configuration
falls back to it and only the working directory is taken from `path`.

Other engines are configured in config/base.json under "bots". KataGo entries
are there and switched off; set "enabled": 1 once its binary and a model file
are in engine/katago.
"""


def project_version() -> str:
    """From CMakeLists, so the archive name cannot drift from the About dialog."""
    text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"project\s*\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    if not m:
        sys.exit("error: no VERSION found in CMakeLists.txt")
    return m.group(1)


def wanted(path: Path) -> bool:
    return path.name not in EXCLUDE_NAMES and path.suffix not in EXCLUDE_SUFFIXES


def copy_tree(src: Path, dst: Path) -> None:
    for item in sorted(src.rglob("*")):
        if not wanted(item) or any(not wanted(p) for p in item.parents
                                   if p.is_relative_to(src)):
            continue
        target = dst / item.relative_to(src)
        if item.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item, target)


def check_assets(bundle: Path, with_engine: bool) -> list:
    """Every ./config/... path the shipped configs name must be in the bundle.

    This is not defensive decoration. `NotoSans-Regular.ttf` was referenced by
    all five language configs while being neither tracked in git nor downloaded
    by CMake — so it existed only on one developer's machine, and any bundle
    built from a clean checkout would have shipped a configuration pointing at a
    font that was not there. Nothing would have failed loudly.
    """
    problems = []
    for cfg in sorted((bundle / "config").glob("*.json")):
        text = cfg.read_text(encoding="utf-8")
        for ref in sorted(set(re.findall(r'\./config/[A-Za-z0-9._/-]+', text))):
            if not (bundle / ref[2:]).exists():
                problems.append("%s references missing %s" % (cfg.name, ref))
    if not with_engine:
        return problems
    # With engines folded in, the enabled ones should actually be there.
    base = json.loads((bundle / "config" / "base.json").read_text(encoding="utf-8"))
    for bot in base.get("bots", []):
        if not bot.get("enabled", 1):
            continue
        path = bot.get("path", "")
        if path.startswith("./") and not (bundle / path[2:]).exists():
            problems.append("bot %r is enabled but %s is not in the bundle"
                            % (bot.get("name", "?"), path))
    return problems


def archive(bundle: Path, out_dir: Path, fmt: str) -> Path:
    """tar.gz for unix, zip for Windows.

    Not a preference: a zip has no place for the executable bit, so a Linux user
    unpacking one gets a file they cannot run. Windows does not have the bit and
    does have zip in Explorer.
    """
    if fmt == "tar":
        out = out_dir / (bundle.name + ".tar.gz")
        with tarfile.open(out, "w:gz") as tar:
            tar.add(bundle, arcname=bundle.name)
    else:
        out = out_dir / (bundle.name + ".zip")
        with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
            for item in sorted(bundle.rglob("*")):
                if item.is_file():
                    zf.write(item, Path(bundle.name) / item.relative_to(bundle))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--binary", required=True, help="the built goban executable")
    ap.add_argument("--platform", required=True,
                    help="name for the archive, e.g. linux-x64, windows-x86, macos-x64")
    ap.add_argument("--with-engine", action="append", default=[], metavar="DIR",
                    help="fold a local engine folder into engine/ (repeatable). "
                         "Used for the hraj.si bundle; CI never passes it.")
    ap.add_argument("--out", default="dist", help="where to write the archive")
    ap.add_argument("--format", choices=["tar", "zip"], default=None,
                    help="default: zip for windows, tar for everything else")
    args = ap.parse_args()

    binary = Path(args.binary)
    if not binary.is_file():
        sys.exit("error: no such binary: %s" % binary)

    version = project_version()
    name = "Goban-%s-%s" % (version, args.platform)
    out_dir = ROOT / args.out
    staging = out_dir / name

    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    shutil.copy2(binary, staging / binary.name)
    os.chmod(staging / binary.name, 0o755)

    copy_tree(ROOT / "config", staging / "config")

    for doc in DOCS:
        src = ROOT / doc
        if src.is_file():
            shutil.copy2(src, staging / doc)
        else:
            print("warning: %s is missing and will not be in the bundle" % doc)

    # Written to, at runtime, from the working directory. Shipping them empty is
    # what makes the folder runnable straight out of the archive.
    (staging / "games").mkdir()
    (staging / "engine").mkdir()
    (staging / "engine" / "README.txt").write_text(ENGINE_README, encoding="utf-8")

    for eng in args.with_engine:
        src = Path(eng)
        if not src.is_dir():
            sys.exit("error: --with-engine %s is not a directory" % eng)
        copy_tree(src, staging / "engine" / src.name)
        print("engine: %s" % src.name)

    problems = check_assets(staging, bool(args.with_engine))
    for p in problems:
        print("error: %s" % p, file=sys.stderr)
    if problems:
        return 1

    fmt = args.format or ("zip" if "windows" in args.platform else "tar")
    out = archive(staging, out_dir, fmt)

    size = sum(f.stat().st_size for f in staging.rglob("*") if f.is_file())
    print("bundle  %s  (%d files, %.1f MB)"
          % (staging, sum(1 for f in staging.rglob("*") if f.is_file()), size / 1e6))
    print("archive %s  (%.1f MB)" % (out, out.stat().st_size / 1e6))
    return 0


if __name__ == "__main__":
    sys.exit(main())
