/**
 * zrnui - popup.h
 * Popup (controls/basic.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/element.h"

typedef struct zrn_popup {
	zrn_element_t base;
	/** Listed in the root while open; zrn_popup_set_open raises PopupOpened and PopupClosed. */
	bool b_is_open;
	/** A press outside and Back do not close it. */
	bool b_stays_open;
	/** Logical px added to its position. */
	float h_offset, v_offset;
	/** The element it is placed against, Z_INVALID_ID to use the offsets alone. */
	Zid target;
	ZPlacementMode placement;
	/** A modal popup (a ContentDialog) dims what is under it and blocks input to it. */
	bool b_modal;
	/** As wide as the root, whatever its child asks for (a flyout on the phone). */
	bool b_full_width;
} zrn_popup_t;

/** The same as zrn_popup_set_open(popup, 0). */
void zrn_popup_close(zrn_element_t* popup);
/**
 * Opens or closes it: in a root it is listed or unlisted and PopupOpened or PopupClosed is raised (a handler may destroy the popup). Does
 * nothing when the state is the same.
 */
void zrn_popup_set_open(zrn_element_t* popup, int open);

extern zrn_class_t zrn_class_popup;
