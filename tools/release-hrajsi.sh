#!/usr/bin/env bash
#
# Builds the hraj.si download bundles and puts them where the site expects them.
#
#   tools/release-hrajsi.sh [--run <ci-run-id>] [--date YYYY-MM-DD]
#                           [--zips-only] [--label <text>] [--deploy]
#
#   --zips-only   build the bundles and stop: no gallery images, no page
#                 instructions. For putting a build up to be tried before it is
#                 announced.
#   --label rc1   goes into the file name after the version, to keep a trial
#                 build apart from the release of the same number.
#   --deploy      upload the zips, and only the zips, then check they download.
#                 Not `make goban`: that sends the whole app/goban folder, an
#                 edited index.pug included. The page still goes up that way.
#
# hraj.si carries Windows bundles with GNU Go already inside — that convenience
# is the whole reason the page exists, as against the engine-less archives on
# GitHub Releases. This takes the binaries CI already built, folds in the
# cross-compiled GNU Go, and writes the zips into the site repository.
#
# Without --deploy it does not upload. Publishing the *page* is `make goban` in
# the site repo, which rsyncs app/goban to the server — run it once you have
# looked at the page. Where the server is comes from that Makefile, read at run
# time; it is not written down in this repository.
#
# The bundle's config and documents are taken from the CI artifact, not from
# this checkout: they are then the assets built with that binary, and a local
# edit — an engine switched on for testing — cannot ride along.
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
ZIPS_ONLY=0
DEPLOY=0
LABEL=""

while [ $# -gt 0 ]; do
    case "$1" in
        --run)  RUN="$2"; shift 2 ;;
        --date) DATE="$2"; shift 2 ;;
        --zips-only) ZIPS_ONLY=1; shift ;;
        --deploy) DEPLOY=1; shift ;;
        --label) LABEL="-$2"; shift 2 ;;
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

# Say what is being packaged. A bundle built from some other commit than the one
# checked out is legitimate — a branch build, say — but not something to find out
# afterwards.
RUN_SHA=$(gh run view "$RUN" --json headSha -q .headSha)
RUN_BRANCH=$(gh run view "$RUN" --json headBranch -q .headBranch)
echo "run $RUN: $RUN_BRANCH @ ${RUN_SHA:0:7}"
if [ "$RUN_SHA" != "$(git rev-parse HEAD)" ]; then
    echo "note: this checkout is at $(git rev-parse --short HEAD), not at that commit"
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
BUILT=()

echo "version $VERSION$LABEL, dated $DATE"
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

    # The artifact names its own version; it has to be the one this script is
    # about to put in the file name.
    case "$(basename "$zip")" in
        "Goban-${VERSION}-windows-${arch}.zip") ;;
        *) echo "error: the artifact is $(basename "$zip"), but CMakeLists says $VERSION" >&2
           exit 1 ;;
    esac

    # Repackaged rather than injected into the existing zip, so package.py's
    # asset check runs again with the engine in place — but from the artifact's
    # own config tree (--from-bundle), which is the one built with this binary.
    python3 tools/package.py \
        --binary "$exe" \
        --from-bundle "$(dirname "$exe")" \
        --platform "windows-$arch" \
        --format zip \
        --with-engine "gnugo=$GNUGO/gnugo-$arch.exe"

    out="$SITE/static/goban_${DATE}_v${VERSION}${LABEL}_win-${arch}.zip"
    cp "dist/Goban-${VERSION}-windows-${arch}.zip" "$out"
    BUILT+=("$out")
    echo "-> $out  ($(du -h "$out" | cut -f1))"
done

deploy() {
    # The server's address lives in the site's Makefile and nowhere here.
    local makefile rule rsh dest
    makefile="$(dirname "$(dirname "$SITE")")/Makefile"
    rule=$(sed -n '/^goban:/,/^$/p' "$makefile" | grep -m1 rsync || true)
    rsh=$(printf '%s' "$rule" | sed -n 's/.*-e "\([^"]*\)".*/\1/p')
    dest=$(printf '%s' "$rule" | awk '{print $NF}')
    [ -n "$rsh" ] && [ -n "$dest" ] || {
        echo "error: no rsync rule for 'goban' in $makefile" >&2; exit 1; }

    echo
    echo "--- upload ---"
    rsync -e "$rsh" --times --partial --info=stats1 "${BUILT[@]}" "${dest%/}/goban/static/"

    # Fetched back, not assumed: the size the server reports has to be the size
    # that was sent.
    local f url want got
    for f in "${BUILT[@]}"; do
        url="https://hraj.si/goban/$(basename "$f")"
        want=$(stat -c %s "$f")
        got=$(curl -fsSIL "$url" | tr -d '\r' \
                  | awk 'tolower($1)=="content-length:" {n=$2} END {print n}')
        if [ "$got" = "$want" ]; then
            echo "ok  $url"
        else
            echo "error: $url reports ${got:-nothing}, expected $want bytes" >&2; exit 1
        fi
    done
}

if [ "$ZIPS_ONLY" = 1 ]; then
    [ "$DEPLOY" = 1 ] && deploy
    exit 0
fi

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

# Asked for explicitly: the zips go up now, the page whenever it is ready.
if [ "$DEPLOY" = 1 ]; then deploy; fi
