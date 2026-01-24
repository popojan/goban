# Game Modes

Red Carpet Goban has two game modes that determine how player interaction works. The mode is always explicit — switched only by the user, never automatically.

## Match Mode

**The default for new games.** Strict match semantics with assigned player roles.

### Behavior

- Players alternate turns according to their assigned color
- If an engine is assigned to a color, it responds automatically after the opponent moves
- **Undo** removes one move (consistent with Analysis mode)
- Engine-vs-engine games run continuously until finished

### When Active

- Starting a new game (Game > New, or board click after game ends)
- Explicitly switching from Analysis mode via menu/Enter key

---

## Analysis Mode

**The default after loading a finished SGF game.** Free exploration without strict turn enforcement.

### Behavior

- Human can play either color by clicking on the board
- After playing a move, the engine assigned to the responding color replies
- **Undo** removes one move
- Navigation (Home/End/Left/Right) works freely

### When Active

- After loading a finished SGF game (has result — resign or score)
- Explicitly switching from Match mode via menu/Enter key

---

## Switching Modes

| Trigger | Result |
|---------|--------|
| **Enter** key or Game menu toggle | Toggles between Match and Analysis |
| **New Game** (clear board) | Resets to Match |
| **Load finished SGF** (has result) | Sets Analysis |
| **Load unfinished SGF** (no result, e.g. session resume) | Sets Match |

Analysis mode is not available when both players are human (no AI to respond).

---

## Use Cases

### 1. Playing a match against an engine

- **Mode**: Match
- Start a new game, assign engine to one color
- Engine responds automatically after each human move
- Undo removes one move at a time

### 2. Undo and try a different move in a match

- **Mode**: Match (stays Match)
- Press U twice — removes engine's response, then your move
- Click a different point — engine responds to your new move
- No mode switch occurs; the match continues naturally

### 3. Reviewing a loaded SGF

- **Mode**: Analysis (set automatically on load)
- Navigate with Home/End/Left/Right
- Click on the board to create variations and explore alternatives
- Engine responds to your exploratory moves if assigned

### 4. Resuming a match from a loaded SGF position

- **Mode**: Switch to Match explicitly (Enter key)
- Load the SGF (starts in Analysis mode)
- Navigate to the desired position
- Toggle to Match mode — the position becomes the starting point
- Play continues as a match from that position

### 5. Observing engine-vs-engine play

- **Mode**: Match
- Assign engines to both colors, start the game
- Engines play automatically
- To pause and review: switch to Analysis mode (Enter key)
- To resume: switch back to Match mode

### 6. Getting AI analysis of a position

- **Mode**: Analysis
- Navigate to or set up the position of interest
- Click to play a move — engine responds with its evaluation
- Press Space to trigger a kibitz suggestion without playing

---

## Interaction with Navigation

Navigation (Home/End/Left/Right) is available in both modes:
- **Match mode**: Navigation is blocked while the engine is thinking
- **Analysis mode**: Navigation is always available (engine auto-play is paused)

Creating a new variation (clicking a new point during navigation) works identically in both modes — the difference is only in whether the engine responds automatically (Match) or on request (Analysis).

---

## Interaction with Undo

Undo always removes exactly one move (navigates back one step), regardless of mode. This is consistent and predictable — the user decides how far to go back.

After undoing, the engine will not auto-respond at a historical position. To try a different move: undo as many times as needed, then click a new point.
