# Red Carpet Goban

The program is configured using JSON configuration file, it can be specified as a command line argument.

## Command line arguments

| short | long                   | description                                                    |  default |
| ----- | ---------------------- | -------------------------------------------------------------- | -------- |
| -v LEVEL   | --verbosity LEVEL      | amount of information logged into console <br> LEVEL is one of _trace_ / _debug_ / _info_ / _warning_ / _error_ |  warning |
| -c FILE   | --config FILE | path to configuration file used for the current run            | last used config from user.json, or [config/en.json](https://github.com/popojan/goban/blob/master/config/en.json)

## User settings (user.json)

The application stores user preferences in `user.json` in the working directory. This file is created automatically and remembers settings between sessions.

### Automatically saved

These settings are saved immediately when changed:

| Setting | Trigger | Description |
| ------- | ------- | ----------- |
| `last_config` | On startup | Path to the configuration file used; restored on next launch |
| `fullscreen` | **F** key | Fullscreen window state |
| `shader.name` | **V** key or dropdown | Selected shader variant name |

### Saved on explicit action

These settings are saved when using **Menu → Camera → Save**:

| Setting | Description |
| ------- | ----------- |
| `camera.rotation` | Camera view angle (quaternion x, y, z, w) |
| `camera.translation` | Camera position including zoom level (x, y, z) |
| `shader.gamma` | Gamma adjustment (**+** / **-** keys) |
| `shader.contrast` | Contrast adjustment (**Up** / **Down** keys) |
| `shader.eof` | Eye offset factor for stereo shaders |
| `shader.dof` | Depth of field parameter |

Use **Menu → Camera → Delete** to reset all user settings to defaults.

### Not persisted

The following settings are **not** saved between sessions:

- Board size, komi, handicap (reset to defaults on new game)
- Player selections (human vs engine assignments)
- Window size and position
- Current game state

## Configuration sections

The JSON file contains following top level sections. Their meaning and default contents is described below.

- [Humans](#humans)
- [Bots, i.e. GTP engines](#bots)
- [Controls](#controls)
- [Sounds](#sounds)
- [Fonts](#fonts)
- [Shaders](#shaders)
- [GUI](#gui)

### Humans

Array of human player names to choose from in the GUI.
Used only as a personalization touch and in the recorded SGF files.


Default: _Human_

### Bots

Or so called *engines*. At the moment GnuGo is required as the main engine, as the Goban relies on its showboard command output and
territory analysis. Misconfigured engine can easily crash the program. Playing an unsupported board size can cause strange behaviour.

There are following required and optional attributes for each engine:

| attribute name | required | attribute value  | description | default |
| -------------- | -------- | ---------------- | ----------- | ------- |
| name           | yes      | string          | name of the engine as displayed in the GUI and in the recorded SGF files | |
| path           | yes      | string          | hint path to the engine executable; not used if the engine is on the system PATH | |
| command        | yes      |                 | name of the engine executable; either a system wide command or search for in the path given above | |
| parameters     | yes      | string          | command line arguments for the (GTP) engine itself | |
| main           |          | {`0`, `1`}      | if it is the main engine enforcing rules; has to be GnuGo at the moment | 0 |
| enabled        |          | {`0`, `1`}      | whether the engine is enabled; to easily switch of some of the configured engines | 1 |
| kibitz         |          | {`0`, `1`}      | whether it is the kibitz engine; to be used for move suggestion | 0 |
| messages       |          | dictionary      | rules to parse GTP engine output and display after each move; _TBD_ | |

Following engines are configured in the default configuration file:

- **GnuGo 3.8** (has to be downloaded separately, except in the windows build)
- **Katago** (disabled, has to be downloaded separately)
- **Pachi 12.60** (disabled, has to be downloaded separately)

Other GTP engines should be easily added, e.g. _leela zero_.

Using [gtp4zen](https://github.com/breakwa11/gtp4zen) middleware (and Wine in linux) it is probably possible to configure **Zen7** and **Zen6**, if you own the program and its respective `zen.dll`.

### Controls

Control section maps keys to commands. Please see the default configuration file for supported commands.

See [RmlUi/Include/RmlUi/Core/Input.h](https://github.com/mikke89/RmlUi/blob/master/Include/RmlUi/Core/Input.h) for actual key codes meaning.

#### Game Controls

| Key | Function |
| ---- | ---- |
| **Space** | Play AI-suggested move (kibitz) / trigger AI move in Analysis mode |
| **P** | Pass |
| **R** | Resign game |
| **U** | Undo last move |
| **Enter** | Toggle Analysis mode (pause AI-vs-AI matches for review) |

#### SGF Navigation

When an SGF game is loaded or during review:

| Key | Function |
| ---- | ---- |
| **Space** or **Right Arrow** | Navigate forward one move |
| **Left Arrow** or **Backspace** | Navigate back one move |
| **Home** | Navigate to start of game |
| **End** | Navigate to end of game |
| **Mouse click** | Click on existing variation to follow it, or create new variation |

#### Camera Controls

| Key | Function |
| ---- | ---- |
| **C** | Center view from above |
| **A** + mouse move | Rotate view |
| **S** + mouse move | Zoom view |
| **D** + mouse move | Pan/drag view |
| **Right-click drag** | Rotate view |
| **Middle-click drag** | Pan view |
| **Mouse wheel** | Zoom in/out |

#### Display Settings

| Key | Function |
| ---- | ---- |
| **V** | Cycle through shader variants (Red Carpet, Minimal, Flat, 2D, Stereo) |
| **T** | Toggle territory display |
| **N** | Toggle text overlay |
| **O** | Trigger intro animation |
| **X** | Toggle max FPS mode (uncapped vs event-driven rendering) |
| **F** | Toggle fullscreen |
| **]** / **[** | Increase / decrease gamma |
| **+** / **-** | Increase / decrease contrast |
| **\\** | Reset contrast and gamma |

#### Stereo 3D Settings

| Key | Function |
| ---- | ---- |
| **H** / **L** | Increase / decrease eye separation (eof) |
| **J** / **K** | Increase / decrease depth of field (dof) |

#### Application

| Key | Function |
| ---- | ---- |
| **Escape** | Quit application |

### Sounds

Paths to wave files for game sounds:
- [stone.wav](https://github.com/popojan/goban/blob/master/config/sound/stone.wav) - stone placement sound
- [collision.wav](https://github.com/popojan/goban/blob/master/config/sound/collision.wav) - stone collision/clash sound

### Fonts

Paths to fonts used in the application:
- **gui** - array of fonts for the RmlUi interface (supports fallback for CJK characters)
  - [NotoSans-Regular.ttf](https://github.com/popojan/goban/blob/master/config/fonts/NotoSans-Regular.ttf)
  - [NotoSansCJKsc-Regular.otf](https://github.com/popojan/goban/blob/master/config/fonts/NotoSansCJKsc-Regular.otf)
- **overlay** - font for stone coordinate overlay
  - [default-font.ttf](https://github.com/popojan/goban/blob/master/config/fonts/default-font.ttf)

### Shaders

GLSL shader variants to choose from. Each shader entry has:
- _name_ - displayed in GUI shader selector
- _vertex_ - path to vertex shader (e.g., [mono.glsl](https://github.com/popojan/goban/blob/master/config/shaders/vertex/mono.glsl) or [stereo.glsl](https://github.com/popojan/goban/blob/master/config/shaders/vertex/stereo.glsl))
- _fragment_ - path to fragment shader (e.g., [red_carpet.glsl](https://github.com/popojan/goban/blob/master/config/shaders/fragment/red_carpet.glsl))
- _height_ - stone height for overlay positioning (0.0 for 2D, 0.85 for 3D)

Available shader variants:
- **Red Carpet** - full ray-traced 3D with reflections
- **Minimal Thin** - simplified 3D rendering
- **Minimal Flat** - flat 3D board
- **Minimal 2D** - pure 2D top-down view
- **[stereo] variants** - anaglyph stereoscopic 3D versions

### GUI

RmlUi GUI definitions organized by language. Each language folder contains:
- [base.rcss](https://github.com/popojan/goban/blob/master/config/gui/base.rcss) - shared base stylesheet
- [goban.rml](https://github.com/popojan/goban/blob/master/config/gui/en/goban.rml) - main game interface (per language)
- [open.rml](https://github.com/popojan/goban/blob/master/config/gui/en/open.rml) - SGF file browser dialog

Supported languages: [en](https://github.com/popojan/goban/tree/master/config/gui/en), [cs](https://github.com/popojan/goban/tree/master/config/gui/cs), [zh](https://github.com/popojan/goban/tree/master/config/gui/zh), [ja](https://github.com/popojan/goban/tree/master/config/gui/ja), [ko](https://github.com/popojan/goban/tree/master/config/gui/ko)
