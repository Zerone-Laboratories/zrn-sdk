/**
 * zrnui - flyout.h
 * FlyoutBase (controls/flyout.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/element.h"

/** FlyoutBase: the Popup it shows in is its one child; the presenter is the child of the popup. */
typedef struct zrn_flyout_base {
	zrn_element_t base;
	zrn_element_t* popup;
	/** Where it opens relative to the target; Z_FLYOUT_PLACEMENT_FULL covers the whole root. */
	ZFlyoutPlacementMode placement;
	/** The element it was last shown at, Z_INVALID_ID before the first showing. */
	Zid target;
	/** true from showing until the popup is closed (a tap outside, Back, Hide or an item). */
	bool b_is_open;
	/** What a flyout class does when it is about to show, and when it has been dismissed (pickers.c): called before Opening and before Closed. */
	void (*on_show)(zrn_element_t* flyout);
	void (*on_hide)(zrn_element_t* flyout);
} zrn_flyout_base_t;

/**
 * Shows `flyout` at `target`, which must be in a tree with a root (else Z_ERROR_INVALID_ARGUMENT). Does nothing when it is open there already; when it is
 * open elsewhere it is closed first. Raises Opening and Opened: a handler may destroy either element, so look them up again.
 */
void zrn_flyout_show_at(zrn_element_t* flyout, zrn_element_t* target);

extern zrn_class_t zrn_class_flyout_base;
