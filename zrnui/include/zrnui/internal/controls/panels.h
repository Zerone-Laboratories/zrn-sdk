/**
 * zrnui - panels.h
 * The panels and Border (controls/panels.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/element.h"

/** A Panel paints its background only when b_has_background; one without it is also not hit itself (only its children are). */
typedef struct zrn_panel {
	zrn_element_t base;
	ZColor background;
	bool b_has_background;
} zrn_panel_t;

typedef struct zrn_stack_panel {
	zrn_panel_t base;
	ZOrientation orientation;
} zrn_stack_panel_t;

typedef struct zrn_wrap_panel {
	zrn_panel_t base;
	ZOrientation orientation;
	float item_width, item_height;
} zrn_wrap_panel_t;

/** A Border exists only through its brushes: with neither b_has_background nor b_has_border_brush it paints nothing and is not hit itself. */
typedef struct zrn_border {
	zrn_element_t base;
	ZColor background, border_brush;
	bool b_has_background;
	bool b_has_border_brush;
	ZThickness border_thickness, padding;
	ZCornerRadius corner_radius;
} zrn_border_t;

/** ItemsWrapGrid (panels.c). */
typedef struct zrn_items_wrap_grid {
	zrn_panel_t base;
	ZOrientation orientation;
	/** A value <= 0 (stored as -1) means no limit; item_width and item_height are Z_AUTO to take the size of the first item. */
	int max_rows_or_columns;
	float item_width, item_height;
} zrn_items_wrap_grid_t;
