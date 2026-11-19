# zrnapp

The window of an ordinary Wayland application on top of [zrnui](../zrnui). zrnui draws a tree and knows no window;
zrnapp is the Wayland client around it: `xdg-shell` window, pointer / touch / wheel / keyboard / text into the
`Zroot_inject_*` functions, the scale of the shell (`wp_fractional_scale` with `wp_viewporter`), and the frames of the
root on the compositor, through `wl_shm` or OpenGL ES 2 on EGL. It talks to zrnui only through `zrnui/platform.h`.
An application started this way is a normal Wayland process: a binary, a `.desktop` file, no launcher or runtime of its own.

A program that embeds zrnui in a shell does not use zrnapp.

## Use

```c
#include <zrnapp.h>
#include <zrnui/controls.h>
#include <zrnui/host.h>

int main(void) {
    if (!Zinit()) return 1;
    const Zid root = Zroot_create();
    /* ... build the tree, Zroot_set_visual(root, ...) ... */
    const ZWindowOptions window = { .title = "Notes" };   /* the app id is the name of the executable */
    const bool ok = Zwindow_run_ex(root, &window);
    Zroot_destroy(root);
    Zshutdown();
    return ok ? 0 : 1;
}
```

Link `zrnapp::zrnapp` and `zrnui::zrnui` (CMake) or `pkg-config --cflags --libs zrnapp zrnui`: `zrnapp.h` only declares the window
(it includes `zrnui/id.h` for `Zid`), the widgets and the root come from the headers of zrnui that the program includes.

## Title and app id

A window has a title (for people) and an app id (for the shell: it finds `<app id>.desktop`, the icon and the grouping of
windows by it). `ZWindowOptions.app_id` is the name of the `.desktop` file without the suffix. Without it the app id is the
file name of the executable, as GTK does, so a program installed as `notes` with `notes.desktop` needs nothing.
`Zwindow_run(root, title)` sets the title only; the title is never used as the app id.

## The window loop

`Zwindow_run_ex` returns true after a normal close and false when no window could be opened (see `Zget_error`). The window is as
big as the root; a root with no size gets 480 x 800 until the compositor sends the first configure, and a resize resizes the root.
Escape that nothing handled closes the window, but only when the visual of the root is a `Frame`; otherwise the program decides
(`Zroot_back`, `Zwindow_quit`). Call everything from the thread that made the root.

`ZWindowOptions.on_connect(wl_display, user_data)` is called once with the Wayland connection (`struct wl_display*` as `void*`),
before the first commit of the window. A shell uses it to bind a protocol of its own on the same connection; the events then
arrive in the loop of the window, in the same thread.

## Build

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --test-dir build/host-debug   # test_app_id; window_software, and window_gpu with EGL, run on cage (headless) when it is installed
```

Without presets: `cmake -S . -B build && cmake --build build`. `phone-debug` / `phone-release` cross build for a phone
(see the [main Readme](../Readme.md#presets)).

- zrnui is the one in `../zrnui`, or the installed one (`pkg-config zrnui`) with `-DZRNAPP_SYSTEM_ZRNUI=ON`.
- Needs wayland-client, xkbcommon, `wayland-scanner` and the XML of `wayland-protocols` (xdg-shell, viewporter,
  fractional-scale-v1; the protocol code is generated at build time). For the GPU window also egl, glesv2 and wayland-egl;
  without them the window draws with `wl_shm`.
- A shared library is built by default (`-DZRNAPP_BUILD_SHARED=OFF` for a static one); `cmake --install` installs it with `zrnapp.h` and `zrnapp.pc`.

## Environment

| variable | effect |
|---|---|
| `ZRNAPP_APP_ID` | the app id the launcher gives (the `.desktop` file it starts); removed from the environment once read |
| `ZRNAPP_SCALE` | overrides the UI scale (debug only): the layout grid is the window / scale; without it the `preferred_scale` of the compositor is used |

The variables of zrnui itself (`ZRNUI_RENDERER`, `ZRNUI_ANIMATIONS`, ...) are read by `Zinit`; see `../zrnui/Readme.md`.
