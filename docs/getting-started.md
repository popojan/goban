# Getting Started

## Requirements

- **Operating System**: Windows 7/10+ or Linux
- **Graphics**: OpenGL 3.1+ compatible GPU
- **GTP Engine**: Any GTP-compliant Go engine (GNU Go, KataGo, Pachi, Leela Zero, etc.)

## Installation

### From Bundle

1. Download the full bundle for your platform from [hraj.si/goban](https://hraj.si/goban)
2. Extract the archive
3. The bundle includes GNU Go and is ready to play

### GTP Engine Setup

Red Carpet Goban requires at least one GTP (Go Text Protocol) engine to play. Configure engines in your [configuration file](configuration.md#bots).

**Popular engines:**
- [GNU Go](https://www.gnu.org/software/gnugo/) - Classic engine, moderate strength
- [KataGo](https://github.com/lightvector/KataGo) - Strong neural network engine
- [Pachi](https://pachi.or.cz/) - Monte Carlo engine
- [Leela Zero](https://zero.sjeng.org/) - Neural network engine

## Your First Game

1. **Launch the application**: Run `./goban` from the application directory

2. **The intro animation plays** - The camera swoops down to the board

3. **Place a stone**: Click on any intersection to place a black stone. The AI will respond.

4. **Continue playing**: Alternate placing stones. The AI responds after each human move.

5. **Pass or Resign**: Use the Game menu or press **P** to pass, **R** to resign

6. **Start a new game**: Use the Game menu or click on the board after a game ends

## Basic Controls

| Action | Control |
|--------|---------|
| Place stone | Left-click on intersection |
| Rotate view | Right-click drag, or hold **A** + move mouse |
| Pan view | Middle-click drag, or hold **D** + move mouse |
| Zoom | Scroll wheel, or hold **S** + move mouse |
| Center camera | Press **C** |

See [Keyboard Shortcuts](keyboard-shortcuts.md) for the complete reference.

## Game Setup

Access game settings through the **Game** menu:

- **New Game** - Start a fresh game
- **Board Size** - Choose 9x9, 13x13, or 19x19
- **Komi** - Set the komi (compensation for white)
- **Handicap** - Set handicap stones (2-9)

### Player Selection

Use the dropdown menus at the top of the screen to select who plays Black and White:
- **Human** - You control this color
- **Engine name** - The AI plays this color

Common setups:
- Human vs Engine (default) - You play Black against the AI
- Engine vs Human - AI plays Black, you play White
- Human vs Human - Two humans play (hotseat)
- Engine vs Engine - Watch two AIs play (use Analysis mode to pause)

## Analysis Mode

Analysis mode enables free exploration of positions. It is set automatically when loading SGF files and can be toggled manually at any time.

**Toggle Analysis Mode**: Press **Enter** or use the Game menu

When Analysis mode is active:
- You can play either color by clicking on the board
- The engine responds with one move (if assigned to the responding color)
- Bot-vs-bot games pause automatically
- Undo removes a single move
- Press **Space** to trigger a kibitz suggestion without playing

When Match mode is active:
- Strict turn alternation with assigned player roles
- Engine responds automatically after your move
- Undo removes two moves (your move + engine's automatic response)

See [Game Modes](game-modes.md) for detailed use cases and behavior.

## Loading SGF Files

1. Use **Game > Open** or the file browser
2. Navigate to the `games/` folder (or any folder with .sgf files)
3. Select a game to load
4. Use navigation keys to step through the game

See [SGF Game Records](sgf-records.md) for more details.

## Visual Settings

### Shader Variants

Press **V** to cycle through visual styles:
- **Red Carpet** - Full ray-traced 3D (default)
- **Minimal Thin** - Simplified 3D rendering
- **Minimal Flat** - Flat board with 3D stones
- **Minimal 2D** - Flat circular stones (can be tilted)
- **[stereo] variants** - Anaglyph 3D (requires red/cyan glasses)

### Display Toggles

- **T** - Toggle territory display
- **N** - Toggle coordinate overlay
- **F** - Toggle fullscreen
- **X** - Toggle max FPS mode

## Tips

- **Save your camera position**: After adjusting the view, use **Menu > Camera > Save** to remember it
- **Kibitz move**: Press **Space** to see the AI's suggested move
- **Undo**: Press **U** to undo the last move (when playing against AI)
- **Quick new game**: Click anywhere on the board after a game ends
