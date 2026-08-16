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

**The map is `docs/architecture.md`** — object graph, thread ownership, the
observer fan-out, and four data flows (a move, a navigation, an SGF load, a new
game) drawn end to end. Read that before changing anything cross-cutting. What
follows here is only an index of names.

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
- **ElementGame**: Main game UI element (RmlUi); owns the whole object graph
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

Dependencies are fetched by CMake (FetchContent / ExternalProject):
- **RmlUi**: GUI framework for the game interface (successor to libRocket)
- **GLFW**: Window, input and context creation
- **libsgfcplusplus**: SGF file parsing and generation
- **glyphy**: Text rendering library for the overlay
- **freetype2**: Font rendering
- **portaudio**: Audio playback
- **libsndfile**: Audio file format support
- **nlohmann::json**: JSON configuration parsing
- **spdlog**: Logging framework
- **clipp**: Command-line argument parsing
- **GLM**: OpenGL mathematics library
- **doctest**: Unit test framework
- **glad**: OpenGL loader (vendored in `src/glad/`)

Boost is no longer used.

### Platform-Specific Code

There is **no `shell/` layer**; it was replaced by GLFW plus the small `AppState`
namespace (`src/AppState.h`), which owns the window handle, fullscreen toggling
and exit/restart requests. The remaining platform-specific code is `#ifdef
_WIN32` inline — process creation and pipes in `gtpclient.cpp`, the window icon
and `_spawnv` restart in `main.cpp`.

### Key Directories

