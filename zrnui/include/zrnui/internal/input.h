/**
 * zrnui - input.h
 * What an input event is.
 */

#pragma once

#include "zrnui/element.h"

/**
 * What cls->input receives. ENTER and LEAVE go to the elements that gained or lost the pointer, the PREVIEW kinds only to ancestors that
 * observe the pointer.
 */
typedef enum zrn_input_type {
	ZRN_IN_POINTER_DOWN = 1,
	ZRN_IN_POINTER_UP,
	ZRN_IN_POINTER_MOVE,
	ZRN_IN_POINTER_ENTER,
	ZRN_IN_POINTER_LEAVE,
	ZRN_IN_WHEEL,
	ZRN_IN_KEY_DOWN,
	ZRN_IN_KEY_UP,
	ZRN_IN_TEXT,
	/** FOCUS_IN and FOCUS_OUT come before the GotFocus and LostFocus events. */
	ZRN_IN_FOCUS_IN,
	ZRN_IN_FOCUS_OUT,
	/** Sent to the element that held the capture when another one takes it (zrn_set_capture); not when it releases it. */
	ZRN_IN_CAPTURE_LOST,
	/** Sent to ancestors that observe the pointer before the target sees the event; they may take the capture (a scroll viewer taking a drag from a button). */
	ZRN_IN_PREVIEW_DOWN,
	ZRN_IN_PREVIEW_MOVE,
	ZRN_IN_PREVIEW_UP
} zrn_input_type_t;

typedef struct zrn_input {
	zrn_input_type_t type;
	/** Pointer position in the coordinates of the element receiving it, in logical px (ENTER and LEAVE carry root coordinates). */
	float x, y;
	/** The ZKey of KEY_DOWN and KEY_UP. */
	int key;
	/** Z_MOD_* bit mask. */
	int mods;
	/** UTF-8 of ZRN_IN_TEXT, valid only during the call. */
	const char* text;
	/** Wheel amount; positive scrolls toward the start of the content. */
	float delta;
} zrn_input_t;
