# Red Carpet Goban Documentation

Ray-traced 3D Go/Baduk/Weiqi board application with GLSL shaders, GTP engine support, and SGF game records.

**Downloads**:
- [hraj.si/goban](https://hraj.si/goban) - Windows bundle with GNU Go included
- [GitHub Releases](https://github.com/popojan/goban/releases) - Bare executables for all platforms

**Cross-platform setup**: Download the Windows bundle for config/data files, then replace the executable with your platform's build from GitHub releases.

## Documentation

- [Getting Started](getting-started.md) - First game, basic controls
- [Game Modes](game-modes.md) - Match vs Analysis mode, use cases, undo behavior
- [Keyboard Shortcuts](keyboard-shortcuts.md) - Complete key binding reference
- [Configuration](configuration.md) - JSON configuration files, GTP engines, controls
- [User Settings](user-settings.md) - Settings persisted between sessions (user.json)
- [SGF Game Records](sgf-records.md) - Game saving, loading, and the games folder
- [Building from Source](building.md) - Compilation instructions for developers
- [Architecture](architecture.md) - Component and thread map, and four data flows end to end
- [Testing](testing.md) - Unit test suite, the mock GTP engine, adding a test
- [Architecture Decision Records](adr/README.md) - Why the code is the way it is
- [Release Notes](RELEASE_NOTES.md) - What changed in each version
- [Credits](CREDITS.md) - Translation credits
- [Third-Party Software](THIRD-PARTY.md) - What Goban is built from, and its licences

## Quick Start

1. Download from [hraj.si/goban](https://hraj.si/goban) or [build from source](building.md)
2. Run `./goban` (or `goban.exe` on Windows)
3. Click on the board to place stones

## Command Line

```bash
./goban                           # Use last config or default (config/en.json)
./goban -c config/zh.json         # Chinese interface
./goban -v debug                  # Verbose logging
```

| Option | Description | Default |
|--------|-------------|---------|
| `-v`, `--verbosity` | Log level: trace/debug/info/warning/error | warning |
| `-c`, `--config` | Path to configuration file | last used or config/en.json |
| `--platform` | Windowing backend: `auto`, `wayland` or `x11` | auto |
| `-s`, `--script` | Run a scenario script instead of an interactive session | — |
| `--user-settings` | Persist preferences to another file | `user.json`, or `scenario-user.json` under `--script` |
| `--record` | Force session recording on during a scripted run | off |

Both windowing backends are compiled in, so `--platform` is a runtime choice and
needs no rebuild. It is worth knowing about on a **multi-monitor Wayland
session**: Wayland exposes no global window position, so fullscreen (**F**)
cannot tell which monitor the window is on and lands on whichever output came
first. `--platform x11` runs through XWayland, where it works correctly.

The last three are development options; see [Testing](testing.md).

## Links

- [Project Homepage](https://hraj.si/goban) - Downloads, screenshots
- [GitHub Repository](https://github.com/popojan/goban) - Source code
- [Issue Tracker](https://github.com/popojan/goban/issues) - Bug reports, feature requests
