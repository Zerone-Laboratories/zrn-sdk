/**
 * zrnui - listview.h
 * ListView and GridView (controls/listview.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/controls/items.h"

/** ListViewBase (listview.c). */
typedef struct zrn_list_view_base {
	zrn_items_control_t base;
	/** IsItemClickEnabled: on_item_click is called after a tap. */
	bool b_item_click_enabled;
	/** IsSwipeEnabled: only stored and returned, nothing reads it. */
	bool b_swipe_enabled;
	/** The ContentControl holders in the presenter (header above the items, footer below), Z_INVALID_ID when there is none. */
	Zid header, footer;
	/** Called with the index after the selection change of a tap; on_item_click_data is its user data. */
	ZItemClickCallback on_item_click;
	void* on_item_click_data;
} zrn_list_view_base_t;

/** The tap on an item of a ListView or a GridView: moves the keyboard item to it, selects as the mode says, then calls on_item_click when enabled. A handler may destroy the list. */
void zrn_lvb_item_tapped(zrn_items_control_t* o, int index);
