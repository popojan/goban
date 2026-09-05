# User Settings

The application stores user preferences in `user.json` in the working directory.
The file is created automatically and remembers your setup between sessions —
including, since the session-restore work, the game you were in the middle of.

## File Location

- **Working directory**: `./user.json`
- Created on the first settings change
- Delete it to reset every preference
- Not tracked in the repository, and nothing ships one: it is yours, and it holds
  local paths and your language choice. Defaults that ship live in
  `config/base.json` — see [The default camera](#the-default-camera).

Writes are atomic. The file is written to `user.json.tmp` and renamed over the
target, so an interrupted save cannot leave a truncated file behind — losing
every preference at once, which is what the previous truncate-then-write did.

A scripted run never touches it: `--script` redirects persistence to
`scenario-user.json`, and `--user-settings <file>` redirects it anywhere you
like. See [Testing](testing.md).

## What is saved

### Saved immediately, when the setting changes

| Key | Trigger | Description |
|---|---|---|
| `last_config` | Language menu, or `-c` on the command line | Configuration file used; restored on next launch |
| `fullscreen` | **F** key | Fullscreen window state |
| `sound_enabled` | Sound toggle | Stone-placement sound |
| `shader.name` | **V** key or the shader dropdown | Selected shader variant |
| `game.board_size`, `game.handicap` | Whenever a new game starts — which the board-size and handicap dropdowns both do | |
| `game.komi` | Komi dropdown | |
| `game.black_player`, `game.white_player` | Player dropdowns | Who plays each colour, by name |

The `game` block is what the *next* fresh start uses — board size, komi, handicap
and the two player assignments are **all remembered**, so the application comes
back configured the way you left it. Starting a new game also clears the
`session` block below: you asked for a fresh board, so there is nothing to
restore.

### Saved on **Menu > Camera > Save**

| Key | Description |
|---|---|
| `camera.rotation` | View angle, as a quaternion (x, y, z, w) |
| `camera.pan` | Look-at point on the board plane (x, y) |
| `camera.distance` | Distance from the camera to that point |
| `shader.gamma` | Gamma adjustment (**]** / **[**) |
| `shader.contrast` | Contrast adjustment (**+** / **-**) |
| `shader.eof` | Eye offset factor, for the stereo shaders (**H** / **L**) |
| `shader.dof` | Depth of field (**J** / **K**) |

This is the *preset*, restored by **Menu > Camera > Reset** (**C**). It is
separate from the camera you happen to be looking through — see below.

### Saved on exit

| Key | Description |
|---|---|
| `camera_current` | Where the camera actually was when you quit, in the same format as `camera` |
| `last_sgf_path` | The SGF that was open |
| `start_fresh` | Set when you cleared the board, so the next launch does not reload a game |
| `session.file` | SGF file to restore |
| `session.game_index` | Which game within that file (a daily session file holds many) |
| `session.tree_path_length` | Navigation depth to restore |
| `session.tree_path` | Branch choices, recorded **only at multi-child nodes** |
| `session.is_external` | Whether the file was an external SGF rather than the daily session |
| `session.game_mode` | `match`, `explore` or `tsumego`. Replaces the `tsumego_mode` / `analysis_mode` pair, which is still read once for migration |

On the next launch, startup peeks at `session.file` for the board size, renders
the board immediately, and queues the tree-path navigation so it runs on the game
thread as soon as the first engine is up. If the file has gone missing the
session state is cleared and the application falls back to `last_sgf_path`, then
to today's daily session file.

### Not persisted

- Window size and position
- Which shader *adjustments* are live, unless you saved the camera preset

## Example user.json

```json
{
  "camera": {
    "distance": 3.1,
    "pan": { "x": 0.0, "y": -0.2 },
    "rotation": { "w": 0.0, "x": -0.9, "y": 0.5, "z": 0.0 }
  },
  "camera_current": {
    "distance": 3.1,
    "pan": { "x": 0.0, "y": -0.2 },
    "rotation": { "w": 0.0, "x": -0.874, "y": 0.486, "z": 0.0 }
  },
  "fullscreen": false,
  "game": {
    "black_player": "GNU Go 3.8",
    "board_size": 13,
    "handicap": 0,
    "komi": 7.5,
    "white_player": "Katago #kata9x9 b18"
  },
  "last_config": "./config/cs.json",
  "last_sgf_path": "./games/2026-08-12.sgf",
  "session": {
    "file": "./games/2026-08-12.sgf",
    "game_index": 1,
    "game_mode": "match",
    "is_external": false,
    "tree_path": [],
    "tree_path_length": 96
  },
  "shader": {
    "contrast": 0.0,
    "dof": 0.0925,
    "eof": 0.0725,
    "gamma": 1.0,
    "name": "Minimal Thin"
  },
  "sound_enabled": true,
  "start_fresh": false
}
```

Every key is optional. A missing one falls back to its default, so a hand-edited
file with only `last_config` in it is perfectly valid.

## Managing Settings

### Save the camera position

Adjust the view, then **Menu > Camera > Save**. The camera *and* the four shader
adjustments are written to the preset.

### Reset to defaults

**Menu > Camera > Delete** removes `user.json` entirely and returns the camera to
the default in `config/base.json`. That resets every preference in this document,
not just the camera. Deleting the file by hand does the same thing.

### The default camera

The view a fresh install opens on — and what **Camera > Reset** returns to when
you have not saved a preset of your own — is the `camera` block at the top of
`config/base.json`:

```json
"camera": {
  "distance": 3.1,
  "pan": { "x": 0.0, "y": -0.2 },
  "rotation": { "w": 0.0, "x": -0.9, "y": 0.5, "z": 0.0 }
}
```

Edit it to change the opening view for every new installation. It lives there,
rather than in `user.json`, because `user.json` is the file the application
rewrites on every settings change: a default kept in it cannot be shipped without
also shipping whatever the last session happened to leave behind.

The camera is resolved most-specific-first — where you left it, then your saved
preset, then this.

### Change the default language

The application remembers the configuration last used:

1. Start with a different config: `./goban -c config/zh.json`
2. The choice is saved to `last_config`
3. Next time, plain `./goban` uses it

Or edit the file directly:

```json
"last_config": "./config/ja.json"
```
