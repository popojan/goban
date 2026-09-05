# ADR-0017: A tunable shader parameter is declared in the configuration; the GLSL declares only the uniform

**Status:** Accepted — M1 implemented 2026-09-05
**Date:** 2026-09-05

## Context

`backlog/shader-parameter-editor.md` (2026-01-26) proposes exposing shader
parameters as runtime-adjustable uniforms with generated UI controls. It picks
**comment metadata in the GLSL** as the authoring format —

```glsl
uniform float tweak_f_ambient; // range(0,1) default(0.3) label("Ambient")
```

— and rejects external JSON as *"separated from shader, can get out of sync"*.

The immediate request that reopens this (2026-09-05) is narrower than the
backlog's five phases: **the bowls and lids should be switchable from the GUI**,
for taste, for frame rate, or because a player prefers the margin counts. That
is one boolean, and it is the slice that decides the format for everything after
it.

Since the entry was written, **ADR-0011 answered the same question the other
way** for the move-quality palette: an appearance value the *CPU* must act on is
declared in the shader's JSON config entry, because a `const` in a fragment
shader is unreachable from a pipeline that never runs the fragment shader.
`GobanShader::resolveShader()` now reads `stereo`, `height`, `annotations` and
`bowls` from the entry. That is four instances of the rejected option against
zero of the chosen one.

Two distinctions have to be drawn before the format question can be answered
honestly, because the backlog entry draws neither and both change the shape of
the answer.

**Calibration is not appearance.** `gamma`, `contrast`, `eof` and `dof` are
already uniforms, already set live by command, already persisted. They look like
four working instances of this feature and they are not: CLAUDE.md places them
deliberately elsewhere — *"they describe the screen and the glasses in front of
it"* — which is why they live **globally** in `user.json`'s `shader` block
rather than per shader. A player who calibrates their monitor's gamma has not
expressed an opinion about the Red Carpet board. They share the uniform
plumbing; they do not share the scope.

**A capability is not a setting.** `"bowls": 1` today means *this scene contains
bowls* — `scene/red.glsl` includes `bowl_stones.glsl` and `scene/thin.glsl` does
not. `PrisonerMode::Auto` reads it to decide whether to draw the counts in the
margin. Turning it into a user setting in place would offer a bowls toggle on
`Minimal Thin`, whose `rScene()` has no `rBowls()` call to gate, and would take
the prisoner counts away with the pile — ADR-0016's collision.

## Decision

**Tunable parameters are declared in the JSON configuration. The GLSL declares
the uniform and nothing else.**

1. **Metadata lives in a keyed `shader_params` object, not in the `shaders`
   array.** `Configuration::load()` applies `merge_patch`, which *replaces* an
   array whole and merges an object per key. Putting parameter metadata inside a
   `shaders[]` entry would mean a language file or a user override had to restate
   the entire array to change one label — the trap `controls` already has, where
   an includer can add a binding but cannot remove one. An object keyed by
   parameter name merges the way `annotations` does.

   The key **is** the uniform name, and a declaration looks like this:

   ```json
   "shader_params": {
     "showBowls": {
       "type": "bool",
       "default": true,
       "label": "Bowls and lids",
       "group": "Scene",
       "requires": "bowls"
     },
     "ambient": {
       "type": "float",
       "default": 0.3,
       "range": { "min": 0.0, "max": 1.0 },
       "label": "Ambient level",
       "group": "Lighting"
     }
   }
   ```

   Four choices inside that shape, each with a live alternative:

   - **`type` is explicit rather than inferred from `default`.** JSON `0` and
     `0.0` are different types to `nlohmann::json` but the same keystroke to a
     person, so an inferred float parameter written `"default": 0` would come
     back an integer. The type is one word and it buys a real error message.
   - **`range` is an object, not `[0, 1]`.** A shader that wants a higher ceiling
     and the same floor overrides `range.max` alone. A two-element array would
     have to be restated whole — which is the array-versus-object argument this
     whole decision rests on, applied one level down. It is absent for a `bool`.
   - **`label` is a plain string, and needs no template mechanism.** Because the
     block is an object keyed by parameter, `config/zh.json` can `merge_patch`
     `shader_params.ambient.label` on its own. A template id would be a second
     localisation mechanism bought for nothing.
   - **`group` is a flat key, not nesting.** Nesting reads better in the file and
     merges worse: regrouping a parameter would move it between objects rather
     than overwrite one string.

   `requires` names a capability key on the shader entry (decision 3). It is what
   keeps the capability check generic — a shader whose entry does not declare
   `bowls` never offers `showBowls`, and no C++ special case knows the word
   "bowls".

