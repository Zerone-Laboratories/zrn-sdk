# Range, progress and primitives

This page covers the controls that show or pick a number in a range (`RangeBase`: Slider, ProgressBar, ScrollBar), the busy indicator ProgressRing, and two primitives, Thumb and Separator. They imitate the Windows Phone 8.1 controls of the same names.

Widgets on this page:

- [Slider](#slider)
- [ProgressBar](#progressbar)
- [ProgressRing](#progressring)
- [ScrollBar](#scrollbar)
- [Thumb](#thumb)
- [Separator](#separator)

## RangeBase (shared by Slider, ProgressBar, ScrollBar)

| Function | Meaning | Default |
|---|---|---|
| `Zrange_base_set_value(range, double)` / `Zrange_base_get_value` | Value, coerced into [Minimum, Maximum]; NaN is ignored | 0 |
| `Zrange_base_set_range(range, minimum, maximum)` | Minimum and Maximum; the value is coerced again (ValueChanged fires when that changes it). If either is NaN nothing changes; a maximum below the minimum becomes the minimum | see below |
| `Zrange_base_get_minimum`, `Zrange_base_get_maximum` | Read the range | |
| `Zrange_base_set_changes(range, small_change, large_change)` | SmallChange (arrow keys of a Slider) and LargeChange (page keys of a Slider, clicks in the track of a ScrollBar) | see below |

The defaults depend on the control:

| Control | Minimum | Maximum | SmallChange | LargeChange |
|---|---|---|---|---|
| RangeBase in general (ScrollBar) | 0 | 1 | 0.1 | 1 |
| Slider | 0 | 100 | 1 | 10 |
| ProgressBar | 0 | 100 | 0.1 | 1 |

The getters return 0 for a Zid that is not a RangeBase.

**Event.** ValueChanged fires when the value changes, also when the program sets it, and never for the same value. One handler per control; `NULL` removes it. The handler may destroy the control.

```c
typedef void (*ZValueChangedCallback)(Zid sender, double old_value, double new_value, void* user_data);
ZRNUI_API void Zrange_base_on_value_changed(Zid range, ZValueChangedCallback callback, void* user_data);
```

## Slider

**What it is.** The WP8.1 `Slider`: a 57 px band (plus the header), as wide as its room.

**Create.**

```c
ZRNUI_API Zid Zslider_create(ZOrientation orientation);
```

`orientation` is `Z_HORIZONTAL` or `Z_VERTICAL`.

**Properties.** The RangeBase functions plus:

| Function | Meaning | Default |
|---|---|---|
| `Zslider_set_orientation(slider, ZOrientation)` | Orientation | as created |
| `Zslider_set_header(slider, const char*)` / `Zslider_get_header` | Text above the slider; `""` or `NULL` removes it | none |
| `Zslider_set_step_frequency(slider, double)` / `Zslider_get_step_frequency` | What a touch or a key snaps the value to; 0 (or a negative value, which is stored as 0) means no snapping | 1 |
| `Zslider_set_tick_frequency(slider, double)` / `Zslider_get_tick_frequency` | Distance between tick marks; 0 draws none | 0 |
| `Zslider_set_tick_placement(slider, ZTickPlacement)` | `Z_TICK_NONE`, `Z_TICK_TOP_LEFT`, `Z_TICK_BOTTOM_RIGHT`, `Z_TICK_OUTSIDE`, `Z_TICK_INLINE` | `Z_TICK_OUTSIDE` |
| `Zslider_set_snaps_to(slider, ZSliderSnapsTo)` | `Z_SNAPS_TO_STEP_VALUES` or `Z_SNAPS_TO_TICKS` | step values |

Defaults: Minimum 0, Maximum 100, Value 0, StepFrequency 1, SmallChange 1, LargeChange 10.

**Events.** ValueChanged through `Zrange_base_on_value_changed`.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_volume(Zid sender, double old_value, double new_value, void* user_data) {
    /* ... */
}

Zid make_slider(Zid stack) {
    Zid slider = Zslider_create(Z_HORIZONTAL);
    Zslider_set_header(slider, "Volume");
    Zrange_base_set_range(slider, 0, 10);
    Zrange_base_set_value(slider, 5);
    Zrange_base_on_value_changed(slider, on_volume, NULL);
    Zpanel_add(stack, slider);
    return slider;
}
```

**Notes.**

- A value that the program sets is not snapped; only touch and keys snap (Home and End go to the minimum and maximum).
- With `Z_SNAPS_TO_TICKS` a touch or a key lands on the tick marks instead of the steps; with TickFrequency 0 the steps are used.
- Keys: the arrows along the slider move by SmallChange, Page Up and Page Down by LargeChange.
- A press that starts on a slider is its own drag; a surrounding ScrollViewer does not take the pointer away (see [scrolling.md](scrolling.md)).

## ProgressBar

**What it is.** The WP8.1 `ProgressBar`: a 3.5 px high bar.

**Create.**

```c
ZRNUI_API Zid Zprogress_bar_create(void);
```

**Properties.** The RangeBase functions (Minimum 0, Maximum 100, Value 0) plus:

| Function | Meaning | Default |
|---|---|---|
| `Zprogress_bar_set_is_indeterminate(bar, bool)` / `Zprogress_bar_get_is_indeterminate` | Five squares run along the bar and the track is hidden | false |
| `Zprogress_bar_set_show_error(bar, bool)` | ShowError: the indicator disappears | false |
| `Zprogress_bar_set_show_paused(bar, bool)` | ShowPaused: the indicator is half transparent | false |

**Events.** ValueChanged through `Zrange_base_on_value_changed`.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_progress(Zid stack, bool busy) {
    Zid bar = Zprogress_bar_create();
    if (busy) {
        Zprogress_bar_set_is_indeterminate(bar, true);
    } else {
        Zrange_base_set_value(bar, 40);
    }
    Zpanel_add(stack, bar);
    return bar;
}
```

**Notes.** The indeterminate animation needs `Zroot_tick` (see [getting-started.md](getting-started.md)). A ProgressBar takes no input and no focus (it is not hit test visible and not in the Tab order).

## ProgressRing

**What it is.** The WP8.1 `ProgressRing`: five dots run round a 57.5 px ring.

**Create.**

```c
ZRNUI_API Zid Zprogress_ring_create(void);
```

**Properties.**

| Function | Meaning | Default |
|---|---|---|
| `Zprogress_ring_set_is_active(ring, bool)` / `Zprogress_ring_get_is_active` | IsActive | false |

**Events.** None.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_ring(Zid grid_or_panel) {
    Zid ring = Zprogress_ring_create();
    Zprogress_ring_set_is_active(ring, true);
    Zpanel_add(grid_or_panel, ring);
    return ring;
}
```

**Notes.**

- The dots run only while IsActive and need `Zroot_tick`. Nothing shows while it is not active.
- While active it paints the Background of the page at 0.8 opacity over the whole area of the control.
- It takes no input.

## ScrollBar

**What it is.** The `ScrollBar`, a RangeBase with a thumb; it is still the WP8 one. It draws only the thumb: a 5 px thick bar, at least 72 px long (the whole length when the bar is shorter) with a 1 px margin, and no track and no arrows. The thumb is as long as ViewportSize / (range + ViewportSize) of the bar, and nothing is drawn while Maximum is not above Minimum. A ScrollViewer shows its own bars; create a ScrollBar yourself only for a custom scrolling area.

**Create.**

```c
ZRNUI_API Zid Zscroll_bar_create(ZOrientation orientation);
```

**Properties.** The RangeBase functions plus:

| Function | Meaning | Default |
|---|---|---|
| `Zscroll_bar_set_viewport_size(scrollbar, double)` | ViewportSize: the size of the thumb relative to the range; a negative value is 0 | 0.1 |

**Events.**

```c
typedef void (*ZScrollCallback)(Zid sender, ZScrollEventType type, double new_value, void* user_data);
ZRNUI_API void Zscroll_bar_on_scroll(Zid scrollbar, ZScrollCallback callback, void* user_data);
```

`ZScrollEventType` is one of `Z_SCROLL_SMALL_DECREMENT`, `Z_SCROLL_SMALL_INCREMENT`, `Z_SCROLL_LARGE_DECREMENT`, `Z_SCROLL_LARGE_INCREMENT`, `Z_SCROLL_THUMB_POSITION`, `Z_SCROLL_THUMB_TRACK`, `Z_SCROLL_FIRST`, `Z_SCROLL_LAST`, `Z_SCROLL_END`.

A standalone ScrollBar raises only some of them:

| Type | When |
|---|---|
| `Z_SCROLL_LARGE_DECREMENT`, `Z_SCROLL_LARGE_INCREMENT` | A press in the track before or after the thumb; the value moves by LargeChange |
| `Z_SCROLL_THUMB_TRACK` | The thumb is dragged (once per move) |
| `Z_SCROLL_END` | The drag ends; the value is the current one |

The other types are never raised by it. `Z_SCROLL_THUMB_POSITION` is what a ScrollViewer reports through `Zscroll_viewer_on_scroll` (see [scrolling.md](scrolling.md)). The Scroll callback runs after ValueChanged and also when the value did not change, and the handler may destroy the bar. ValueChanged is also available.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_scroll(Zid sender, ZScrollEventType type, double new_value, void* user_data) {
    /* ... */
}

Zid make_scroll_bar(Zid panel) {
    Zid bar = Zscroll_bar_create(Z_VERTICAL);
    Zrange_base_set_range(bar, 0, 1000);
    Zscroll_bar_set_viewport_size(bar, 200);
    Zscroll_bar_on_scroll(bar, on_scroll, NULL);
    Zpanel_add(panel, bar);
    return bar;
}
```

**Notes.**

- A ScrollBar made with `Zscroll_bar_create` takes the pointer: the thumb can be dragged and the track clicked. Only the bars that a ScrollViewer draws itself do not.
- A press that starts on a ScrollBar is its own drag; a surrounding ScrollViewer does not take the pointer away (see [scrolling.md](scrolling.md)).
- It takes no focus.

## Thumb

**What it is.** The WP8.1 `Thumb`: a draggable primitive that reports how far it moved.

**Create.**

```c
ZRNUI_API Zid Zthumb_create(void);
```

**Properties.**

| Function | Meaning |
|---|---|
| `Zthumb_is_dragging(thumb)` | true between DragStarted and DragCompleted |

**Events.** One handler each; `NULL` removes it. All three use the same callback type. Started gets the position of the press inside the thumb, Delta how far the pointer moved since the last call, Completed the total distance of the drag:

```c
typedef void (*ZDragCallback)(Zid sender, float horizontal_change, float vertical_change, void* user_data);
ZRNUI_API void Zthumb_on_drag_started(Zid thumb, ZDragCallback callback, void* user_data);
ZRNUI_API void Zthumb_on_drag_delta(Zid thumb, ZDragCallback callback, void* user_data);
ZRNUI_API void Zthumb_on_drag_completed(Zid thumb, ZDragCallback callback, void* user_data);
```

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_delta(Zid sender, float dx, float dy, void* user_data) {
    /* move something by dx, dy */
}

Zid make_thumb(Zid panel) {
    Zid thumb = Zthumb_create();
    Zelement_set_width(thumb, 40);
    Zelement_set_height(thumb, 40);
    Zthumb_on_drag_delta(thumb, on_delta, NULL);
    Zpanel_add(panel, thumb);
    return thumb;
}
```

**Notes.**

- A press that starts on a Thumb is its own drag; a surrounding ScrollViewer does not take the pointer away (see [scrolling.md](scrolling.md)).
- A disabled Thumb ignores presses. Disabling it during a drag, or any other loss of the pointer capture, ends the drag with DragCompleted.
- A Thumb moves nothing itself and takes no focus.

## Separator

**What it is.** The WP8.1 `Separator`: a thin line between items. It is horizontal in a vertical stack and the other way round.

**Create.**

```c
ZRNUI_API Zid Zseparator_create(void);
```

**Properties and events.** None beyond the shared element properties.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void add_divider(Zid stack) {
    Zpanel_add(stack, Zseparator_create());
}
```

Back to the [documentation index](index.md).
