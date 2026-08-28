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
| `path` | yes | string | Directory containing the engine executable, **relative to the application folder** | |
| `command` | yes | string | Executable name (found in `path` or system PATH) | |
| `parameters` | yes | string | Command line arguments for the engine — see *Paths in parameters* below | |
| `main` | no | 0/1 | Primary engine ("coach"): holds the authoritative board and does the scoring | 0 |
| `enabled` | no | 0/1 | Whether this engine is loaded at all | 1 |
| `kibitz` | no | 0/1 | Use for move suggestions (Space key) and analysis-mode replies | 0 |
| `timeout_ms` | no | int | Longest to wait for a reply to one GTP command | 300000 (5 min) |
| `scoring_timeout_ms` | no | int | Longest to wait when *scoring* a finished game | 30000 (30 s) |
| `messages` | no | array | Regex rules to parse engine output for display | |

`timeout_ms` is the backstop for an engine that stops answering. The default is
deliberately generous — a strong engine at long time settings, or KataGo loading
network weights, can legitimately take tens of seconds. A negative value waits
forever.

`scoring_timeout_ms` is deliberately much shorter. Nothing about counting a
finished position needs minutes, and when it *does* take minutes the engine is
wedged or sitting at the wrong position — the case that should fail rather than
freeze the game on "Calculating score…". It never *raises* the limit: an engine
capped at 5s by `timeout_ms` still gets 5s for scoring, and it overrides a
negative `timeout_ms`, which is the whole point of having it.

### Paths in parameters

A `path` that does not exist is **not** fatal. It only ever supplied the working
directory, so if `command` names something the system can resolve from `PATH`,
the engine starts from the application folder instead and the log says so. That
is how the stock `"path": "./engine/gnugo"` with `"command": "gnugo"` works on a
machine where GNU Go came from the distribution.

The engine runs **with its working directory set to `path`**, so a relative path
in `parameters` is resolved by the engine against the engine's own folder — not
against the application folder that `path` itself is relative to.

With the stock layout the distinction is invisible, because the model sits inside
the engine folder:

```json
"path": "./engine/katago",
"parameters": "gtp -model ./models/kata1.bin.gz -config ./default_gtp.cfg"
```

`./models/kata1.bin.gz` means `./engine/katago/models/kata1.bin.gz`, which is
where it is.

It stops being invisible as soon as the engine lives somewhere else. Writing
`-model ../user_folder/weights/kata1.bin.gz` alongside
`"path": "../user_folder/katago"` reads naturally as "relative to goban", but the
engine resolves it against its own folder and looks for
`../user_folder/katago/../user_folder/weights/…` — the folder name doubled.

Goban corrects this case for you: **an argument that does not exist relative to
the engine's folder, but does exist relative to the application folder, is
rewritten to an absolute path**, and the substitution is logged. An argument that
resolves inside the engine folder is passed exactly as written, so existing
configurations are unaffected.

If an engine fails to start, the log now says which folder and which program,
with the system's own reason:

```
Engine [katago] folder does not exist: '../user_folder/katago' (paths are relative to the application folder)
cannot start './katago' in '../user_folder/katago': No such file or directory
```

**Absolute paths always work**, in both `path` and `parameters`, and are the
simplest option for an engine installed outside the application folder.

### Engine Roles

- **Main engine** (`main: 1`), called the *coach* in the code: every move is
  played into it and it does the scoring. Exactly one engine should carry the
  flag — the first enabled one that does wins.

  **If the engine carrying `main` fails to start, another is promoted** — an
  arbitrary one, since engines load in parallel and it is whichever finished
  first. The log warns when this happens. It matters more than it sounds: the
  coach decides legality and scoring, so an engine that cannot count ends up
  refereeing. If games start ending on "Calculating score…", check the log for
  that warning first.

- **Kibitz engine** (`kibitz: 1`): provides move suggestions when you press
  Space, and plays every reply in analysis mode. If unset, the coach is used.

All enabled engines are kept in sync at the same position regardless of role, so
you can switch a player to any of them mid-game.

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

### Modifier keys

An entry may also carry `ctrl`, `shift` and `alt` booleans, in any combination:

```json
"controls": [
  {"key": 30, "command": "zoom camera"},
  {"key": 30, "ctrl": true, "command": "save"},
  {"key": 26, "ctrl": true, "shift": true, "command": "archive"}
]
```

Three rules:

- **An entry with no modifiers means the unmodified key**, which is what every
  binding meant before modifiers existed. Existing `controls` arrays keep working
  unchanged.
- **Modifiers must match exactly.** `S` and `Ctrl+S` are different bindings on
  the same key, and neither falls back to the other — otherwise every accelerator
  would also fire its unmodified twin.
- **Navigation keys ignore bindings entirely when modified.** Left, Right,
  Backspace and Space's forward step are handled before the table is consulted,
  but only unmodified, so `Ctrl+Left` is free to be bound.

Modifiers exist because there was nowhere left to put an accelerator: every
unmodified letter was already spent on the camera and shader controls, so `save`,
`load`, `clear` and `toggle ai vs ai` were registered commands that no key could
reach.

See [Keyboard Shortcuts](keyboard-shortcuts.md) for the complete mapping.

### Available Commands

The authoritative list is the `help` command, which prints every registered name
with its arguments. The ones worth binding to a key:

| Command | Description |
|---------|-------------|
| `play once` | Ask the kibitz engine for a move (Space) |
| `genmove` | Make the engine move for the colour to play |
| `start` | Hand the turn to the engine |
| `pass` | Pass turn |
| `resign` | Resign current game |
| `clear` | Start a new game on the same board size (asks first) |
| `undo move` | Step back one move |
| `navigate_start` / `navigate_end` | Go to the start / end of the game |
| `navigate_back` / `navigate_forward` | Step one move back / forward |
| `prev_game` / `next_game` | Cycle games within a loaded SGF collection |
| `save` | Save the current game |
| `archive` | Close the daily session file and start a new one |
| `load` | Open the file browser |
| `report_bug` | Write a replayable script of the recent session |
| `toggle_analysis_mode` | Toggle analysis mode |
| `toggle ai vs ai` | Let both engines play each other |
| `toggle_territory` | Show/hide territory markers |
| `toggle_last_move_overlay` / `toggle_next_move_overlay` | Show/hide the move markers |
| `toggle_fullscreen` | Toggle fullscreen mode |
| `toggle_fps` | Toggle uncapped rendering (and the FPS counter) |
| `toggle_sound` | Toggle stone sounds |
| `quit` | Exit application |
| `animate` | Trigger the intro animation |
| `reset camera` | Return to the saved camera preset |
| `save camera` / `delete camera` | Store / remove the preset (see [User Settings](user-settings.md)) |
| `zoom stones` | Frame all stones on screen |
| `pan camera` / `rotate camera` / `zoom camera` | Hold-to-drag camera modes |
| `cycle shaders` | Switch to the next shader |
| `increase gamma` / `decrease gamma` | Adjust gamma |
| `increase contrast` / `decrease contrast` | Adjust contrast |
| `reset contrast and gamma` | Reset both to defaults |
| `increase eof` / `decrease eof` | Adjust stereo eye separation |
| `increase dof` / `decrease dof` | Adjust depth of field |

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
