# Red Carpet Goban 0.2.1

Ray-traced 3D Go board with GLSL rendering, playing through external GTP engines.

A maintenance release on top of 0.2.0. Most of it is fixes — several of them to
things that damaged a game record without saying so — plus the handful of
changes that were already waiting when 0.2.0 went out.

## Easy setup

Bundles with GNU Go already included are at **[hraj.si/goban](https://hraj.si/goban)**.

The archives here contain the application and all its assets but **no Go
engine** — Goban needs at least one GTP engine to play against. Unpack, put an
engine in `engine/` (see `engine/README.txt`), and run from the unpacked folder;
if `gnugo` is already on your PATH it will be found without any configuration.

## What is new

**The window stays alive while a shader is built.** The first use of each shader
takes seconds while the graphics driver links it. That used to freeze the window
with nothing to say so; it now happens in the background, with a message.

**The engine answers after you have decided.** Best Moves has three settings:
off, always, and *on demand* — where the suggestions appear only once you have
taken a stone and held it over a point. The readout says what the point you are
aiming at would cost against the engine's own choice.

**Tsumego is a game mode**, beside Match and Explore (formerly "Analysis"), and
a wrong move is now answered with the refutation the problem records.

**Prisoners are counted on the board**, on the right-hand margin. The bowls and
their lids can be switched off under View → Scene, which is worth about half
again the frame rate in fullscreen.

**An engine that fails tells you why.** When an engine process exits — a crash,
or a model file it cannot open — the message panel now carries what the engine
itself said on the way out. It is restarted, and if it keeps failing the game is
paused rather than left waiting for a move that will never come.

## Fixed

- Reviewing a finished game could rewrite it. Asking the engine for a move, or
  in Explore mode simply clicking the recorded next move, put the engine's move
  over the game actually played and deleted its result.
- A hint in a tsumego replaced the problem's recorded answer with the engine's
  move, after which the wrong move read as "Solved".
- A komi chosen after the board appeared never reached the engines: they played
  and scored the game with the previous one.
- Kibitz on your own turn searched with no sign of it, and a second press played
  a second move.
- Starting a new game while the score was being calculated, or while the engines
  were being resynchronised, froze the window until the engine answered.
- On Windows, an engine could fail to start when Goban was unpacked into a
  folder with a space in its name. A path with a space can also be written in
  an engine's `parameters` now, in double quotes.
- Starting a new game waited for the main engine before the window responded —
  seconds, when that engine is KataGo and the board size changed.
- An SGF with a board larger than 19×19 was loaded and read past the board.
- A drawn game was recorded as `B+0` and announced as a win.
- One value of the wrong type in `user.json` discarded every setting after it.
- Game records are now written atomically; an interrupted save could leave the
  day's file empty.
- The players of a resumed session reverted to those named in the SGF.
- `delete camera` deleted every setting, not just the saved camera.
- Without an audio device, every stone added two entries to the message panel.

## Known limitations

- The bundle is a **portable folder**: it reads and writes `config/`, `games/`
  and `user.json` beside the executable, so unpack it somewhere you can write to
  rather than into a system directory.
- macOS builds are experimental and unsigned.
- The evaluation needs an engine that supports `lz-analyze` or `kata-analyze`;
  GNU Go does not.
- Boards larger than 19×19 are not supported.

## Building

See `docs/building.md`. `THIRD-PARTY.md` lists what Goban is built from.
