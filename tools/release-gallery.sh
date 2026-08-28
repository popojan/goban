#!/usr/bin/env bash
#
# Attaches the screenshots to a published GitHub release and embeds them in its
# body, so the release page shows what that version looks like.
#
#   tools/release-gallery.sh [tag]        # default: the most recent tag
#
# Release assets live outside git history entirely, which is the whole point:
# the images are regenerated every release and tracking them would add megabytes
# of blobs to a 16 MB repository for files nothing in it reads. They are
# gitignored, so CI cannot attach them either — it has no display, no engines and
# no GPU to render with. This runs from a machine that does, after the tag.
#
# Run tools/make-screenshot.sh first.
set -euo pipefail

cd "$(dirname "$0")/.."

OUT_DIR="res/screenshot"
SHOTS="hero territory closeup shader stereo detail"
MARKER="<!-- gallery -->"

TAG="${1:-$(git describe --tags --abbrev=0 2>/dev/null || true)}"
[ -n "$TAG" ] || { echo "error: no tag given and none found" >&2; exit 1; }

gh release view "$TAG" >/dev/null 2>&1 || {
    echo "error: no published release for $TAG — tag and let CI publish first" >&2
    exit 1; }

missing=0
for shot in $SHOTS; do
    for f in "$OUT_DIR/$shot.png" "$OUT_DIR/$shot-thumb.png"; do
        [ -f "$f" ] || { echo "missing $f"; missing=1; }
    done
done
[ "$missing" = "0" ] || { echo "error: run tools/make-screenshot.sh first" >&2; exit 1; }

if gh release view "$TAG" --json body -q .body | grep -qF "$MARKER"; then
    echo "$TAG already has a gallery; nothing to do."
    exit 0
fi

echo "uploading to $TAG..."
files=""
for shot in $SHOTS; do
    files="$files $OUT_DIR/$shot.png $OUT_DIR/$shot-thumb.png"
done
# shellcheck disable=SC2086
gh release upload "$TAG" $files --clobber

BASE="https://github.com/popojan/goban/releases/download/$TAG"

# Thumbnails inline, each linking to the full image. Embedding the full ones
# would make the release page several megabytes; this mirrors what hraj.si does
# and keeps it to a few hundred kilobytes.
gallery="$MARKER
## Screenshots
"
for shot in $SHOTS; do
    case $shot in
        stereo) alt="anaglyph shader — needs red/cyan glasses" ;;
        detail) alt="the engine's evaluation, drawn on the board" ;;
        hero)   alt="13x13 with the live evaluation" ;;
        territory) alt="a counted ending, with territory" ;;
        closeup)   alt="stones on wood" ;;
        shader)    alt="an alternative shader" ;;
    esac
    gallery="$gallery
[![$alt]($BASE/$shot-thumb.png)]($BASE/$shot.png)"
done

body=$(gh release view "$TAG" --json body -q .body)
printf '%s\n\n%s\n' "$body" "$gallery" > /tmp/release-body.md
gh release edit "$TAG" --notes-file /tmp/release-body.md
rm -f /tmp/release-body.md

echo "done: https://github.com/popojan/goban/releases/tag/$TAG"
