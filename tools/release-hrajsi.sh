#!/usr/bin/env bash
#
# Builds the hraj.si download bundles and puts them where the site expects them.
#
#   tools/release-hrajsi.sh [--run <ci-run-id>] [--date YYYY-MM-DD]
#
# hraj.si carries Windows bundles with GNU Go already inside — that convenience
# is the whole reason the page exists, as against the engine-less archives on
# GitHub Releases. This takes the binaries CI already built, folds in the
# cross-compiled GNU Go, and writes the zips into the site repository.
#
# It does not upload. Deployment is `make goban` in the site repo, which rsyncs
# app/goban to rosti.cz — run it once you have looked at the page.
#
# The GNU Go binaries are built from the official 3.8 tarball plus one patch;
# see cmake/patches/gnugo-3.8-implicit-int.patch. Shipping them obliges us to
# offer that source, which THIRD-PARTY.md does, and THIRD-PARTY.md is in the
# bundle.
set -euo pipefail

cd "$(dirname "$0")/.."

SITE="${SITE:-$HOME/github/hraj-si/app/goban}"
GNUGO="${GNUGO:-$HOME/github/gnugo-3.8-build/interface}"
RUN=""
DATE="$(date +%Y-%m-%d)"

while [ $# -gt 0 ]; do
    case "$1" in
        --run)  RUN="$2"; shift 2 ;;
        --date) DATE="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

VERSION=$(sed -n 's/.*project(goban.*VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ -n "$VERSION" ] || { echo "error: no VERSION in CMakeLists.txt" >&2; exit 1; }

[ -d "$SITE/static" ] || { echo "error: no site at $SITE (set SITE=)" >&2; exit 1; }
for arch in x86 x64; do
    [ -f "$GNUGO/gnugo-$arch.exe" ] || {
        echo "error: missing $GNUGO/gnugo-$arch.exe (set GNUGO=)" >&2; exit 1; }
done

if [ -z "$RUN" ]; then
    RUN=$(gh run list --workflow=build.yml --status=success --limit 1 \
              --json databaseId -q '.[0].databaseId')
    echo "using the most recent successful build: run $RUN"
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

echo "version $VERSION, dated $DATE"
for arch in x64 x86; do
    echo
    echo "--- windows-$arch ---"
    rm -rf "$WORK/$arch" && mkdir -p "$WORK/$arch"
    gh run download "$RUN" -n "goban-windows-$arch" -D "$WORK/$arch"

    zip=$(find "$WORK/$arch" -name '*.zip' | head -1)
    [ -n "$zip" ] || { echo "error: no zip in the artifact" >&2; exit 1; }
    (cd "$WORK/$arch" && unzip -q "$(basename "$zip")")

    exe=$(find "$WORK/$arch" -name goban.exe | head -1)
    [ -n "$exe" ] || { echo "error: no goban.exe in the artifact" >&2; exit 1; }

    # Repackaged rather than injected into the existing zip: package.py takes the
    # config tree from *this checkout*, so the assets and the binary come from
    # the same revision, and its own asset check runs again with the engine in
    # place. Be on the release tag when running this.
    python3 tools/package.py \
        --binary "$exe" \
        --platform "windows-$arch" \
        --format zip \
        --with-engine "gnugo=$GNUGO/gnugo-$arch.exe"

    out="$SITE/static/goban_${DATE}_v${VERSION}_win-${arch}.zip"
    cp "dist/Goban-${VERSION}-windows-${arch}.zip" "$out"
    echo "-> $out  ($(du -h "$out" | cut -f1))"
done

# The gallery. The page's previous images were six years old and two of them
# showed a menu bar removed in the RmlUi migration, so they advertised a program
# nobody could download. These are regenerated per release by
# tools/make-screenshot.sh; copying them here is the last manual-ish step.
SHOTS="hero territory closeup shader stereo detail"
echo
echo "--- images ---"
for shot in $SHOTS; do
    for suffix in "" "-thumb"; do
        src="res/screenshot/${shot}${suffix}.png"
        [ -f "$src" ] || { echo "warning: no $src (run tools/make-screenshot.sh)"; continue; }
        cp "$src" "$SITE/static/image/goban-${VERSION}-${shot}${suffix}.png"
    done
done
echo "-> $SITE/static/image/goban-${VERSION}-*.png"

GALLERY=""
for shot in $SHOTS; do
    [ -f "res/screenshot/${shot}.png" ] || continue
    case $shot in
      stereo) title=' title="anaglyph shader — needs red/cyan glasses"' ;;
      detail) title=' title="engine evaluation drawn on the board"' ;;
      *)      title="" ;;
    esac
    GALLERY="${GALLERY}      a(href=\"/goban/image/goban-${VERSION}-${shot}.png\")
        img(src=\"/goban/image/goban-${VERSION}-${shot}-thumb.png\"${title})
      | &nbsp;
"
done

cat <<EOF

Replace the gallery block in $SITE/views/pages/index.pug with:

${GALLERY}
Add to $SITE/views/pages/index.pug — the download link near the top, and a
changelog section. The link text is what the analytics event records, so keep it
the file name:

      a(href="/goban/goban_${DATE}_v${VERSION}_win-x64.zip" onclick="trackdown(this);" target="_blank") goban_${DATE}_v${VERSION}_win-x64.zip
      br
      a(href="/goban/goban_${DATE}_v${VERSION}_win-x86.zip" onclick="trackdown(this);" target="_blank") goban_${DATE}_v${VERSION}_win-x86.zip

Then deploy:

      cd $(dirname "$SITE") && make goban
EOF
