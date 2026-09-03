# ADR-0013: Shaders are linked on a worker thread with a shared GL context

**Status:** Accepted — implemented 2026-09-03
**Date:** 2026-09-03

## Context

The first launch on a machine froze the window with nothing drawn and nothing
said. Measured ~15 s on Windows/NVIDIA (Jan, testing the v0.2.0 bundle) and
2019 ms on this Intel/Mesa laptop. It shipped in 0.2.0 as a known limitation.

Measuring it first changed the problem in three ways.

**It is `glLinkProgram`, not compilation.** Instrumented on Mesa 26.1.8 with a
cold driver cache: read 0 ms, vertex 1 ms, **fragment 19 ms**, **link 2019 ms**.
Everything written about this beforehand — including the name "shader compile" —
pointed at the wrong call.

**It was paid three times.** The same program was linked from `GobanShader`'s
constructor via `init()`, from `GobanView`'s constructor for the saved shader,
and again when the shader dropdown synced itself. The constructor's link happens
before settings are read, so it can only ever build number 0 — discarded for any
user with a different shader saved, and on a cold cache it is the expensive one.
Fixed separately and first (commit a5f9d3c): warm linking 278 → 98 ms.

**It is not only the first launch.** Every shader links the first time it is
selected — cold, `red_carpet` 2019 ms, `red_carpet_stereo` 2038 ms, `thin`
360 ms, `flat` 328 ms, `2d` 259 ms. Cycling through the View menu on a fresh
machine froze the window once per shader. This is what settled the question of
whether a real fix was worth it: a once-per-machine cost is arguably tolerable,
a once-per-shader-per-machine one is not.

## Decision

Link on a worker thread holding a **second GLFW window whose GL context shares
objects with the main one**, while the UI thread keeps drawing. `AppState` owns
the hidden 1×1 window because `glfwCreateWindow` may only be called from the main
thread and `GobanShader` is constructed deep inside RmlUi's document load.

The build splits in two, and the split is the whole of the correctness argument:

- **`buildProgram()`** — create, compile, link, read the log. Touches only
  locals and objects that are *shared* between contexts. Runs on the worker.
- **`adoptProgram()`** — the buffers, the binding point, every uniform location.
  Runs on the **UI thread**, because `glBindBufferRange()` binds to the
  *context*, not to the program or the buffer. Done on the worker it would leave
  the drawing context with no uniform buffer bound at all and the board would
  render its stones from whatever happened to be there.

Only two values cross the thread boundary: the program name the worker produced,
and an atomic saying it finished. The ~50 uniform locations are queried on the UI
thread, where a lookup in an already-linked program costs microseconds — so no
part of the shader's state needs publishing, and the concurrency is small enough
to hold in one's head.

`GobanView::takeShaderBuild()` takes delivery from `ElementGame::OnUpdate()`,
**before** its `isReady()` gate, because that is the call that opens the gate.

While a build is in flight the interface is live and `#lblStatus` says
`Preparing the board — first time only on this computer… 3s`.

## Alternatives rejected

**`KHR_parallel_shader_compile`.** The obvious fix, and it does not work.
Mesa advertises it here; measured, `glLinkProgram` itself took 2065 ms and
`GL_COMPLETION_STATUS_KHR` reported complete on the *first* poll. The link is
synchronous inside the call, so there is nothing to overlap with. **Do not
retry this without re-measuring** — if a driver ever does defer, it would be
strictly simpler than a second context and worth switching to.

**Paint a message, then link synchronously.** Half a fix, and the earlier note in
this repository said so: the thread is still blocked, so the window is still
frozen — now with a caption. At 15 s the compositor offers to kill it.

**Cache linked programs in a map instead of deleting on switch.** Not rejected,
just not this change: it would make switching *back* to a shader free but does
nothing for first use, which is the expensive case.

**A progress bar.** Not viable and not merely unimplemented: nothing can estimate
how long a driver will take to link, and a bar that does not measure anything is
decoration. ADR-0012 already settled the form a wait takes here — a count that
turns over once a second, the physical kind of change, rather than motion
carrying no information. The message says the cost is one-time because that is
the fact that turns a worrying pause into an acceptable one.

## Consequences

- **Selecting a shader is now asynchronous**, so anything asserting on the
  shader must wait for it. `GobanControl::isIdle()` gained a sixth term, and it
  is the first that is not about the game. Three scenarios needed `wait_idle`
  after `cycle shaders`.
- **Idle is not the same as repainted.** `wait_idle` returns on the frame the
  program became current; `overlay_glyphs` reports what the glyph pass last
  *drew*. Assertions about what was rendered use `wait_until`.
- **A latent bug became live and is fixed.** `populateUIElements()` filled the
  shader dropdown without suppressing widget events — the one dropdown left off
  the list in the repopulation invariant — and asked `getCurrentProgram()`, which
  is -1 during a build. It therefore selected entry 0 and *fired a change event*
  that replaced the shader the user had saved. Three fixes, because the rule
  should hold from both ends: the population is wrapped in a
  `WidgetEventGuard`, the `shader` branch in `EventHandlerNewGame` now asks
  `acceptsUiEvents()` like its four siblings, and widgets ask the new
  `selectedProgram()` — what is or will be current — rather than what is drawing.
- **The old program is kept alive until the new one lands**, so switching shaders
  keeps drawing the previous board instead of blanking for the seconds the link
  takes. At startup there is nothing to keep, which is when the message shows.
- **Engine loading now overlaps the link** rather than queueing behind it.
  Measured: 96 log lines emitted during a 3.3 s link.
- **`getIdleTimeout()` gained a fourth wait to cover.** Without it the loop
  blocks in `glfwWaitEvents()`, never repaints, and never collects the finished
  program either — since `OnUpdate()` is what collects it. The same trap as the
  resync and the genmove, and the third time it has been walked into.
- **A failed link keeps the current shader** rather than dropping to a blank
  board, and says so in the log.
- **No shared context is not an error.** `chooseAsync()` falls back to
  `choose()`, which is exactly the old behaviour, freeze included.
- **Unverified on Windows.** glad's function pointers come from
  `glfwGetProcAddress`, which under WGL is context-specific in principle. It
  works in practice for contexts of one driver and pixel format, but the fallback
  above is the safety net and the Windows build is the thing to watch.
- The message exists **in English only**. Other languages fall back to the
  English string in `ElementGame.cpp`, which is what `templateText()` is for.
