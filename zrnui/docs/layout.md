# Layout: elements, panels and containers

zrnui builds a tree of elements, as Silverlight and WinRT XAML do. Every
element is a `Zid` handle. Layout is the two passes of Silverlight: Measure
asks each element how much room it wants (bottom-up), Arrange gives each one
its final rectangle (top-down). This page covers the properties every element
has and the panels and containers that arrange other elements.

On this page:

- [The element tree and Zid handles](#the-element-tree-and-zid-handles)
- [FrameworkElement layout properties](#frameworkelement-layout-properties)
- [Render transforms, animation and tilt](#render-transforms-animation-and-tilt)
- [Panel (common to all panels)](#panel-common-to-all-panels)
- [StackPanel](#stackpanel)
- [VirtualizingStackPanel](#virtualizingstackpanel)
- [ItemsStackPanel](#itemsstackpanel)
- [ItemsWrapGrid](#itemswrapgrid)
- [WrapPanel](#wrappanel)
- [Grid](#grid)
- [Canvas](#canvas)
- [Border](#border)
- [Viewbox](#viewbox)
- [Popup](#popup)

Text, images: [text.md](text.md). Pages, Pivot and Hub: [navigation.md](navigation.md).
Lists that use these panels: [items.md](items.md). Scroll viewers: [scrolling.md](scrolling.md).

## The element tree and Zid handles

A `Zid` (`uint32_t`, `id.h`) is an opaque handle to any zrnui object: a root,
an element, a control. It carries an index and a generation counter, so using a
destroyed handle is detected and reported instead of crashing. `Z_INVALID_ID`
and 0 are never valid.

Class hierarchy (`element.h`):

```
Element (UIElement + FrameworkElement)
  Panel          StackPanel (VirtualizingStackPanel, ItemsStackPanel), WrapPanel,
                 ItemsWrapGrid, Grid, Canvas
  Border, Viewbox, TextBlock, Image, Popup
  Control        see controls.h
```

Ownership:

- A parent owns its children and destroys them with itself.
- An element with no parent is owned by the caller until it is added to a
  panel, set as content, or set as the visual of a root (`Zroot_set_visual`).
- An element has one parent: adding one that already has a parent moves it.
  Adding an element under itself or under one of its descendants is refused with
  `Z_ERROR_CYCLE` and nothing changes.
- The single-child setters (`Zborder_set_child`, `Zviewbox_set_child`,
  `Zpopup_set_child`, the `Zcontent_*` setters) destroy the old child. The new
  child may be inside the old one: it is moved out first.
- Attached properties (Grid.Row, Canvas.Left, Panel.ZIndex) are stored on the
  child, with `Zgrid_set_row(child, ...)`, and ignored when the parent is not
  that kind of panel.

| Function | Meaning |
| --- | --- |
| `Zelement_create(ZClass)` | Creates an element of a class chosen at run time. Abstract classes (`Z_CLASS_PANEL`...) are refused. Shortcuts per class exist (`Zbutton_create`...). |
| `Zelement_destroy(element)` | Destroys the element and everything under it. A root's visual is detached first. Safe from an event handler of the element or of one of its ancestors. |
| `Zelement_is_valid(element)` | True for a live handle; false, without an error report, for a destroyed or never valid one. |
| `Zelement_get_class(element)` | Most derived `ZClass`, 0 for an invalid handle. |
| `Zelement_is_a(element, class)` | True when the element is of the class or derives from it (a CheckBox is a ToggleButton, a ButtonBase, a ContentControl, a Control). |
| `Zelement_get_class_name(element)` | The class name as a string. |
| `Zelement_set_name` / `Zelement_get_name` | FrameworkElement.Name. |
| `Zelement_find_name(scope, name)` | Searches the subtree of `scope` for a name. |
| `Zelement_set_tag` / `Zelement_get_tag` | FrameworkElement.Tag: a `void*` of the program, never touched. |
| `Zelement_get_parent(element)` | The parent. |
| `Zelement_get_child_count(element)` / `Zelement_get_child(element, index)` | The visual tree. A Button has one child, its content; a ListBox has its scroll viewer, which has the item panel. |
| `Zelement_get_root(element)` | The root showing the element, or `Z_INVALID_ID`. |
| `Zelement_to_root(element, point)` / `Zelement_from_root(element, point)` | Transform a point to the root coordinates and back (TransformToVisual). |
| `Zelement_hit_test(scope, point)` | The element under a point given in the coordinates of `scope`, or `Z_INVALID_ID`. The child of a Popup and the child of a Viewbox are skipped. |

To change the contents of a panel use `Zpanel_*`, the content of a control
`Zcontent_*`, the items of a list `Zitems_control_*` (see [items.md](items.md)).
The visual-tree functions above only read the tree.

## FrameworkElement layout properties

These are common to every element. Lengths are logical pixels. `Z_AUTO` means
"not set" (Double.NaN of Silverlight); `Z_INFINITE` means no limit.

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zelement_set_width` / `Zelement_get_width` | Width. `Z_AUTO` sizes to the content. | `Z_AUTO` |
| `Zelement_set_height` / `Zelement_get_height` | Height. | `Z_AUTO` |
| `Zelement_set_min_size(element, min_w, min_h)` | MinWidth, MinHeight. | 0, 0 |
| `Zelement_set_max_size(element, max_w, max_h)` | MaxWidth, MaxHeight. | `Z_INFINITE` |
| `Zelement_set_margin` / `Zelement_get_margin` | Margin as a `ZThickness` (left, top, right, bottom). Build one with `Zthickness(l, t, r, b)` or `Zthickness_uniform(all)`. | 0 |
| `Zelement_bind_margin(element, key)` | Binds the margin to a Thickness theme resource (for example `Z_RES_PIVOT_ITEM_MARGIN`). It follows the theme, accent and root overrides until `Zelement_set_margin` sets another value. Returns false for a key that is not a Thickness. | not bound |
| `Zelement_set_horizontal_alignment` / `Zelement_get_horizontal_alignment` | `Z_HALIGN_LEFT`, `Z_HALIGN_CENTER`, `Z_HALIGN_RIGHT`, `Z_HALIGN_STRETCH`. | `Z_HALIGN_STRETCH` |
| `Zelement_set_vertical_alignment` / `Zelement_get_vertical_alignment` | `Z_VALIGN_TOP`, `Z_VALIGN_CENTER`, `Z_VALIGN_BOTTOM`, `Z_VALIGN_STRETCH`. | stretch |
| `Zelement_set_visibility` / `Zelement_get_visibility` | `Z_VISIBLE` or `Z_COLLAPSED`. A collapsed element is neither drawn, nor measured, nor hit. There is no "hidden". | `Z_VISIBLE` |
| `Zelement_set_opacity` / `Zelement_get_opacity` | Opacity of the element and its children, 0..1 (values multiply down the tree). A value outside the range is clamped, NaN is ignored. | 1 |
| `Zelement_set_enabled` / `Zelement_get_enabled` | IsEnabled. A disabled element ignores input and its descendants are disabled too. Disabling lets go of a pointer capture held by the element or a descendant (a held press ends) and takes the focus off an element that is no longer enabled. The getter returns the value set on the element itself. | enabled |
| `Zelement_is_effectively_enabled(element)` | True when neither the element nor an ancestor is disabled. | read-only |
| `Zelement_set_hit_test_visible(element, visible)` | IsHitTestVisible. False lets pointer events fall through the element and its children. | visible |
| `Zelement_set_clip_to_bounds(element, clip)` | Cuts the children where the element ends: they are neither drawn nor hit outside it. | off (some controls, such as ScrollViewer and Pivot, turn it on for themselves) |
| `Zelement_set_tooltip(element, text)` | ToolTipService.ToolTip as plain text, shown when the pointer rests on the element. NULL removes it. | none |
| `Zelement_focus(element)` / `Zelement_has_focus(element)` | Gives the keyboard focus (Control.Focus). Returns false when the element cannot take it. | |

Read-only results of layout:

| Function | Result |
| --- | --- |
| `Zelement_get_actual_width` / `Zelement_get_actual_height` | Size after the last Arrange, without the margin. |
| `Zelement_get_desired_size` | DesiredSize of the last Measure (`ZSize`), with the margin. |
| `Zelement_get_bounds` | `ZRect` of the element, margin excluded, in the coordinates of its parent. |

Layout control:

| Function | Meaning |
| --- | --- |
| `Zelement_invalidate_measure` / `Zelement_invalidate_arrange` | Force the next Measure / Arrange of the element. |
| `Zelement_measure(element, available)` / `Zelement_arrange(element, final_rect)` | Run Measure and Arrange of one element by hand (UpdateLayout of a detached element). |
| `Zroot_update_layout(root)` (`host.h`) | Runs both passes for a whole root. Rendering and input run them when something changed, so a program calls it only to read sizes (`Zelement_get_actual_width`, `Zelement_get_bounds`...) before a render: they are 0 until the first layout. |

### Events

`Zelement_on(element, event, callback, user_data)` sets the handler of a routed
event. There is one handler per event and element; passing NULL removes it. The
callback type is:

```c
typedef void (*ZRoutedCallback)(Zid sender, void* user_data);
```

Routed events carry no data: read the new state from the sender. The events are
the `Z_EVENT_*` values of `ZEvent` (for example `Z_EVENT_CLICK`,
`Z_EVENT_LOADED`, `Z_EVENT_GOT_FOCUS`, `Z_EVENT_LOST_FOCUS`). Events with data have
their own setters; each control page lists them.

A handler runs on the element that raises the event only, never on its
ancestors, and may destroy that element (or any other): the library checks the
handle again after the callback and does not touch a destroyed element. Do not
use the sender after destroying it.

### Example

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_loaded(Zid sender, void* user_data) {
    (void)user_data;
    ZRect r = Zelement_get_bounds(sender);
    (void)r;
}

Zid make_card(void) {
    Zid card = Zborder_create();
    Zelement_set_margin(card, Zthickness(19, 9.5f, 19, 0));
    Zelement_set_min_size(card, 100, 48);
    Zelement_set_max_size(card, 400, Z_INFINITE);
    Zelement_set_horizontal_alignment(card, Z_HALIGN_LEFT);
    Zelement_on(card, Z_EVENT_LOADED, on_loaded, NULL);
    return card;
}
```

### Notes

- Margin is outside the element; `ActualWidth` and `Zelement_get_bounds` exclude it,
  `Zelement_get_desired_size` includes it.
- The default alignment of an element is stretch (`Z_HALIGN_STRETCH` is documented
  as "the default of an element: takes all the space of its slot").
- A panel with the default `Z_AUTO` size and stretch alignment fills its slot.

## Render transforms, animation and tilt

A render transform changes how an element and its content are drawn, not the
layout, so a rotation or a slide costs no new Measure or Arrange.

- Only the OpenGL renderer draws them; the software renderer shows the element
  untransformed.
- `Zroot_set_animations` (`host.h`) decides whether `Zelement_animate` moves or jumps.
- A transformed element is hit tested where layout put it. ClipToBounds of what
  is inside it is not applied while it is rotated.

| Function | Meaning |
| --- | --- |
| `Zelement_set_projection(element, rotation_x, rotation_y, center_x, center_y)` | PlaneProjection: rotation (degrees) about the X and Y axes through a point of the element. `center_x`, `center_y` are fractions of its size (0.5, 0.5 is the middle; 0 is the left or top edge). The viewer is a 34 degree field of view away. |
| `Zelement_set_render_scale(element, scale)` | ScaleTransform about the centre of the projection. 1 is no scale. |
| `Zelement_set_translate(element, x, y)` | TranslateTransform in logical pixels. |
| `Zelement_get_property(element, ZAnimatedProperty)` | Current value of an animatable property. |
| `Zelement_animate(element, property, to, duration_ms, easing, delay_ms)` | Moves a property to `to` from where it is now. A new animation of the same property replaces the running one. With animations off, or an element not in a root, the property takes `to` at once. |
| `Zelement_is_animating(element)` | True while an animation of the element runs. |
| `Zeasing(easing, t)` | The easing curve itself: t in [0, 1] to [0, 1]. |
| `Zelement_set_tilt(element, enabled)` | Tilt: while pressed the element leans toward the finger (WP's TiltEffect). On by default for list and grid items (not for FlipView), group headers, hyperlinks, check boxes, radio buttons and menu flyout items; not for Button. |

`ZAnimatedProperty`: `Z_PROP_OPACITY`, `Z_PROP_ROTATION_X`, `Z_PROP_ROTATION_Y`
(degrees), `Z_PROP_SCALE`, `Z_PROP_TRANSLATE_X`, `Z_PROP_TRANSLATE_Y`.

`ZEasing`:

| Value | Curve |
| --- | --- |
| `Z_EASE_LINEAR` | Linear. |
| `Z_EASE_OUT` | Cubic: quick start, soft end. |
| `Z_EASE_IN` | Cubic: slow start. |
| `Z_EASE_EMPHASIZED` | Cubic Bezier (0.2, 0.8, 0.2, 1): a quick start and a long soft settle. |

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void fade_in(Zid element) {
    Zelement_set_opacity(element, 0.0f);
    Zelement_animate(element, Z_PROP_OPACITY, 1.0f, 300.0f, Z_EASE_OUT, 0.0f);
}
```

## Panel (common to all panels)

**What it is.** The abstract base of StackPanel, WrapPanel, Grid, Canvas and the
items panels (WPF/WinRT `Panel`). It cannot be created directly.

**Properties and functions.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zpanel_add(panel, child)` | Appends a child (Children.Add). A child that has a parent is moved here. Returns `child`, so calls nest; `Z_INVALID_ID` when a handle is invalid or `child` is the panel or one of its ancestors (`Z_ERROR_CYCLE`, nothing changes). | |
| `Zpanel_insert(panel, index, child)` | Inserts a child at an index; an index outside 0..count appends. Otherwise as `Zpanel_add`. | |
| `Zpanel_remove(panel, child)` | Detaches a child without destroying it. Nothing happens when it is not a child of this panel. | |
| `Zpanel_clear(panel)` | Destroys all the children. | |
| `Zpanel_get_count(panel)` / `Zpanel_get_child(panel, index)` | Read the children, in order of insertion (`Z_INVALID_ID` out of range). | |
| `Zpanel_set_background(panel, color)` | Panel.Background. | none: a panel without a background lets the pointer through. Once set, even to a transparent color, it catches the pointer |
| `Zpanel_set_zindex(child, zindex)` | Panel.ZIndex (attached): higher values are drawn above and hit first; equal values keep the order of the children. | 0 |

**Notes.** `Zpanel_remove` hands the child back to the caller; `Zpanel_clear` and
destroying the panel destroy the children.

## StackPanel

**What it is.** Arranges children in one line, horizontal or vertical (WP8.1
`StackPanel`).

**Create.**

```c
Zid Zstack_panel_create(ZOrientation orientation);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zstack_panel_set_orientation` / `Zstack_panel_get_orientation` | `Z_HORIZONTAL` or `Z_VERTICAL`. | the value passed to create |

**Events.** None of its own (routed events of `Zelement_on`).

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_column(void) {
    Zid stack = Zstack_panel_create(Z_VERTICAL);
    Zelement_set_margin(stack, Zthickness(19, 32, 19, 0));
    Zpanel_add(stack, Ztext_block_create("one"));
    Zpanel_add(stack, Ztext_block_create("two"));
    return stack;
}
```

**Notes.** For a scrolling column put the stack inside a ScrollViewer
([scrolling.md](scrolling.md)).

## VirtualizingStackPanel

**What it is.** A StackPanel for the items of a ListBox.

**Create.**

```c
Zid Zvirtualizing_stack_panel_create(void);
```

**Properties.** As StackPanel (orientation through the StackPanel setters).

**Events.** None of its own.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_item_panel(void) {
    return Zvirtualizing_stack_panel_create();
}
```

**Notes.** This implementation measures and arranges every container but paints
only the ones in view. A list of thousands of rows scrolls well, but Measure is
linear in the number of items.

## ItemsStackPanel

**What it is.** The panel of a ListView (WP8.1 `ItemsStackPanel`): a StackPanel
for items.

**Create.**

```c
Zid Zitems_stack_panel_create(ZOrientation orientation);
```

**Properties.** `Zstack_panel_set_orientation` sets its Orientation (vertical by
default for a ListView).

**Notes.** It lays out every container, as the VirtualizingStackPanel does. See
[items.md](items.md) for ListView.

## ItemsWrapGrid

**What it is.** The panel of a GridView (WP8.1 `ItemsWrapGrid`): the items go in
cells of one size. Horizontal fills a row left to right and then starts the next
row below; Vertical fills a column top to bottom and then starts the next one to
the right.

**Create.**

```c
Zid Zitems_wrap_grid_create(ZOrientation orientation);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zitems_wrap_grid_set_orientation` / `Zitems_wrap_grid_get_orientation` | Fill direction. | value passed to create |
| `Zitems_wrap_grid_set_maximum_rows_or_columns` / `Zitems_wrap_grid_get_maximum_rows_or_columns` | The most cells in a line, along the orientation. | -1: as many as fit |
| `Zitems_wrap_grid_set_item_width` / `Zitems_wrap_grid_get_item_width` | ItemWidth. | `Z_AUTO`: size of the first item |
| `Zitems_wrap_grid_set_item_height` / `Zitems_wrap_grid_get_item_height` | ItemHeight. | `Z_AUTO`: size of the first item |

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_tiles_panel(void) {
    Zid grid = Zitems_wrap_grid_create(Z_HORIZONTAL);
    Zitems_wrap_grid_set_item_width(grid, 173.0f);
    Zitems_wrap_grid_set_item_height(grid, 173.0f);
    Zitems_wrap_grid_set_maximum_rows_or_columns(grid, 2);
    return grid;
}
```

**Notes.** A row (column) has as many cells as fit, at most
MaximumRowsOrColumns. The header item of a group takes a line of its own.

## WrapPanel

**What it is.** Lays children out in a line and wraps to the next line when the
room ends (Silverlight Toolkit `WrapPanel`).

**Create.**

```c
Zid Zwrap_panel_create(ZOrientation orientation);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zwrap_panel_set_orientation(panel, orientation)` | `Z_HORIZONTAL` or `Z_VERTICAL`. | value passed to create |
| `Zwrap_panel_set_item_size(panel, item_width, item_height)` | Every child gets this size; `Z_AUTO` keeps its own. | `Z_AUTO` |

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_wrap(void) {
    Zid wrap = Zwrap_panel_create(Z_HORIZONTAL);
    Zwrap_panel_set_item_size(wrap, 100.0f, Z_AUTO);
    Zpanel_add(wrap, Ztext_block_create("a"));
    Zpanel_add(wrap, Ztext_block_create("b"));
    return wrap;
}
```

## Grid

**What it is.** Rows and columns (WP8.1 `Grid`). Children are placed in cells
with attached properties.

**Create.**

```c
Zid Zgrid_create(void);
```

**Properties and functions.**

| Functions | Meaning |
| --- | --- |
| `int Zgrid_add_column(grid, ZGridUnitType unit, float value)` | Adds a ColumnDefinition and returns its index. `value` is the width in pixels for `Z_GRID_PIXEL`, the weight for `Z_GRID_STAR` ("2*" is 2), ignored for `Z_GRID_AUTO`. |
| `int Zgrid_add_row(grid, unit, value)` | The same for rows. |
| `Zgrid_set_column_limits(grid, column, min_width, max_width)` | MinWidth / MaxWidth of a column. |
| `Zgrid_set_row_limits(grid, row, min_height, max_height)` | MinHeight / MaxHeight of a row. |
| `Zgrid_get_column_count` / `Zgrid_get_row_count` | Number of definitions. |
| `Zgrid_get_column_width(grid, column)` / `Zgrid_get_row_height(grid, row)` | Size after the last Arrange. |
| `Zgrid_set_row(child, row)`, `Zgrid_set_column(child, column)` | Grid.Row, Grid.Column of a child (attached). |
| `Zgrid_set_row_span(child, span)`, `Zgrid_set_column_span(child, span)` | Grid.RowSpan, Grid.ColumnSpan. |
| `Zgrid_get_row(child)` / `Zgrid_get_column(child)` | Read the attached values. |
| `Zid Zgrid_place(grid, child, column, row)` | Adds `child` at a cell: the usual three lines in one. |

`ZGridUnitType`: `Z_GRID_AUTO`, `Z_GRID_PIXEL`, `Z_GRID_STAR`.

Limits and defaults:

- A grid holds at most 256 column and 256 row definitions; the add functions return -1 past that (and for an invalid handle).
- A star `value` of 0 or less counts as 1.
- A grid with no column (or no row) has one star column (row). `Zgrid_get_column_count` still gives 0, and `Zgrid_get_column_width(grid, 0)` is the whole grid.
- A row or column below 0 is 0, a span below 1 is 1, and a cell or span past the last definition is cut to it. The size getters give 0 for an index out of range.
- `Zgrid_place` returns `Z_INVALID_ID` and leaves the child as it was (its attached column and row too) when `grid` is not a Grid, `child` is not an element, the add is refused (a cycle) or there is no memory; otherwise it returns what `Zpanel_add` returns. The cell is set after the child is added.

**Events.** None of its own.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_form(void) {
    Zid grid = Zgrid_create();
    Zgrid_add_row(grid, Z_GRID_AUTO, 0);
    Zgrid_add_row(grid, Z_GRID_STAR, 1);
    Zgrid_add_column(grid, Z_GRID_PIXEL, 120);
    Zgrid_add_column(grid, Z_GRID_STAR, 1);

    Zgrid_place(grid, Ztext_block_create("Name"), 0, 0);
    Zid value = Ztext_block_create("value");
    Zgrid_place(grid, value, 1, 0);
    Zgrid_set_row_span(value, 2);
    return grid;
}
```

**Notes.** Attached properties are ignored when the parent is not a Grid.

## Canvas

**What it is.** Absolute positioning (WP8.1 `Canvas`).

**Create.**

```c
Zid Zcanvas_create(void);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zcanvas_set_left(child, left)` / `Zcanvas_set_top(child, top)` | Canvas.Left / Canvas.Top (attached). | `Z_AUTO`, which is 0 |
| `Zid Zcanvas_place(canvas, child, left, top)` | Adds `child` at a position. Returns `Z_INVALID_ID` and leaves the child as it was (its attached Left and Top too) when `canvas` is not a Canvas, `child` is not an element, the add is refused or there is no memory; otherwise as `Zpanel_add`. | |

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_canvas(void) {
    Zid canvas = Zcanvas_create();
    Zid label = Ztext_block_create("at 40, 60");
    Zcanvas_place(canvas, label, 40.0f, 60.0f);
    return canvas;
}
```

## Border

**What it is.** One child with a background, a border and rounded corners (WP8.1
`Border`).

**Create.**

```c
Zid Zborder_create(void);
```

**Properties.**

| Functions | Meaning |
| --- | --- |
| `Zborder_set_child(border, child)` / `Zborder_get_child(border)` | Border.Child. Replacing a child destroys the old one (and its subtree); `Z_INVALID_ID` leaves the border empty. A `child` that has a parent is moved here, also when it lies inside the old child; the border itself or one of its ancestors is refused (`Z_ERROR_CYCLE`) and the old child stays. |
| `Zborder_set_background(border, color)` | Background color. |
| `Zborder_set_border_brush(border, color)` | BorderBrush color. |
| `Zborder_set_border_thickness(border, thickness)` | `ZThickness`. |
| `Zborder_set_corner_radius(border, ZCornerRadius radius)` | Corner radius (top_left, top_right, bottom_right, bottom_left). |
| `Zborder_set_padding(border, thickness)` | Padding. |
| `Zborder_bind_background(border, key)`, `Zborder_bind_border_brush(border, key)` | Bind to a color theme resource (`ZThemeResource`). |
| `Zborder_bind_border_thickness(border, key)`, `Zborder_bind_padding(border, key)` | Bind to a Thickness theme resource. |

Bindings return false for a key of the wrong type. The direct setter removes the
binding.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_framed_text(void) {
    Zid border = Zborder_create();
    Zborder_set_border_brush(border, Zcolor_argb(0xFFFFFFFF));
    Zborder_set_border_thickness(border, Zthickness_uniform(2));
    Zborder_set_padding(border, Zthickness_uniform(12));
    Zborder_set_child(border, Ztext_block_create("framed"));
    return border;
}
```

## Viewbox

**What it is.** Scales one child to fit its slot (WP8.1 `Viewbox`).

**Create.**

```c
Zid Zviewbox_create(void);
```

**Properties.**

| Functions | Meaning |
| --- | --- |
| `Zviewbox_set_child(viewbox, child)` | The child. Replaces and destroys the old one, as `Zborder_set_child`. The child is laid out at its natural size and painted scaled; a Viewbox is not hit tested, it only shows. |
| `Zviewbox_set_stretch(viewbox, ZStretch)` | `Z_STRETCH_NONE` (original size), `Z_STRETCH_FILL` (the content is drawn at the smaller of the two scales, centred; the ratio is lost only for an Image), `Z_STRETCH_UNIFORM` (largest size that fits, ratio kept), `Z_STRETCH_UNIFORM_TO_FILL` (smallest size that covers, ratio kept, cropped). |
| `Zviewbox_set_stretch_direction(viewbox, ZStretchDirection)` | `Z_STRETCH_UP_ONLY`, `Z_STRETCH_DOWN_ONLY`, `Z_STRETCH_BOTH` (default; the default stretch is `Z_STRETCH_UNIFORM`). |

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_scaled(void) {
    Zid box = Zviewbox_create();
    Zviewbox_set_stretch(box, Z_STRETCH_UNIFORM);
    Zviewbox_set_stretch_direction(box, Z_STRETCH_DOWN_ONLY);
    Zviewbox_set_child(box, Ztext_block_create("fits"));
    return box;
}
```

## Popup

**What it is.** Shows its child above the rest of the page (WP8.1 `Popup`). It
takes no room in the layout. A ComboBox drops down in one, and the flyouts and
the ContentDialog are shown in one.

**Create.**

```c
Zid Zpopup_create(void);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zpopup_set_child(popup, child)` | The child; replaces and destroys the old one, as `Zborder_set_child`. It is drawn in the overlay of the root, only while the popup is open, and the tree below does not hit test it. | |
| `Zpopup_set_offset(popup, horizontal_offset, vertical_offset)` | Offset from the top left corner of its root (or from the placement target). | |
| `Zpopup_set_placement_target(popup, target, ZPlacementMode mode)` | The popup opens next to `target`. Modes: `Z_PLACE_BOTTOM`, `Z_PLACE_TOP`, `Z_PLACE_LEFT`, `Z_PLACE_RIGHT`, `Z_PLACE_MOUSE`. Bottom and Top flip to the other side when the popup would leave the screen; `Z_PLACE_MOUSE` is laid out like `Z_PLACE_BOTTOM`. With no target, or a destroyed one, only the offset counts. The popup is kept inside the root. | `Z_PLACE_BOTTOM` |
| `Zpopup_set_open(popup, open)` / `Zpopup_is_open(popup)` | IsOpen. It changes at once and raises `Z_EVENT_POPUP_OPENED` or `Z_EVENT_POPUP_CLOSED` (the handler may destroy the popup). | closed |
| `Zpopup_set_stays_open(popup, stays_open)` | StaysOpen. False (the default of every Popup) closes the popup when the pointer is pressed outside it, and on Back or Escape. | false |

**Events.** `Z_EVENT_POPUP_OPENED` and `Z_EVENT_POPUP_CLOSED` through
`Zelement_on` (callback `ZRoutedCallback`).

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_opened(Zid sender, void* user_data) {
    (void)sender;
    (void)user_data;
}

void show_popup(Zid page_panel, Zid anchor) {
    Zid popup = Zpopup_create();
    Zpanel_add(page_panel, popup);
    Zpopup_set_child(popup, Ztext_block_create("hello"));
    Zpopup_set_placement_target(popup, anchor, Z_PLACE_BOTTOM);
    Zpopup_set_stays_open(popup, false);
    Zelement_on(popup, Z_EVENT_POPUP_OPENED, on_opened, NULL);
    Zpopup_set_open(popup, true);
}
```

**Notes.** A popup is shown only while it is in the tree of a root. `IsOpen`
changes at once, but a popup that is not in such a tree appears as soon as it
gets into one. In the example `page_panel` is a panel that is already in the page;
the popup is added to it before it is opened.

Back to the [documentation index](index.md)