- **src/**: Main source code
- **config/**: Configuration files, shaders, fonts, sounds, GUI resources
- **config/shaders/**: GLSL shader files for ray-traced rendering
- **config/gui/**: RmlUi GUI templates and stylesheets, one folder per language
- **games/**: SGF game record storage
- **engine/**: External Go engines (GNU Go, KataGo, etc.)
- **cmake/**: CMake find modules for dependencies
- **tests/**: unit tests, SGF fixtures, the mock GTP engine, and `tests/scenarios/`
- **docs/architecture.md**: the component and thread map — start here
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
- **Quiescence means all five**: `GobanControl::isIdle()` must account for engine thinking, queued *or in-flight* navigation, a deferred action still running, `EngineSync::Syncing`, and a move sitting in `queuedMove` waiting for the loop to take it up (`GameThread::hasQueuedMove()`). The last was the fourth gap: `playLocalMove()` leaves a move there whenever nobody is blocked in `genmove()`, so `wait_idle` returned with the stone still in the air and every placing scenario worked around it with `wait_until move_count >= N`. It is deliberately false while the loop is *stopped* — a move nobody will collect is stranded, not in flight — and it ignores `INTERRUPT`, which `interrupt()` leaves in the slot. It deliberately does **not** cover territory scoring — the obvious predicate is permanently true when scoring fails, so it would hang instead of tighten; scenarios use `wait_until has_result true`. Every gap here has produced a flaky scenario at least once.

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
> **`EngineSync` and `LoopState` are done too** (step 4), both private to
> `GameThread`. `enginesSynced` is now `Unsynced` / `Syncing` / `Synced`, and
> `hasThreadRunning` + `interruptRequested` are `Stopped` / `Running` /
> `Stopping`. Ask via `stopRequested()`, `isSyncingEngines()`,
> `shouldDiscardMove()`, `isRunning()`. `deferredPending` deliberately stays a
> flag — it is queued work, not lifecycle.
>
> **ADR-0002 is complete** (step 5 done). `syncingUI` was *split*, not retired:
> it conflated a startup gate with an RmlUi repopulation gate, and decomposing
> `OnUpdate()` would not have removed either. Ask `acceptsUiEvents()`; suppress
> events with `GobanControl::WidgetEventGuard`. See
> `docs/adr/0002-explicit-game-state.md` and its Implementation log, which
> records where the plan was wrong and what was done instead.

### Navigation & Engine Synchronization
- **No genmove during navigation**: Navigation commands (back/forward/home/end) must not interleave with GTP genmove. Use `navigationInProgress` atomic flag.
- **Block navigation while engine thinking**: `isThinking()` returns true only for ENGINE types (not human players). Navigation keys are blocked when engine is processing.
- **Navigation in bot-bot matches**: Requires switching to Analysis mode first (pauses genmove loop).
- **Engine sync invariant**: All enabled engines stay in sync at the same position. After load/new game, `EngineSync::Unsynced` triggers initial sync on the game thread: coach syncs first (enables scoring), then remaining engines. After initial sync, every move is sent to ALL engines via `syncOtherEngines`. No special cases for coach/player/kibitz roles.
- **`Unsynced` is not "busy", `Syncing` is**: only `Syncing` — held while the game thread is actually replaying — may make `isIdle()` false. Waiting on `Unsynced` would never return, since nothing guarantees anyone will act on it. The replay always leaves `Syncing`, failure included.
- **The sync starts when the board changes, not when the player moves — but only once the new record exists.** `GameThread::startSyncingNewGame()` is called from `newGameNow()` *after* `createNewRecord()`, never from `clearGame()`: the replay reads whatever record the model holds, and starting it inside `clearGame()` raced the empty record's installation. The engines were handed the game being discarded, the board drew empty, and GNU Go refused the player's opening move because that point was the old game's first stone. The function checks the precondition (`moveCount > 0` means the old record is still there) and logs an error rather than syncing — the failure is otherwise invisible until an engine disagrees with the board. A new game, a board size change and a handicap all begin the replay immediately — while the user is still looking at an empty board. It used to leave the engines `Unsynced` with the loop *stopped* until a click called `start()` + `run()`, which billed a several-second KataGo rebuild to the first move: stone stuck in hand, nothing on screen. The SGF load path had always started the thread early (`loadSGF`, "start game thread early"); the two paths had simply grown apart. Ordering matters twice — after `setFixedHandicap()`, because the replay carries its setup stones, and behind `if (!isRunning())`, because `clearGame()` also runs *on* the game thread when a discarding action was deferred, where `run()` would take a mutex that path may hold. Pinned by `tests/scenarios/sync_before_first_move.scn`, whose every assertion is "without a move having been played".
- **All GTP from game thread**: Engine commands during active game must go through the game thread (navigation queue, initial sync). Direct GTP from UI thread is only safe when game thread is stopped (after `interrupt()`).
- **`LoopState` has one writer outside the loop, and `run()` is a no-op on the
  game thread.** `GameThread::reset()` was a second writer, and because it wrote
  `Stopped` unconditionally it told the truth only on the UI thread. On the game
  thread — where `newGameNow()` runs when a discarding action was deferred past a
  genmove — it claimed the running loop had stopped, which defeated
  `startSyncingNewGame()`'s `if (!isRunning())` guard and sent the game thread
  into `thread->join()` on itself: `std::system_error`, "Resource deadlock
  avoided", uncaught, `std::terminate`. `interrupt()` already leaves
  `loop == Stopped` and `playerToMove` null on every path that reaches it, so
  `reset()` was pure redundancy on the thread where it was not fatal. `run()` now
  refuses on the game thread the way `interrupt()` does, because the call sites
  cannot all know which thread they are on. Same rule as
  `GobanModel::transitionTo()`: one writer. Pinned by
  `tests/scenarios/new_game_while_engine_thinking.scn`.
- **Navigation waits for `EngineSync::Synced`.** `processNavigationQueue()`
  leaves a command queued while the engines are behind the record: `BACK` issues
  `undo` and `FORWARD` issues `play`, both against whatever position the engine
  still holds, and `GameNavigator::syncEngines()` can only warn about the
  divergence, not repair it. `TO_TREE_PATH` is the one exemption and must stay
  one — it sets the cursor and syncs the coach itself, and startup deliberately
  queues it *before* the initial sync so the sync lands on the restored position
  rather than the root. The sync block is reached on every iteration where
  `engineSync != Synced`, so nothing can stall behind this.
- **A killed engine is put back, not written off.** See
  `docs/adr/0009-a-killed-engine-is-restarted-and-resynced.md`.
  `GtpClient::terminateProcess()` had no counterpart, so one unanswered
  `final_status_list` — GNU Go's real behaviour on a sparse 19×19 — removed the
  coach for the session and every later move went to a dead process.
  `GameThread::reviveFailedEngines()` respawns it at the top of the loop and
  marks everything `Unsynced`; the count is bounded (`GtpClient::MAX_REVIVES`).
  **`terminated_` means "shut down on purpose" and nothing else** — an engine
  flagged that way is deliberately not revivable, which is why the timeout path
  calls the private `killProcess()` instead.
- **`PlayerManager::getPlayers()` returns a copy, taken under the mutex.** It
  used to hand out a reference to the vector with no lock, which is the
  partial-locking shape the file comment warns about, only inverted.
  `loadEnginesParallel()` starts the game loop as soon as the *coach* is ready
  and lets the remaining engines keep loading, so the game thread walks that list
  while loader threads are still `push_back`ing into it. Ask a single locked
  question (`isActivePlayerEngine()`, `humanPlayer()`) rather than combining two
  accessors, or the index and the list it indexes come from different moments.

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
- **We are the referee, not the engine.** `GobanModel::isLegalMove()` decides
  whether a click becomes a move, and it is asked *before* anybody sends
  anything: `GobanControl::placeStone()` for all three ways of putting a stone
  down, and `GobanView::updateCursor()` for the ghost stone that shows where one
  would go. There used to be no check at all on that path — every click left as
  `play <colour> <point>` and whatever GNU Go answered decided it. So an endgame
  misclick onto a stone ten moves old came back as `? illegal move`, which the
  message-log sink takes at **error** level and turns into a red badge, and
  which was the entire contents of `last_run.log` for that session (2026-08-16,
  `play B M10`). The renderer already knew better and drew no ghost stone there,
  so the display and the click disagreed about the same point — the same shape
  as a button that disagrees with its command. A refusal now leaves the stone in
  hand and says nothing, which is what a real board does; **an engine refusal
  after this one means the engine has drifted out of sync**, and that is worth
  an error. It also keeps an illegal move out of the record: `GameRecord::move()`
  appends whatever it is handed, so an engine with other rules — or one already
  desynchronised — could write a move `replayMoves()` then refuses, silently
  truncating every later reconstruction of the position. This is *not* policy
  and does not belong in `availableActions()`, which is per-action and knows
  nothing about points: `a.play` stays true while an individual point is closed.
  Pinned by `tests/scenarios/illegal_click_is_refused_locally.scn` (which asserts
  `log_badge none` — the record does not move either way, so `move_count` alone
  would pass with the engine still being asked) and `tests/test_board_rules.cpp`.

### Game State
- **isGameFinished()**: True only for resign or double-pass (two consecutive passes).
- **Resignation only at the end of the line**: a resignation writes `RE` on the SGF root and adds no node, so unlike a stone or a pass it cannot describe a branch. Applied with a continuation still ahead of the cursor it relabels a game whose recorded moves then contradict the result. `GobanControl::canResign()` is the single predicate — at end of navigation, not already `Finished`, not tsumego, human to move — and `GameRecord::move()` refuses mid-tree as a backstop. Resigning implies starting, as passing does.
- **Territory display**: Only shown at finished game positions, not at end of unfinished variations.
- **Loaded games stay paused**: loading leaves the phase at `Paused` (or `Finished`), never `Playing` — `onBoardSized()` calls `enterReview()`, which resolves against the record. Human moves work through the navigation path (`navigateToVariation`). Engine play requires an explicit "Start" (`model.start()`).
- **Finished is left and re-entered by navigation**: `navigateBack`/`navigateToStart` call `enterReview()`, which drops `Finished` because the position shown is no longer the end of the game. `navigateForward`/`navigateToEnd`/`navigateToTreePath` call `endGame()` again on reaching the end of a game with `hasGameResult()` — but only when the phase is not `Playing`, so a user who pressed Start keeps playing.
- **`pause()` cannot un-finish a game**: it means "stop playing", and is a no-op in `Finished`. Use `enterReview()`, and only once the position has actually moved off the end.
- **`state.reason` is not part of the phase and outlives it**: `enterReview()` leaves it set, and the UI's own "is over" test (`ElementGame::OnUpdate`, `GobanControl::setHandicap`) reads `state.reason != NO_REASON`, not the phase. Only `start()` and `onBoardSized()` clear it. Known inconsistency, pinned by `tests/test_gamephase.cpp`; don't add readers of `state.reason` as a lifecycle test.

### Scoring
See `tests/test_scoring.cpp`; every rule here comes from one hang on 2026-08-13.
- **A failed score is not a score of zero.** `Engine::final_score()` returns `std::optional<float>` because 0.0 is a legitimate result (jigo) and a bare float cannot tell the two apart. Reading failure as zero is what made `applyTerritory()` claim a drawn game, which tripped the "suspiciously zero, ask someone else" fallback and took the whole chain down. Same for `applyTerritory()`, which returns whether a *score* was obtained: shading can be valid when the number is not, and it signals that with `showTerritory` set but `territoryReady` clear.
- **Seki is not territory, and only the engine knows where it is.** The flood
  fill cannot tell a seki's eye from an ordinary one — both are reached from the
  same stones — so `applyTerritory()` asks `final_status_list seki` as well as
  `dead` and passes both to `calculateTerritoryFromDeadStones()`. Asking only for
  `dead`, which is what it did, hands a seki's eyes to whoever surrounds them.
  All three statuses (`alive`, `seki`, `dead`) are GTP 2 standard — only GNU Go's
  `dame` / `black_territory` / `white_territory` are extensions — but support is
  uneven, so a refusal is read as "no seki here" rather than as an error, the
  same degradation the dead list itself gets. It costs nothing where it works:
  GNU Go computes the status once, so `seki` returns instantly after `dead`
  (measured: 10.3 s for `dead` on a sparse 9×9, then microseconds).
- **The cross-engine fallback must not depend on the coach having shaded
  anything.** `applyTerritory()` sets `showTerritory` only *after*
  `final_status_list dead` succeeds, so gating the fallback on that flag made it
  unreachable in the one case it exists for — an engine that cannot produce a
  dead list at all. The gate is now `engineSync == Synced` alone: another engine
  is asked for `final_score`, and the board stays unshaded, which is truthful
  because nobody could compute the shading.
- **Only ask a synced engine to score.** The initial sync runs coach → score → everyone else, so the other engines are at whatever position they still held when `processScoring()` runs inside it. Asking one there is not merely wrong, it blocks: a CPU KataGo asked to score the empty 19×19 board it was never told about does not answer for minutes, with the game thread inside the sync block and the UI stuck on "Calculating score…". The cross-engine fallback is gated on `engineSync == Synced`; the loop calls scoring again once that holds, so nothing is lost by deferring.
- **A position that failed to score must not be retried.** `territoryReady == false` alone would re-enter `processScoring()` on every loop iteration, ten times a second, hammering the engines. `Board::territoryFailed` latches it — and clears *itself*, because every position change builds a fresh `Board` and `updateStones()` copies the flag in. Never clear it by hand.
- **Scoring is bounded more tightly than a genmove.** `GtpClient::scoringTimeout()` (30s, `scoring_timeout_ms` per engine) applies for the duration of `applyTerritory()` via `ScopedTimeout`. It never *raises* a stricter `timeout_ms`, and it does override a negative one — a wedged engine must not be able to freeze scoring for the full five-minute command timeout.
- **The coach may not be the engine you configured.** `coach`/`kibitz` are indices whose "unset" value is 0, which is also a valid engine, so `PlayerManager` tracks `coachConfigured`/`kibitzConfigured` separately. When the engine carrying `main` fails to load, `currentCoach()` still hands out `players[0]` — an arbitrary engine under parallel loading — and that promotion is now warned about. Keep the warning: an engine that cannot count silently refereeing is the top of this whole failure chain.
- **A missing engine folder is not fatal.** `path` only supplies the working directory; if `command` resolves from `PATH` the engine runs from the application folder with a warning. Making it an error regressed the stock `"path": "./engine/gnugo"` + `"command": "gnugo"` config on every machine with a distribution GNU Go — and that regression is what removed the real coach.

### Replacing the Game on Screen
- **Three paths replace the game, and all three confirm first**: `clear`, the board-size dropdown and the handicap dropdown. `GobanModel::hasGameWorthKeeping()` is the gate — a record with moves, in a game that is not finished and not tsumego — and `GobanControl::requestNewGame()` / `requestHandicap()` route the dropdowns through it. Anything new that discards the current game belongs on the same route; a silent replacement is the bug this fixed.
- **The answer is asynchronous**: the prompt callback arrives after the handler returns, so a widget cannot revert its own selection at the call site. Take an `onSettled(bool changed)` callback and revert there, as `EventHandlerNewGame` does.
- **A confirmation and the action behind it must not disagree.** Once `hasGameWorthKeeping()` has decided the question is worth asking, the action must honour a yes: a second guard inside it can only produce a prompt that does nothing. This shipped once — `setHandicap()` kept a `phase() != Playing` test that refused in exactly the case the prompt was shown for, so confirming did nothing at all. The `request*` half holds the policy; the half that does the work holds none. Same failure as a button whose enabled state disagrees with its command, below.
- **`new_game <size>` deliberately does not prompt.** It is the scripting entry point; scenarios begin with it, and a modal would deadlock them. The `board_size` and `handicap` commands take the dropdowns' route instead, prompt included, which is how `tests/scenarios/discard_game_prompt.scn` can cover a path the harness cannot click.

### UI Widget State
- **One policy decides what a player may do: `availableActions()` in `src/UiActions.h`.** The toolbar (`ElementGame::syncActionAvailability()`) and *every* command guard read the same answer from the same gatherer, `GobanControl::uiInputs()`. This used to be a rule — "make the button ask the same question the command asks" — and it decayed three separate times, each time producing a disabled button that looked like a guard and was not one. ADR-0005 finished the job: all nine actions now dispatch through `GobanControl::actions()`, and a command handler holds **no** policy of its own. Add a new action's rule to `availableActions()` and a case to `tests/test_uiactions.cpp`; do not hand-roll a second condition at either call site.
- **Kibitz away from the end of the line is a variation, delivered by
  navigation.** `play once` routes to `GameThread::requestKibitzNav()` when the
  snapshot says `!atEnd`, and does **not** call `model.start()` there. Two
  reasons, both learned from a recorded session: `playKibitzMove()` only reaches
  a player already blocked in `genmove()` and otherwise leaves the move in
  `queuedMove` *without waking the loop*, so while reviewing it did nothing at
  all — and the `start()` that ran first left the game Playing at a mid-tree
  cursor, so the stale request later fired against a position nobody was looking
  at and the turn order broke. Asking once must not resume the match; `Start`
  lights up for that, since it is then the engine's turn.
- **Navigation's engine sync must not swallow failures.** `GameNavigator::syncEngines()`
  discarded the result of every `undo`/`play` it issued, so one refusal left that
  engine tracking a different board with nothing said about it — and everything
  it was asked afterwards answered for the wrong position. It warns now. The
  matching rule for `KIBITZ_NAV`: a coach that rejects the suggestion means the
  engines have diverged, so resync both from the record and ask again rather than
  dropping the request.
- **A genmove from the navigation queue must set `playerToMove` too.** `KIBITZ_NAV`
  did not, so `isThinking()` was false for the whole search: the toolbar stayed
  lit, no engine was named, navigation was not blocked, and a second press
  queued another request. With a mock answering in microseconds nothing showed;
  with KataGo on a CPU backend the silence lasts tens of seconds, and two bug
  reports called it "nothing happened". The flag is not only a display — ADR-0001's
  refusals and the analysis overlay's yield both read it.
- **A resync is engine-busy too, and it is the slow one.** `EngineSync::Syncing`
  covers the replay into every engine after a board size change, a clear, a
  handicap or an SGF load — seconds while KataGo rebuilds for the new size, with
  the UI fully live: board drawn, overlay running, toolbar lit. `isIdle()` has
  always counted it; `uiInputs()` did not, and `isThinking()` is false there
  because no genmove is in flight. So a board click was accepted, `playLocalMove()`
  found `playerToMove` null and fell through to `queuedMove` — **a single slot,
  not a queue** — and each further click overwrote it. Four clicks, one stone.
  `availableActions()` now folds both into one `engineBusy` term.
- **The move that *starts* the sync cannot be refused, only the ones during it.**
  After a board size change the engines are `Unsynced` with the loop stopped;
  it is `model.start()` + `run()` on that first click that begins the replay. So
  `a.play` is true for `Unsynced` and false only for `Syncing`: the first move is
  accepted and queued, every click during the wait is refused, and the stone
  staying in hand is then truthful — the move is pending, not lost.
- **A wait the user cannot see is a frozen program.** Two things were missing and
  either one alone fixes nothing: `getIdleTimeout()` did not cover
  `isSyncingEngines()`, so the loop blocked in `glfwWaitEvents()` and drew no
  frame at all during the sync — and there was no message to draw. Both are
  there now (`tplStatusSyncing`, in all five languages, with an English
  fallback). Same trap as the exit hang: no input event arrives, so nothing
  repaints, so nothing can be reported.
  **And the same hole was open for a genmove — the longest wait of the three.**
  Nothing in the UI read `isThinking()` at all, so an engine searching was a
  greyed toolbar and a board that did not move: 30.9 s measured for one kibitz
  from the stock 9x9 KataGo on a CPU backend, reported as "nothing happens".
  Both halves again — `getIdleTimeout()` now covers `isThinking()`, and
  `tplStatusThinking` names the engine, because with several configured
  "thinking" does not say which. Pinned by `kibitz_two_engines.scn`, which
  asserts `status_text` rather than the condition behind it: what was painted is
  the thing that was missing. *The status bar itself is provisional — see the
  diegetic-HUD direction; replace the surface, keep both halves.*
- **A click that cannot place a stone can still mean Start.** When it is an
  engine's turn, `boardClick()` dispatches the `start` command — by asking
  `availableActions()` a *different* question (`a.start`), never by going round
  it. This is restored behaviour, not new: `boardClick()` called `model.start()`
  + `run()` unconditionally for years, so clicking the board with a bot to move
  began the match. It was lost as collateral twice and remarked on neither time —
  4c5dcfc moved `start()` behind "a stone is actually being placed" (rightly:
  merely *picking a stone out of the bowl* was flipping the phase to Playing on
  an empty board), and 2d6b222's `!actions().play` guard then refused the click
  outright. The same commit message that removed it still cited "a board click
  already starts the game" as a reason for something else. Pinned by
  `tests/scenarios/click_starts_engine_turn.scn`.
- **A board click asks `a.play`, which *is* `a.pass`.** Not an equal-looking
  copy — assigned, so they cannot drift. They are one act at one point, and the
  review branch used to test `isThinking()` while the branch that starts a game
  tested nothing at all. `Finished` is exempt at the call site because a click
  there means `clear`, not a move.
- **`isThinking()` is not "the engine is busy".** The game loop clears `playerToMove` *before* its 500 ms inter-move sleep, so `isThinking()` is false for that window on every move. Guards written as a bare `isThinking()` test therefore have a hole once per move — that is how navigation keys were accepted in a locked bot-versus-bot match the toolbar had greyed out, and how a kibitz request could be dropped on the floor. Ask `actions()`, which carries the `aiVsAiLocked` term as well.
- **Away from the end of the line, a move is a variation, not a turn.** `pass` and a board click must agree about this: `boardClick`'s review branch has never consulted turn ownership, so `a.pass` reads `(humanToMove || reviewingMidTree)`. The engine-thinking lock stays outside that disjunction — passing is a *preserving* action in ADR-0001's sense, and a click refuses there too.
- **Every way of *making* one goes through `GobanControl::playVariationAt()`.**
  A click on a fresh point, a click on a point already explored, and a pass were
  one act; agreeing about `a.pass` and then each carrying its own promotion rule
  is the same drift in a second place. The tsumego half — `promote=false`, and
  no `model.start()` — had been written at the fresh-point call site *only*, so
  a pass in a puzzle, and a second click on a wrong move already explored, both
  promoted themselves over the recorded solution (`main_line_moves` 2 → 1) and
  the pass flipped the phase to Playing as well. A puzzle's answer is what
  defines "correct" for every later verdict; nothing the solver tries may
  displace it, and trying does not start a match. Pinned by
  `tests/scenarios/tsumego_mode.scn`.
- **But a click on a move the record already has *follows* it, and following is
  not making.** That case left the funnel above, which is the one thing about it
  that is not drift: it is the Right arrow with a mouse. `engine.
  navigateToVariation(move, false)` — no `model.start()`, no promotion (clicking
  a branch is not a vote for it), and no rules check, since the position was
  legal when it was recorded. Through `playVariationAt()` it started the match
  at a mid-tree cursor instead, so the game loop asked the engine for the next
  move and `GameRecord::move()` appended the answer even though the node already
  had that move as a child. GNU Go answers deterministically, so the recorded
  move got an identical twin drawn on top of itself and the main line acquired a
  letter it never had — `16`, then `16b`, then `16c` for the same point, one per
  visit. Resuming from a reviewed position is Start's job; same division as
  `play once`. Pinned by `tests/scenarios/click_follows_recorded_variation.scn`.
  **The duplicate child is still reachable deliberately**: Start at a mid-tree
  cursor has the engine play there, and `GameRecord::move()` appends rather than
  matching an identical existing child. That is the "resume from here" case and
  it is not addressed — if it should be, the fix is in `move()`, not at a call
  site.
- **A solved puzzle is not asked for a move, and a puzzle is never asked through
  `playKibitzMove()`.** `a.kibitz` carries a `tsumego && atEnd && !onBadMovePath`
  term, so the Kibitz item — *Nápověda*, Hint, in Czech — greys exactly where the
  board click is already refused; it used to play the engine's move on past the
  recorded answer and start the game doing it. And in a tsumego `play once`
  routes to `requestKibitzNav()` from *anywhere*, end of the line included: the
  end-of-line branch reaches the engine only through `playKibitzMove()`, which
  hands the move to a player already blocked in `genmove()` and otherwise leaves
  it in `queuedMove` without waking anything — so on a real configuration the
  menu item asked no engine at all while `model.start()` quietly turned the
  puzzle into a game. **It passed against the mock**, whose timing let the loop
  collect the queued move; only a run with real engines showed it. The Space
  handler dispatches the command rather than calling `requestKibitzNav()` itself,
  which is how the key and the menu item came to disagree in the first place.
  *What the engine answers is a separate, open problem: a genmove is a
  whole-board answer, and both stock engines abandon the corner rather than
  punish the mistake — measured, KataGo `F7` and GNU Go `B6` where the kill was
  `B2`. Needs an ADR; do not assume asking works because the plumbing does.*
- **A snapshot field written *after* the position change must be republished.**
  `notifyBoardChangeWithMove()` publishes from inside `navigateToVariation()`,
  but the tsumego verdict — `markBadMove()` — runs after it returns, in
  `executeNavCommand()`. So `GameSnapshot::onBadMovePath` described the move one
  instant before it was condemned, and both readers fail closed on that: no
  stone could be taken out of the bowl to play the refutation out
  (`GobanControl.cpp`), and Space never reached `requestKibitzNav()`, so no
  engine ever punished a wrong move. The whole dead-branch feature was inert and
  nothing showed it, because the "Wrong!" verdict is read from `model.state`
  rather than from the snapshot — the display was right and the policy was
  stale. `executeNavCommand()` calls `publishSnapshot()` once the verdict is
  settled. This is the ADR-0006 publish rule applied to a *write ordered after*
  the funnel rather than to one that bypasses it.
- **Territory needs a score, not just an ending.** `a.territory` is `finished && scoredEnd`; a resignation counted nothing, so it has no territory. `scoredEnd` is `GameRecord::shouldShowTerritory()` **published by the game thread** into `GobanModel::scoredEndPosition`, never recomputed by the reader — see the threading note below.
- **Keep `availableActions()` pure over plain data.** It must not take a `GobanModel` or `GameThread`: `isThinking()` reads a member only the game loop sets, so the engine-thinking cases would stop being testable — and they are half of what it decides.
- **The UI thread must not read the SGF tree *or* `GameState`'s strings — it reads `GobanModel::snapshot()`.** The game thread owns that tree and mutates it freely; `GameRecord`'s const accessors take no lock, and its own mutex covers neither the readers nor half the mutators. `uiInputs()` and `dumpState()` take every record fact from the published `GameSnapshot` (ADR-0006). **Whoever changes what the UI displays must publish it**: `onBoardChange()` is the funnel that covers moves, navigation, load, switch, scoring and handicap; `createNewRecord()` and `onBoardSized()` publish for themselves because the new-game path bypasses it. A missed publish shows up as stale UI and the scenario suite catches it — both misses in the original change were caught on the first run. A plain scalar that changes off the position-change path becomes atomic instead, as `GameRecord::unsavedChanges` did; saving is not a position change. **ADR-0006 is complete through stage 5**: `comment`, `markup`, `scoringError` and `passVariationLabel` are all published, so nothing on a per-frame or per-keystroke path reads the record *or* copies a `GameState` string. The player dropdowns compare `getActivePlayer()` — a `size_t` handed out under `PlayerManager::mutex` — rather than `state.black`/`state.white`, because the index is what the widget holds and the string was a race for no information. `GobanModel::onBoardSized()` does `state = GameState()`, reassigning every one of those strings at once, which is the sharpest version of the hazard. What remains by design is listed in the ADR — `hasGameWorthKeeping()`, save/archive, and the dialog seed — all on explicit user actions. Note that `ElementGame`'s old `positionNumber` guard was not wrong, only insufficient: an atomic edge makes a write *visible* but grants no exclusion, so copying a `std::string` or walking a `std::vector` across it is still a use-after-free. The same partial-locking shape has now been found three times — `GameRecord`, `PlayerManager` (writers unlocked while readers locked), and the process pipes — so when a reader crosses this boundary, check the writers before assuming a mutex means anything.
- **Widget state reads the phase, not `state.reason`.** They diverge after navigating back from a finished game: the phase returns to `Paused` while the reason stays set. See the `state.reason` invariant above.
- **A quantity displayed in two places is written by one function.**
  `ElementGame::syncPrisonerLabels()` writes all four prisoner labels. It was two
  copies, and they disagreed: `capturedBlack` counts *black stones removed*, so
  it is what **White** has taken, and the game-over branch had the pairing
  backwards — every finished game showed both counts swapped. Same shape as the
  buttons that disagreed with their commands before ADR-0005.

### Rendering Across Threads
- **A `GameObserver` callback may hand data over; it may not act on the view.**
  `GobanView`'s callbacks arrive on the game thread — and during startup on the
  engine-loader thread, with the board already drawn — so its own file comment
  limits them to raising `updateFlag` and copying plain data.
  `onBoardSized()` broke that: `board.clear()` assigns a `std::string` into each
  of 361 points, and it emptied four `std::vector`s that
  `updateNavigationOverlay()` walks once per repaint, which is an invalidated
  iterator rather than a stale value. It now stores the size in
  `pendingBoardSize` and `applyPendingResize()` does the work on the UI thread.
- **`applyPendingResize()` has two callers, and both are needed.**
  `GobanView::Update()` is the ordinary one. `GobanControl::finishGameReplacement()`
  is the other, and it is not redundant: it rebuilds the overlays, and it runs
  *earlier in the frame* than `Update()` — so with only the `Update()` call the
  resize would clear the overlays that had just been rebuilt.

### Sound
- **Queue the playback, then start the stream.** `StreamHandler::processEvent()`
  called `Pa_StartStream()` first and pushed the `Playback` afterwards. The
  callback runs on PortAudio's own thread and fires as soon as the stream is
  live, so it could find an empty list — and it used to answer that by returning
  `paComplete`, stopping the stream and silently discarding the sound that had
  just been queued. The callback never returns `paComplete` now; the stream runs
  until `stopIfInactive()` releases the device after the idle timeout, which was
  always the intended way to give it back.
- **The audio callback locks like everyone else.** It shares `data` with the UI
  thread, which `push_back`s into it — the writers-locked-readers-not shape
  again, and a reallocation mid-mix invalidates the iterator it is walking.
- **`sounds_played` counts what was *heard*, not what was asked for.** A request
  the mixer never saw looked exactly like one it played, which is why a swallowed
  stone sound had no symptom to assert on. It is an atomic incremented by the
  callback, reported by `dumpState()`, and pinned by
  `tests/scenarios/stone_sound_is_played.scn` — which needs a real output device,
  so it is marked `# sound: on` and skipped unless `GOBAN_SCENARIO_AUDIO=1`.

### Telling the User Something
- **Three surfaces, and they do not overlap.** `#lblStatus` / `#pnlLog` (top left)
  carry *application* status: which engine is still loading, and a badge for
  warnings and errors. `#lblMessage` (bottom, centre) carries *game* content —
  results, SGF comments — plus command feedback and confirmation prompts.
  `#grpAnalysis` (bottom right) carries the live evaluation. Loading
  text used to live in `lblMessage`; it was unnamed, English in every language,
  and competing with four other claimants.
- **The first two split on ownership; the third splits on tense.** `#lblMessage`
  carries **events** — things that happened once, that scroll past.
  `#grpAnalysis` carries **state** — a value that is continuously true and
  repaints twice a second. That is why the evaluation could not join the message
  box as a fifth claimant, and it is the rule any further surface has to answer.
- **Diagnostics reach the user through a spdlog sink, not through call sites.**
  `installMessageLogSink()` feeds `MessageLog`, so every existing
  `spdlog::warn`/`error` surfaces without being touched, and a new one surfaces
  by existing. Do not add a parallel "show this error" path.
- **The sink takes `warn` and above.** At `info` the panel is useless:
  `GtpClient` logs every command and response at that level, so one genmove
  against KataGo evicts the engine failure the user opened the panel for.
  Demoting that traffic to `debug` is *not* the fix — `last_run.log` at default
  verbosity is what users attach to a bug report, and that trace is the most
  diagnostic thing in it.
- **The badge counts arrivals since the panel was last opened**, not
  `MessageLog::size()`, which saturates at the buffer capacity and so cannot tell
  "nothing new" from "the buffer is full". Opening the panel is what marks them
  seen.
- **A blank line is not a message.** `GtpClient::issueCommand()` used to log the
  blank line that terminates every GTP response *before* breaking out of the
  read loop, so an engine refusal arrived as two entries — the second an error
  reading `gnugo >>  (command: ...)` with nothing in it. The log counts entries,
  and an empty one costs the user a badge exactly as a real one does.
- **While the panel is open, the status line must stay visible.** It is the only
  close affordance; when it hid itself (because opening cleared the badge) the
  panel could not be dismissed except from a menu.
- **Counts go in parentheses after the noun** — "Zprávy (3)", never "3 zpráv".
  Czech needs three plural forms and ja/ko/zh have none; the parenthesised form
  is grammatical in all five without a plural-rules library.
- **A user-visible string comes from a template, with an English fallback.**
  `ElementGame::templateText(id, fallback)`. A missing template used to yield an
  *empty* message, so a translation lacking one entry said nothing at all.

### Screenshots and the Windowing Platform
- **A hidden Wayland window renders nothing.** It is never mapped, so its surface
  has no buffer attached and every draw is discarded — `glReadPixels` returns
  black with no GL error. Rendering into a framebuffer object of our own does
  *not* help: the discard is not about the target. Scripted runs therefore ask
  GLFW for X11 (XWayland counts) when `DISPLAY` is set, and fall back if that
  fails. This is why `screenshot` produced black images for as long as it existed.
- **SIGPIPE is ignored.** Two callers need it: `GtpClient` writes into an engine's
  pipe and is written to handle a failed write — which it only gets to do if the
  default disposition has not already killed the process — and the X11 display
  connection closing during shutdown raises it too, which killed every scripted
  run with signal 13 *after* the scenario had passed.

### Persisted Settings
- **`user.json` is the runtime scratchpad and is not tracked.** Defaults that
  ship — the opening camera — live in `config/base.json`, which the application
  never writes. Keeping both in one file leaked local paths and language into the
  repository once (5fe4d48), and a `.gitignore` entry cannot prevent a repeat
  because ignoring does nothing to a file already tracked.
- **The camera resolves most-specific-first**: where the user left it
  (`camera_current`), then their saved preset (`camera`), then the config
  default. `reset camera` uses the last two — without the fallback it reset to
  wherever the view already was on a fresh install.
- **Writes are atomic and locked on both sides.** `std::ofstream` truncates on
  open, so the previous save lost every preference if interrupted; it now writes
  a `.tmp` and renames. `GameThread::setFixedHandicap()` calls `setKomi()` from
  the game thread while `GobanView` saves the camera from the UI thread, so the
  mutex covers the readers too — a writers-only lock is the partial-locking shape
  this codebase has produced three times, and the camera accessors return by
  value because a reference handed out under a lock is not protected by it.

### Live Evaluation
The feature issue #49 asks for. See
`docs/adr/0007-analysis-engine-owns-its-own-pipe.md`; every rule here is one
of its decisions, and `tests/test_analysis.cpp` plus
`tests/scenarios/evaluation_*.scn` enforce them.
- **A tsumego shows no evaluation at all.** The engine's first suggestion *is*
  the answer to the problem. `availableActions()` refuses both toggles there,
  but that is only half of it: the rule has to be enforced where the display is
  produced, or whoever switched the overlay on before opening a problem keeps
  it — and cannot switch it off, because the menu item is greyed by the very
  rule meant to hide it. `AnalysisService::loop()` therefore drops the report
  while `tsumegoMode` holds, which silences all three surfaces at once since
  each keys off `report()`. Suppressed, not disabled: the process stays alive
  (respawning KataGo costs a weights load per problem) and the user's toggles
  are untouched, so closing the problem restores exactly what was on.
- **The analysis engine is a separate process, always.** Never the coach's and
  never the kibitz engine's, even when all three are the same binary with the
  same command line. An analysis stream is a GTP command that deliberately never
  replies until told to stop, and ADR-0001 gives the game thread exclusive
  ownership of the playing engines' pipes. Only the *configuration* is inherited
  (`"analysis": 1`, else `"kibitz": 1`, else nothing) — never the process, and
  never a `Player`: it is not registered with `addEngine()`, so it cannot reach
  the player dropdowns or `syncOtherEngines()`.
- **It follows the review cursor, not the game position.** That is what makes the
  separate process necessary rather than merely tidy — the coach already holds
  the game position. The cost that argues against it is an artefact of reusing
  `syncEngineToPosition()`: `planIncrementalSync()` diffs against the path last
  sent, so an arrow key is one `undo` or one `play`. Replaying moves rather than
  setting up stones is also what KataGo's history planes need.
- **`playableMoves()` runs before the diff, never at the point of sending.** The
  `undo` count is counted against what was actually sent; skipping an unsendable
  move later would leave the two out of step by one for the rest of the game, and
  the engine would then be undoing somebody else's move.
- **Yield with `analysisMayRun()`, not `isThinking()`.** The loop clears
  `playerToMove` *before* its 500 ms inter-move sleep, so a bare test flaps the
  stream on and off once per move in a bot-versus-bot match — the
  two-searches-at-once case decision 6 exists to prevent. The predicate also asks
  whether the *loop is running*, because a loaded game paused with an engine to
  move has nobody searching, and review is when the numbers are worth most.
- **Absent is not zero, and stale is not absent.** No report hides the panel
  outright; a bar resting at 50% because nothing has been computed cannot be told
  from a genuine even game. The yield window is the one *stale* case — the
  numbers were true for a position no longer shown — and is dimmed rather than
  blanked, because blanking would flicker once per move. Same distinction as
  `Engine::final_score()`'s `std::optional`.
- **The win rate is anchored to Black on both surfaces; the score names the
  leader on both.** Anchoring is chess's convention and it is what makes a figure
  comparable across moves — "62% then 48%" reads as the swing your move caused,
  where naming whoever leads never drops below 50% and hides exactly that. The
  score keeps Go's own convention because that is how a result is written, and it
  matches `RE` and the message line.
- **Everything is converted to Black's frame of reference at parse time.**
  Engines report for the side to move, so raw numbers flip every move and read as
  noise. Same shape as the prisoner counts that shipped swapped.
- **`order` defaults to -1, not 0.** Otherwise "the engine did not say" is
  indistinguishable from "this is the best move", and the first block of a report
  that omits `order` wins on a technicality. `scoreLead` is stored per block
  because it arrives *before* `order` in KataGo's output.
- **The analysis client is quiet.** `GtpClient::setQuiet()` demotes its command
  traffic to `debug`. `last_run.log` at default verbosity is what a bug report is
  read from, and a continuous stream plus a replay per navigation jump would bury
  it — the same reason the message-log sink takes `warn` and above.
- **There is no `stop` command.** Any input line terminates an lz/kata-analyze
  stream; `stopStreaming()` sends `name` and drains through *both* the stream's
  closing blank line and that command's own response. Skipping the drain makes
  the next ordinary command read the tail of the stream as its reply — silently
  wrong results rather than an error. Bounded, on the `scoringTimeout()`
  precedent.
- **The panel and the board suggestions are two features, and the board one is
  off by default.** Not for clutter — for judgement. The numbers are read *after*
  a move, so a player can invent their own and evaluate it post-hoc; stars on the
  board are read *before*, and once the engine has pointed at a point you cannot
  un-see it. Turning the panel on must never silently start pointing at the
  board. `toggle_evaluation` and `toggle_evaluation_moves` are separate, both
  sticky, and the second lives in View → Overlay with its siblings Last Move and
  Next Move.
- **Colour means move quality, text keeps its old meaning.** Where a suggestion
  lands on a point the navigation overlay already labelled, the `3a` stays and is
  only tinted; only suggestions with no label of their own get one. Explicit SGF
  markup is left alone entirely — it is the user's own annotation, and it already
  outranks variation labels. Rank letters do not skip: if the engine's first
  choice was tint-only, the next labelled move is `A`, because a board showing
  `B` and `C` with no `A` reads as broken.
- **The ramp is win-rate loss against the best move, never absolute win rate.**
  In a decided game every move's absolute win rate is pinned near 100% or 0%, so
  a ramp over that distinguishes nothing exactly when review matters most.
  `|best.winrateBlack − move.winrateBlack|` — both are in Black's frame and the
  best move is the least bad for whoever is to move, so the absolute difference
  is the loss whichever colour it is and can never come out negative.
- **A report for another position is not drawn.** `updateAnalysisOverlay()`
  compares `report->positionId` against the snapshot's; drawing a stale one would
  put the engine's opinion of one position onto the stones of another.
- **A recommended pass sets the baseline but takes no point.** KataGo reports
  `move pass` like any other candidate, routinely once the endgame is settled.
  Filtering it out before the baseline was taken left every board move measured
  against the best of themselves, so the top one came out at zero loss, green,
  wearing an `A` — the overlay recommended filling your own territory. It is
  ranked with the rest, drawn in the margin as the word `pass`, and consumes no
  letter, for the same reason a tint-only move consumes none. Its `pos` is set
  to `(-1, -1)` explicitly: a default `Position` is `(0, 0)`, a real point, so a
  caller that forgot to check `pass` would have annotated A1.
- **The suggestions stand down at a scored end, like the readout.** Not merely
  for tidiness — they shared a channel and did not survive it. `setBoardOverlay()`
  writes `mAnnotation` into the same `glStones` float that `updateArea()` fills
  with `mBlackArea`/`mWhiteArea`, and `updateArea()` writes only when a point's
  *influence* changes, so the shading never came back; `removeBoardOverlay()`
  then reset the point to `mEmpty`. Switching Best Moves on at a counted ending
  erased the territory permanently. `scoredEnd` is the predicate, so a
  resignation — which counted nothing — keeps its suggestions, and navigating
  back off the end brings them back.
- **The wake decision comes from `fetch_or`'s previous value.** `requestRepaint()`
  read `updateFlag`, then OR-ed into it — two operations, and between them the UI
  thread can `exchange()` the flag to `UPDATE_NONE` and block in
  `glfwWaitEvents()`. The read said "not idle, somebody will draw", the exchange
  took the bits that somebody was going to draw, and the new bits landed with
  nobody left to notice them: nothing repaints until an unrelated input event
  arrives. A repaint is what plays the stone sound, so the symptom is a move that
  is silent and, until the mouse moves, invisible. With `fetch_or` the cases are
  exhaustive — bits landing before the exchange are drawn by it, bits landing
  after observe `UPDATE_NONE` and post. Called from the game thread on every
  move; not theoretical.
- **`UPDATE_ALL` does not include `UPDATE_SOUND_STONE`, so never *assign* it.**
  Sound is an event, not a surface, so it is rightly outside the redraw-everything
  mask — which means `updateFlag = UPDATE_ALL` on a window resize threw away a
  stone sound that had been requested and not yet played. It is `|=` now.
- **Waking the renderer for a suggestion needs `UPDATE_STONES` too.** A label
  sets the annotation material, and that has to reach the stone upload or the
  grid stays drawn under it. This is why the publish gate does not ask for a
  bare repaint.
- **A label can sit off the grid.** `FloatingLabel` carries its own board
  coordinate — the same space the point overlays use, but float and signed. The
  grid is [0, N-1]; the wood extends **0.85 grid spacings past it on all four
  sides**, on every board size, because the constant in `Metrics::calc()` is in
  grid units. So a margin coordinate needs no per-size arithmetic. Unlike a point
  overlay it touches no material — there is no grid out there to erase — and
  nothing clears it when a stone lands. `add_text` centres on the point given.
- **`add_text` centres on its point** — that is what puts a move number over the
  middle of its intersection — and takes a `TextAlign` to place the point at the
  left or right edge of the text instead. The alignment is applied after the
  measuring pass, which is the only moment the text width is known.
- **The readout stands down at a scored end.** `GameSnapshot::scoredEnd` — the
  result is a fact there, `#lblMessage` already states it, and the estimate
  contradicts it: KataGo's `scoreLead` and GNU Go's `final_score` differed by a
  tenth of a point on screen, which reads as a bug. A **resignation** scores
  nothing and keeps the readout, and navigating back off the end brings it back,
  because `scoredEnd` follows the cursor.
- **Stale is a second colour, not a factor.** `annotations.readout_stale_color`
  defaults to `readout_color`, so the guard is invisible until someone opts in.
  Multiplying an alpha the user has already tuned down has no defensible default.
- **Annotation ink is global, not per shader — for now.** It arguably belongs to
  the shader, which is what decides the board is wood-coloured; but all six
  shaders draw the same board today, so per-shader ink would be six copies of one
  value. `annotations` in `config/base.json` is the default, and a shader entry
  gaining its own `annotations` block to override it is a pure addition whenever
  a dark-board shader makes it stop being duplication.
- **A shipped default goes in `config/base.json`, a user's choice in `user.json`,
  and an unmade choice is written nowhere.** `annotations.readout_color` ships the
  readout's ink; `evaluation_color` overrides it. `UserSettings` writes
  `evaluation_color` only when it is non-empty — writing it unconditionally would
  pin today's default into every `user.json` and quietly defeat any later change
  to it. Same two-level arrangement as the camera.
- **The coordinate convention has one implementation.** `Position::columnLabel()`
  and `rowLabel()` — skipping `I`, numbering rows from 1 at the bottom — are what
  both the board's margin labels and `operator<<` use. Board rows run opposite to
  SGF rows, so a second copy would drift silently and the board would end up
  disagreeing with what `click` and the record mean by the same point.
- **Every character drawn must be in the atlas string** (`GobanOverlay.cpp:59`).
  The font is not the gate; that string is. A glyph absent from it simply does
  not appear, silently.
- **One glyph pass draws all the text, so nothing may gate the pass but the pass
  itself.** Move numbers, variation labels, SGF markup, the coordinate margin,
  the evaluation's `A`/`B`/`C` and its board readout are built into the same
  buffers by `GobanOverlay::Update()`. `GobanView::Render()` ran the draw only
  `if (showLastMoveOverlay || showNextMoveOverlay)` — correct the day it was
  written, when the two markers *were* the overlay, and silently outgrown by
  everything added since. Switching both markers off therefore took the
  coordinates, the markup and the entire evaluation display down with them. A
  toggle belongs where its own labels are **placed** — `updateLastMoveOverlay()`
  and `updateNavigationOverlay()` already check theirs — never around the draw.
  Pinned by `tests/scenarios/overlays_outlive_the_move_markers.scn` through
  `overlay_glyphs`, which counts what the pass actually drew: `coordinates_shown`,
  `markup_count` and `eval_labels` all stayed true throughout, which is exactly
  why the suite could not see this. Same distinction as `sounds_played`.
- **The overlay is not part of `isIdle()`.** An analysis stream never finishes on
  its own, so treating it as work-in-flight would make quiescence unreachable —
  the same trap as waiting on `EngineSync::Unsynced`.
- **Do not block the main loop once an exit is requested.** `AppState::
  RequestExit()` is set from *inside* a loop iteration, and the condition is not
  re-evaluated until the next one, so an unconditional `glfwWaitEvents()` waits
  for an event that will never arrive. It stayed latent because whatever ends a
  run normally dirties the view; the publish gate deliberately requests no repaint
  when nothing changed, which is a clean frame, which hung the process on exit.

### Keybindings
- **A binding is a chord**: key plus a `KeyMod` bitmask from optional
  `ctrl`/`shift`/`alt` booleans. An entry with none means the unmodified key,
  which is what every binding meant before, so old configs keep working.
- **Modifiers match exactly, with no fallback.** Otherwise every accelerator
  would also fire its unmodified twin — Ctrl+S would `save` *and* `zoom camera`.
- **Navigation and prompt keys are handled before the table, and only
  unmodified**, which leaves Ctrl+Left bindable.
- **Bind `clear`, never `new_game`.** `new_game` is the scripting entry point and
  deliberately does not prompt; putting it on a key would be a silent
  game-discarder, against the rule that all three replacement paths confirm.
- **Keybindings accumulate across an `$include` chain.** `load()` recurses before
  `merge_patch` replaces the `controls` array, so an includer can *add* a binding
  but cannot remove one, and only the JSON looks replaced. Pinned by
  `tests/test_configuration.cpp`.

### UI Event Suppression
- **`syncingUI` flag**: `GobanControl::syncingUI` suppresses game actions triggered by UI change events during programmatic dropdown updates. Any method that repopulates dropdowns (player, board size, komi, handicap) must wrap the repopulation with `setSyncingUI(true/false)`. Event handlers in `EventHandlerNewGame` check `isSyncingUI()` and skip side effects when true. This prevents transient intermediate states (e.g. briefly activating an engine player during dropdown clear/repopulate) from triggering game actions.

## Coding Principles

- **Search before creating**: Before introducing a new flag, helper, or mechanism, search the codebase for existing patterns that solve the same problem. Use `grep` for related keywords (e.g. "suppress", "syncing", "guard", "flag"). Reusing an existing mechanism is always preferable to adding a new one.
- **Fix at the source**: When a race condition or unwanted side effect is discovered, fix the root cause rather than adding compensating workarounds downstream. A guard at the event source is better than a deferred correction after the fact.
- **Fail early**: Always check return values from operations that can fail (GTP commands, file I/O, engine communication). When a prerequisite fails, bail out immediately rather than continuing with corrupt state. Silent failures cascade — a failed `boardsize` followed by blind move replay produces wrong results that are hard to diagnose.
- **Record decisions, not just invariants**: a non-obvious design choice — especially one where you rejected a plausible alternative — belongs in `docs/adr/` as an Architecture Decision Record, with the alternatives and the downsides you accepted. Read the existing ADRs before proposing something that revisits an old decision. Invariants (rules that must always hold) stay in the section above and should be enforced by a test; decisions are append-only and superseded rather than edited.
- **Document new invariants**: When introducing a new invariant, flag, or cross-cutting concern, add it to the Design Invariants section above. Not all invariants can be documented upfront, but capturing them as they're discovered prevents future regressions.

## Test Scenarios

Known error-prone sequences that should be regression tested. **All five prose
sequences once listed here are now executable** under `tests/scenarios/*.scn` — run them
with `./tests/run_scenarios.sh`, and see `docs/testing.md` for the directive
syntax. They are kept here as an index; the scenario file is the specification.

| Sequence | Scenario |
|---|---|
| Navigation During Engine Play | `match_mode_blocks_navigation.scn`, `bot_match_locks_player_actions.scn` |
| Player Switch During Navigation | `player_switch_during_navigation.scn` |
| Space Key at Branch End | `navigation_keys.scn` |
| SGF Modification and Save | `variation_promotes_main_line.scn` |
| Analysis Mode Workflow | `analysis_mode_toggle.scn`, `analysis_mode_auto_reply.scn` |

Areas covered since (each found at least one real defect): board clicking and
the stone-in-hand model (`board_click_stone_in_hand.scn`), tsumego mode
(`tsumego_mode.scn`, `tsumego_collection.scn`), territory after a resignation
(`territory_needs_a_score.scn`), multi-game collections
(`multi_game_collection.scn`), the Clear/Quit confirmations
(`discard_prompts.scn`), the live evaluation overlay
(`evaluation_overlay.scn`, `evaluation_unavailable.scn` — which found the
exit-while-idle hang above), a click the rules refuse
(`illegal_click_is_refused_locally.scn`), a click on a move the record already
has (`click_follows_recorded_variation.scn`), and the glyph pass every overlay
shares (`overlays_outlive_the_move_markers.scn`).

Tsumego is the worked example of coverage that looks complete and is not: the
mode had a scenario and five unit tests, and every path the scenario did *not*
walk — the dead branch, a second visit to a wrong move, a pass — turned out to
hold a defect. What the suite covers is what it executes, not what it is about.

Converting a prose sequence is a good way to pin a bug you have just fixed.

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

The sources are in `res/stereo.zip` — Matěj Boháč, *Less is More! Keeping the
Deviation Under Control* and *Výpočet maximální deviace* (klub stereoskopické
fotografie). `src/Stereo.h` is the one implementation; `tests/test_stereo.cpp`
and `tests/scenarios/stereo_depth_budget.scn` hold it to this.

**Deviation** is the horizontal separation of the two images of a point, as a
fraction of the image width, and what must be bounded is the *difference*
between the nearest and the furthest point in frame:

```
deviation = base * focal_length * (1/near - 1/far)      // far = infinity: base * f / near
```

**Ceiling: 1/30th (3.3%) of the image width.** Projection is the demanding case
and this is its number; being more conservative costs nothing, which is the
article's own advice, so `Stereo::DEFAULT_DEVIATION` asks for 1/40.

**It is set by the near point, not by the camera distance.** This is the whole
of it, and getting it wrong is not a matter of taste: the board is a fixed-size
object, so the nearest point in frame shrinks *far faster* than the camera
distance as you zoom in. The base used to be `eof * cameraDistance / f`, which
measured 1/20 of the image width at the default zoom and 1/12 zoomed in, on a
1/30 ceiling — an error that grows as you approach, so a check at one zoom
says nothing about another. `GobanView::stereoNearPoint()` asks the board box
*and* the table's near edge, because "the nearest point is often not the
subject, but the blades of grass at the bottom edge": the board binds at
ordinary zoom, the table once the camera pulls back.

**The window (`dof`) is not part of the deviation.** The horizontal image shift
slides the whole depth range through the screen plane; it cancels in
near-minus-far. It decides where the scene sits relative to the glass, not how
much depth the eyes must accept.

**One implementation, used by both layers.** `GobanView::stereoHalfBase()` is
uploaded to the vertex shader (as the `eof` uniform, which therefore carries a
*computed half-base in world units*, not the preference) **on every frame, from
`shadeIt()`** — it lived in `setMetrics()` for one afternoon, which runs only on
a board or shader change, so the board's base froze at whatever the camera was
when the shader was last switched while the overlay's followed the camera. The
same value places the glyph overlay's two eyes. Two implementations would let
the labels drift off the wood they are lying on, which is exactly what a
mismatched pair looks like.

**Do not normalize a ray direction in a vertex shader.** `rdbl`/`rdbr` are
varyings, so the fragment gets the interpolation of the four corner values, and
interpolating unit vectors is not interpolating directions — it is only correct
when the corners have equal length. The mono shader gets away with it, since
`(±ratio, ±1, f)` all have the same length; `dof` breaks that symmetry, so
pre-normalizing warped each eye and shrank the stereoscopic window to **85%** of
the configured value (0.849 predicted, 0.853 measured at 4:3, `dof` 0.0925) —
and the attenuation varies across the screen, which no constant can correct.
The fragment shader normalizes anyway; that is where it belongs.

**Anaglyph is greyscale, so annotation colour does not survive it.**
`partial/stereo/on.glsl` writes `vec3(luma_left, 0.1, luma_right)`;
`GobanOverlay::eyeInk()` reduces every label to its own brightness for the same
reason, or text tinted green would vanish from the red eye. The evaluation's
move-quality ramp therefore reads as brightness under a stereo shader — the
`A`/`B`/`C` letters survive, the colour meaning does not. The ink is baked into
the glyph buffers, so `GobanView::Update()` asks for an overlay rebuild whenever
the selected shader changes.

**Both eyes draw at the same depth**, so the overlay pass turns depth writes off
— with them on, the first eye rejects the second and the right eye's text is
simply missing.

**One depth buffer, two eyes: take the nearer.** Depth here is a *layer* rather
than a distance — `partial/algorithm/render.glsl` classifies a pixel as 0.25
(board from below), 0.5 (a stone) or 0.75 (everything else), and the overlay
draws at 0.4 (a label on a stone, in front) or 0.6 (a label on the board,
behind). That is what hides an annotation under a stone. The stereo shader calls
`render()` twice into one buffer, so writing `gl_FragDepth` inside it meant the
second eye won outright: wherever the two eyes disagreed about a stone — most of
the stone, once the window is wide — the best-move letter showed straight
through it, which is what a stone carried in hand over a suggestion looked like.
`render()` reports into `sceneDepth` and the caller decides; `stereo/on.glsl`
takes `min(dl, dr)`, so a pixel either eye sees as a stone counts as one.

**Why parallel cameras (not toe-in):**
- Toe-in convergence causes keystone distortion and eye strain
- Parallel cameras are physically correct (like human eyes at distance)
- Zero parallax at infinity; objects closer have increasing parallax
- Stereo effect naturally decreases with distance (correct for depth perception)

**Parameters:**
- `eof`: the deviation asked for, as a fraction of the image width, **clamped to
  `Stereo::MAX_DEVIATION` (1/30)**. It used to be a bare eye-offset factor that
  the shader scaled by the camera distance; the clamp is what makes a value left
  over from those days — `user.json` ships whatever was last set, typically
  0.0725 — safe rather than three times the ceiling. Lower is calmer; 1/40 is
  the shipped default.
- `dof`: the stereoscopic window, in `q0` units — where the scene sits relative
  to the screen plane. Independent of the deviation above.
