# zrnSDK - everything to create your own ztOS app

zrnSDK is a C23 toolkit for applications that look and behave like Windows Phone 8.1 apps: pages, a Pivot and a Hub,
buttons, sliders, lists, pickers, app bars, flyouts. It is made of two libraries.

| library | what it is |
|---|---|
| [zrnui](zrnui) | The UI core: the element tree, layout, all widgets, theme, text and the painters (software and OpenGL ES 2). It opens no window and has no event loop. |
| [zrnapp](zrnapp) | The window of an ordinary Wayland application on top of zrnui: the event loop, pointer / touch / keyboard input, the scale of the shell, and the frames going to the compositor. |

An application that embeds zrnui into its own shell links zrnui alone (see [Embedding](zrnui/docs/embedding.md)).
An ordinary Wayland application links both.

**Documentation:** [zrnui/docs/index.md](zrnui/docs/index.md) - getting started, every widget, theme, embedding.

## Hello world

```c
#include <stdint.h>
#include <stdio.h>
#include <zrnapp.h>
#include <zrnui/controls.h>
#include <zrnui/host.h>

static Zid main_page(const Zid frame, void* const parameter) {
    const Zid page = Zpage_create();
    const Zid stack = Zstack_panel_create(Z_VERTICAL);
    Zpage_set_content(page, stack);
    Zelement_set_margin(stack, Zthickness(19, 32, 19, 0));
    Zpanel_add(stack, Ztext_block_create("HELLO WORLD"));
    return page;
}

int main(void) {
    if (!Zinit()) return 1;
    const Zid root = Zroot_create();
    const Zid frame = Zframe_create();
    Zroot_set_visual(root, frame);
    Zframe_navigate(frame, main_page, nullptr);
    const ZWindowOptions window = { .title = "Hello World" };
    const bool ok = Zwindow_run_ex(root, &window);
    Zroot_destroy(root);
    Zshutdown();
    return ok ? 0 : 1;
}
```

The complete versions are [examples/helloworld](examples/helloworld) and [examples/demoapp](examples/demoapp)
(a tour of the widgets).

## Building

There is no top-level `CMakeLists.txt`: every part is its own CMake project, and a project that needs another one pulls it
in by itself (zrnapp builds `../zrnui`, the examples build `../../zrnapp`). Build the directory you need.

