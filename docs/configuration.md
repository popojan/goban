# Configuration

Red Carpet Goban uses JSON configuration files to define GTP engines, keyboard controls, visual settings, and UI resources.

## Configuration Files

The application uses a hierarchical configuration system where language configs extend a shared base:

- **[config/base.json](https://github.com/popojan/goban/blob/master/config/base.json)** - Shared settings (bots, controls, shaders, sounds, fonts)
- **Language configs** - Extend base.json with GUI paths:
  - [config/en.json](https://github.com/popojan/goban/blob/master/config/en.json) - English
  - [config/cs.json](https://github.com/popojan/goban/blob/master/config/cs.json) - Czech
  - [config/zh.json](https://github.com/popojan/goban/blob/master/config/zh.json) - Chinese
  - [config/ja.json](https://github.com/popojan/goban/blob/master/config/ja.json) - Japanese
  - [config/ko.json](https://github.com/popojan/goban/blob/master/config/ko.json) - Korean

The application loads configuration in this order:
1. Path specified via `-c` / `--config` command line argument
2. Last used config stored in [user.json](user-settings.md)
3. Default: `config/en.json`

**To modify engine configurations, controls, or shaders**: Edit `config/base.json` (applies to all languages).
**To change GUI language**: Edit the specific language config (e.g., `config/en.json`) to point to a different GUI folder.
**To modify GUI layout**: Edit the RML files in `config/gui/<lang>/` (e.g., `config/gui/en/goban.rml`).

## Configuration Sections

- [Humans](#humans) - Human player names
- [Bots](#bots) - GTP engine definitions
- [Controls](#controls) - Keyboard mappings
- [Sounds](#sounds) - Sound effect files
- [Fonts](#fonts) - Font files
- [Shaders](#shaders) - Visual style definitions
- [GUI](#gui) - User interface resources

---

## Humans

Array of human player names available in the player selection dropdowns.

```json
"humans": ["Alice", "Bob", "Human"]
```

These names appear in the GUI and are recorded in SGF game files.

**Default**: `["Human"]`

---

## Bots

GTP engine configurations defined in `config/base.json`. Any GTP-compliant engine can be used.

The bundled configuration includes several predefined engines:
- **GNU Go 3.8** - Included and enabled by default (`main: 1`)
- **KataGo** (multiple models) - Predefined but disabled (`enabled: 0`)
- **Pachi** - Predefined but disabled
- **Zen 6/7** - Predefined but disabled

```json
"bots": [
  {
    "name": "GNU Go 3.8",
    "path": "./engine/gnugo",
    "command": "gnugo",
    "parameters": "--mode gtp --japanese-rules",
    "main": 1
  },
  {
    "name": "KataGo",
    "path": "./engine/katago",
    "command": "katago",
    "parameters": "gtp -model ./models/kata1.bin.gz -config ./default_gtp.cfg",
    "enabled": 0,
    "kibitz": 1
  }
]
```

**To enable a predefined engine**: Install the engine binary in the specified path, then change `"enabled": 0` to `"enabled": 1` in `config/base.json`.

### Bot Attributes

| Attribute | Required | Type | Description | Default |
|-----------|----------|------|-------------|---------|
| `name` | yes | string | Display name in GUI and SGF files | |
| `path` | yes | string | Directory containing the engine executable | |
| `command` | yes | string | Executable name (found in `path` or system PATH) | |
| `parameters` | yes | string | Command line arguments for the engine | |
| `main` | no | 0/1 | Primary engine for game rules and board state | 0 |
| `enabled` | no | 0/1 | Whether this engine is active | 1 |
| `kibitz` | no | 0/1 | Use for move suggestions (Space key) | 0 |
| `messages` | no | array | Rules to parse engine output for display | |

### Engine Roles

- **Main engine** (`main: 1`): Handles game rules, validates moves, tracks board state. Exactly one engine should have this flag.
- **Kibitz engine** (`kibitz: 1`): Provides move suggestions when you press Space. If not set, the main engine is used.

### Message Parsing

The `messages` array defines regex rules to extract and display information from engine output:

```json
"messages": [
  {
    "regex": "^:\\s+T.*--\\s*([A-Z0-9]+)",
    "output": "$1",
    "var": "$primaryMove"
  },
  {
    "regex": "^$primaryMove.*(W\\s+[^\\s]+)",
    "output": "$1"
  }
]
```

### Example Configurations

**GNU Go** (classic engine):
```json
{
  "name": "GNU Go 3.8",
  "command": "gnugo",
  "path": "./engine/gnugo",
  "parameters": "--mode gtp --japanese-rules",
  "main": 1
}
```

**KataGo** (neural network):
```json
{
  "name": "KataGo",
  "command": "katago",
  "path": "./engine/katago",
  "parameters": "gtp -model ./models/model.bin.gz -config ./default_gtp.cfg",
  "enabled": 1,
  "kibitz": 1
}
```

---

## Controls

Maps keyboard keys to commands. Keys are specified as [RmlUi KeyIdentifier](https://github.com/mikke89/RmlUi/blob/master/Include/RmlUi/Core/Input.h) numeric values.

```json
"controls": [
  {"key": 1, "command": "play once"},
  {"key": 81, "command": "quit"},
  {"key": 72, "command": "toggle_analysis_mode"}
]
```

See [Keyboard Shortcuts](keyboard-shortcuts.md) for the complete mapping.

### Available Commands

| Command | Description |
|---------|-------------|
| `play once` | Trigger kibitz/AI move |
| `quit` | Exit application |
| `toggle_fullscreen` | Toggle fullscreen mode |
| `fps` | Toggle FPS display/unlimited mode |
| `animate` | Trigger intro animation |
| `toggle_territory` | Show/hide territory markers |
| `toggle_overlay` | Show/hide coordinate overlay |
| `resign` | Resign current game |
| `pass` | Pass turn |
| `reset camera` | Center camera view |
| `undo move` | Undo last move |
| `pan camera` | End pan mode |
| `rotate camera` | End rotation mode |
| `zoom camera` | End zoom mode |
| `cycle shaders` | Switch to next shader |
| `increase gamma` / `decrease gamma` | Adjust gamma |
| `increase contrast` / `decrease contrast` | Adjust contrast |
| `reset contrast and gamma` | Reset to defaults |
| `increase eof` / `decrease eof` | Adjust stereo eye separation |
| `increase dof` / `decrease dof` | Adjust depth of field |
| `toggle_analysis_mode` | Toggle analysis mode |
| `navigate_start` | Go to game start |
| `navigate_end` | Go to game end |
| `save` | Save current game |
| `load` | Open file browser |

---

## Sounds

Paths to sound effect files (WAV format):

```json
"sounds": {
  "move": "./config/sound/stone.wav",
  "clash": "./config/sound/collision.wav"
}
```

- **move**: Played when a stone is placed
- **clash**: Played on stone collision (capture)

---

## Fonts

Font files for the application:

```json
"fonts": {
  "gui": [
    "./config/fonts/NotoSans-Regular.ttf",
    "./config/fonts/NotoSansCJKsc-Regular.otf"
  ],
  "overlay": "./config/fonts/default-font.ttf"
}
```

- **gui**: Array of fonts for the RmlUi interface. Multiple fonts enable fallback for CJK characters.
- **overlay**: Font used for stone coordinate overlay.

---

## Shaders

GLSL shader configurations for different visual styles:

```json
"shaders": [
  {
    "name": "Red Carpet",
    "vertex": "./config/shaders/vertex/mono.glsl",
    "fragment": "./config/shaders/fragment/red_carpet.glsl",
    "height": 0.85
  },
  {
    "name": "[stereo] Red Carpet",
    "vertex": "./config/shaders/vertex/stereo.glsl",
    "fragment": "./config/shaders/fragment/red_carpet_stereo.glsl",
    "height": 0.85
  }
]
```

### Shader Attributes

| Attribute | Description |
|-----------|-------------|
| `name` | Display name in shader selector |
| `vertex` | Path to vertex shader |
| `fragment` | Path to fragment shader |
| `height` | Stone height for overlay positioning (0.0 for 2D, 0.85 for 3D) |

### Available Shaders

- **Red Carpet** - Full ray-traced 3D
- **Minimal Thin** - Simplified 3D rendering
- **Minimal Flat** - Flat board with 3D stones
- **Minimal 2D** - Flat circular stones (can be tilted)
- **[stereo] variants** - Anaglyph stereoscopic 3D (red/cyan glasses)

---

## GUI

Path to RmlUi GUI definition folder:

```json
"gui": "./config/gui/en"
```

Each language folder contains:
- `goban.rml` - Main game interface layout
- `open.rml` - SGF file browser dialog
- `fonts.rcss` - Font definitions

Shared resources:
- [config/gui/base.rcss](https://github.com/popojan/goban/blob/master/config/gui/base.rcss) - Base stylesheet
- [config/gui/open.rcss](https://github.com/popojan/goban/blob/master/config/gui/open.rcss) - File browser styles

---

## SGF Dialog Settings

Configuration for the SGF file browser:

```json
"sgf_dialog": {
  "game_columns": ["index", "board", "black", "white", "moves", "result"],
  "games_path": "./games"
}
```

- **game_columns**: Columns displayed in the game list
- **games_path**: Default directory for SGF files
