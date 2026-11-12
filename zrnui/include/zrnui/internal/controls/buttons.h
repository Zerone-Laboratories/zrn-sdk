/**
 * zrnui - buttons.h
 * The buttons (controls/buttons.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/controls/control.h"

typedef struct zrn_button_base {
	zrn_content_control_t base;
	/** When Click is raised: on press, on release or on hover. */
	ZClickMode click_mode;
	/** Held by the pointer or by Space and Enter; IsPressed also needs b_pointer_over. */
	bool b_pressed;
	/** The pointer is on the button; it is updated while held too, as the capture keeps the events coming. */
	bool b_pointer_over;
	/** Held by Space or Enter. */
	bool b_keyboard_pressed;
	/** RepeatButton: Click is raised on press, again after delay_ms and then every interval_ms while held (interval_ms 0: once per tick). */
	bool b_repeat;
	/** RepeatButton.Delay and Interval, in ms, never negative. */
	int delay_ms, interval_ms;
	/** ms since the press or the last repeat. */
	float repeat_timer;
	/** The delay has passed: the repeats have begun. */
	bool b_repeating;
	/** HyperlinkButton.NavigateUri: an owned copy, NULL when unset; the library never opens it. */
	char* uri;
	/** Button.Flyout: a child of the button (destroyed with it), shown by a tap; NULL when none. */
	zrn_element_t* flyout;
} zrn_button_base_t;

typedef struct zrn_toggle_button {
	zrn_button_base_t base;
	/** Z_INDETERMINATE only when b_three_state allows the user to reach it (the program can always set it). */
	ZToggleState state;
	bool b_three_state;
	/** RadioButton group, an owned copy: with NULL or "" the radios that share a parent are one group. */
	char* group_name;
} zrn_toggle_button_t;

extern zrn_class_t zrn_class_button, zrn_class_toggle_button;
