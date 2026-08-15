# Testing

Goban's game logic is separated from its rendering so that it can be tested
without a graphics context.

## Layout

| Target | What it is | Needs a GPU? |
|---|---|---|
| `goban_core` | Static library: rules, SGF, navigation, engine sync, GTP client, settings | No |
| `goban` | The application: rendering, RmlUi, audio, platform glue | Yes |
| `goban_tests` | doctest suite over `goban_core` | No |
| `mock_gtp_engine` | Deterministic GTP engine used as a test double | No |

The split is what makes the tests cheap. `Board`, `GobanModel`, `GameRecord`,
`GameNavigator`, `GameThread`, `PlayerManager` and `GtpClient` contain no
OpenGL, GLFW or RmlUi rendering calls, so a test binary links them directly.

The one deliberate exception: `Configuration.h` uses `Rml::Input::KeyIdentifier`
as its keybinding key type. That is a header-only enum, so it costs an include
path and nothing else.

## Running the tests

Full suite, via the build system:

```bash
cd cmake-build-release
make -j4 goban_tests
./goban_tests            # or: ctest --output-on-failure
```

Useful doctest flags:

```bash
./goban_tests --list-test-cases
./goban_tests --test-case="*capture*"
./goban_tests --success            # show passing assertions too
```

### Fast single-file loop

Rebuilding through cmake is unnecessary when iterating on one test file, and it
serialises work if several people (or agents) are editing different suites at
once. Use:

```bash
./tests/compile_one.sh tests/test_board_rules.cpp
./tests/compile_one.sh tests/test_board_rules.cpp -- --test-case="*ko*"
```

This compiles just that file against the prebuilt `libgoban_core.a`, builds the
mock engine if needed, and runs it. It does not touch `cmake-build-release`
beyond reading the library. After changing anything under `src/`, rebuild the
library first: `cd cmake-build-release && make goban_core`.

## Adding a test

Drop a new `tests/*.cpp` file in. CMake globs the directory with
`CONFIGURE_DEPENDS`, so no build file edit is needed.

```cpp
#include <doctest/doctest.h>
#include "Board.h"

TEST_CASE("a single stone in atari is captured") {
    Board board(9);
    // ...
    CHECK(board.getSize() == 9);
}
```

`tests/test_main.cpp` is the only file that defines `main()`. It also defines
the `config` global that `goban_core` declares but the application normally
provides — it defaults to null, and `GameRecord` guards every use, so tests
that do not care about configuration can ignore it.

## The mock GTP engine

`tests/mock_gtp_engine.cpp` implements the GTP subset goban uses, with its own
rules implementation. It deliberately does **not** reuse `src/Board.cpp`: a test
double that shares an implementation with the code under test cannot catch a bug
in that implementation.

It is deterministic. `genmove` either replays a scripted vertex list or plays
the first legal point in a fixed scan order.

```bash
printf 'boardsize 9\ngenmove B\nquit\n' | ./cmake-build-release/mock_gtp_engine --script E5
```

| Option | Effect |
|---|---|
| `--name` / `--version` | What `name` / `version` report |
| `--script D4 Q16 pass` | Scripted `genmove` replies, then first-legal fallback |
| `--delay-ms <n>` | Delay every response, to simulate a slow engine |
| `--hang-on <cmd>` | Never answer `<cmd>` — used to test read timeouts |
| `--fail-on <cmd>` | Answer `<cmd>` with a GTP error |
| `--unknown <cmd>` | Report `<cmd>` as an unknown command, to exercise graceful degradation |
| `--log <file>` | Record every command received, to assert what the app sent |
| `--stderr-file <p>` | Emit a file's contents on stderr before each `genmove`, to test the `config/*.json` regex filters |
| `--no-analyze` | Drop `lz-analyze` / `kata-analyze` from `list_commands` and refuse them, to exercise the degrade-to-absent path |
| `--analyze-interval-ms <n>` | How often the analysis stream reports when the command does not say (default 100) |
| `--analyze-winrate <f>` / `--analyze-score <f>` | The fixed values the stream reports, **from the side to move's point of view** |

