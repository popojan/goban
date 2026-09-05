# Red Carpet Goban 0.2.0

Ray-traced 3D Go board with GLSL rendering, playing through external GTP engines.

The first release since 0.1.0, and a large one: 239 commits. The headline is that
the engine's opinion is now drawn **into the board** rather than in panels over
it, and that a great deal that used to be silent — a thinking engine, a failed
engine, a wait — now says so.

## Easy setup

Bundles with GNU Go already included are at **[hraj.si/goban](https://hraj.si/goban)**.

The archives here contain the application and all its assets but **no Go
engine** — Goban needs at least one GTP engine to play against. Unpack, put an
engine in `engine/` (see `engine/README.txt`), and run from the unpacked folder;
if `gnugo` is already on your PATH it will be found without any configuration.

## What is new

**Live evaluation.** A second engine process, independent of the ones playing,
analyses whatever position you are looking at. It shows as a win rate and score
estimate carved into the board's edge, and — switched on separately — as the
engine's candidate moves lettered on the board and coloured by how much they give
up against its best. Off by default: the numbers are read after a move, and once
an engine has pointed at a point you cannot un-see it.

**Everything on the board.** Coordinate labels in the margin, an elapsed clock
while an engine thinks, and a mouse pointer drawn *in* the scene under a stereo
shader, where the window system's pointer sits at the wrong depth by definition.
Each of the three on-board displays switches on its own under View → Overlay.

**Stereo you can match to your glasses.** Anaglyph modes for red/cyan and
red/blue, per-channel crosstalk cancellation, per-eye brightness, and a control
for the green channel that real lenses disagree about. The depth budget is
documented in `docs/stereo.md` and enforced by tests.

**Tsumego mode.** Load a problem collection, solve interactively, explore wrong
answers and be told they are wrong.

**SGF and review.** A file chooser, multi-game collections, variations, and
navigation that keeps every configured engine in step with what you are looking
at.

**It tells you what is happening.** Engine failures, load progress and warnings
reach a message panel instead of only the log; a killed engine is restarted
rather than written off; and the application no longer freezes when you start a
new game, load a file or quit while an engine is thinking.

## Fixed

Far too many to list — including a hang on scoring, a hang on exit, several
crashes from data shared between threads without a lock, prisoners never being
drawn in the bowls, screenshots that never worked, and, on Wayland, the
compositor offering to kill the application whenever its window was not on screen.

## Known limitations

- The bundle is a **portable folder**: it reads and writes `config/`, `games/`
  and `user.json` beside the executable, so unpack it somewhere you can write to
  rather than into a system directory.
- macOS builds are experimental and unsigned.
- The evaluation needs an engine that supports `lz-analyze` or `kata-analyze`;
  GNU Go does not.
- **The first launch on a machine pauses while the shader is compiled** — around
  15 seconds on one Windows/NVIDIA box, 6 on Intel — with the window unresponsive
  and nothing to say so. The graphics driver caches the result, so it happens
  once and every later start is immediate. It is a genuine wait, not a hang.

## Building

See `docs/building.md`. `THIRD-PARTY.md` lists what Goban is built from.
