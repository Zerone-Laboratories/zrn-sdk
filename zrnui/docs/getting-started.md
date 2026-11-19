# Getting started

This page explains the model of zrnui: handles, the root, the tree, pages and frames, events, and the life cycle of
an application. Build instructions are on the [main page](../../Readme.md#building).

## Handles: `Zid`

Every object (a root, an element, a control) is a `Zid`, a 32-bit handle. A handle carries an index and a generation, so
a handle of a destroyed object is detected and reported as an error instead of crashing. `Z_INVALID_ID` (0) is never valid. Using a handle that is not valid is reported through the error callback and the call does nothing.
A function that creates something returns `Z_INVALID_ID` on failure; the reason is in `Zget_error()` and
`Zget_error_message()`. `Zset_error_callback` replaces the default printing to `stderr`.

```c
const Zid button = Zbutton_create("OK");
if (button == Z_INVALID_ID) fprintf(stderr, "%s\n", Zget_error_message());
```

Ownership is by the tree: a parent owns its children. `Zelement_destroy` destroys an element with everything under it,
`Zroot_destroy` destroys the tree of the root. `Zshutdown` gives back one `Zinit`; the call that balances the first one
destroys everything left.

Nothing in the library is thread-safe: create, change and destroy objects, and call `Zroot_*`, from one thread.

## The life cycle of an application

```c
if (!Zinit()) return 1;           // fonts, theme, configuration from $ZRNUI_*  (Zinit_ex(&config) to pass your own)
Zid root = Zroot_create();        // a root hosts one visual tree
Zroot_set_visual(root, visual);   // the tree shown; the root owns it and destroys the previous one
Zwindow_run_ex(root, &options);   // zrnapp: window + event loop, returns when the window is closed
Zroot_destroy(root);
Zshutdown();
```

- `Zinit` returns false when the fonts cannot be loaded (the reason is in `Zget_error`).
- `Zinit` / `Zshutdown` are counted: it is safe to call `Zinit` twice, and every call needs its `Zshutdown`. The configuration of
  the first call stays. `Zroot_create` starts the library when needed and holds one `Zinit` until `Zroot_destroy`.
- A root has a size, a theme, an accent and a renderer. A new root is 0 x 0: zrnapp gives it 480 x 800 in `Zwindow_run_ex`
  until the compositor configures the window, and a program without zrnapp calls `Zroot_set_size`.
- The version is `ZRNUI_VERSION_STRING` at compile time and `Zget_version()` at run time.
- `Zwindow_run_ex` (see [zrnapp](../../zrnapp/README.md)) calls `Zroot_tick` every ~16 ms, injects input and presents frames.
  A program without zrnapp does this itself, see [Embedding](embedding.md).

## Building a tree

Elements are created with `Z<class>_create`, configured with `Z<class>_set_<property>` and put into a parent:

| parent | how to add a child |
|---|---|
| a panel (`StackPanel`, `Grid`, `Canvas`, ...) | `Zpanel_add(panel, child)` |
| a content control (`Button`, `Page`, `UserControl`, `ScrollViewer`, ...) | `Z..._set_content(control, child)` |
| an items control (`ListBox`, `ComboBox`, ...) | `Zitems_control_add(...)`, `Zitems_control_add_text(...)` |

```c
const Zid stack = Zstack_panel_create(Z_VERTICAL);
Zelement_set_margin(stack, Zthickness(19, 32, 19, 0));   // left, top, right, bottom

const Zid title = Ztext_block_create("SETTINGS");
Ztext_block_set_style(title, Z_TEXT_STYLE_TITLE);
Zpanel_add(stack, title);

const Zid wifi = Ztoggle_switch_create();
Ztoggle_switch_set_header(wifi, "Wi-Fi");
Zpanel_add(stack, wifi);
```

Layout is the one of XAML: measure then arrange, `Margin`, `HorizontalAlignment`, `Width`/`Height`. See [Layout](layout.md).
Sizes are in logical pixels of a 480 x 800 grid; the shell scale (`Zroot_set_scale`) enlarges the picture, not the grid.
The actual size of an element (`Zelement_get_actual_width`, ...) is known after a layout: call `Zroot_update_layout(root)` first
when you need it before the first render.

## Pages and navigation

An application is normally a `Frame` that shows `Page`s. A page is made by a factory function, so the frame can build it
again when going back:

```c
static Zid main_page(const Zid frame, void* const parameter) {
    const Zid page = Zpage_create();
    Zpage_set_content(page, build_content());
    return page;
}

const Zid frame = Zframe_create();
Zroot_set_visual(root, frame);
Zframe_navigate(frame, main_page, nullptr);     // later: Zframe_navigate(frame, other_page, &parameter); Zframe_go_back(frame);
```

A `Frame` keeps a safe area of 32 px at the top for the status bar of the shell, so a page starts below it; the 32 px top margin
of the samples above comes on top of that.

The Back key (Escape on a keyboard, which `Zroot_inject_key` turns into `Zroot_back`) closes the topmost popup, flyout or dialog
that Back closes, else the open app bar that is not sticky, else goes back in the frame when the visual of the root is a `Frame`.
It returns false when none of that happened. zrnapp then closes the window, but only when the visual is a `Frame`. See [Navigation](navigation.md) for `Pivot`, `Hub` and cache modes.

## Events

Each widget documents its events. Subscribe with the typed helper if there is one (`Zbutton_on_click`) or with the generic
`Zelement_on(element, Z_EVENT_..., callback, user_data)`. A callback has the shape
`void callback(Zid sender, void* user_data)`; read the new state from the sender
(`Zselector_get_selected_index(sender)`, `Zrange_base_get_value(sender)`, ...).

```c
static void on_click(const Zid sender, void* const user_data) {
    Zroot_back((Zid)(uintptr_t)user_data);
}
Zbutton_on_click(button, on_click, (void*)(uintptr_t)root);
```

A handler may destroy the control that raised it or navigate away; the library checks the handle after the handler and
never touches the element if it is gone. Handlers that run from `Zroot_tick` and `Zroot_inject_*` may also destroy elements.

## Input, focus, capture

Pointer, touch, wheel, keyboard and text are injected into the root (`Zroot_inject_*`); zrnapp does that from Wayland events.
Each returns true when an element handled the event (see [Embedding](embedding.md#input-details)).
The element under the pointer gets the press and may *capture* the pointer until release. A `ScrollViewer`, `Pivot` and
`FlipView` watch presses of their descendants: a drag along their axis takes the pointer from a button (the press is cancelled)
and scrolls or pages. Controls that own their drag (`Slider`, `ScrollBar`, `TextBox`) are not interrupted.
Tab and Shift+Tab move focus between tab stops (`Zroot_move_focus`).

## Time

Animations, the caret, `RepeatButton` repeats, indeterminate progress, inertia of scrolling and the delay of a `ToolTip` advance
only through `Zroot_tick(root, dt_ms)`; `Zwindow_run` calls it for you. `Zroot_needs_render` tells whether a new frame is due. Animations run only with the GPU renderer unless `Zroot_set_animations` forces them
(see [Theme and configuration](theme-and-resources.md#renderer-and-animations)).

## Where to go next

- [Layout and panels](layout.md), [Text](text.md), [Navigation](navigation.md)
- Widgets by group: [index](index.md)
- [Theme, fonts, configuration](theme-and-resources.md)
- [Embedding without zrnapp](embedding.md)
- Working code: [`examples/helloworld`](../../examples/helloworld) and [`examples/demoapp`](../../examples/demoapp)
