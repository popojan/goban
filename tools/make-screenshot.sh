#!/usr/bin/env bash
#
# Produces the release screenshot: a full-resolution hero image and the small
# preview, from a pinned game, move, camera and overlay set.
#
# The point is reproducibility. The previous screenshot was hand-captured and
# nobody could recreate it, so every release started with hunting for a position
# again. This runs res/screenshot/hero.scn, which fixes all of it.
#
#   tools/make-screenshot.sh [preview-width]
#
# Needs a real analysis engine — the suggestions and the win rate in the shot are
# genuine, not illustrative — so it runs against the shipped config rather than
# the mock engines, and takes a minute or so while KataGo loads its weights.
#
# The window must be REAL: a scripted run hides it by default, and a hidden
# surface renders nothing under Wayland, so GOBAN_SCENARIO_VISIBLE is set. Expect
# a window to appear, go fullscreen, and vanish.
set -euo pipefail

cd "$(dirname "$0")/.."

PREVIEW_W="${1:-640}"
OUT_DIR="res/screenshot"
PPM="$OUT_DIR/hero.ppm"
HERO="$OUT_DIR/hero.png"
PREVIEW="res/screenshot.png"      # what README.md references
DETAIL="$OUT_DIR/detail.png"      # 1:1 crop, for hraj.si

if [ ! -x ./goban ]; then
    echo "error: ./goban not built" >&2
    exit 1
fi

rm -f "$PPM"

# --user-settings keeps the pinned camera out of the real user.json, and the real
# user.json out of the screenshot.
GOBAN_SCENARIO_VISIBLE=1 ./goban \
    --verbosity warn \
    --config config/en.json \
    --user-settings "$OUT_DIR/hero-user.json" \
    --script "$OUT_DIR/hero.scn"

if [ ! -f "$PPM" ]; then
    echo "error: no capture was written — see last_run.log" >&2
    exit 1
fi

python3 - "$PPM" "$HERO" "$PREVIEW" "$PREVIEW_W" "$DETAIL" <<'PY'
import sys
from PIL import Image

ppm, hero, preview, width, detail = (sys.argv[1], sys.argv[2], sys.argv[3],
                                     int(sys.argv[4]), sys.argv[5])
im = Image.open(ppm)
im.save(hero)
print("hero    %s  %dx%d" % (hero, im.size[0], im.size[1]))

# The preview keeps the hero's aspect ratio rather than a fixed 16:9, so the
# board is never stretched or cropped by the downscale. LANCZOS because the
# annotations are 25% alpha on wood and a box filter loses them outright.
h = max(1, round(width * im.size[1] / im.size[0]))
im.resize((width, h), Image.LANCZOS).save(preview)
print("preview %s  %dx%d" % (preview, width, h))

# The detail crop, at native resolution — no resampling, because the point is to
# show the annotations at the size they are actually drawn.
#
# 880 px wide: GitHub's .markdown-body is max-width 980 with 45 px padding, so
# about 890 px is usable, and images carry max-width:100% — anything wider is
# scaled down and is no longer 1:1. (On a narrow window it scales anyway; that
# cannot be helped.) For a page you control, cut it to that page's content width.
#
# The band holds all five suggestions and the readout, so it shows the whole
# green-amber-red ramp rather than a couple of letters. Coordinates are in hero
# pixels and follow the camera pinned in hero-user.json — change the framing and
# they need re-cutting, which is why a capture of another size is skipped rather
# than cropped blindly.
BAND = (590, 460, 1470, 990)
if im.size == (1920, 1080):
    c = im.crop(BAND)
    c.save(detail)
    print("detail  %s  %dx%d  (1:1, no resampling)" % (detail, c.size[0], c.size[1]))
else:
    print("detail  skipped: hero is %dx%d, and the crop band assumes 1920x1080"
          % im.size)
PY

rm -f "$PPM"
echo
echo "Look at $HERO before shipping it: the annotations are deliberately faint,"
echo "and whether they survive at $PREVIEW_W px wide is a judgement, not a test."
