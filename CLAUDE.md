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
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PREREQUISITIES=ON
make
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_PREREQUISITIES=OFF
make
```

### Debug Build
```bash
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_PREREQUISITIES=ON
make
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_PREREQUISITIES=OFF
make
```

### Running the Application
```bash
# Default run
./goban

# Chinese translation with suppressed logging
./goban --verbosity error --config config/zh.json
```

## Architecture Overview

### Core Components

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

### Navigation & Engine Synchronization
- **No genmove during navigation**: Navigation commands (back/forward/home/end) must not interleave with GTP genmove. Use `navigationInProgress` atomic flag.
- **Block navigation while engine thinking**: `isThinking()` returns true only for ENGINE types (not human players). Navigation keys are blocked when engine is processing.
- **Navigation in bot-bot matches**: Requires switching to Analysis mode first (pauses genmove loop).

### SGF Game Record Consistency
- **PB/PW updated on first move**: Player names in SGF header are updated when the first move is made, capturing actual players after setup.
- **Annotations only after first move**: Player switch annotations (`switched_player:`) only recorded after `moveCount() > 0`. Setup changes are silent.
- **No annotations during SGF loading**: `started` flag must be false during SGF load to prevent spurious annotations.
- **Result removed if main line unfinished**: When saving a modified SGF, if the main line (first children) doesn't end in resign/double-pass, remove the RE property.

### Game State
- **isGameFinished()**: True only for resign or double-pass (two consecutive passes).
- **Territory display**: Only shown at finished game positions, not at end of unfinished variations.
- **Loaded games stay paused**: After loading an SGF, `model.started` remains false. Human moves work through the navigation path (`navigateToVariation`). Engine play requires explicit "Start" button press (`model.start()`).
- **`isGameOver` lifecycle during navigation**: Cleared by `model.start()` when navigating back/home from a finished game. Restored by `navigateForward`/`navigateToEnd` when reaching end of a game with `hasGameResult()`.

### UI Event Suppression
- **`syncingUI` flag**: `GobanControl::syncingUI` suppresses game actions triggered by UI change events during programmatic dropdown updates. Any method that repopulates dropdowns (player, board size, komi, handicap) must wrap the repopulation with `setSyncingUI(true/false)`. Event handlers in `EventHandlerNewGame` check `isSyncingUI()` and skip side effects when true. This prevents transient intermediate states (e.g. briefly activating an engine player during dropdown clear/repopulate) from triggering game actions.

## Coding Principles

- **Search before creating**: Before introducing a new flag, helper, or mechanism, search the codebase for existing patterns that solve the same problem. Use `grep` for related keywords (e.g. "suppress", "syncing", "guard", "flag"). Reusing an existing mechanism is always preferable to adding a new one.
- **Fix at the source**: When a race condition or unwanted side effect is discovered, fix the root cause rather than adding compensating workarounds downstream. A guard at the event source is better than a deferred correction after the fact.
- **Fail early**: Always check return values from operations that can fail (GTP commands, file I/O, engine communication). When a prerequisite fails, bail out immediately rather than continuing with corrupt state. Silent failures cascade — a failed `boardsize` followed by blind move replay produces wrong results that are hard to diagnose.
- **Document new invariants**: When introducing a new invariant, flag, or cross-cutting concern, add it to the Design Invariants section above. Not all invariants can be documented upfront, but capturing them as they're discovered prevents future regressions.

## Test Scenarios

Known error-prone sequences that should be regression tested:

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