The analysis stream is the one place the mock is not request/response: it writes
the success header with *no* terminating blank line, reports until any input
line arrives, and only then closes the response. That is the shape a real
analysis engine has, and the reason `GtpClient` needs `streamCommand()` rather
than `issueCommand()`. The fixed win rate is reported for the side to move, so a
test asserting 60 with Black to move and 40 with White to move is checking the
client's conversion to Black's frame of reference.

`GtpClient` splits its configured parameter string on whitespace, so a
multi-word option value cannot survive as a single argument. `--script` absorbs
following non-flag arguments to work around this; anything containing spaces or
`--` (such as KataGo-style analysis lines) is passed by file instead.

## Scenarios: driving the real application

Unit tests cover logic in isolation. Scenarios cover *sequences* — mode
switches, navigation while an engine is running, the interactions that no unit
test reaches and where most of this project's bugs have actually been.

A scenario drives the real binary through its command registry and asserts on
state it reads back, so the result is machine-checkable rather than something a
human has to eyeball.

```bash
cd cmake-build-release && make goban mock_gtp_engine
cd ..
./tests/run_scenarios.sh                                   # all
./tests/run_scenarios.sh tests/scenarios/analysis_mode_toggle.scn
```

The window is created hidden, and settings are redirected to a scratch file, so
a run cannot take over your screen or touch `user.json`, `games/` or your saved
camera. In CI, wrap it: `xvfb-run -s "-screen 0 1024x768x24" tests/run_scenarios.sh`.

### Screenshots need X11

The `screenshot <path>` directive writes the finished frame as a binary PPM
(`magick shot.ppm shot.png` to view it). It only works under X11 — XWayland
counts — because **a window hidden under Wayland is never mapped, so its surface
has no buffer and everything rendered into it is discarded**: `glReadPixels`
returns black with no GL error to explain it. Rendering into a framebuffer object
does not help; the discard happens regardless of the target.

So a scripted run asks GLFW for X11 when `DISPLAY` is set, and falls back to
whatever GLFW finds if that fails. Override with `--platform wayland|x11|auto`.
On a Wayland-only box with no XWayland, scenarios run fine and screenshots come
out black — there is no way around that short of Xvfb.

For a one-off look at something, write a throwaway scenario ending in
`screenshot /tmp/shot.ppm` rather than adding a permanent one: the harness cannot
assert on pixels, so a committed screenshot scenario guards nothing.

### Script syntax

| Directive | Meaning |
|---|---|
| `<command> [args]` | anything in the `GobanControl` registry (`help` lists them) |
| `expect <key> [op] <value>` | assert on a state key; `op` is `==` (default), `!=`, `>=`, `<=`, `>`, `<` |
| `expect_not <key> <value>` | negation |
| `wait_idle [ms]` | wait until no engine is thinking and no navigation is queued |
| `wait_until <key> [op] <value>` | wait for a state key to reach a value |
| `wait <ms>` | unconditional delay — prefer `wait_idle` |
| `key <name>` | press a key, down then up |
| `dump_state [label]` | log the whole state; use this when authoring |
| `fail_fast on\|off` | stop at the first failure (default on) |

`key` exists because a few behaviours live in `GobanControl::keyPress()` and
never reach the command registry — most importantly **Space at the end of an
unfinished branch, which falls through to kibitz while Right does not**. Names:
`space`, `left`, `right`, `up`, `down`, `home`, `end`, `backspace`, `enter`,
`escape`, a single letter `a`–`z`, or a raw RmlUi key code. See
`tests/scenarios/navigation_keys.scn`.

A value of `$otherKey` compares two pieces of state, e.g.
`expect view_position $main_line_moves`.