2. **Two levels, global then per shader**, resolved in `resolveShader()` beside
   `stereo`, `height` and `annotations`. A parameter declared once globally
   applies to every shader that has the uniform; a shader entry may override its
   range, default or label. This is ADR-0011's arrangement and the camera's.

3. **Capability stays descriptive in the shader entry; the user's choice is a
   value in `user.json`; the renderer asks for their conjunction.**
   `GobanShader::drawsBowls()` becomes `capability && userValue`, and only a
   shader whose entry declares the capability offers the toggle at all. The key
   already in `config/base.json` keeps exactly the meaning it has.

4. **Values are persisted per shader in `user.json`, keyed by the shader's
   `name`.** That is already the identity `UserSettings` uses to restore the
   selected shader (`shader.name`), and a second identity for the same object is
   how two keys drift apart. **It follows that shader names must not be
   translated** — they are not today, and this decision is why they must not
   start.

5. **Calibration is out of scope and stays global.** `gamma`, `contrast`, `eof`
   and `dof` are not shader parameters in this sense and do not move into
   `shader_params`. The boundary is: *does this describe the screen, or does it
   describe the board?*

6. **A boolean is a `uniform bool`, never a `#define`.** A `#define` needs a
   relink, measured at **2019 ms cold** in ADR-0013 — the exact cost this feature
   exists to avoid paying. A branch on a uniform is what a toggle is.

7. **Drift is detected and loud.** The one genuine advantage of comment metadata
   is that it cannot get out of sync with the uniform. That is answered rather
   than conceded: a declared parameter whose `glGetUniformLocation()` returns
   `-1` is warned about when the program is adopted, which covers both a typo in
   the name and a uniform the compiler eliminated as unused. The reverse — a
   uniform with no declaration — is simply *not exposed*, which is a legitimate
   state and needs no diagnostic.

## Alternatives rejected

### Comment metadata in the GLSL (the backlog's Option A)

The authoring convenience is real: one file to edit, and the range sits beside
the code that consumes it. Four things count against it.

It is **a new parsing mechanism where an existing one fits**. The metadata is a
hand-rolled format read by regex over shader source, with no schema, no
validation and no error path, against `Configuration` + `nlohmann::json`, which
already does layered defaults, per-shader override and `$include`. The project's
own rule is to search for an existing mechanism before adding one.

It **cannot be localised**. Labels are user-visible strings and the program ships
in five languages. A comment in a `.glsl` file has no override point; a JSON key
has one per language file.

It **splits one concept across two formats keyed differently** — the metadata by
shader source path, the persisted value by shader name in `user.json` — for a
feature whose whole content is the pairing of the two.

And the sync objection it exists to answer **is answered better by a
diagnostic** than by co-location: decision 7 turns silent drift into a warning,
which is what "fail early" asks for. Co-location prevents the mismatch from
being written; a warning prevents it from being shipped.

### A split — GPU-only values in GLSL, CPU-visible values in JSON

The tempting compromise, and the one the 2026-09-05 backlog review suggested: a
light position that only the fragment shader reads could reasonably live beside
it, while anything the CPU acts on follows ADR-0011.

Rejected because **the boundary moves, and `bowls` is the worked example**. It
was a purely descriptive fact about a scene; the moment `PrisonerMode::Auto`
needed to reason about it, it became CPU-visible, and the moment a menu toggles
it, it becomes a setting with a persisted value. A parameter crossing that line
under the split would need its declaration rewritten in another format *and* its
persisted user values migrated. Two mechanisms would be a cost even if the
boundary were stable; one that moves under you is worse than either mechanism
alone.

### Naming convention only (the backlog's Option C)

`tweak_f_0_1_0p3_ambient`. Needs no parsing and carries no label, so it cannot
be localised or grouped, and it makes the shader source ugly in service of the
tooling. Listed for completeness; the backlog already rejected it.

### Leave the parameters as GLSL constants and keep editing the source

The status quo, and defensible for a maintainer who is also the shader author:
the edit-and-relaunch loop is slow but familiar, and nothing is more expressive
than the source. Rejected for the reason ADR-0011 rejected keeping the palette
as C++ literals — the tuning loop *is* the feature, and a **2019 ms** relink per
attempt is not a tuning loop. It also does not answer the request, which came
from wanting to switch the bowls off while playing.

## Consequences

- **Two files to touch when authoring a parameter**: the uniform in GLSL, the
  declaration in `config/base.json`. That is the price of this decision, and
  decision 7's warning is what keeps the pair honest.
