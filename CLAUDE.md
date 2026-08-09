# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Red Carpet Goban is a ray-traced 3D Go/Baduk/Weiqi board application with GUI rendered using OpenGL/GLSL. It supports external GTP engines (GNU Go is the main engine) and targets Windows 7/10 and Linux platforms.

## Build Commands

### Basic Build
```bash
# Build script (recommended)
./make.sh

# Manual build
mkdir -p cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DBuildPrerequisites=ON
make
cmake .. -DCMAKE_BUILD_TYPE=Release -DBuildPrerequisites=OFF
make
```

### Debug Build
```bash
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBuildPrerequisites=ON
make
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBuildPrerequisites=OFF
make
```

### Tests
```bash
cd cmake-build-release
make -j4 goban_tests mock_gtp_engine
./goban_tests            # or: ctest --output-on-failure
cd .. && ./tests/run_scenarios.sh
```
Fast single-file loop while writing tests: `./tests/compile_one.sh tests/test_x.cpp`.
See `docs/testing.md`.

### Running the Application
```bash
# Default run
./goban

# Chinese translation with suppressed logging
./goban --verbosity error --config config/zh.json
```

## Architecture Overview

### Core Components

#### Target layout
- **goban_core** (static lib): rules, SGF, navigation, engine sync, GTP client, settings — no OpenGL/RmlUi rendering, so tests link it without a graphics context
- **goban**: the application (rendering, UI, audio, platform glue)
- **goban_tests**, **mock_gtp_engine**: see `docs/testing.md`

#### Game Engine Layer
- **GobanModel**: Core game state and board representation
- **GobanView**: 3D rendering and visualization using OpenGL/GLSL
- **GobanControl**: Input handling and game flow control
- **GobanShader**: Shader management for ray-traced rendering
- **GobanOverlay**: UI overlay rendering with glyphy text

#### GTP Engine Integration
- **gtpclient**: GTP (Go Text Protocol) client for engine communication
- **GameThread**: Manages game logic and engine communication in separate thread
- **player**: Player abstraction for both human and AI players

#### UI Framework
- **ElementGame**: Main game UI element using libRocket
- **EventManager/EventHandler**: Event system for UI interactions
- **EventHandlerNewGame**: New game dialog handling
- **EventHandlerFileChooser**: File selection dialog handling
- **FileChooserDataSource**: Data source for file browser

#### Graphics and Rendering
- **Camera**: 3D camera system with pan/zoom/rotate
- **Board**: 3D board geometry and stone placement
- **Metrics**: Performance metrics and FPS tracking
- **Quaternion/Vector**: 3D math utilities