**Values may contain spaces** — everything after the key, and after the operator
when one is given, is the value. `expect comment markup node` works; before this
"markup" was read as an operator and the line failed as malformed, so an SGF
comment could not be asserted at all. `""` means the empty string, since a value
is required and an absent one cannot be told from a typo: `expect comment ""`.
Runs of whitespace cannot be expressed, though: the value is re-joined from the
split tokens with single spaces, so `a  b` and `a b` are the same expectation.

Confirmation prompts are scriptable: `board_size <n>` and `handicap <n>` take
the same route as the dropdowns, so they ask before replacing a game worth
keeping. Answer with `prompt_yes` / `prompt_no`, and assert `prompt_active` to
show the prompt really appeared — otherwise a command that silently does nothing
looks identical to one that was refused. `new_game <size>` stays unprompted.
`komi <points>` is the komi dropdown, which never replaces a game and so never
asks; it is simply refused once play has begun.

`load_tsumego <path> [gameIndex]` is the file chooser's tsumego toggle. Without
it the mode was reachable only by clicking that dialog, so nothing about it
could be tested at all — see `tests/scenarios/tsumego_mode.scn`.

The Load dialog itself is scriptable through the `chooser_*` family:
`chooser_open`, `chooser_cancel`, `chooser_confirm`, `chooser_path <dir>`,
`chooser_up`, `chooser_select_file <name>`, `chooser_select_game <index>`,
`chooser_tsumego <on|off>`, `chooser_files_page <next|prev>` and
`chooser_games_page <next|prev>`. They drive the real handler, so a scenario
takes the path a user does. Files are selected **by name**, never by index — a
scenario cannot know the order the filesystem produced, and pinning one would
make the test depend on it. State keys: `chooser_active`, `chooser_path`,
`chooser_file_count`, `chooser_game_count`, `chooser_file`, `chooser_game`,
`chooser_tsumego`.

The logic *underneath* the dialog is unit-tested instead, in
`tests/test_filechooser.cpp`. `FileChooserDataSource` includes no RmlUi at all —
it is filesystem, SGF parsing and pagination — and was untestable only because
it sat in the `goban` app target rather than `goban_core`. Prefer a doctest case
there to a scenario for anything that does not involve the widgets.

Four rules learned the hard way:

- **Never assert an exact value on something that moves on its own.** The
  genmove loop is fast, so `wait_until move_count 4` can race straight past 4.
  Use `>=`, or compare against another key.
- **`wait_idle`, not `wait <ms>`.** Navigation is queued onto the game thread,
  so a fixed sleep is both slower and less reliable.
- **But `wait_idle` does not cover a click.** A click hands the move to a
  `LocalHumanPlayer` blocked in `genmove()`; until the game loop wakes and takes
  it, nothing is thinking, queued, syncing or deferred, so `isIdle()` is briefly
  true with the move not yet in the record. Observed as `move_count 0` alongside
  `color_to_move` already flipped and `unsaved_changes true` — roughly one run in
  eight. After a click use `wait_until move_count >= n`, not `wait_idle` followed
  by `expect`.
- **Board coordinates are not SGF coordinates.** `click <col> <row>` takes board
  columns and rows, and board rows run opposite to SGF rows: board row N is SGF
  row `size-1-N`. `B[bh]` on a 9×9 is `click 1 1`. Getting it backwards is
  silent — the click lands on some other empty point, quietly creates a
  variation, and the scenario fails several assertions further down.
- **Placing a stone takes two clicks.** One takes it out of the bowl
  (`holds_stone` becomes true and *nothing is recorded* — the game does not even
  start), one puts it down. The second click decides the point, not the first.

### Recording a session: About → Report bug

Recording is always on — a bounded ring buffer of the last 500 interactions.
You do not have to arm it in advance, which matters because the bugs worth
capturing are the ones you did not expect.

When something looks wrong, choose **Report bug** under the version group in the
menu (or run the `report_bug` command). It writes
`reports/bugreport-<timestamp>.scn` and shows the path.

The report contains:

