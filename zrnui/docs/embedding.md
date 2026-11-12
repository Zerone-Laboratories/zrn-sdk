# Embedding zrnui without zrnapp

zrnui opens no window, owns no event loop and reads no input devices. A *platform layer* does that: zrnapp is one (a Wayland
window), a phone shell that embeds zrnui is another. The whole seam is two headers:

- `zrnui/host.h`: the root, rendering, input injection, time;
- `zrnui/platform.h`: size, density, invalidation, renderer report, GPU target.

Link `zrnui::zrnui` (or `pkg-config zrnui`) and do not link zrnapp.

## The loop

A fragment, not a whole program: `page`, `x`, `y`, `delta`, `dt_ms` and `present` are the program's own.

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>
#include <zrnui/platform.h>

ZConfig config;
Zconfig_default(&config);
config.font_directory = "/usr/share/fonts/zrnui/selawik";
if (!Zinit_ex(&config)) return 1;                   // the environment is not read; false when the fonts failed

const Zid root = Zroot_create();
Zroot_set_size(root, 480, 800);                     // logical pixels
Zroot_set_visual(root, page);

for (;;) {
    // 1. input, from your events (logical pixels)
    Zroot_inject_pointer(root, Z_POINTER_DOWN, x, y);
    Zroot_inject_pointer(root, Z_POINTER_MOVE, x, y);
    Zroot_inject_pointer(root, Z_POINTER_UP, x, y);
    Zroot_inject_wheel(root, delta);
    Zroot_inject_key(root, Z_KEY_ENTER, 0, true /* pressed */);
    Zroot_inject_text(root, "text");                // UTF-8 to the focused element

    // 2. time
    Zroot_tick(root, dt_ms);                        // animations, caret, repeat buttons, tooltip delay

    // 3. pixels
    ZFrame frame = {0};
    if (Zroot_render(root, &frame)) present(frame.pixels, frame.width, frame.height, frame.stride);
}
```

`Zroot_render` returns true when something new was drawn since the last frame (and the first time); the `ZFrame` is 8-bit RGBA,
rows top to bottom, owned by the root and valid until the next render. Its size is in device pixels: the logical size of the
root times its density. `frame` is filled whether or not something was drawn; it is left untouched only when the call failed
(invalid root, memory, GPU), so initialise it. When `Zroot_render` returns false the previous picture is still right, so a loop
can sleep until the next event or tick. `Zroot_needs_render(root)` (`zrnui/platform.h`) says in advance whether a render would
draw something new, for a loop that wants to skip the call. A program that needs the actual size of an element before a
render calls `Zroot_update_layout(root)`.

## Rules

- Call everything from one thread; nothing in the library is thread-safe.
- `Zroot_tick` must be called for animations, the caret, `RepeatButton` repeats, indeterminate progress, scrolling inertia and the
  delay of a `ToolTip`; without it they stand still. A tick before each frame with the time since the last one is enough.
- Handlers run inside `Zroot_inject_*` and `Zroot_tick` and may destroy the control that raised them (the library re-checks the handle afterwards); from `Zroot_tick` they may
  destroy the root too, so check `Zroot_is_valid(root)` after it.
- `Zroot_set_visual` destroys the previous visual with its tree; `Zroot_destroy` destroys the visual, and gives back the `Zinit`
  that `Zroot_create` held. `Zinit_ex` returns false when the fonts cannot be loaded (see `Zget_error`).
- The version is `ZRNUI_VERSION_STRING` (compile time) and `Zget_version()` (run time).

## Input details

- Coordinates are in the logical pixels of the root. A touch screen is one pointer: send `Z_POINTER_MOVE` to the finger's
  position, then `Z_POINTER_DOWN`, moves, `Z_POINTER_UP`, and `Z_POINTER_LEAVE` when the finger is gone.
- `Zroot_inject_*` return true when an element handled the event, so a shell can pass unhandled events on. `Z_POINTER_LEAVE` always
  returns false; `Zroot_inject_text` returns false with no focused element or a NULL text.
- Back: `Zroot_back(root)` closes the topmost popup, flyout or dialog that Back closes, else the open app bar that is not sticky,
  else goes back in the root's `Frame`; it returns false when none of that happened (the first page, or the visual is not a
  `Frame`), which is where a shell leaves the app. Escape does the same through `Zroot_inject_key`, and when nothing handled it
  the shell decides: zrnapp closes the window when the visual is a `Frame`.
- Keys known to the library: Backspace, Tab, Enter, Escape, Space, arrows, Home/End, PageUp/PageDown, Delete, and A/C/V/X for the
  shortcuts with `Z_MOD_CTRL`. Everything else arrives as text through `Zroot_inject_text`.

## Size, scale and density

| call | meaning |
|---|---|
| `Zroot_set_size(root, w, h)` | the window size in logical pixels (a new root is 0 x 0, and a value of 0 or less is ignored); `Zroot_get_size` reads it |
| `Zroot_set_scale(root, s)` | UI scale of the shell: the layout grid is the window divided by `s` |
| `Zroot_set_density(root, d)` | device pixels per logical pixel, only the crispness of the picture: `ZFrame` is `size * d` pixels |
| `Zroot_invalidate(root)` | the next frame is painted whole (the target buffer is new or blank) |
| `Zroot_start_intro(root)` | a first page given to a `Frame` before the window existed swings in now |

## Drawing with OpenGL ES 2

Instead of reading `ZFrame`, a platform that owns an OpenGL ES 2 context lets zrnui draw into it:

```c
ZGlTarget* target = Zgl_target_create(make_current, user);   // context current; NULL when this build has no GL
Zroot_report_presenter(root, "gles2", true);                   // animations that need the GPU run only then
// every frame:
if (Zgl_target_paint(root, target, width_px, height_px)) swap_buffers();
// at the end, with the context current:
Zgl_target_destroy(target);
```

`make_current` (may be NULL) is called before the target does any GL work, possibly outside a frame, for instance when a
texture is deleted. `Zgl_target_paint` draws into framebuffer 0 only when something changed.
For a software presenter report `Zroot_report_presenter(root, "software", false)`.

## Errors

Failures are reported through `Zset_error_callback` and remembered per thread (`Zget_error`, `Zget_error_message`).
A platform layer reports its own errors with `Zreport_error`.

## A reference platform layer

[zrnapp](../../zrnapp) is a complete one, under a thousand lines: `zrnapp/src/window.c` shows the Wayland events turned into
`Zroot_inject_*`, the tick every 16 ms, and both presenters (`wl_shm` from `Zroot_render`, EGL from `Zgl_target_paint`).
The test suite of zrnui ([`zrnui/tests`](../tests)) is a platform too: it injects input and compares rendered frames
without any window.

Back to the [documentation index](index.md).
