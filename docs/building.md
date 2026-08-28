# Building from Source

## Requirements

- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+
- OpenGL 3.1+ development libraries
- Git

## Dependencies

Most dependencies are fetched and built automatically by CMake:

- clipp
- doctest (tests only)
- freetype2
- GLFW
- GLM
- glyphy
- libsgfcplusplus
- libsndfile
- nlohmann::json
- portaudio
- RmlUi
- spdlog

glad is vendored in `src/glad/`. Boost is no longer used.

The `BuildPrerequisites` option below controls the first pass. Spelling it any
other way is silently ignored by CMake, and the dependencies then never get
built.

## Linux Build

### Quick Build

```bash
./make.sh
```

### Manual Build

```bash
# Create build directory
mkdir -p cmake-build-release
cd cmake-build-release

# First pass: build dependencies
cmake .. -DCMAKE_BUILD_TYPE=Release -DBuildPrerequisites=ON
make -j$(nproc)

# Second pass: build application
cmake .. -DCMAKE_BUILD_TYPE=Release -DBuildPrerequisites=OFF
make -j$(nproc)
```

### Debug Build

```bash
mkdir -p cmake-build-debug
cd cmake-build-debug

cmake .. -DCMAKE_BUILD_TYPE=Debug -DBuildPrerequisites=ON
make -j$(nproc)

cmake .. -DCMAKE_BUILD_TYPE=Debug -DBuildPrerequisites=OFF
make -j$(nproc)
```

## Windows Build

Windows builds use vcpkg for some dependencies.

### Prerequisites

1. Install [vcpkg](https://github.com/microsoft/vcpkg)
2. Install required packages:
   ```cmd
   vcpkg install freetype:x64-windows-static
   vcpkg install portaudio:x64-windows-static
   vcpkg install libsndfile:x64-windows-static
   ```

### Build

```cmd
mkdir cmake-build-release
cd cmake-build-release

cmake .. -DCMAKE_BUILD_TYPE=Release -DBuildPrerequisites=ON -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build . --config Release

cmake .. -DCMAKE_BUILD_TYPE=Release -DBuildPrerequisites=OFF
cmake --build . --config Release
```

## Running

Run from the project root directory (not the build directory) to ensure assets are found:

```bash
# From project root
./goban

# Or via symlink (created by build)
./cmake-build-release/goban
```

### With Custom Configuration

```bash
./goban -c config/zh.json         # Chinese interface
./goban -v debug                  # Debug logging
./goban -v trace                  # Verbose trace logging
```

### Log Output

Debug output is written to `last_run.log` in the working directory.

## Development Notes

### Project Structure

```
goban/
├── src/           # Main source code
├── config/        # Configuration, shaders, GUI, sounds, fonts
├── deps/          # Dependency build scripts and patches
├── games/         # SGF game storage
├── cmake/         # CMake find modules
└── docs/          # Documentation
```

### Key Source Files

- `src/main.cpp` - Application entry point
- `src/GobanModel.cpp` - Game state and logic
- `src/GobanView.cpp` - 3D rendering
- `src/GobanControl.cpp` - Input handling
- `src/GameThread.cpp` - GTP engine communication
- `src/ElementGame.cpp` - RmlUi game interface

See [Architecture](architecture.md) for how these fit together and which thread
owns what.

### Shaders

GLSL shaders are in `config/shaders/`:
- `vertex/mono.glsl` - Standard vertex shader
- `vertex/stereo.glsl` - Stereoscopic vertex shader
- `fragment/red_carpet.glsl` - Main ray-traced shader
- `fragment/thin.glsl`, `flat.glsl`, `2d.glsl` - Simplified variants
- `fragment/red_carpet_stereo.glsl`, `thin_stereo.glsl` - Anaglyph variants
- `fragment/partial/` - Shared includes pulled in by the above

## Troubleshooting

### Missing OpenGL

Ensure OpenGL development libraries are installed:
```bash
# Debian/Ubuntu
sudo apt install libgl1-mesa-dev

# Fedora
sudo dnf install mesa-libGL-devel
```

### Shader Compilation Errors

Check `last_run.log` for GLSL compiler errors. Common issues:
- GPU doesn't support required OpenGL version
- Driver issues with specific shader features

### GTP Engine Not Found

Ensure the engine path in your config file is correct:
- Absolute path: `/usr/games/gnugo`
- Relative path: `./engine/gnugo/gnugo`

Check engine works standalone: `gnugo --mode gtp`
