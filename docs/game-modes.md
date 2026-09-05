# Game Modes

Red Carpet Goban has three game modes that determine how player interaction
works: **Match**, **Explore** and **Tsumego**. They are one setting, not a mode
plus a flag — see `docs/adr/0015-tsumego-is-a-game-mode.md` for why that matters.

The user switches between the first two from **Review > Game mode**, with one
exception: loading a finished SGF (one carrying a result) selects Explore by
itself, since there is no match left to play. **Tsumego is not chosen from the
menu at all** — it follows the record you open, and the menu only reports it.

## Match Mode

**The default for new games.** Strict match semantics with assigned player roles.

### Behavior

- Players alternate turns according to their assigned color
- If an engine is assigned to a color, it responds automatically after the opponent moves
- **Undo** removes one move (consistent with Explore mode)
- Engine-vs-engine games run continuously until finished

### When Active

- Starting a new game (Game > New, or board click after game ends)
- Explicitly switching from Explore mode via the mode select or the Enter key

---

## Explore Mode

**The default after loading a finished SGF game.** Free exploration without strict turn enforcement.

### Behavior

- Human can play either color by clicking on the board
- After playing a move, **the configured kibitz engine replies** — whichever
  color is to move, and regardless of who is assigned to it. Explore mode does
  not consult the color assignment at all.
- **Undo** removes one move
- Navigation (Home/End/Left/Right) works freely

### When Active

- After loading a finished SGF game (has result — resign or score)
- Explicitly switching from Match mode via the mode select or the Enter key

---

## Tsumego Mode

**Set by opening a problem, and by nothing else.** The record *is* the opponent.

### Behavior

- The recorded main line is the answer, and it defines what "correct" means.
  Nothing you play may promote itself over it — a variation stays a variation.
- The record answers, on the solution path and on a refuted one alike. A move
  the problem marks BM is a wrong move you may play out to see it punished.
- The verdict is **Solved!** or **Wrong!**; a solved position refuses further
  moves and refuses Kibitz.
- Player assignment is not consulted, so unlike Explore this mode is never
  refused. Resign is refused instead: a puzzle is not a game to concede.
- The live evaluation is suppressed entirely — the engine's first suggestion
  *is* the answer (ADR-0007). Your toggles are left alone and come back with the
  next ordinary game.
- Branches are not copied into the daily session document.

### When Active

- **File > Load game...** with the Tsumego checkbox armed (it arms itself when
  the file looks like a problem), or the `load_tsumego` command
- Stepping through a collection with Previous/Next game keeps it
- A restored session that was in tsumego mode

It is left by starting a new game (**Clear**) or loading an ordinary record. The
mode select shows *Tsumego* greyed while it is active, and its Tsumego option is
greyed at all other times: offering Match mid-puzzle would re-arm promotion and
let your next attempt overwrite the recorded answer.

---

## Switching Modes

| Trigger | Result |
|---------|--------|
| **Review > Game mode** select, or the **Enter** key | Match <-> Explore |
| `game_mode match\|explore\|tsumego` | Named directly; Tsumego is refused |
| **New Game** (clear board) | Resets to Match |
| **Load finished SGF** (has result) | Sets Explore |
| **Load unfinished SGF** (no result, e.g. session resume) | Sets Match |
| **Load with the Tsumego toggle**, or `load_tsumego` | Sets Tsumego |

The select is greyed while a puzzle is open, and its Tsumego option is greyed
while one is not, so no route — menu, key or script — can reach a mode the
control refuses.

Explore mode is **not available when both players are human**, and the refusal
says so. The reason is the reply, not its absence: explore mode answers every
move with the kibitz engine, so entering it with two humans would quietly turn
the game into human-versus-engine.

This costs nothing, because **Kibitz does not need explore mode**. The Kibitz
button and the Space key ask the engine for a move in an ordinary match, with
two humans, at any time. Explore mode is for handing every reply to the engine;
Kibitz is for asking once.

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

### 2b. Ask the engine what it would play, mid-game

- **Mode**: Match (stays Match)
- Undo back to the position you want to reconsider
- Press Kibitz (or Space) — the engine plays **one** move at the cursor, as a
  variation, for whichever colour is to move there
- The game stays **paused**: asking once is not asking to resume the match
- If it is now the engine's turn, **Start** lights up; press it to hand the game
  back to the engine and continue from the new line

The difference from clicking a point yourself is only the reply: a click resumes
the match and the engine answers automatically, while Kibitz plays the one move
and stops. That is the same distinction as the one between the two modes, at the
scale of a single move.

### 3. Reviewing a loaded SGF

- **Mode**: Explore (set automatically on load)
- Navigate with Home/End/Left/Right
- Click on the board to create variations and explore alternatives
- Engine responds to your exploratory moves if assigned

### 4. Resuming a match from a loaded SGF position

- **Mode**: Switch to Match explicitly (mode select, or Enter key)
- Load the SGF (starts in Explore mode)
- Navigate to the desired position
- Switch to Match — the position becomes the starting point
- Play continues as a match from that position

### 5. Observing engine-vs-engine play

- **Mode**: Match
- Assign engines to both colors, start the game
- Engines play automatically
- To pause and review: switch to Explore (Enter key)
- To resume: switch back to Match

### 6. Getting AI analysis of a position

- **Mode**: Explore
- Navigate to or set up the position of interest
- Click to play a move — engine responds with its evaluation
- Press Space to trigger a kibitz suggestion without playing

---

## Interaction with Navigation

Navigation (Home/End/Left/Right) is available in every mode:
- **Match mode**: Navigation is blocked while the engine is thinking, and
  blocked outright in a bot-versus-bot match — see below
- **Explore mode**: Navigation is always available (engine auto-play is paused)

In a **bot-versus-bot match outside explore mode** the human is a spectator:
navigation, Undo, Pass, Resign and Kibitz are all refused, by the keys exactly
as by the greyed-out buttons (ADR-0005). Switching to explore mode is the
supported way to step in, and it pauses the match.

Creating a new variation (clicking a new point during navigation) works
identically in Match and Explore — the difference is only in whether the engine
responds automatically (Match) or on request (Explore). In Tsumego the variation
is never promoted over the recorded answer, which is the one place the three
modes differ about what a click *means*.

---

## Interaction with Undo

Undo always removes exactly one move (navigates back one step), regardless of mode. This is consistent and predictable — the user decides how far to go back.

After undoing, the engine will not auto-respond at a historical position. To try a different move: undo as many times as needed, then click a new point.