### Quick start: run the demo app on this computer

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf freetype2 libpng fontconfig mesa wayland libxkbcommon wayland-protocols
cd examples/demoapp
cmake --preset host-debug             # configure first: the build directory does not exist before this
cmake --build --preset host-debug
./build/host-debug/demoapp            # inside a Wayland session
```

### Presets

Every project (`zrnui`, `zrnapp`, `examples/*`) has a `CMakePresets.json` with the same four presets, read by CLion, VS Code,
Qt Creator and Visual Studio as well as by `cmake --preset`. Presets need `ninja`.

| preset | for | build directory |
|---|---|---|
| `host-debug`, `host-release` | this computer (x86_64 or whatever it is) | `build/host-<type>` |
| `phone-debug`, `phone-release` | a phone (aarch64 Arch Linux ARM), cross build with clang and a sysroot; zrnui and zrnapp linked statically, no tests | `build/phone-<type>` |

`cmake --preset <name>` configures, `cmake --build --preset <name>` builds; configure comes first. Your own presets go to
`CMakeUserPresets.json` (not committed). The `cmake -S . -B build` commands below do the same without presets.

### Requirements

| needed for | packages (Arch names; other distributions have the same libraries) |
|---|---|
| everything | a C23 compiler (GCC 13+ or Clang 19+, for `constexpr`, `auto` and `nullptr`), `cmake` >= 3.21 for presets (3.16 without), `ninja` (presets), `pkgconf`, `freetype2`, `libpng` |
| better text | `fontconfig` (optional): without it only the Latin letters of the bundled Selawik fonts are drawn, Cyrillic and the rest need a system font |
| GPU renderer | `mesa` (`egl`, `glesv2`), optional; without it everything is drawn by the software painter |
| zrnapp | `wayland`, `libxkbcommon`, `wayland-protocols` (the protocol code is generated at build time with `wayland-scanner`); `wayland-egl` for the GPU window, otherwise the window draws through `wl_shm` |
| window tests | `cage` (a headless compositor), optional |

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf freetype2 libpng fontconfig mesa wayland libxkbcommon wayland-protocols
```

### zrnui

```sh
cd zrnui
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build            # the test suite (17 programs, 18 with EGL and glesv2: test_platform)
```

| option | default | effect |
|---|---|---|
| `CMAKE_BUILD_TYPE` | - | `Debug` also defines `Z_DEBUG` (debug logging, extra checks); `Release` for production |
| `ZRNUI_BUILD_SHARED` | `ON` | shared library; `OFF` builds a static one |
| `ZRNUI_BUILD_TESTS` | `ON` when top level | build the tests |
| `ZRNUI_DEBUG` | `ON` in Debug | define `Z_DEBUG` on `zrnui` and on everything that links it |
| `ZRNUI_WITH_GL` | `ON` | build the OpenGL ES 2 renderer when `egl` and `glesv2` are found |
| `ZRNUI_WARNINGS_AS_ERRORS` | `OFF` | `-Werror` |
| `ZRNUI_FONT_DIR` | `zrnui/fonts/selawik` | where the bundled fonts are looked for (compiled in) |
| `ZRNUI_THEME_FILE` | `zrnui/theme/theme.ini` | the compiled-in theme file |
| `ZRNUI_BUILD_FUZZERS` | `OFF` | libFuzzer targets (needs clang) |
| `ZRNUI_COVERAGE` | `OFF` | gcov instrumentation |

### zrnapp

```sh
cd zrnapp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

By default it builds the zrnui next to it (`../zrnui`). With `-DZRNAPP_SYSTEM_ZRNUI=ON` it links an installed zrnui found
through `pkg-config zrnui` instead. Other options: `ZRNAPP_BUILD_SHARED` (`ON`), `ZRNAPP_BUILD_TESTS`.
When wayland-client or xkbcommon is missing the library still builds, its functions exist and fail.

### Examples

```sh
cd examples/helloworld            # or examples/demoapp
cmake --preset host-debug
cmake --build --preset host-debug
./build/host-debug/helloworld     # run it inside a Wayland session
```

### On a phone

Build on this computer for aarch64, upload over ssh and run there with the output in your terminal, with one script:

```sh
tools/zrn sysroot                                 # one time: the aarch64 sysroot, from an Arch Linux ARM container
tools/zrn -p - setup-key 192.168.7.2              # one time per phone: put your ssh key on it (user alarm by default)
tools/zrn run 192.168.7.2 examples/helloworld     # build, upload, run
```

The user (`-u`, default `alarm`), the password (`-p`, optional) and the key (`-i`) are arguments; nothing is stored.
The details, the options and troubleshooting are in [zrnui/docs/devices.md](zrnui/docs/devices.md).

### Installing

```sh
cmake --install zrnui/build --prefix /usr/local
cmake --install zrnapp/build --prefix /usr/local     # configured with -DZRNAPP_SYSTEM_ZRNUI=ON
```

zrnui installs the library, the public headers (`zrnui/internal` is not installed), the fonts
(`share/fonts/zrnui/selawik`), the theme (`share/zrnui/theme/theme.ini`) and `zrnui.pc`; zrnapp installs the library,
`zrnapp.h` and `zrnapp.pc`. There is no CMake package file: use pkg-config or `add_subdirectory`.

For Arch Linux each part has a `PKGBUILD` (`zrnui` first, zrnapp depends on it):

```sh
cd zrnui   && makepkg -si
cd ../zrnapp && makepkg -si
```

### Using it in your project

With pkg-config:

```sh
cc -std=c23 app.c $(pkg-config --cflags --libs zrnapp zrnui) -o app
```

With CMake, from inside the checkout (this is what the examples do):

```cmake
add_subdirectory(path/to/zrnsdk/zrnapp ${CMAKE_BINARY_DIR}/zrnapp)   # also builds zrnui
add_executable(myapp main.c)
target_link_libraries(myapp PRIVATE zrnapp::zrnapp zrnui::zrnui)
```

With an installed copy, `pkg_check_modules(ZRN REQUIRED IMPORTED_TARGET zrnapp zrnui)` and link `PkgConfig::ZRN`.

### Environment

| variable | effect |
|---|---|
| `ZRNUI_RENDERER` | `auto` / `software` / `gpu`: the renderer of a new root |
| `ZRNUI_ANIMATIONS` | `on` / `off`: forces animations on or off for a new root |
| `ZRNUI_ANIMATION_SPEED` | multiplier of every animation (0.5 half as fast, 2 twice as fast) |
| `ZRNUI_THEME` | path of a `theme.ini` that replaces the compiled-in one |
| `ZRNUI_FONT_DIR` | the font directory; it wins over `Zset_font_directory` |
| `ZRNAPP_APP_ID` | the app id the launcher gives (the `.desktop` file it starts) |
| `ZRNAPP_SCALE` | overrides the UI scale of the window (debugging) |

Only `Zinit` reads the `ZRNUI_*` variables; a program that calls `Zinit_ex` with its own `ZConfig` is not affected.

## Repository layout

```
zrnui/        the UI core (include/, src/, tests/, fonts/, theme/, fuzz/)
zrnapp/       the Wayland window
examples/     helloworld, demoapp
cmake/        the cross toolchain file for phones (toolchain-aarch64.cmake)
tools/        zrn (build, upload and run on a phone), Dockerfile (the aarch64 build image)
zrnui/docs/   the documentation of the UI library
docs/         CODESTYLE.md (code style)
```

Code style: [docs/CODESTYLE.md](docs/CODESTYLE.md).
