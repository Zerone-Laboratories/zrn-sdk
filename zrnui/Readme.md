# zrnui - a wp8.1 UI lib

## Build

From sources:

```sh
cmake --preset host-release        # or host-debug; configure first, then build
cmake --build --preset host-release
ctest --test-dir build/host-release
```

Without presets: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`.
Presets also exist for a phone (`phone-release`, cross build): see the [main Readme](../Readme.md#presets)
and [docs/devices.md](docs/devices.md).

Arch Linux package:

```sh
makepkg -s
```

## Layers

- **zrnui** (this directory): the UI core. The tree, layout, controls, theme, text and the painters (software, OpenGL ES 2).
  No window, no event loop; the seam to the platform is `include/zrnui/platform.h`.
- **zrnapp** (`../zrnapp`, its own project and README): the window of an ordinary Wayland application on top of zrnui.
  A shell that embeds zrnui does not use it and links `zrnui::zrnui` alone.

All the options are in the [main Readme](../Readme.md#zrnui). The version is `ZRNUI_VERSION_STRING` (`ZRNUI_VERSION_MAJOR`,
`_MINOR`, `_PATCH` in `zrnui/core.h`) at compile time and `Zget_version()` at run time. Nothing in the library is thread-safe:
call it from one thread.

A shared build (the default) hides every internal `zrn_*` symbol; only the `Z...` API is exported. The tests reach into the
internals, so they link `zrnui_static`, a static twin of the same sources that is built next to the shared library
when tests or fuzzers are on. `ctest` runs 17 programs, 18 with EGL and glesv2 (`test_platform`).

`CMAKE_BUILD_TYPE=Debug` (or `-DZRNUI_DEBUG=ON`) defines `Z_DEBUG` on the `zrnui` target and on every target
that links it. Debug features are compiled under `#ifdef Z_DEBUG` (`Z_DEBUG_ONLY(...)`, `Z_DEBUG_LOG(...)`
in `zrnui/core.h`) and are not in a release binary.

## Fonts

`Zinit` scans the font directory and, with fontconfig, every system font, and resolves the five faces from
that inventory; nothing is searched for while text is drawn (a Debug build prints what was found).
The font directory is `ZConfig.font_directory` (`$ZRNUI_FONT_DIR` with `Zinit`), which replaces a `Zset_font_directory` made
before, then `Zset_font_directory`, then the compiled-in `ZRNUI_FONT_DIR` (`fonts/selawik` of the sources).
`Zinit` returns false when the fonts cannot be loaded (see `Zget_error`); every `Zinit` needs a `Zshutdown`.

## Environment

| variable | effect |
|---|---|
| `ZRNUI_ANIMATION_SPEED` | multiplier of the speed of every animation (0.5 half as fast, 2 twice as fast); default 1.0, a value of 0 or less is ignored |
| `ZRNUI_ANIMATIONS` | `on` / `off`: forces animations on or off for a new root |
| `ZRNUI_RENDERER` | `auto` / `software` / `gpu`: the renderer of a new root |
| `ZRNUI_THEME` | path of a `theme.ini` that replaces the compiled-in one |
| `ZRNUI_FONT_DIR` | the font directory; it wins over `Zset_font_directory` |

## Fuzzing and coverage

```sh
# libFuzzer (clang): text layout and input, PNG files, theme.ini
cmake -S . -B build-fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DZRNUI_BUILD_FUZZERS=ON -DZRNUI_BUILD_TESTS=OFF
cmake --build build-fuzz
ASAN_OPTIONS=detect_leaks=0 build-fuzz/fuzz/fuzz_text corpus_dir -max_total_time=60

# gcov: build with -DZRNUI_COVERAGE=ON, run ctest, then gcov the .gcda files
```
