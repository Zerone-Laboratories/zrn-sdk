/**
 * zrnui - text.h
 * Text layout, measuring and drawing (text.c).
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_painter zrn_painter_t;

/** A font request: `size` in logical px, `spacing` in thousandths of an em added after each glyph. */
typedef struct zrn_font {
	ZFontFamily family;
	float size;
	int spacing;
} zrn_font_t;

/** One line of a layout: the byte range [start, end) of the text (without the newline) and its width in logical px. */
typedef struct zrn_line {
	int start, end;
	float width;
} zrn_line_t;

/**
 * The lines of a text for a width. `text` is borrowed, `lines` owned. `width` is the widest line, `height` is line_count * line_height;
 * b_truncated: text was left out because of max_lines.
 */
typedef struct zrn_text_layout {
	zrn_font_t font;
	const char* text;
	zrn_line_t* lines;
	int line_count;
	bool b_truncated;
	float width, height, line_height;
} zrn_text_layout_t;

/** Starts the font module (zrn_fonts_init); false, with zrn_error, when there is no regular face. */
bool zrn_text_init(void);
/** Frees the glyph cache and shuts the font module down. */
void zrn_text_shutdown(void);
/**
 * Lays text out. `length` is in bytes (no NUL needed); `max_width` <= 0 is unlimited and `b_wrap` breaks at spaces; `max_lines` 0 is no limit; `line_height` <= 0 takes the
 * font's. The text must outlive the layout. NULL on no memory (no zrn_error). Free with zrn_text_layout_free.
 */
zrn_text_layout_t* zrn_text_layout_new(zrn_font_t font, const char* text, int length, float max_width, bool b_wrap, int max_lines, float line_height);
/** NULL is allowed. */
void zrn_text_layout_free(zrn_text_layout_t* l);
/** true when the font, or a fallback found on the system, has the glyph. */
bool zrn_font_has_glyph(zrn_font_t font, uint32_t cp);
/** The natural line height in logical px; 1.3 times the size when there is no face. */
float zrn_font_line_height(zrn_font_t font);
/** From the top of a line to the baseline, in logical px. */
float zrn_font_ascent(zrn_font_t font);
/** The width in logical px of the first `length` bytes of `s`, up to the first newline. */
float zrn_text_width(zrn_font_t font, const char* s, int length);
/** Byte offset in [0, length] nearest to `x` (logical px from the start of the string; newlines are not special). */
int zrn_text_index_at(zrn_font_t font, const char* s, int length, float x);
/** The byte index after the character at `index`; `length` when `index` is at or past the end. */
int zrn_utf8_next(const char* s, int length, int index);
/** The byte index of the character before `index` (0 at the start); `s` must be valid UTF-8 up to `index`. */
int zrn_utf8_prev(const char* s, int index);
/**
 * The code point at `s` (reading at most `length` bytes) and the bytes it took in *consumed. A bad or cut sequence gives U+FFFD with 1 byte;
 * `length` <= 0 gives 0 with 0.
 */
uint32_t zrn_utf8_decode(const char* s, int length, int* consumed);
/** Writes `cp` as UTF-8 into `out` (room for 4 bytes, no NUL is added) and returns the byte count. `cp` is not validated. */
int zrn_utf8_encode(uint32_t cp, char* out);

/**
 * Draws a layout with its top left at x, y (logical, local) in a room `width` wide, each line aligned by `align`; Z_TRIM_WORD_ELLIPSIS cuts a
 * line wider than `width` and ends it with an ellipsis.
 */
void zrn_draw_text(zrn_painter_t* p, const zrn_text_layout_t* l, float x, float y, float width, ZTextAlignment align, ZTextTrimming trim, ZColor c);
/** One run of a single line, no wrapping, with its top left at x, y (logical, local). */
void zrn_draw_string(zrn_painter_t* p, zrn_font_t font, const char* s, int length, float x, float y, ZColor c);
