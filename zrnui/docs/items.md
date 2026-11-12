# Items controls

Controls that show a collection of items: a plain items control, selectors (ListBox, ComboBox, FlipView), the list and grid views of the phone, the looping column used by the pickers, and SemanticZoom, which switches between two views of the same data. An item is a container (ListBoxItem, ListViewItem...) in the items panel; an item that is a string gets a TextBlock for content. Items are not virtualized: every container exists (thousands are fine), but only what is in view is painted.

Widgets on this page:

- [ItemsControl and Selector](#itemscontrol-and-selector) (shared functions)
- [ListBox](#listbox)
- [ListView and GridView](#listview-and-gridview)
- [ComboBox](#combobox)
- [FlipView](#flipview)
- [LoopingSelector](#loopingselector)
- [SemanticZoom](#semanticzoom)

Scrolling inside a list is covered in [scrolling.md](scrolling.md); panels used as `ItemsPanel` in [layout.md](layout.md). Setup of a root is in [getting-started.md](getting-started.md).

## ItemsControl and Selector

**What it is.** `ItemsControl` is the base of every list: `Items`, `ItemsPanel`. `Selector` adds `SelectedIndex` and `SelectionChanged`. The functions below work on a ListBox, ListView, GridView, ComboBox and FlipView alike (and `Zitems_control_get_count` / `Zitems_control_get_text` also on an AutoSuggestBox).

**Create**

```c
Zid Zitems_control_create(void);
```

A plain ItemsControl: its items are the elements themselves, in a StackPanel, with no container around them.

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zitems_control_add_text(ctl, text)` | Adds a string item; returns the container | |
| `Zitems_control_add(ctl, item)` | `Items.Add` of an element; wrapped in the container of the control unless it already is a SelectorItem. Returns the container, `Z_INVALID_ID` when nothing was added. In a Selector or an AutoSuggestBox an element that is to be wrapped must have no parent: otherwise it is refused with `Z_ERROR_HAS_PARENT` and stays where it is. The control owns the item from now on | |
| `Zitems_control_remove_at(ctl, index)` / `Zitems_control_clear(ctl)` | Remove one item or all; the containers are destroyed (`clear` also the group headers). An index out of range does nothing. SelectionChanged is raised when a selected item was removed | |
| `Zitems_control_get_count(ctl)` | Number of items (group headers are not counted) | |
| `Zitems_control_get_container(ctl, index)` | The container at an index | |
| `Zitems_control_index_from_container(ctl, container)` | Index of a container, -1 when it is not one of the items | |
| `Zitems_control_get_text(ctl, index)` | Text of a string item, or the text content of its container | |
| `Zitems_control_set_items_panel(ctl, panel)` / `Zitems_control_get_items_panel(ctl)` | `ItemsPanel`: `Zitems_stack_panel_create` or `Zitems_wrap_grid_create` (or a StackPanel, VirtualizingStackPanel, WrapPanel). The control takes ownership | ListView: ItemsStackPanel; GridView: ItemsWrapGrid |
| `Zitems_control_get_scroll_viewer(ctl)` | The ScrollViewer of a ListBox, ListView or GridView, and the one in the drop-down of a ComboBox or an AutoSuggestBox; `Z_INVALID_ID` for the others (a plain ItemsControl, a FlipView) | |
| `Zselector_set_selected_index(sel, index)` / `Zselector_get_selected_index(sel)` | `SelectedIndex`; -1 for none; with several items selected, the first. Setting it selects that item alone; an index out of range (below -1 or not below the count) deselects, as -1. With selection mode None nothing happens; a FlipView that has items cannot be left with none. SelectionChanged fires only when the selection changes | -1 |
| `Zselector_get_selected_container(sel)` | Container of `SelectedItem`, `Z_INVALID_ID` for none | |
| `Zselector_item_set_is_selected(item, on)` / `Zselector_item_get_is_selected(item)` | `SelectorItem.IsSelected`; selecting a container selects it in its control the way `SelectionMode` allows: Single deselects the others, Multiple and Extended just set the flag, None changes nothing; in Single, clearing the flag of the selected item deselects it. SelectionChanged is raised. A container that is not in a selector only stores the flag | |

**Events**

`SelectionChanged` is `Z_EVENT_SELECTION_CHANGED`, subscribed with `Zelement_on(element, event, ZRoutedCallback, user_data)`; the callback is `void (*)(Zid sender, void* user_data)`. One handler per event and element; `NULL` removes it. A handler may destroy the control.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void changed(Zid sender, void* user_data) {
	int i = Zselector_get_selected_index(sender);
	(void)i; (void)user_data;
}

Zid make_list(void) {
	Zid list = Zlist_box_create();
	Zitems_control_add_text(list, "one");
	Zitems_control_add_text(list, "two");
	Zselector_set_selected_index(list, 0);
	Zelement_on(list, Z_EVENT_SELECTION_CHANGED, changed, NULL);
	return list;
}
```

**Notes**

- The text of a string item is 12 px (the `ControlContentThemeFontSize` of the ListBox and ListView styles); use `Zcontrol_set_font_size` on the control or on a container to change it.
- The view scrolls along the orientation of the items panel: sideways for a horizontal stack and for a vertical wrap grid.

## ListBox

**What it is.** The Windows Phone 8.1 `ListBox`: a white (80%) box with black text; a row leans while pressed and fills with the accent when selected.

**Create**

```c
Zid Zlist_box_create(void);
Zid Zlist_box_item_create(const char* content);   // a container by itself, for Zitems_control_add
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zlist_box_set_selection_mode(box, mode)` / `Zlist_box_get_selection_mode(box)` | `ZSelectionMode`: `Z_SELECTION_SINGLE` (a tap selects), `Z_SELECTION_MULTIPLE` (a tap toggles), `Z_SELECTION_EXTENDED` (a tap selects that one only) | `Z_SELECTION_SINGLE` |
| `Zlist_box_get_selected_indices(box, indices, max)` | Writes up to `max` indices, ascending; returns how many are selected (also when more than `max`) | |
| `Zlist_box_select_all(box)` | `SelectAll` (Multiple and Extended) | |
| `Zlist_box_scroll_into_view(box, index)` | `ScrollIntoView` | |

**Events.** `Z_EVENT_SELECTION_CHANGED` via `Zelement_on`.

**Example**

```c
Zid box = Zlist_box_create();
Zlist_box_set_selection_mode(box, Z_SELECTION_MULTIPLE);
Zitems_control_add_text(box, "red");
Zitems_control_add_text(box, "green");
Zitems_control_add(box, Zlist_box_item_create("blue"));
int picked[8];
int n = Zlist_box_get_selected_indices(box, picked, 8);
(void)n;
```

**Notes**

- In the Light theme the 8.1 dictionary gives the box black 75% and the text black on black; here the text of a ListBox row is white in Light (the one departure from `themeresources.xaml`).

## ListView and GridView

**What it is.** The 8.1 `ListView` (items stacked) and `GridView` (items wrapped in an ItemsWrapGrid, rows left to right), both `ListViewBase`. Selection looks as in the templates: Single shows nothing (the program reads `SelectedIndex`); Multiple and Extended show a 40.5 px check box at the left of a list item and an accent border with a check in the corner of a grid item. A pressed item leans; a disabled one is at 40%.

**Create**

```c
Zid Zlist_view_create(void);
Zid Zgrid_view_create(void);
Zid Zlist_view_item_create(const char* content);   // container by itself, for Zitems_control_add
Zid Zgrid_view_item_create(const char* content);
```

**Properties** (the `Zlist_view_*` functions work on both a ListView and a GridView)

| Function | Meaning | Default |
|---|---|---|
| `Zlist_view_set_selection_mode(v, mode)` / `Zlist_view_get_selection_mode(v)` | `ZListViewSelectionMode`: `Z_LIST_VIEW_SELECTION_NONE`, `_SINGLE`, `_MULTIPLE`, `_EXTENDED`. Changing it clears a selection the new mode cannot hold | Single |
| `Zlist_view_set_is_item_click_enabled(v, on)` / `Zlist_view_get_is_item_click_enabled(v)` | `IsItemClickEnabled` | false |
| `Zlist_view_set_is_swipe_enabled(v, on)` / `Zlist_view_get_is_swipe_enabled(v)` | `IsSwipeEnabled`; only stored, nothing reads it (a swipe does not select) | true |
| `Zlist_view_get_selected_indices(v, indices, max)` | Up to `max` indices, ascending; returns how many are selected | |
| `Zlist_view_select_all(v)` | `SelectAll` (Multiple and Extended) | |
| `Zlist_view_set_header(v, text)` / `Zlist_view_set_header_content(v, element)` | Header above the items, inside the scrolling area; `""`/`NULL`/`Z_INVALID_ID` removes it. The view owns an element header | none |
| `Zlist_view_set_footer(v, text)` / `Zlist_view_set_footer_content(v, element)` | Footer below the items | none |
| `Zlist_view_scroll_into_view(v, index)` | Scrolls the least that brings the item into view | |
| `Zlist_view_scroll_into_view_with_alignment(v, index, alignment)` | `Z_SCROLL_INTO_VIEW_DEFAULT` or `Z_SCROLL_INTO_VIEW_LEADING` (item at the start of the viewport) | |
| `Zlist_view_add_group_header(v, title)` | Adds a group header item; the items that follow belong to it. Returns the header item | |
| `Zlist_view_get_group_count(v)` | Number of groups | |

**Events**

- `Z_EVENT_SELECTION_CHANGED` via `Zelement_on`.
- ItemClick: `Zlist_view_on_item_click(view, ZItemClickCallback, user_data)` with `void (*)(Zid sender, int index, void* user_data)`; `index` is the tapped item. One handler; `NULL` removes it. The handler may destroy the view.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void opened(Zid sender, int index, void* user_data) {
	(void)sender; (void)index; (void)user_data;
}

Zid make_settings(void) {
	Zid list = Zlist_view_create();
	Zlist_view_set_selection_mode(list, Z_LIST_VIEW_SELECTION_NONE);
	Zlist_view_set_is_item_click_enabled(list, true);
	Zlist_view_on_item_click(list, opened, NULL);
	Zlist_view_set_header(list, "settings");
	Zlist_view_add_group_header(list, "network");
	Zitems_control_add_text(list, "Wi-Fi");
	Zitems_control_add_text(list, "Cellular");
	return list;
}
```

**Notes**

- A tap: Single selects, Multiple toggles, Extended selects that one alone, None does nothing; then, when `IsItemClickEnabled` and a handler is set, ItemClick is raised, also in mode None. So ItemClick comes after the SelectionChanged of the tap, and it is skipped when a SelectionChanged handler removed the item (or destroyed the view). (The order is not in the SDK; this is what the library does.)
- Keyboard: arrow keys move through the items, Space or Enter acts as a tap, Ctrl+A selects all in Multiple and Extended.
- Indices count items only, not group headers. In a GridView a header takes a row of its own. There are no collapsing groups and no sticky headers.

## ComboBox

**What it is.** The inline Windows Phone 8.1 `ComboBox`: a 2.5 px outlined box with the selected item (or the placeholder); a tap opens a white list with an accent border over the box.

**Create**

```c
Zid Zcombo_box_create(void);
Zid Zcombo_box_item_create(const char* content);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zcombo_box_set_is_drop_down_open(box, on)` / `Zcombo_box_get_is_drop_down_open(box)` | `IsDropDownOpen` | false |
| `Zcombo_box_set_max_drop_down_height(box, h)` / `Zcombo_box_get_max_drop_down_height(box)` | `MaxDropDownHeight` in pixels; the list scrolls past it. 0 or less is `Z_INFINITE` | `Z_INFINITE` (keeps it to the screen) |
| `Zcombo_box_set_header(box, text)` / `Zcombo_box_get_header(box)` | Small text above the box; `""`/`NULL` removes it | none |
| `Zcombo_box_set_placeholder_text(box, text)` / `Zcombo_box_get_placeholder_text(box)` | Shown while nothing is selected | none |

Items and selection use the shared [Items and Selector functions](#itemscontrol-and-selector).

**Events.** `Z_EVENT_SELECTION_CHANGED`, `Z_EVENT_DROP_DOWN_OPENED`, `Z_EVENT_DROP_DOWN_CLOSED`, all via `Zelement_on(box, event, callback, user_data)` with `void (*)(Zid sender, void* user_data)`.

**Example**

```c
Zid box = Zcombo_box_create();
Zcombo_box_set_header(box, "country");
Zcombo_box_set_placeholder_text(box, "choose");
Zitems_control_add_text(box, "Norway");
Zitems_control_add_text(box, "Sweden");
Zelement_on(box, Z_EVENT_SELECTION_CHANGED, changed, NULL);   // changed() as in the example above
```

**Notes**

- A tap on an item, a tap outside or Escape closes the list.
- The full-screen picker the phone shows for more than five items is not made: the list is always inline.

## FlipView

**What it is.** The 8.1 `FlipView`: one item at a time, as wide and as high as the view; a finger drags the items sideways.

**Create**

```c
Zid Zflip_view_create(void);
Zid Zflip_view_item_create(const char* content);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zflip_view_set_use_touch_animations_for_all_navigation(fv, on)` / `Zflip_view_get_use_touch_animations_for_all_navigation(fv)` | false makes a change of `SelectedIndex` by the program jump instead of slide | true |

**Events.** `Z_EVENT_SELECTION_CHANGED` via `Zelement_on`, raised when the item in view changes.

**Example**

```c
Zid flip = Zflip_view_create();
Zid page = Zflip_view_item_create("first");
Zitems_control_add(flip, page);
Zitems_control_add_text(flip, "second");
Zselector_set_selected_index(flip, 1);
```

**Notes**

- Lets go on the next item when the finger went a fifth of the width or flicked; Left and Right keys go to the previous and next. With animations the snap takes 300 ms.
- It does not wrap round, and the flip buttons of the desktop FlipView are not drawn.

## LoopingSelector

**What it is.** The column of the phone's date and time pickers: items that go round, one in the middle. At rest only the chosen item shows, filled with the accent; while touched, the others come out as outlined boxes.

**Create**

```c
Zid Zlooping_selector_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zlooping_selector_add_item(ls, primary, secondary)` | Appends an item with a text and a smaller one under it (`NULL` or `""` for none); both are copied. The first item added becomes the selected one, without SelectionChanged. Returns the index, -1 on failure | |
| `Zlooping_selector_clear_items(ls)` | Removes all items; the selected index becomes -1, without SelectionChanged | |
| `Zlooping_selector_get_item_count(ls)` | | |
| `Zlooping_selector_get_item_text(ls, i)` / `Zlooping_selector_get_item_secondary_text(ls, i)` | | |
| `Zlooping_selector_set_selected_index(ls, i)` / `Zlooping_selector_get_selected_index(ls)` | -1 without items; a change by the program is at once and raises SelectionChanged when the index differs. An index out of range is clamped to the first or last item; without items nothing happens | |
| `Zlooping_selector_set_should_loop(ls, on)` / `Zlooping_selector_get_should_loop(ls)` | false stops the column at the first and last item | true |
| `Zlooping_selector_set_item_width(ls, w)` / `Zlooping_selector_get_item_width(ls)` | 0 or less is the size of the items as they come; the getter tells the size in use | 0 |
| `Zlooping_selector_set_item_height(ls, h)` / `Zlooping_selector_get_item_height(ls)` | same | 0 |

**Events.** SelectionChanged: `Zlooping_selector_on_selection_changed(ls, ZRoutedCallback, user_data)`, or `Zelement_on(ls, Z_EVENT_SELECTION_CHANGED, ...)`. Callback: `void (*)(Zid sender, void* user_data)`.

**Example**

```c
Zid col = Zlooping_selector_create();
Zlooping_selector_add_item(col, "1", "Mon");
Zlooping_selector_add_item(col, "2", "Tue");
Zlooping_selector_set_selected_index(col, 1);
Zlooping_selector_on_selection_changed(col, changed, NULL);   // changed() as above
```

**Notes**

- A drag has inertia and ends on an item, which becomes the chosen one when the column stops; a tap on an item goes to it; arrow keys and the wheel move by one. Without animations the column ends at once where the inertia would have taken it.
- Normally you do not create it yourself: the pickers own LoopingSelectors, reachable through `Zpicker_flyout_base_get_selector` ([pickers.md](pickers.md)).

## SemanticZoom

**What it is.** The 8.1 `SemanticZoom`: a zoomed-in view (a list of items) and a zoomed-out view (a list of its groups); one shows at a time, filling the control.

**Create**

```c
Zid Zsemantic_zoom_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zsemantic_zoom_set_zoomed_in_view(sz, view)` / `Zsemantic_zoom_set_zoomed_out_view(sz, view)` | Any elements (a ListView, a GridView); the control takes ownership and destroys the view it had. `Z_INVALID_ID` takes the view away (and destroys it). A view that has a parent is refused with `Z_ERROR_HAS_PARENT` and the current view stays | none |
| `Zsemantic_zoom_set_is_zoomed_in_view_active(sz, on)` / `Zsemantic_zoom_get_is_zoomed_in_view_active(sz)` | Setting it is a view change, with the events; nothing happens when that view shows already or when CanChangeViews is false | true |
| `Zsemantic_zoom_toggle_active_view(sz)` | `ToggleActiveView`: a view change to the other view, with the events | |
| `Zsemantic_zoom_set_can_change_views(sz, on)` | false keeps the view that shows: the setter of the active view and the toggle do nothing | true |
| `Zsemantic_zoom_set_is_zoom_out_button_enabled(sz, on)` | Only stored: the phone template has no button and nothing reads it | true |

**Events**

- `Zsemantic_zoom_on_view_change_started(sz, ZSemanticZoomCallback, user_data)`
- `Zsemantic_zoom_on_view_change_completed(sz, ZSemanticZoomCallback, user_data)`

Callback: `void (*)(Zid sender, bool is_source_zoomed_in_view, void* user_data)`; the flag says whether the view being left (that was left) is the zoomed-in one; it is the same in both events. One handler each; `NULL` removes it. A handler may destroy the control; when Started does, the view does not change and Completed is not raised.

**Example**

```c
static void jump(Zid sender, int index, void* user_data) {
	Zid sz = (Zid)(uintptr_t)user_data;   // pass the SemanticZoom as user data
	(void)sender; (void)index;
	Zsemantic_zoom_toggle_active_view(sz);
}

Zid make_zoom(void) {
	Zid sz = Zsemantic_zoom_create();
	Zid in = Zlist_view_create();
	Zid out = Zgrid_view_create();
	Zlist_view_set_is_item_click_enabled(out, true);
	Zlist_view_on_item_click(out, jump, (void*)(uintptr_t)sz);
	Zsemantic_zoom_set_zoomed_in_view(sz, in);
	Zsemantic_zoom_set_zoomed_out_view(sz, out);
	return sz;
}
```

**Notes**

- There is no pinch: the program toggles the view, usually from the ItemClick of the zoomed-out list.
- The zoomed-out view is shown in place of the zoomed-in one, not in a Popup.

Back to the [documentation index](index.md)