- a **prologue** reconstructing the state the oldest retained action started
  from — board size, players, mode, and a `load_sgf` line if a file was open.
  Skipped when the recording already begins with `new_game` or `load_sgf`.
- the actions themselves, each followed by `settle` and a `# state:` comment
  showing what that action produced.
- the **engine's genmove replies**, as a ready-made
  `mock_gtp_engine --script ...` line. This is what lets a session played
  against KataGo replay with no engine installed.

To turn it into a regression test: find the state comment that looks wrong,
promote the relevant key to an `expect` line, correct the value to what it
*should* have been, and delete the comments you do not need.

Two honest limits:

- Replay runs at machine speed. It reproduces **state and logic** bugs
  reliably, but can mask **timing races** that happened at human speed.
- If the ring buffer overflowed, the report says so. The prologue still
  reconstructs a valid starting point, but anything before it is gone.

`--record` forces recording on during a scripted run; it is off by default
there, since recording a replay is noise.

### Assertable state keys

`GobanControl::dumpState()` is the single source; a failure prints all of it.
Currently: `move_count`, `view_position`, `main_line_moves`, `navigating`,
`at_end`, `variations`, `has_result`, `result`, `board_size`, `color_to_move`, `komi`,
`handicap`, `black_stones`, `white_stones`, `captured_black`, `captured_white`,
`mode`, `ai_vs_ai`, `phase`, `loop_state`, `engine_sync`, `running`, `thinking`, `syncing_ui`,
`pending_nav`, `queued_nav`, `deferred_task`,
`tsumego`, `holds_stone`, `show_territory`, `unsaved_changes`, `comment`,
`markup_count`, `prompt_active`,
`camera_animating`, `msg`, `black_player`, `white_player`, `sgf_file`,
`game_index`, `board_hash`.

Plus the nine booleans behind the toolbar: `can_start`, `can_pass`,
`can_resign`, `can_undo`, `can_kibitz`, `can_navigate`, `can_territory`,
`can_clear`, `can_save`. Since ADR-0005 these are also the guards every command
applies to itself, so asserting one pins the button *and* the keybinding. Prefer
them to inferring a refusal from an unchanged position: `expect can_undo false`
says what you mean, whereas "the move count did not change" cannot tell a
refusal apart from an action that legitimately had nothing to do.

Add a key there and every scenario can assert on it.

Plus the live evaluation overlay: `eval_enabled` (the toggle), `eval_state`
(`disabled`/`starting`/`unavailable`/`yielded`/`running`), `eval_winrate` (an
integer percentage **for Black**, or `-1` when there is no report),
`eval_has_score`, `eval_score`, `eval_moves`, `eval_stale` and `can_evaluation`.
For the board suggestions: `eval_moves_shown` (the toggle, off by default),
`eval_labels` (suggestions this overlay labelled) and `eval_tints` (suggestions
that landed on a move already in the record and were only coloured). The split
between the last two *is* the combine rule, which is why they are separate keys.
For the diegetic readout: `eval_on_board` (the toggle), `eval_align`
(`center`/`left`/`right`), `eval_color` (`#rrggbbaa`) and `eval_board_text`, the
composed string. The glyphs are never drawn in a headless run, so that string is
the only way to check the readout's *content* — which colour leads, how the score
is written. Both report what was last **drawn**, so they need `wait_until` rather
than `expect` right after a toggle.
Note that the mock always proposes the first two empty points in scan order, so a
scenario testing a collision has to play *there* — A1 and B1 — for a suggestion
and a recorded move to meet.
**Wait on `eval_state`, not `eval_enabled`**: the toggle flips the instant it is
pressed, long before a process has started or a number has arrived. And
`eval_winrate` is `-1` rather than `50` when nothing has been computed, for the
same reason `Engine::final_score()` returns an optional — a bar resting at the
midpoint cannot be told from a genuine even game.

The overlay is deliberately **not** part of `isIdle()`, so `wait_idle` says
nothing about it. An analysis stream never finishes on its own; treating it as
work-in-flight would make quiescence unreachable, exactly as `EngineSync::Unsynced`
would.

