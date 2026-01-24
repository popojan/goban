# Keyboard Shortcuts

Complete reference of keyboard controls. Keys can be customized in the [configuration file](configuration.md#controls).

## Game Controls

| Key | Function |
|-----|----------|
| **Space** | Play AI-suggested move (kibitz) / trigger AI move in Analysis mode |
| **P** | Pass |
| **R** | Resign game |
| **U** | Undo last move |
| **Enter** | Toggle Analysis mode (pause AI-vs-AI matches for review) |

## SGF Navigation

When an SGF game is loaded or during game review:

| Key | Function |
|-----|----------|
| **Space** or **Right Arrow** | Navigate forward one move |
| **Left Arrow** or **Backspace** | Navigate back one move |
| **Home** | Navigate to start of game |
| **End** | Navigate to end of game |

**Mouse**: Click on an existing variation marker to follow it, or click on an empty intersection to create a new variation.

## Camera Controls

| Key | Function |
|-----|----------|
| **C** | Center camera (top-down view) |
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
| **N** | Toggle coordinate overlay |
| **O** | Trigger intro animation |
| **X** | Toggle max FPS mode (uncapped vs event-driven) |
| **F** | Toggle fullscreen |

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

## Application

| Key | Function |
|-----|----------|
| **Escape** | Quit application |

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
| 88 | End |
| 89 | Home |
| 90 | Left Arrow |
| 91 | Up Arrow |
| 92 | Right Arrow |
| 93 | Down Arrow |
