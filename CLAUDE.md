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
./goban --verbosity error --config data/zh/config.json
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
- **data/**: Configuration files, shaders, fonts, sounds, GUI resources
- **data/shaders/**: GLSL shader files for ray-traced rendering
- **data/gui/**: libRocket GUI templates and stylesheets
- **data/sgf/**: SGF game record storage
- **engine/**: External Go engines (GNU Go, KataGo, etc.)
- **cmake/**: CMake find modules for dependencies

### Configuration System

The application uses JSON configuration files:
- **data/config.json**: Main configuration with bot definitions, controls, shaders
- **data/zh/config.json**: Chinese language configuration
- Bot configuration includes GTP engine paths, parameters, and message parsing

### SGF Game Records

- Games are automatically saved to `data/sgf/` directory
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

## Ray-Traced Rendering - Coordinate System and Shaders

The Go board is rendered using ray tracing in GLSL fragment shaders. The vertex shaders set up the camera and compute ray origins/directions for each pixel.

### World Coordinate System

- **Origin (0, 0, 0)**: Center of the Go board
- **Y-axis**: Points UP (board surface is at y ≈ 0)
- **Z-axis**: Points toward viewer (camera is at negative z)
- **X-axis**: Points right (completes right-handed system)

### Mono Vertex Shader (`config/shaders/vertex/mono.glsl`)

Sets up a single camera for standard rendering.

**Key variables:**
- `ro = (0, 0, -3)`: Base camera position, 3 units back on z-axis
- `ta = (0, 0, 0)`: Target/look-at point (board center)
- `up = (0, 1, 0)`: World up vector
- `m`: Model-view matrix (includes user rotation via `glModelViewMatrix` and intro animation)
- `tt`: Translation from `iTranslate` (pan/zoom offset)

**Camera setup:**
```
roo = m * ro + tt          // Final camera position in world space
ta = ta + tt               // Translated target point
cw = normalize(ta - roo)   // Forward direction (toward target)
cu = normalize(cross(up, cw))  // Right direction
cv = cross(cw, cu)         // Up direction (orthogonal to cw, cu)
```

**Ray direction:**
```
q0 = vertex.xy * aspectRatio   // Screen coordinate (-1 to 1, aspect corrected)
rdb = normalize(q0.x*cu + q0.y*cv + 3.0*cw)
```
The `3.0` is the focal length - rays spread from camera through a virtual screen at distance 3.0.

### Stereo Vertex Shader (`config/shaders/vertex/stereo.glsl`)

Sets up two cameras (left/right eyes) for stereoscopic 3D rendering (anaglyph, etc.).

**Additional uniforms:**
- `eof`: Eye offset factor (stereo base = 2 × eof)
- `dof`: Unused (kept for compatibility)

**Stereo camera setup (parallel cameras):**
```
// Constant stereo base, like human IPD
// Left eye at -X, right eye at +X in camera space
rool = (m * vec4(ro - eoff.xyz, 1.0)).xyz + tt  // Left eye
roor = (m * vec4(ro + eoff.xyz, 1.0)).xyz + tt  // Right eye

// Parallel cameras: both eyes share the same view direction
roo = 0.5 * (rool + roor)      // Center position
cw = normalize(tt - roo)       // Shared view direction (toward target)
cu = normalize(cross(up, cw))  // Shared right vector
cv = cross(cw, cu)             // Shared up vector

// Same ray direction for both eyes, different origins
rd = normalize(q0.x*cu + q0.y*cv + 3.0*cw)
rdbl = rdbr = rd
```

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
