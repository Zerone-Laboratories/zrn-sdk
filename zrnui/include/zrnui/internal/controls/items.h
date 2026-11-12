/**
 * zrnui - items.h
 * ItemsControl and Selector, the base of the lists (controls/items.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/controls/control.h"
#include "zrnui/internal/input.h"

/** The selection modes of the items controls: ListBox and ListView have all four (a ListBox has no None). */
typedef enum zrn_sel_mode {
	ZRN_SEL_NONE = 0,
	ZRN_SEL_SINGLE,
	ZRN_SEL_MULTIPLE,
	ZRN_SEL_EXTENDED
} zrn_sel_mode_t;

/** SelectorItem (items.c): a container of an item, ListBoxItem, ComboBoxItem, ListViewItem, GridViewItem, FlipViewItem. */
typedef struct zrn_selector_item {
	zrn_content_control_t base;
	/** IsSelected, 0 or 1. */
	int selected;
	/** Held by the pointer: a release inside the item is a tap. */
	bool b_pressed;
	/** Where the press began (local); written, but nothing reads it. */
	float press_x, press_y;
} zrn_selector_item_t;

/**
 * ItemsControl and Selector (items.c). The items are containers in `host`, the items panel, with the header
 * items of the groups between them (only a ListView or a GridView has any). `scroll` is the ScrollViewer
 * a ListBox, a ListView and a GridView show the panel in; `presenter` is what it holds: the header, the panel and the footer.
 */
typedef struct zrn_items_control {
	zrn_control_t base;
	zrn_element_t* scroll;
	zrn_element_t* host;
	zrn_element_t* presenter;
	/** The index of the first selected item, -1 for none; the selection functions keep it up to date. */
	int selected_index;
	/** ZRN_SEL_NONE for a plain ItemsControl, which selects nothing. */
	zrn_sel_mode_t sel_mode;
	/** The item the keyboard is on: its container, Z_INVALID_ID for none. */
	Zid focus_item;
	/** The keyboard moved it there: a tap gives the list the focus without showing where. */
	bool b_focus_visible;
	/** The header items of the groups in `host`: when 0 an item index is the index of its child. */
	int group_count;
} zrn_items_control_t;

/** items.c: what the panel of an items control needs to tell it when its orientation changes. */
void zrn_items_panel_changed(zrn_element_t* panel);

/* the parts of the items controls that the sources of the classes share (items.c) */
/** The items control that `item` sits in (the nearest ancestor that is one), NULL when none. */
zrn_items_control_t* zrn_item_owner(zrn_element_t* item);
/** The number of items; the header items of the groups are not counted. */
int zrn_ic_count(const zrn_items_control_t* o);
/** The container at `index` (headers not counted), NULL when out of range. Owned by the host panel. */
zrn_element_t* zrn_ic_item(const zrn_items_control_t* o, int index);
/** The index of a container, -1 when it is not an item of the host (a header, or elsewhere). */
int zrn_ic_index_of(const zrn_items_control_t* o, const zrn_element_t* container);
/**
 * Adds `item` or, when it is NULL, a row of `text`. A plain ItemsControl shows the element itself, a Selector wraps it in a container. Returns
 * the container, NULL on failure (an element that is in a tree already and needs a wrapper: Z_ERROR_HAS_PARENT). The control owns `item` on
 * success.
 */
zrn_element_t* zrn_ic_add(zrn_items_control_t* o, zrn_element_t* item, const char* text);
/** The text of a container when it shows a string, "" otherwise. Valid until that text changes. */
const char* zrn_container_text(zrn_element_t* container);
/** Selection by the program: the item alone (-1 none; an index out of range is -1). Raises SelectionChanged when the selection changed. Does nothing in mode None; a FlipView keeps one item selected. */
void zrn_ic_select(zrn_items_control_t* o, int index);
/** Selection by a tap on an item, as the mode says. Raises SelectionChanged. */
void zrn_ic_tap(zrn_items_control_t* o, int index);
/**
 * Changes the mode: going to None or Single trims the selection (SelectionChanged), and the rows are measured again because the check boxes
 * come or go.
 */
void zrn_ic_set_sel_mode(zrn_items_control_t* o, zrn_sel_mode_t mode);
/** Selects every item, in Multiple and Extended only. Raises SelectionChanged when something changed. */
void zrn_ic_select_all(zrn_items_control_t* o);
/** Writes up to `max` selected indices into `out` (NULL allowed) and returns how many are selected in all, which can be more than `max`. */
int zrn_ic_selected_indices(const zrn_items_control_t* o, int* out, int max);
/**
 * Replaces the items panel with `panel`, which must have no parent and is owned from now on. The items move to it and the old panel is
 * destroyed. On no memory nothing changes.
 */
void zrn_ic_set_host(zrn_items_control_t* o, zrn_element_t* panel);
/**
 * Scrolls the ScrollViewer until the item shows (Leading puts it at the start). Lays the root out first (Zroot_update_layout), so the control
 * must be in a root.
 */
void zrn_ic_scroll_into_view(zrn_items_control_t* o, int index, ZScrollIntoViewAlignment alignment);
/** The keys of a list: the arrows, Home and End move the item the keyboard is on, Space and Enter tap it, Ctrl+A selects all. true when the key was used. */
bool zrn_ic_key(zrn_element_t* e, zrn_input_t* in);
/** SelectorItem.IsSelected without any event and without updating the owner's selected_index, for the controls that keep it in step with a state of their own. */
void zrn_item_set_selected(zrn_element_t* item, int selected);

extern zrn_class_t zrn_class_items_control, zrn_class_selector, zrn_class_selector_item;