- **Locations are queried on the UI thread.** ADR-0013 splits the build in two:
  `buildProgram()` on the worker, `adoptProgram()` on the UI thread, because
  binding is context state. Tunable-parameter locations join the ~50 already
  queried in `adoptProgram()`, and so does decision 7's warning.
- **`config/base.json` grows a `shader_params` block**, which is shipped and
  tracked. A value tuned by eye belongs in `user.json` — this file has twice
  taken a live experiment into a commit.
- **`drawsBowls()` gains a second input**, and every reader of it must go through
  the accessor rather than the config key. `PrisonerMode::Auto` is the reader
  that matters, and ADR-0016 already says it must follow the live value.
- **The frame-rate claim is real, and modest.** Measured on `tests/bench/`
  (19×19, 140 stones, Intel/Mesa): **27.4 fps** with the vessels and **31.6 fps**
  without, six one-second samples each, first discarded. That is **+15%** for
  removing four spheres and their shadows — worth having on a slow machine,
  nowhere near a reason to switch them off by default. It is the *shadows* that
  cost: `sBowls` is called per shading point, which is why the gate is inside it
  as well as inside `rBowls`.
- **The `const MAT materials[10]` problem is untouched.** Colours are the
  expensive third milestone and this ADR does not decide them: a const array
  cannot be a uniform array without either per-property uniforms, a UBO, or a
  mix-in offset. The format decided here applies to whichever is chosen.
- **A user can configure a parameter into an ugly board.** Same latitude
  `readout_color` and the move-quality stops already have; ranges constrain the
  slider, not the file.

## Milestones this unblocks

- **M1 — booleans.** Scene features on and off per shader: bowls, lids. Delivers
  the request that prompted this, and makes the frame-rate question measurable.
- **M2 — scalars.** Ambient level, light size, shininess. First point at which
  generated UI is needed.
- **M3 — colours.** Blocked on the material-array decision above, and on a colour
  control RmlUi does not have.

## Implementation log — M1, 2026-09-05

`src/ShaderParams.{h,cpp}` in `goban_core`, pure over `nlohmann::json` so the
capability rule is testable without a renderer (`tests/test_shaderparams.cpp`,
9 cases). `showBowls` and `showLids` are declared in `config/base.json` and
gated inside `rBowls`, `sBowls` and `rBowlStones` per cup index rather than at
the `rScene()` call sites, because the two vessels are one loop.

**The lids hold the prisoners, and that was not what the plan assumed.** cc[0]
and cc[1] are the lids and take `iBlackCapturedCount` / `iWhiteCapturedCount`;
cc[2] and cc[3] are the bowls and hold the reservoir. So `drawsPrisonerPile()`
follows `showLids`, and hiding the *bowls* costs no information at all. A single
"bowls" toggle — which is how the request was phrased, and how decision 3 was
first drafted — would have hidden the pile while `PrisonerMode::Auto` went on
believing it was there. Two parameters, not one.

Worth noting for anyone reading the GLSL: `bowls.glsl` assigns `oid` as
`idCup*` for 0 and 1 and `idLid*` for 2 and 3, which is backwards against both
`Metrics::calc()`'s comments and the contents. Left alone — it is a material
name and changing it would change the look — but it is not the authority on
which vessel is which.

**One real bug, found by testing persistence rather than by testing behaviour.**
The write keyed `user.json` by `UserSettings::getShaderName()`, which is empty
until something saves the selection, while the read keyed it by the shader
entry's `name`. Values persisted under `""` and were never loaded. That is
exactly the failure decision 4 is written against, committed while implementing
decision 4 — the accessor is now `GobanShader::currentShaderName()`, one source
for both halves. The scenario suite could not have caught it: `run_scenarios.sh`
gives each scenario a throwaway `user.json`, so no scenario outlives a restart.

`menu_click <element-id>` was added to the command set alongside it — the
sibling `menu_select` never had. Every menu item that is *clicked* rather than
chosen from a list had no coverage of its wiring at all, which is the same gap
that let three selects ship with no branch to carry their value. It refuses a
`disabled` item, because `pointer-events: none` is style and a dispatched event
goes straight past it.

**Left as a wart:** a menu item's text lives in its `.rml`, like every other
menu item, *and* the parameter carries a `label` in the config. Two labels for
one thing. The JSON one is unused at M1 and becomes load-bearing at M2, where
the UI is generated rather than written out; until then, the `.rml` wins because
that is where a translator looks.
