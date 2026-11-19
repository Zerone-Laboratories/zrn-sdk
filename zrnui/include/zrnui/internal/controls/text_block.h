/**
 * zrnui - text_block.h
 * TextBlock (controls/basic.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/element.h"

typedef struct zrn_text_block {
	zrn_element_t base;
	/** An owned copy; the cached layout points into it. */
	char* text;
	/** font_size, font_family and foreground count only when their ZRN_TB_ bit is in set_flags; otherwise they are inherited (zrn_inherited_*). */
	float font_size;
	ZFontFamily font_family;
	ZColor foreground;
	unsigned set_flags;
	/** A foreground that follows the theme: 0 none, 1 subtle, 2 contrast, 3 accent. */
	int fg_role;
	ZTextWrapping wrapping;
	ZTextAlignment alignment;
	ZTextTrimming trimming;
	/** 0 is unlimited. */
	int max_lines;
	/** Logical px, 0 for the font's own. */
	float line_height;
	/** In thousandths of an em. */
	int character_spacing;
	/* cache of the last layout */
	/** The cached zrn_text_layout_t (owned), NULL when it must be made again. */
	void* layout;
	/** The width the layout was made for, 0 when the text does not wrap. */
	float layout_width;
} zrn_text_block_t;

/** Bits of zrn_text_block_t.set_flags. */
enum {
	ZRN_TB_FONT_SIZE = 1,
	ZRN_TB_FONT_FAMILY = 2,
	ZRN_TB_FOREGROUND = 4
};
