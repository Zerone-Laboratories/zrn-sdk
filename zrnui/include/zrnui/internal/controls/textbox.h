/**
 * zrnui - textbox.h
 * TextBox (controls/textbox.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/controls/control.h"
#include "zrnui/internal/input.h"

typedef struct zrn_text_box {
	zrn_control_t base;
	/** Owned UTF-8, never NULL after creation; length_bytes is its length, kept in step. */
	char* text;
	int length_bytes;
	/** Byte offsets into text: the selection is between anchor and caret (equal: none). */
	int caret;
	int anchor;
	/** Multi-line: Enter inserts a newline; otherwise newlines are dropped from what is typed or set. */
	bool b_accepts_return;
	ZTextWrapping wrapping;
	bool b_read_only;
	/** In characters, not bytes; 0 is unlimited. */
	int max_length;
	/** placeholder and header are owned, NULL when unset. */
	char* placeholder;
	char* header;
	bool b_password;
	/** The code point shown for each character of a password box; 0 means U+25CF. */
	uint32_t password_char;
	/** ms since the last edit or move; the caret blinks with a period of 2 x 530 ms. */
	float caret_blink;
	/** How far the text is scrolled inside the box, in logical px. */
	float scroll_x, scroll_y;
	/** A pointer drag is extending the selection. */
	bool b_selecting;
	/** Set while a program changes the text, to not fire TextChanged twice. */
	bool b_quiet;
	/** true while TextChanged is raised for a change the user typed. */
	bool b_from_user;
	/** Called first for a key (an AutoSuggestBox takes Up, Down, Enter and Escape); true when it took the key. */
	int (*key_hook)(struct zrn_text_box* box, zrn_input_t* in);
} zrn_text_box_t;
