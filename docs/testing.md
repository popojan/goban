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

### Script syntax

| Directive | Meaning |
|---|---|
| `<command> [args]` | anything in the `GobanControl` registry (`help` lists them) |
| `expect <key> [op] <value>` | assert on a state key; `op` is `==` (default), `!=`, `>=`, `<=`, `>`, `<` |
| `expect_not <key> <value>` | negation |
| `wait_idle [ms]` | wait until no engine is thinking and no navigation is queued |
| `wait_until <key> [op] <value>` | wait for a state key to reach a value |
| `wait <ms>` | unconditional delay — prefer `wait_idle` |
| `dump_state [label]` | log the whole state; use this when authoring |
| `fail_fast on\|off` | stop at the first failure (default on) |

A value of `$otherKey` compares two pieces of state, e.g.
`expect view_position $main_line_moves`.

Two rules learned the hard way:

- **Never assert an exact value on something that moves on its own.** The
  genmove loop is fast, so `wait_until move_count 4` can race straight past 4.
  Use `>=`, or compare against another key.
- **`wait_idle`, not `wait <ms>`.** Navigation is queued onto the game thread,
  so a fixed sleep is both slower and less reliable.

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
`at_end`, `variations`, `has_result`, `board_size`, `color_to_move`, `komi`,
`handicap`, `black_stones`, `white_stones`, `captured_black`, `captured_white`,
`mode`, `ai_vs_ai`, `started`, `game_over`, `running`, `thinking`, `syncing_ui`,
`tsumego`, `holds_stone`, `show_territory`, `msg`, `black_player`,
`white_player`, `board_hash`.

Add a key there and every scenario can assert on it.

### The engine used by scenarios

`tests/scenarios/mock.json` overrides the bot list with `mock_gtp_engine`, so
scenarios are deterministic and need no installed engine. It `$include`s
`config/en.json`, so the GUI, fonts and shaders are the real ones.

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
