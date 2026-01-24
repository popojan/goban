# User Settings

The application stores user preferences in `user.json` in the working directory. This file is created automatically and remembers settings between sessions.

## File Location

- **Working directory**: `./user.json`
- Created on first settings change
- Delete to reset all preferences

## Settings Overview

### Automatically Saved

These settings are saved immediately when changed:

| Setting | Trigger | Description |
|---------|---------|-------------|
| `last_config` | On startup | Path to the configuration file used; restored on next launch |
| `fullscreen` | **F** key | Fullscreen window state |
| `shader.name` | **V** key or dropdown | Selected shader variant name |

### Saved on Explicit Action

These settings are saved when using **Menu > Camera > Save**:

| Setting | Description |
|---------|-------------|
| `camera.rotation` | Camera view angle (quaternion x, y, z, w) |
| `camera.translation` | Camera position including zoom level (x, y, z) |
| `shader.gamma` | Gamma adjustment (**]** / **[** keys) |
| `shader.contrast` | Contrast adjustment (**+** / **-** keys) |
| `shader.eof` | Eye offset factor for stereo shaders |
| `shader.dof` | Depth of field parameter |

### Not Persisted

The following settings are **not** saved between sessions:

- Board size, komi, handicap (reset to defaults on new game)
- Player selections (human vs engine assignments)
- Window size and position
- Current game state

## Example user.json

```json
{
  "camera": {
    "rotation": {
      "w": 0.0,
      "x": -0.886,
      "y": 0.464,
      "z": 0.0
    },
    "translation": {
      "x": 0.077,
      "y": 0.273,
      "z": -0.432
    }
  },
  "fullscreen": false,
  "last_config": "./config/en.json",
  "shader": {
    "contrast": 0.0,
    "dof": 0.01,
    "eof": 0.025,
    "gamma": 1.0,
    "name": "Red Carpet"
  }
}
```

## Managing Settings

### Save Camera Position

After adjusting the view to your liking:

1. Open **Menu > Camera**
2. Click **Save**

The camera position and shader adjustments are now saved.

### Reset to Defaults

To reset all user settings:

1. Open **Menu > Camera**
2. Click **Delete**

Or manually delete `user.json` from the application directory.

### Change Default Language

The application remembers which language configuration was last used. To change:

1. Start with a different config: `./goban -c config/zh.json`
2. The choice is saved to `last_config` automatically
3. Next time, `./goban` will use Chinese

Or edit `user.json` directly:
```json
"last_config": "./config/ja.json"
```
