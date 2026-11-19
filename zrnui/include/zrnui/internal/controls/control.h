/**
 * zrnui - control.h
 * Control and ContentControl, the base of every control (controls/basic.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/element.h"

/** Bits of zrn_control_t.set_flags: the values the program (or a ThemeResource binding) set. */
enum {
	ZRN_C_FOREGROUND = 1,
	ZRN_C_BACKGROUND = 2,
	ZRN_C_BORDER_BRUSH = 4,
	ZRN_C_BORDER_THICKNESS = 8,
	ZRN_C_PADDING = 16,
	ZRN_C_FONT_SIZE = 32,
	ZRN_C_FONT_FAMILY = 64
};

typedef struct zrn_control {
	zrn_element_t base;
	ZColor foreground, background, border_brush;
	/** ZRN_C_* bits: a color whose bit is clear falls back to the theme (zrn_ctl_*); the other values are always read. */
	unsigned set_flags;
	ZThickness border_thickness, padding;
	/** Logical px; TextBlocks inside inherit it and the font family. */
	float font_size;
	ZFontFamily font_family;
	/** How the content is aligned inside the control (HorizontalContentAlignment and the vertical one); centered by default. */
	ZHorizontalAlignment h_content;
	ZVerticalAlignment v_content;
} zrn_control_t;

typedef struct zrn_content_control {
	zrn_control_t base;
	/** The one child: owned (destroyed with the control or when replaced), NULL when none. */
	zrn_element_t* content;
	/** true when `content` is the TextBlock made for a string. */
	bool b_content_is_text;
} zrn_content_control_t;

/** The measure of a content control: its content plus zrn_cc_insets. */
ZSize zrn_cc_measure(zrn_element_t* e, ZSize available);
/** A content control forgets its content when it is removed: the classes that have children of their own chain to it. */
void zrn_cc_child_removed(zrn_element_t* e, zrn_element_t* child);
/** Arranges the content inside the insets, aligned by h_content and v_content; returns `final`. */
ZSize zrn_cc_arrange(zrn_element_t* e, ZSize final);
/** Chrome + border + padding of a content control. */
ZThickness zrn_cc_insets(zrn_element_t* e);

/* content controls (basic.c) */
/** Replaces the content (NULL clears it); the control owns it and the old content is destroyed. A cycle is refused as in zrn_set_only_child. */
void zrn_content_set(zrn_content_control_t* c, zrn_element_t* content);
/** Shows `text` (NULL is "") in a TextBlock that the control makes and reuses; does nothing on no memory. */
void zrn_content_set_text(zrn_content_control_t* c, const char* text);
/** The effective values of a control, theme defaults filled in; `fallback` is what a color gets while the program has not set it. */
ZColor zrn_ctl_foreground(const zrn_control_t* c);
ZColor zrn_ctl_background(const zrn_control_t* c, ZColor fallback);
ZColor zrn_ctl_border_brush(const zrn_control_t* c, ZColor fallback);
ZColor zrn_ctl_color(const zrn_control_t* c, unsigned flag, ZColor stored, ZColor fallback);

extern zrn_class_t zrn_class_control, zrn_class_content_control, zrn_class_user_control;
