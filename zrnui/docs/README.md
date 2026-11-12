# zrnui

zrnui is a C UI library inspired by Windows Phone 8.1 XAML controls. It provides
elements, controls, layout, input, animation and theming. It draws a tree and knows no window:
the window and the app cycle are `zrnapp`, the OS integration layer built on top of it (`../../zrnapp`).

## Build

```sh
cmake --preset host-release          # configure first; the build directory is created by this step
cmake --build --preset host-release
```

For a phone see [devices.md](devices.md). The options and the dependencies are in the [main Readme](../../Readme.md#zrnui); the layers, the environment and the tests are in
[zrnui/Readme.md](../Readme.md).

## Usage

Include the headers you use (`zrnui/controls.h` for the widgets, `zrnui/host.h` for the root, `zrnui/platform.h` for a shell) and
link against `zrnui`. zrnui has no window: a program that embeds it (a shell) passes input and
time in through `Zroot_inject_*` / `Zroot_tick` and takes pixels out through `Zroot_render` or a `ZGlTarget` in its own
OpenGL ES 2 context (`zrnui/platform.h`). An application that is an ordinary Wayland client uses zrnapp (`../../zrnapp`).
Controls are created with `Z...` factories and attached to a parent element. See [Getting started](getting-started.md).

## Fonts

At `Zinit` the font directory (`ZConfig.font_directory` or `$ZRNUI_FONT_DIR`, then `Zset_font_directory`, then the compiled-in
one) and all system fonts (fontconfig, optional) are registered. The five faces are resolved from that registry in this order:
1. Exact Segoe files in the font directory.
2. System Segoe ("Segoe WP", then "Segoe UI"), nearest weight.
3. Stand-in files in the font directory (Segoe Regular/Bold, then the bundled Selawik).

A code point a face lacks is drawn with the best system font that has it (upright, proportional, nearest weight).

## Navigation

`Zframe_navigate` manages a page stack. Back/forward reuse cached pages when
available; otherwise rebuild. See [Navigation](navigation.md).

## Memory

Manual memory management: a parent owns its children, `Zelement_destroy` destroys an element with its subtree and
`Zroot_destroy` the tree of a root. A page cached for navigation is kept for its entry of the history; it is destroyed with the frame or when `Zframe_navigate` drops its entry. `Zinit` and `Zshutdown` are counted: every `Zinit` needs a `Zshutdown`, and the call that balances the first one
destroys every root and element that is left.
