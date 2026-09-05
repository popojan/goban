# ADR-0016: The Review menu holds no state — prisoners go on the board, and the analysis control names its engine

**Status:** Accepted — implemented 2026-09-05
**Date:** 2026-09-05

## Context

ADR-0015 renamed the Analyze menu to Review and left two things in it that had
been noticed and not addressed: a *Prisoners* readout, and an *Evaluation*
toggle under a group headed *Engine*. Neither is a review tool.

The prisoner counts turned out to be in **three** places at once:

- two labels in that menu;
- two hidden corner elements, `#cntBlack` and `#cntWhite`, which
  `syncPrisonerLabels()` still wrote to on every frame — `base.rcss` said
  `/* Bottom status area - prisoner counts hidden (now in Analyze menu) */` and
  set `display: none` rather than deleting them;
- the stones in the bowls, drawn by the shader.

Only the third is on the board, and only **two of the six shipped shaders draw
bowls at all**: `partial/scene/red.glsl` is the one scene that includes
`bowl_stones.glsl`, so Red Carpet and its stereo twin show the pile and Minimal
Thin, Flat, 2D and stereo Thin show nothing. On four of six shaders the captures
were invisible outside a menu.

The move into the menu predates ADR-0012, which decided the opposite: a value
that is continuously true belongs on the board, not in a panel over it.

The evaluation control had a smaller fault. `AnalysisState` has five values
(Disabled / Starting / Unavailable / Yielded / Running) and the menu showed none
of them; on the stock GNU Go configuration the item is greyed forever with no
explanation, and nothing anywhere says *which* engine would answer — the same
anonymity ADR-0012 left open about the wait indicator.

## Decision

**The prisoner counts are drawn on the board's right margin, by the glyph pass.**

- **The right edge, because it is the only free one.** Coordinates are pinned to
  top and left; the bottom row is already shared three ways by the recommended
  pass (left), the readout (centre, user-aligned) and the wait clock (right).
- **The ink is the colour of the stones *counted*** — a black number counts black
  stones taken, which is what sits in White's bowl. It survives a stereo shader,
  where `GobanOverlay::eyeInk()` flattens every label to its own brightness and
  black against white is exactly a brightness difference.
- **Nothing until a stone has been taken; then both counts, a zero included.**
  Two separate rules. Silent at first because two noughts standing in the margin
  from move one are the bar resting at 50% that ADR-0007 decision 12 rejects.
  Both afterwards because a lone digit with nothing beside it reads as a fault:
  the colour convention is legible only as a contrast, and one number cannot show
  one.
- **`PrisonerMode` is `Auto` / `Always` / `Never`**, sticky, on the `PointerMode`
  precedent — `Auto` means "where the real thing is missing", which here is a
  shader with no bowls. It is offered as a select in View → Overlay beside the
  other things drawn on the wood, and as the `prisoners` command.
- **`"bowls": 1` is declared in the shader's config entry**, resolved by
  `GobanShader::resolveShader()` beside `stereo`, `height` and `annotations` —
  ADR-0011's rule, that an appearance fact the CPU must act on is declared rather
  than inferred from GLSL.

**The evaluation toggle becomes a two-option select whose "on" option carries the
engine's name.** The group header stays the constant *Analysis*: a heading that
changed with the configuration would read as two different menus. When nobody
claimed the role, or the engine that did cannot analyse, the option says
*unavailable* instead of naming something that cannot answer — and
`UiActions::evaluation` greys the whole control in exactly those cases, so the
word explains a refusal rather than making a false offer.

## Consequences

- `syncPrisonerLabels()`, `templatePrisoners*`, `#grpPlayers`, `#cntBlack`,
  `#cntWhite` and their `display: none` rules are all gone. One representation on
  the board, one in the bowls where the shader has them, and nothing in a menu.
- The counts need no translation, which is what makes an unlabelled pair
  acceptable: they are digits, and the overlay font is Roboto — 98 glyphs, ASCII
  only — so a labelled margin readout could not have been written in ja, ko or
  zh at all.
- **A zoomed-in camera can put them out of frame**, as it can the readout and the
  clock. Accepted, on the same terms.
- The analysis select is the *shape* of an engine picker with exactly one engine
  in it. That is deliberate: the role is resolved once from the configuration and
  there is no second candidate to choose (ADR-0007 decision 2), so this changes
  no ownership — but it can grow a picker without changing shape.
- **If the bowls ever become a user setting**, `GobanShader::drawsBowls()` must
  follow the live value rather than the config entry, or switching them off would
  take the prisoners away entirely: no pile, and `Auto` still believing there is
  one. See `backlog/shader-parameter-editor.md`.

## Alternatives rejected

**Move the labels to the Game menu, beside Komi and Handicap.** The smallest
change, and it keeps a translated, labelled reading. But it leaves continuously
true state inside a menu you have to open, which is the thing ADR-0012 decided
against, and it would have been the third copy rather than the first.

**Delete them and rely on the bowls alone.** Removes code rather than moving it,
and the pile is the diegetic answer. Rejected because four of six shaders have no
bowls, and because a pile saturating at `maxCaptured = 91` is not a reading — you
would count stones.

**Give the Minimal shaders bowls too, and delete every label.** One
representation everywhere, and the honest fix if the goal were only consistency.
Rejected because those scenes are minimal on purpose.

**Draw the counts always, with no mode and no shader flag.** Two mechanisms
fewer. Rejected because on Red Carpet the number would sit beside a bowl already
showing the same thing — the redundancy this ADR exists to remove.

**Name the *group* after the engine ("KataGo") instead of the option.** Tried on
paper and dropped: the group header would then read differently depending on what
was installed, and differently again when nothing was, which is three headings for
one menu. The constant noun with a varying value under it is how every other
select in that menu already behaves.
