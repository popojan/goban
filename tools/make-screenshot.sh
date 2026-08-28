#!/usr/bin/env bash
#
# Regenerates the screenshots for the README and hraj.si.
#
#   tools/make-screenshot.sh                 # every shot, then derive
#   tools/make-screenshot.sh hero stereo     # only these
#   tools/make-screenshot.sh --derive        # re-cut previews from existing PNGs
#   tools/make-screenshot.sh --derive 720    # ...at a different preview width
#
# The point is that the images are *reproducible*: the game, the move, the
# camera, the shader and the overlays are pinned per shot, so a release
# regenerates the whole set instead of inheriting a snapshot. The previous set on
# hraj.si was six years old, and two of its images showed a menu bar that had not
# existed since the RmlUi migration — which is worse than an old screenshot,
# because it shows a program that is not the one being downloaded.
#
# Each shot is a pair in res/screenshot/: <name>.scn drives the application and
# <name>-user.json pins the camera and the shader. --user-settings keeps both out
# of the real user.json, and the real user.json out of the screenshots.
#
# `hero` additionally derives the README preview and the 1:1 detail crop.
#
# A capture takes the screen fullscreen for about a minute per shot, and `hero`
# needs a real analysis engine, so the whole set is a few minutes with the
# display unusable. --derive re-cuts from the PNGs already on disk and launches
# nothing.
set -euo pipefail

cd "$(dirname "$0")/.."

OUT_DIR="res/screenshot"
PREVIEW="res/screenshot.png"      # what README.md references
DETAIL="$OUT_DIR/detail.png"      # 1:1 crop, for hraj.si
ALL_SHOTS="hero territory stereo shader closeup"

DERIVE_ONLY=0
PREVIEW_W=880
if [ "${1:-}" = "--derive" ]; then
    DERIVE_ONLY=1
    shift
    case "${1:-}" in ''|*[!0-9]*) ;; *) PREVIEW_W="$1"; shift ;; esac
fi
SHOTS="${*:-$ALL_SHOTS}"

derive_from_hero() {
    python3 - "$OUT_DIR/hero.png" "$PREVIEW" "$PREVIEW_W" "$DETAIL" <<'PY'
import sys
from PIL import Image
hero, preview, width, detail = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
im = Image.open(hero)

# The preview keeps the hero's aspect ratio rather than a fixed 16:9, so the
# board is never stretched or cropped by the downscale. LANCZOS because the
# annotations are 25% alpha on wood and a box filter loses them outright.
h = max(1, round(width * im.size[1] / im.size[0]))
im.resize((width, h), Image.LANCZOS).save(preview)
print("  preview %s  %dx%d" % (preview, width, h))

# The detail crop, at native resolution — no resampling, because the point is to
# show the annotations at the size they are actually drawn.
#
# 880 px wide: GitHub's .markdown-body is max-width 980 with 45 px padding, so
# about 890 px is usable, and images carry max-width:100% — anything wider is
# scaled down and is no longer 1:1. For a page you control, cut it to that page's
# content width.
#
# The band holds all five suggestions and the readout, so it shows the whole
# green-amber-red ramp. Coordinates are in hero pixels and follow the camera
# pinned in hero-user.json — change the framing and they need re-cutting, which
# is why a capture of another size is skipped rather than cropped blindly.
BAND = (590, 460, 1470, 990)
if im.size == (1920, 1080):
    c = im.crop(BAND)
    c.save(detail)
    print("  detail  %s  %dx%d  (1:1, no resampling)" % (detail, c.size[0], c.size[1]))
else:
    print("  detail  skipped: hero is %dx%d, crop band assumes 1920x1080" % im.size)
PY
}

if [ "$DERIVE_ONLY" = "1" ]; then
    [ -f "$OUT_DIR/hero.png" ] || { echo "error: --derive needs $OUT_DIR/hero.png" >&2; exit 1; }
    derive_from_hero
    exit 0
fi

[ -x ./goban ] || { echo "error: ./goban not built" >&2; exit 1; }

for shot in $SHOTS; do
    scn="$OUT_DIR/$shot.scn"
    settings="$OUT_DIR/$shot-user.json"
    ppm="$OUT_DIR/$shot.ppm"
    png="$OUT_DIR/$shot.png"

    [ -f "$scn" ] || { echo "error: no scenario $scn" >&2; exit 1; }
    [ -f "$settings" ] || { echo "error: no settings $settings" >&2; exit 1; }

    echo "=== $shot ==="
    rm -f "$ppm"
    GOBAN_SCENARIO_VISIBLE=1 ./goban \
        --verbosity warn \
        --config config/en.json \
        --user-settings "$settings" \
        --script "$scn"

    [ -f "$ppm" ] || { echo "error: $shot produced no capture — see last_run.log" >&2; exit 1; }
    python3 - "$ppm" "$png" <<'PY'
import sys
from PIL import Image
im = Image.open(sys.argv[1]); im.save(sys.argv[2])
print("  %s  %dx%d" % (sys.argv[2], im.size[0], im.size[1]))

# A thumbnail beside it. hraj.si's gallery is <a href=full><img src=thumb></a>
# with max-height:120px in the page CSS, so 480 wide is comfortably past what a
# HiDPI screen asks for and still small enough to load a page full of them.
thumb = sys.argv[2].replace(".png", "-thumb.png")
t = im.copy(); t.thumbnail((480, 480), Image.LANCZOS); t.save(thumb)
print("  %s  %dx%d" % (thumb, t.size[0], t.size[1]))
PY
    rm -f "$ppm"

    if [ "$shot" = "hero" ]; then derive_from_hero; fi
done

echo
echo "Look at them before shipping: the annotations are deliberately faint, and"
echo "whether they survive at $PREVIEW_W px is a judgement, not a test."
