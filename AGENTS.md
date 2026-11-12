# AGENTS.md

zrnSDK: a C23 toolkit for Windows Phone 8.1 style apps on Wayland. Two independent CMake projects (plus a metapackage), no root `CMakeLists.txt`:

- `zrnui/`: the UI core (element tree, layout, widgets, theme, text, software and OpenGL ES 2 painters). Opens no window.
- `zrnapp/`: the Wayland window around zrnui. Talks to the core **only** through `zrnui/include/zrnui/platform.h`.
- `zrnkit/`: not code. A metapackage (`PKGBUILD` only) for the end device: it installs nothing itself and only depends on
  `zrnui` and `zrnapp`, so one package pulls in everything. Do not add sources or CMake files there.
- `examples/helloworld`, `examples/demoapp`: apps that `add_subdirectory` zrnapp (which pulls in zrnui).

Docs for people: `Readme.md` (build), `zrnui/docs/index.md` (widgets, API model), `docs/CODESTYLE.md`.

## Commands

Every project has a `CMakePresets.json` with `host-debug`, `host-release`, `phone-debug`, `phone-release` (Ninja; the
build directory is `<project>/build/<preset>`, ignored by git). **Configure first** (`cmake --preset`), then
`cmake --build --preset`: the build step fails with "is not a directory" otherwise. Never run cmake in a directory that
already has a `cmake-build-*` from another path (they are stale and ignored by git).

```sh
# the library and its tests
(cd zrnui && cmake --preset host-debug && cmake --build --preset host-debug)
ctest --test-dir zrnui/build/host-debug --output-on-failure   # all tests
zrnui/build/host-debug/tests/test_input                       # one test program

# the window, and the demo on this computer (needs a Wayland session to run)
(cd zrnapp && cmake --preset host-debug && cmake --build --preset host-debug)    # window tests need `cage`, skipped without it
(cd examples/demoapp && cmake --preset host-debug && cmake --build --preset host-debug)
examples/demoapp/build/host-debug/demoapp
```

For a phone (aarch64 Arch Linux ARM) use `tools/zrn` (docs: `zrnui/docs/devices.md`): `tools/zrn sysroot` once, then
`tools/zrn run <address> examples/helloworld` builds with the `phone-release` preset, uploads over ssh and runs.
The user is `-u` (default `alarm`), the password `-p` (optional, `-p -` asks for it), a key `-i`; never put a password or an
address of a device into a file of the repository. The agent has no phone: build with `tools/zrn build <dir>` and check
`file` says aarch64, and leave the run on the device to the user.

Run the tests of the area you touched, then the whole `ctest` before saying a change is done. A bug fix needs a test that
fails without it (tests inject input and render frames, see `zrnui/tests/test_input.c`; no window is needed).

## Rules

- Style is in `docs/CODESTYLE.md` (it wins over anything repeated here): C23, tabs, `Z`-prefixed public API, `zrn_` internals, comments in English only.
- Public API lives in `zrnui/include/zrnui/*.h`. Every public function has a
  `/** @brief ... */` that states defaults and units. `zrnui/include/zrnui/internal/` is not installed and not API.
- `zrnui/include/zrnui/resources.h` is generated from the Windows Phone 8.1 `themeresources.xaml` ("do not edit"). Do not hand-edit it.
- Tests link a static twin of zrnui because the shared library hides `zrn_*` internals; new test programs are listed in
  `zrnui/tests/CMakeLists.txt`.
- Do not add dependencies between zrnapp and zrnui internals. If zrnapp needs something, extend `platform.h`.
- Texts you write for yourself or for the user (plans, notes, analyses, drafts, reports) go to the `agents/` directory of the
  project you work in: `zrnui/agents/`, `zrnapp/agents/`, `examples/<name>/agents/`, `agents/` for the root, `zrnkit/` and
  `tools/`. Create it if it is missing. `agents/` is ignored by git: never commit it and never `git add -f` anything from it.
  Never put such files next to the sources (`src/`, `include/`, `tests/`, `docs/`, project roots). Documentation for people
  is not a scratch text: it goes to `docs/`.

## Input model (easy to break)

Pointer events go through `Zroot_inject_pointer` in `zrnui/src/host.c`: `preview()` first walks **every ancestor** of the
target with `ZRN_IN_PREVIEW_*` (only elements with `b_observes_pointer`), then the target and its parents get the real event.
Only one element has the pointer capture (`zrn_set_capture` / `zrn_release_capture`).

- Whoever sets state on `PREVIEW_DOWN` (ScrollViewer, Pivot, FlipView) must clear it on `PREVIEW_UP` too: the release
  goes to the capture holder, not to them. A stale `pressed` makes a Pivot page along with a mouse that has no button down.
- A popup (ComboBox list, flyout) is a child of its owner, so events inside it also pass through the owner's ancestors,
  including the page's ScrollViewer.
- ScrollViewer takes the capture from any element after a drag past ~10 px, except from elements with
  `b_blocks_scroll = true` (Slider, ScrollBar, Thumb, TextBox). Set it on a new control that owns its own drag.
- Handlers may destroy elements: re-look up by `Zid` (`RELOOK`) after anything that can run user code.

## Gotchas

- A `Zid` is a generation-checked handle, not a pointer. A destroyed handle is an error, not a crash.
- Layout units are logical pixels of a 480 x 800 grid; the UI scale is applied when painting.
- `Zinit` reads `ZRNUI_*` environment variables, `Zinit_ex` does not. Tests and embedders should pass a `ZConfig`.
- Animations run only with the GPU renderer unless forced (`Zroot_set_animations`); the software painter has none. Tests
  render in software, so a test of an animation must call `Zroot_set_animations(root, Z_ANIMATIONS_ON)` first (see
  `zrnui/tests/test_bars.c`), otherwise it checks a frame that never animated.
