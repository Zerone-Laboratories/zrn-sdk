/**
 * zrnui - scroll.h
 * ScrollViewer (controls/scroll.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/controls/control.h"

typedef struct zrn_scroll_viewer {
	zrn_content_control_t base;
	ZScrollBarVisibility v_visibility, h_visibility;
	/** The scroll position in logical px, kept within [0, extent - viewport] by arrange and by scrolling. */
	double offset_x, offset_y;
	/** The size of the content and the size of the viewer; both are set by arrange. */
	double extent_w, extent_h, viewport_w, viewport_h;
	/** A bar is shown (set by arrange); it still needs bar_visible > 0 unless the bar is Visible. */
	bool b_show_v;
	bool b_show_h;
	/** Milliseconds the bars still stay visible after a scroll (WP8 shows them only while scrolling); counted down by tick. */
	float bar_visible;
	/** A drag owns the pointer: the viewer holds the capture. */
	bool b_dragging;
	/** Where the press began and the offset then: a drag sets the offset from them. */
	float drag_start_y, drag_start_x;
	double drag_offset_y, drag_offset_x;
	/** The finger passed the 10 px drag threshold. */
	bool b_drag_moved;
	/** Velocity of a flick, px/s. */
	float velocity_x, velocity_y;
	/** The previous pointer position, for the velocity. */
	float last_move_x, last_move_y;
	/** Room added to the extent along x, for a content that goes round (the looping hub). */
	double extra_extent_x;
	/** Called with the offset along the scrolling axis; on_scroll_data is its user data. */
	ZScrollCallback on_scroll;
	void* on_scroll_data;
} zrn_scroll_viewer_t;

/** Moves the horizontal offset by delta with no event, the inertia and a drag in progress going on as if nothing happened. It does not clamp the offset or ask for a layout. */
void zrn_scroll_viewer_shift_x(zrn_element_t* e, double delta);
/** Room added to the horizontal extent (the content is not laid out in it); the offset is not changed. */
void zrn_scroll_viewer_set_extra_extent_x(zrn_element_t* e, double extra);
