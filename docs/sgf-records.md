# SGF Game Records

Red Carpet Goban uses the SGF (Smart Game Format) standard for saving and loading game records.

## Games Folder

By default, games are saved to and loaded from the `games/` directory in the application folder.

```
goban/
├── games/
│   ├── 2024-01-15.sgf              # Current daily session
│   ├── 2024-01-14T18-30-45.sgf     # Archived session
│   └── ...
├── goban
└── ...
```

The path can be configured in [configuration.md](configuration.md):
```json
"sgf_dialog": {
  "games_path": "./games"
}
```

## Auto-Save

Games are automatically saved:
- When a game ends (resignation or double pass)
- When starting a new game (saves the previous game)
- When quitting the application

### File Naming

The **daily session** file uses the format: `YYYY-MM-DD.sgf`

All games played during a day are appended to this file as separate game records. When you use the **Archive** command (Game menu), the daily session is renamed to a timestamped archive file: `YYYY-MM-DDTHH-MM-SS.sgf`

Examples:
- `2024-01-15.sgf` — today's daily session (multi-game file)
- `2024-01-14T18-30-45.sgf` — archived session from yesterday

## Loading Games

### Via Menu

1. Open **Game > Open** (or press the load shortcut)
2. The file browser shows the `games/` folder
3. Games are listed with metadata: board size, players, moves, result
4. Click a game to load it

### Via Command Line

Currently, SGF files can only be loaded through the GUI file browser.

## Navigation

After loading a game, use navigation keys to step through:

| Key | Action |
|-----|--------|
| **Home** | Go to start (empty board / handicap setup) |
| **End** | Go to end of game |
| **Left** / **Backspace** | Back one move |
| **Right** / **Space** | Forward one move |

## Variations

SGF supports game tree variations (alternate lines of play).

### Viewing Variations

When at a position with variations:
- Variation markers appear on the board
- Click a marker to follow that variation
- The main line is typically the first variation

### Creating Variations

During navigation (not at the end of a game):
1. Click on an empty intersection
2. A new variation is created from the current position
3. The new move becomes the main line

### AI Responses in Variations

After creating a variation:
- If an AI is assigned to the responding color, it will play
- Press **Space** to trigger a kibitz suggestion
- Use Analysis mode to prevent automatic AI responses

## SGF Metadata

Games include standard SGF properties:

| Property | Description |
|----------|-------------|
| PB | Black player name |
| PW | White player name |
| SZ | Board size |
| KM | Komi |
| HA | Handicap |
| RE | Result (e.g., "B+5.5", "W+R") |
| DT | Date |

### Result Format

- `B+5.5` - Black wins by 5.5 points
- `W+R` - White wins by resignation
- `B+R` - Black wins by resignation

## Annotations

The application adds annotations to track events:
- Player switches during the game
- AI engine used for moves

These are stored in SGF comment nodes and visible in other SGF editors.

## Compatibility

SGF files created by Red Carpet Goban are compatible with:
- Other Go programs (Sabaki, CGoban, etc.)
- Online Go servers that accept SGF uploads
- SGF editors and viewers

The application can load SGF files from other sources, including:
- Professional game databases
- Online game downloads
- Other Go software exports
