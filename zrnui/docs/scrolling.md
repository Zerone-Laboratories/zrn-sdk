# Scrolling

This page covers ScrollViewer, the container that scrolls one piece of content by touch, wheel or scroll bars, like the WP8.1 `ScrollViewer`. ListBox, ListView and GridView already show their items in a ScrollViewer (see [items.md](items.md)).

Widgets on this page:

- [ScrollViewer](#scrollviewer)

## ScrollViewer

**What it is.** A viewport over one content element, with a vertical and a horizontal scroll bar. A new ScrollViewer scrolls vertically and not horizontally.

**Create.**

```c
ZRNUI_API Zid Zscroll_viewer_create(void);
```

**Properties.**

| Function | Meaning | Default |
|---|---|---|
| `Zscroll_viewer_set_content(sv, Zid)` | Content: any element, which the viewer owns; it is measured with unlimited room along the scrolling axes. Replacing it, or `Z_INVALID_ID`, destroys the old one | empty |
| `Zscroll_viewer_set_vertical_scrollbar_visibility(sv, ZScrollBarVisibility)` | Vertical bar mode | `Z_SCROLLBAR_AUTO` |
| `Zscroll_viewer_set_horizontal_scrollbar_visibility(sv, ZScrollBarVisibility)` | Horizontal bar mode | `Z_SCROLLBAR_DISABLED` |
| `Zscroll_viewer_scroll_to_vertical_offset(sv, double)` | Jump to an offset in px, stopping any inertia. Clamped to [0, extent - viewport] as known from the last layout, so before the first layout it ends up 0. Raises Scroll, also when the offset did not change | |
| `Zscroll_viewer_scroll_to_horizontal_offset(sv, double)` | The same for the horizontal axis | |
| `Zscroll_viewer_get_vertical_offset`, `Zscroll_viewer_get_horizontal_offset` | Current offsets, as of the last layout | 0 |
| `Zscroll_viewer_get_extent_height`, `Zscroll_viewer_get_extent_width` | Size of the content, as of the last layout | 0 |
| `Zscroll_viewer_get_viewport_height`, `Zscroll_viewer_get_viewport_width` | Size of the visible area, as of the last layout | 0 |

`ZScrollBarVisibility` is `Z_SCROLLBAR_DISABLED`, `Z_SCROLLBAR_AUTO`, `Z_SCROLLBAR_HIDDEN` or `Z_SCROLLBAR_VISIBLE`. `Z_SCROLLBAR_DISABLED`: the axis does not scroll and the content gets the size of the viewer. `Z_SCROLLBAR_AUTO`: scrolls, the thin bar shows while it moves. `Z_SCROLLBAR_HIDDEN`: scrolls, no bar. `Z_SCROLLBAR_VISIBLE`: scrolls, the bar always shows.

**Events.**

```c
typedef void (*ZScrollCallback)(Zid sender, ZScrollEventType type, double new_value, void* user_data);
ZRNUI_API void Zscroll_viewer_on_scroll(Zid scroll_viewer, ZScrollCallback callback, void* user_data);
```

Scroll is raised when the offset moves: a touch drag, each step of the inertia (it needs `Zroot_tick`), the wheel, or the program. One handler; NULL removes it. A ScrollViewer always reports `Z_SCROLL_THUMB_POSITION` and its offset along the axis that scrolls: the horizontal one when only the horizontal axis can scroll (a Hub), otherwise the vertical one. The handler may destroy the viewer.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_scroll(Zid sender, ZScrollEventType type, double new_value, void* user_data) {
    (void)sender;
    (void)type;
    (void)user_data;
    double offset = new_value;
    (void)offset;
}

Zid make_scrolling_page(void) {
    Zid viewer = Zscroll_viewer_create();
    Zid stack = Zstack_panel_create(Z_VERTICAL);
    Zelement_set_margin(stack, Zthickness(19, 0, 19, 0));
    Zscroll_viewer_set_content(viewer, stack);
    Zscroll_viewer_on_scroll(viewer, on_scroll, NULL);
    for (int i = 0; i < 30; ++i) {
        Zpanel_add(stack, Zbutton_create("item"));
    }
    return viewer;
}
```

**Notes.**

- Offsets, extent and viewport are those of the last layout. To read them (or to scroll to an offset) right after changing the content, call `Zroot_update_layout(root)` first.
- Touch: dragging the content scrolls it.
- A press that starts on a Slider, ScrollBar, Thumb or TextBox is that control's own drag; a surrounding ScrollViewer does not take the pointer away from it.
- On a button, a drag past about 10 px along a scrollable axis scrolls the page and cancels the press (no Click fires).
- Put a ScrollViewer around a stack that can be taller than the screen; see [layout.md](layout.md) for panels and [buttons.md](buttons.md) for the controls placed inside.

Back to the [documentation index](index.md).