#### Audio System
- **AudioPlayer**: Audio playback using PortAudio
- **AudioFile**: Audio file loading and management
- **sound/**: Sound effect management

#### Configuration and Data
- **Configuration**: JSON-based configuration system
- **GameRecord**: SGF game record management
- **SGF**: Smart Game Format parsing and export

### External Dependencies

The project uses CMake's ExternalProject system to manage dependencies:
- **boost**: System, filesystem, and iostreams libraries
- **libRocket**: GUI framework for game interface
- **libsgfcplusplus**: SGF file parsing and generation
- **glyphy**: Text rendering library for overlay
- **freetype2**: Font rendering
- **portaudio**: Audio playback
- **libsndfile**: Audio file format support
- **nlohmann::json**: JSON configuration parsing
- **spdlog**: Logging framework
- **clipp**: Command-line argument parsing
- **GLM**: OpenGL mathematics library

### Platform-Specific Code

The codebase supports multiple platforms with platform-specific implementations in:
- **shell/src/win32/**: Windows-specific shell implementation
- **shell/src/x11/**: Linux X11 implementation
- **shell/src/macosx/**: macOS implementation (if needed)

### Key Directories

- **src/**: Main source code
- **config/**: Configuration files, shaders, fonts, sounds, GUI resources
- **config/shaders/**: GLSL shader files for ray-traced rendering
- **config/gui/**: libRocket GUI templates and stylesheets
- **games/**: SGF game record storage
- **engine/**: External Go engines (GNU Go, KataGo, etc.)
- **cmake/**: CMake find modules for dependencies
- **tests/**: unit tests, SGF fixtures, the mock GTP engine, and `tests/scenarios/`
- **docs/adr/**: Architecture Decision Records — why the code is the way it is

### Configuration System

The application uses JSON configuration files:
- **config/base.json**: Main configuration with bot definitions, controls, shaders
- **config/zh.json**: Chinese language configuration
- Bot configuration includes GTP engine paths, parameters, and message parsing

### SGF Game Records

- Games are automatically saved to `games/` directory
- Each session creates a timestamped SGF file
- SGF files contain complete game records with move history

### Development Notes

- C++17 standard required
- OpenGL 3.1+ compatibility profile
- Uses glad for OpenGL loading
- Cross-platform window management through custom shell layer
- Real-time 3D rendering with multiple shader options
- Support for both mono and stereo rendering modes
- GTP engine integration allows playing against various AI opponents
- see last_run.log when debugging errors

### Windows Build Notes

- Windows builds use vcpkg for dependency management (freetype, boost, portaudio, libsndfile)
- Use the `x64-windows-static` triplet for static linking
- **Patch file line endings**: Windows Git with `autocrlf=true` converts LF to CRLF on checkout, which corrupts patch files (like `deps/_patches/*.patch`). The `.gitattributes` file ensures patch files always use LF endings. The CMakeLists.txt also uses `git -c core.autocrlf=false apply --ignore-whitespace` to handle this robustly.

### Release Checklist

Before creating a version tag (e.g., `v0.1.0`):
1. **Update VERSION in CMakeLists.txt** (line 6) - this appears in the About dialog
2. **Update RELEASE_NOTES.md** if needed
3. **Commit all changes** before tagging
4. Push the tag to trigger automatic GitHub Release creation

## Design Invariants

These invariants must be maintained to prevent race conditions and ensure consistent behavior:

### Engine-Exclusive UI Actions
See `docs/adr/0001-engine-exclusive-ui-actions.md` for the reasoning.
- **No UI-thread wait for a genmove**: a GTP command in flight owns the engine's pipes until it replies, and standard GTP cannot abort one. Only the game thread may wait. Any UI path that calls `interrupt()` while an engine is thinking freezes the whole application.
- **Actions are classified by what they do to the current game**:
  - *Preserving* (board click, navigation, pass, resign) — the pending genmove is still valid, so these are **refused** while an engine is thinking.
  - *Discarding* (new game, clear, load SGF, switch game) — the pending genmove is worthless, so these are **deferred** to the game thread via `GameThread::runWhenEngineFree()`, which drops the move and then runs them. The UI shows `Waiting for <engine>...`.
  - *Terminating* (quit) — engine processes are killed outright (`shutdown()`).
- **Deferred actions split in two**: the engine/model half may run on the game thread; every widget update must go in `GobanControl::finishGameReplacement()`, which the UI thread calls via `takeDeferredTaskDone()`. RmlUi is not thread safe.
- **`interrupt()` is a no-op on the game thread**: joining there would deadlock on self-join, and setting `interruptRequested` would kill the loop the caller still needs.
- **Re-evaluate the loop after initial sync**: the `phase() != Playing` early-out runs before `enginesSynced` flips, so the sync branch must `continue` rather than fall through. Otherwise a paused loaded game calls `genmove` on a `LocalHumanPlayer`, which blocks forever and silently wedges the loop — with `isThinking()` reporting false, because the stuck player is not an engine.
- **Quiescence means all three**: `GobanControl::isIdle()` must account for engine thinking, queued navigation, and a deferred action still running.

> **In flux:** ADR-0002 replaces the lifecycle flags below with explicit
> `GamePhase` / `EngineSync` / `LoopState` machines.
>
> **`GamePhase` is done** (steps 1–3). `GobanModel::gamePhase` is the
> authoritative lifecycle state — `Setup` / `Playing` / `Paused` / `Finished` —
> and the `started` / `isGameOver` booleans are gone with no trace left.
> `phase()` is the only way to ask, and it is what `dumpState()` reports, so
> scenarios assert `expect phase <name>`.
>
> Change it only through the named transitions on `GobanModel`: `start()`,
> `pause()`, `enterReview()`, `endGame(reason)`, `createNewRecord()`. They all
> funnel into a single private `transitionTo()`, which is the one place the
> lifecycle is logged — do not add a second writer.
>
> `enginesSynced` and `hasThreadRunning` are still raw flags (step 4), and
> `syncingUI` waits on `ElementGame::OnUpdate()` being decomposed (step 5). See
> `docs/adr/0002-explicit-game-state.md` and its Implementation log.

### Navigation & Engine Synchronization
- **No genmove during navigation**: Navigation commands (back/forward/home/end) must not interleave with GTP genmove. Use `navigationInProgress` atomic flag.
- **Block navigation while engine thinking**: `isThinking()` returns true only for ENGINE types (not human players). Navigation keys are blocked when engine is processing.
- **Navigation in bot-bot matches**: Requires switching to Analysis mode first (pauses genmove loop).
- **Engine sync invariant**: All enabled engines stay in sync at the same position. After load/new game, `enginesSynced = false` triggers initial sync on game thread: coach syncs first (enables scoring), then remaining engines. After initial sync, every move is sent to ALL engines via `syncOtherEngines`. No special cases for coach/player/kibitz roles.
- **All GTP from game thread**: Engine commands during active game must go through the game thread (navigation queue, initial sync). Direct GTP from UI thread is only safe when game thread is stopped (after `interrupt()`).

### SGF Game Record Consistency
- **PB/PW updated on first move**: Player names in SGF header are updated when the first move is made, capturing actual players after setup.
- **Annotations only after first move**: Player switch annotations (`switched_player:`) only recorded after `moveCount() > 0`. Setup changes are silent.
- **No annotations during SGF loading**: the phase must not be `Playing` during SGF load, or player-switch annotations leak into the loaded record.
- **Result removed if main line unfinished**: When saving a modified SGF, if the main line (first children) doesn't end in resign/double-pass, remove the RE property.
- **Session copy only for diverged games**: A game is copied into the daily session document only once the player has actually played a move (`gameHasNewMoves`). A record that is merely being replayed, without diverging from its SGF tree, must never be copied into the session — otherwise reviewing a game would silently duplicate it. `suppressSessionCopy` excludes tsumego mode entirely.
- **`gameInDocument` is the double-append guard**, not `gameHasNewMoves`. The `gameHasNewMoves` check at the `move()` call sites is a once-only latch; `appendGameToDocument()` itself keys on `gameInDocument`. Don't clear `gameHasNewMoves` to force a re-append — that destroys the replay-vs-diverge distinction above.
- **Archive keeps the game in progress**: `clearSession()` drops the accumulated session document (those games now live in the timestamped archive) but immediately re-attaches the current game if it has diverged. Deferring that to the next move does not work: `appendGameToDocument()`'s only caller is `move()`, behind `if (!gameHasNewMoves)`, which is already true mid-game — so a null `doc` would never be rebuilt and every later move would be silently dropped by `saveAsInternal()`.
- **replayMoves is the board reconstruction path**: `GameRecord::buildBoardFromMoves()` replays the whole path from the root, and `replayMoves()` stops at the first move it rejects. Always check its return value against the path length — an ignored short count means a silently truncated position.

### Go Rules
- **Simple ko requires the capturing stone to be in atari**: `koPosition` is set only when exactly one stone was captured **and** the capturing stone is a lone stone with exactly one liberty. Testing only "one stone captured" also flags **snapback**, where the capturing group is larger and the recapture is legal in every ruleset — that false positive made `replayMoves()` abort and silently truncate board reconstruction. After any other single-stone capture, retaking the point is already rejected as suicide, so no ko ban is needed there.
- **`koPosition` is legality-only**: it feeds `isValidMove()` and `replayMoves()`. The out-parameter of `buildBoardFromMoves()` is not used for display, so narrowing ko detection cannot affect rendering.

### Game State
- **isGameFinished()**: True only for resign or double-pass (two consecutive passes).
- **Territory display**: Only shown at finished game positions, not at end of unfinished variations.
- **Loaded games stay paused**: loading leaves the phase at `Paused` (or `Finished`), never `Playing` — `onBoardSized()` calls `enterReview()`, which resolves against the record. Human moves work through the navigation path (`navigateToVariation`). Engine play requires an explicit "Start" (`model.start()`).
- **Finished is left and re-entered by navigation**: `navigateBack`/`navigateToStart` call `enterReview()`, which drops `Finished` because the position shown is no longer the end of the game. `navigateForward`/`navigateToEnd`/`navigateToTreePath` call `endGame()` again on reaching the end of a game with `hasGameResult()` — but only when the phase is not `Playing`, so a user who pressed Start keeps playing.
- **`pause()` cannot un-finish a game**: it means "stop playing", and is a no-op in `Finished`. Use `enterReview()`, and only once the position has actually moved off the end.
- **`state.reason` is not part of the phase and outlives it**: `enterReview()` leaves it set, and the UI's own "is over" test (`ElementGame::OnUpdate`, `GobanControl::setHandicap`) reads `state.reason != NO_REASON`, not the phase. Only `start()` and `onBoardSized()` clear it. Known inconsistency, pinned by `tests/test_gamephase.cpp`; don't add readers of `state.reason` as a lifecycle test.

### UI Event Suppression
- **`syncingUI` flag**: `GobanControl::syncingUI` suppresses game actions triggered by UI change events during programmatic dropdown updates. Any method that repopulates dropdowns (player, board size, komi, handicap) must wrap the repopulation with `setSyncingUI(true/false)`. Event handlers in `EventHandlerNewGame` check `isSyncingUI()` and skip side effects when true. This prevents transient intermediate states (e.g. briefly activating an engine player during dropdown clear/repopulate) from triggering game actions.

## Coding Principles

- **Search before creating**: Before introducing a new flag, helper, or mechanism, search the codebase for existing patterns that solve the same problem. Use `grep` for related keywords (e.g. "suppress", "syncing", "guard", "flag"). Reusing an existing mechanism is always preferable to adding a new one.
- **Fix at the source**: When a race condition or unwanted side effect is discovered, fix the root cause rather than adding compensating workarounds downstream. A guard at the event source is better than a deferred correction after the fact.
- **Fail early**: Always check return values from operations that can fail (GTP commands, file I/O, engine communication). When a prerequisite fails, bail out immediately rather than continuing with corrupt state. Silent failures cascade — a failed `boardsize` followed by blind move replay produces wrong results that are hard to diagnose.
- **Record decisions, not just invariants**: a non-obvious design choice — especially one where you rejected a plausible alternative — belongs in `docs/adr/` as an Architecture Decision Record, with the alternatives and the downsides you accepted. Read the existing ADRs before proposing something that revisits an old decision. Invariants (rules that must always hold) stay in the section above and should be enforced by a test; decisions are append-only and superseded rather than edited.
- **Document new invariants**: When introducing a new invariant, flag, or cross-cutting concern, add it to the Design Invariants section above. Not all invariants can be documented upfront, but capturing them as they're discovered prevents future regressions.

## Test Scenarios

Known error-prone sequences that should be regression tested. Several of these
are now **executable** under `tests/scenarios/*.scn` — run them with
`./tests/run_scenarios.sh`, and see `docs/testing.md` for the directive syntax.
The remaining ones below are still prose; converting one is a good way to pin a
bug you have just fixed.

### Navigation During Engine Play
```
new_game 13
switch_player black gnugo
switch_player white gnugo
start
wait 500ms
navigate_home          # Should be blocked or queue safely
navigate_back          # Should be blocked or queue safely
```

### Player Switch During Navigation
```
load_sgf famous_game.sgf
navigate_home
navigate_forward 10
switch_player black gnugo
space                  # Should trigger AI response
navigate_back          # Should work after AI responds
```

### Space Key at Branch End
```
load_sgf famous_game.sgf
navigate_home
navigate_forward 5
click_board 4 4        # Create new variation
space                  # Should trigger kibitz (not block)
```

### SGF Modification and Save
```
load_sgf game_with_result.sgf
navigate_home
navigate_forward 3
click_board 5 5        # New variation becomes main line
save                   # RE property should be removed (unfinished)
```

### Analysis Mode Workflow
```
new_game 19
switch_player black gnugo
switch_player white gnugo
start
wait 2000ms
toggle_analysis_mode   # Pause genmove
navigate_home          # Should work immediately
navigate_end
toggle_analysis_mode   # Resume match
```

## Ray-Traced Rendering - Coordinate System and Shaders

The Go board is rendered using ray tracing in GLSL fragment shaders. The vertex shaders set up the camera and compute ray origins/directions for each pixel.

### World Coordinate System

- **Origin (0, 0, 0)**: Center of the Go board
- **Y-axis**: Points UP (board surface is at y ≈ 0)
- **Z-axis**: Points toward viewer (camera is at negative z)
- **X-axis**: Points right (completes right-handed system)

### Camera Model

The camera is parameterized by three independent values passed as uniforms:
- `cameraPan` (vec2): Look-at point on the board plane (x, z)
- `cameraDistance` (float): Distance from camera to look-at point (default 3.5)
- `glModelViewMatrix`: Rotation quaternion (via OpenGL model-view matrix)

**Vertex shader camera setup** (`config/shaders/vertex/mono.glsl`):
```
ta = (cameraPan.x, 0, cameraPan.y)         // Target on board plane
viewDir = normalize(m * (0, 0, 1, 0))      // View direction (rotated +Z)
roo = ta - cameraDistance * viewDir         // Camera position behind target
cw = viewDir                               // Forward = toward board
cu = normalize(cross(up, cw))              // Right
cv = cross(cw, cu)                         // Up
```

**Ray direction:**
```
q0 = vertex.xy * aspectRatio               // Screen coordinate (-1 to 1)
rdb = normalize(q0.x*cu + q0.y*cv + 3.0*cw)
```
The `3.0` is the focal length — rays spread from camera through a virtual screen at distance 3.0.

**C++ side** (`GobanView`): `cameraPan`, `cameraDistance`, and `cam.rLast` (quaternion) are the authoritative state. `boardCoordinate()` replicates the same camera model for screen→board ray casting. `zoomToRect()` projects board-plane corners into camera space to compute the exact distance for framing.

### Stereo Vertex Shader (`config/shaders/vertex/stereo.glsl`)

Same `cameraPan`/`cameraDistance` uniforms. Eye positions are lateral offsets from the center camera position, scaled by distance to maintain consistent stereo base angle.

### Stereoscopic Deviation Theory

**On-screen deviation formula:**
```
deviation = stereo_base * focal_length / object_distance
```

**Maximum comfortable deviation:** 1/30th (3.3%) of screen width

**Why parallel cameras (not toe-in):**
- Toe-in convergence causes keystone distortion and eye strain
- Parallel cameras are physically correct (like human eyes at distance)
- Zero parallax at infinity; objects closer have increasing parallax
- Stereo effect naturally decreases with distance (correct for depth perception)

**Parameters:**
- `eof` (default 0.025): Eye offset factor. Stereo base = 2 × eof.
  - 0.025 gives ~1/40 screen deviation at default zoom (conservative, comfortable)
  - 0.017 would give exactly 1/30 (maximum recommended)
  - Adjust to taste based on display and viewing distance