Plus the message log: `log_count`, `log_unseen`, `log_open`, `log_badge`
(`none`/`warning`/`error`) and `engine_loading` (the engine the status indicator is naming, empty
once loading is done). **Assert `log_badge`, not `log_count`** when you mean "the
user was told": the count changes even when nothing is shown, so a test written
against it passes even if the badge never appears — which is the entire failure
this feature exists to prevent.

`pending_nav`, `queued_nav` and `deferred_task` are the quiescence terms
`wait_idle` is built from, exposed separately so a scenario can pin *which* of
them is holding things up. `pending_nav` covers commands that have been popped
from the queue but not yet finished; `queued_nav` is the queue alone.

**Beware what `dumpState()` costs.** It runs on the UI thread and reads the SGF
tree, which the game thread owns and mutates without a lock. Do not add a key
whose getter walks that tree — publish the value from the game thread into an
atomic instead, as `phase` and the territory predicate do. Adding one such call
to `uiInputs()` produced an intermittent segfault in `SgfcProperty`'s
destructor, roughly one run in six.

### The engine used by scenarios

`tests/scenarios/mock.json` overrides the bot list with `mock_gtp_engine`, so
scenarios are deterministic and need no installed engine. It `$include`s
`config/en.json`, so the GUI, fonts and shaders are the real ones.

It declares `"main": 1` on the mock engine, and that is not decoration. With no
engine carrying the flag, `currentCoach()` falls back to `players[0]` — under
parallel loading, whichever of the two mock engines finished first. The suite ran
that way for months; the coach being the fast mock rather than the slow one was
luck, not design.

A scenario that needs a different bot list declares it with a `# config: <file>`
line, resolved relative to `tests/scenarios`:

```
# config: mock-eval.json
```

`tests/scenarios/mock-eval.json` configures one engine carrying both `main` and
`kibitz` whose playing instance cannot analyse and whose analysis instance can —
the two-processes-from-one-configuration shape ADR-0007 is about.
`mock-noeval.json` is the same engine with no `analysis_parameters`, so the
analysis instance inherits `--no-analyze` and the overlay degrades to absent.
Without this directive the suite would have one config for the whole run, and
anything needing a differently configured engine could only be run by hand — so
`./tests/run_scenarios.sh` would silently not cover it.

An explicit `GOBAN_SCENARIO_CONFIG` still overrides the directive, which is what
makes the language sweep below work on every scenario.

To run a scenario against another language's interface — worth doing after
touching any `.rml`, since a missing element degrades silently to no display:

```bash
GOBAN_SCENARIO_CONFIG=$PWD/tests/scenarios/mock-cs.json \
    ./tests/run_scenarios.sh tests/scenarios/message_log.scn
```

**Do not run `./goban --script` directly to get around the harness.**
`run_scenarios.sh` also redirects `user.json` and the games folder into a scratch
directory; without it a scripted run saves over the real ones.

## What to test, and what not to

Aim tests at where the bugs have actually been. In this project that is SGF
handling, tree navigation, and engine synchronisation — not rendering.

Worth covering:

- Go rules: captures, suicide, ko, snapback, edges and corners
- SGF round-trips, variations, and the invariants in CLAUDE.md's
  "SGF Game Record Consistency" section
- Navigation: back/forward/start/end, variations, tree paths
- Engine sync and graceful degradation when an engine lacks a command

Not worth unit-testing:

- Shader output and camera framing — cover these with golden images instead
  (rendering is deterministic apart from `Board`'s stone-placement RNG)
- RmlUi layout

## When a test fails

A failing test on this codebase is as likely to have found a real bug as to be
wrong itself, because most of `goban_core` predates having any tests.

Work out which it is before changing anything. If the production code is wrong,
do not weaken the test — mark it `TEST_CASE("..." * doctest::skip())` with a
comment explaining the suspected bug, so the suite stays green while the defect
stays recorded, and fix it separately.
