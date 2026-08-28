# Keyboard Shortcuts

Complete reference of keyboard controls. Keys can be customized in the [configuration file](configuration.md#controls).

## Game Controls

| Key | Function |
|-----|----------|
| **Space** | Navigate forward if there is a move ahead; otherwise ask the engine for one (kibitz) |
| **P** | Pass |
| **R** | Resign game |
| **U** | Undo — step back one move |
| **Enter** | Toggle Analysis mode (pause AI-vs-AI matches for review) |

## SGF Navigation

When an SGF game is loaded or during game review:

| Key | Function |
|-----|----------|
| **Space** or **Right Arrow** | Navigate forward one move |
| **Left Arrow** or **Backspace** | Navigate back one move |
| **Home** | Navigate to start of game |
| **End** | Navigate to end of game |
| **Page Up** / **Page Down** | Previous / next game in a loaded SGF collection |

**Space and Right differ at the end of a branch.** With no move ahead, Space
falls through to whatever `controls` binds it to — `play once` by default, which
asks the engine for a move; Right simply does nothing. That is what makes Space a
single "carry on" key. At the end of a *finished* game neither does anything.

Left, Right, Backspace and Space's forward step are handled directly and are
**not** remappable through `controls`. Home, End, Page Up and Page Down are
ordinary bindings like everything else on this page.

**Mouse**: Click on an existing variation marker to follow it, or click on an empty intersection to create a new variation.

## Camera Controls

| Key | Function |
|-----|----------|
| **B** | Zoom to stones (fit all stones in view) |
| **C** | Reset camera to the saved preset (see [User Settings](user-settings.md)) |
| **A** + mouse move | Rotate view |
| **S** + mouse move | Zoom view |
| **D** + mouse move | Pan/drag view |

**Mouse controls**:
- **Right-click drag** - Rotate view
- **Middle-click drag** - Pan view
- **Scroll wheel** - Zoom in/out

## Display Settings

| Key | Function |
|-----|----------|
| **V** | Cycle through shader variants |
| **T** | Toggle territory display |
| **N** | Toggle the last-move marker |
| **M** | Toggle the next-move marker (shows what comes next during review) |
| **O** | Trigger intro animation |
| **X** | Toggle max FPS mode (uncapped vs event-driven) |
| **F** | Toggle fullscreen, on the monitor the window is on |

## Image Adjustments

| Key | Function |
|-----|----------|
| **]** | Increase gamma |
| **[** | Decrease gamma |
| **+** (or **=**) | Increase contrast |
| **-** | Decrease contrast |
| **\\** | Reset contrast and gamma |

## Stereo 3D Settings

For use with anaglyph stereo shaders (red/cyan glasses):

| Key | Function |
|-----|----------|
| **H** | Increase eye separation (eof) |
| **L** | Decrease eye separation (eof) |
| **J** | Increase depth of field (dof) |
| **K** | Decrease depth of field (dof) |

## File and game

These use modifier keys, which the `controls` table gained in order to have them
— every unmodified letter was already spent on the camera and shader controls.

| Key | Function |
|-----|----------|
| **Ctrl+S** | Save the game record now |
| **Ctrl+O** | Open the file browser |
| **Ctrl+N** | New game on the same board size (asks first if there is a game worth keeping) |
| **Ctrl+Z** | Undo — step back one move |
| **Ctrl+B** | Toggle bot-versus-bot play |
| **Ctrl+M** | Show or hide the message log |

`Ctrl+N` runs `clear` rather than `new_game`, because `clear` is the path that
asks before discarding a game in progress.

A binding with modifiers is distinct from the same key without them: **S** still
zooms the camera, and **U** still undoes. See
[Configuration](configuration.md#modifier-keys) for the format.

## Application

| Key | Function |
|-----|----------|
| **Escape** | Quit application |

On a **multi-monitor Wayland session**, fullscreen may open on the wrong output:
Wayland gives an application no way to ask where its own window is. Run with
`--platform x11` if this affects you — see [the option table](README.md#command-line).

## Key Code Reference

Keys are mapped using [RmlUi KeyIdentifier](https://github.com/mikke89/RmlUi/blob/master/Include/RmlUi/Core/Input.h) values. Common codes:

| Code | Key |
|------|-----|
| 1 | Space |
| 12-37 | A-Z |
| 39 | + (OEM_PLUS) |
| 41 | - (OEM_MINUS) |
| 45 | [ (OEM_4) |
| 46 | \\ (OEM_5) |
| 47 | ] (OEM_6) |
| 69 | Backspace |
| 72 | Enter |
| 81 | Escape |
| 86 | Page Up |
| 87 | Page Down |
| 88 | End |
| 89 | Home |
| 90 | Left Arrow |
| 91 | Up Arrow |
| 92 | Right Arrow |
| 93 | Down Arrow |
