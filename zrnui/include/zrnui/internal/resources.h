/**
 * zrnui - resources.h
 * ThemeResource: the values of the keys of a root, and the properties of an element that follow one (resources.c).
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_element zrn_element_t;
typedef struct zrn_root zrn_root_t;

/** The value of a key of a root (NULL: the default theme, Dark with Cobalt) as the type says; an override of the root wins. false when the key has no such value, `out` is then untouched. */
bool zrn_res_color(const zrn_root_t* r, ZThemeResource key, ZColor* out);
bool zrn_res_double(const zrn_root_t* r, ZThemeResource key, float* out);
bool zrn_res_int(const zrn_root_t* r, ZThemeResource key, int* out);
bool zrn_res_thickness(const zrn_root_t* r, ZThemeResource key, ZThickness* out);
bool zrn_res_font_family(const zrn_root_t* r, ZThemeResource key, ZFontFamily* out);

/** The properties of an element that can follow a key. */
typedef enum zrn_bind_prop {
	ZRN_BP_ELEMENT_MARGIN = 1,
	ZRN_BP_TEXT_FOREGROUND,
	ZRN_BP_TEXT_FONT_SIZE,
	ZRN_BP_TEXT_CHARACTER_SPACING,
	ZRN_BP_CONTROL_FOREGROUND,
	ZRN_BP_CONTROL_BACKGROUND,
	ZRN_BP_CONTROL_BORDER_BRUSH,
	ZRN_BP_CONTROL_BORDER_THICKNESS,
	ZRN_BP_CONTROL_PADDING,
	ZRN_BP_CONTROL_FONT_SIZE,
	ZRN_BP_BORDER_BACKGROUND,
	ZRN_BP_BORDER_BORDER_BRUSH,
	ZRN_BP_BORDER_BORDER_THICKNESS,
	ZRN_BP_BORDER_PADDING
} zrn_bind_prop_t;

/** Binds `prop` of `e` to `key` (replacing its binding) and applies it; false with zrn_error when the key is not of the type the property takes. `e` must be of the class the property belongs to (not checked). */
bool zrn_bind(zrn_element_t* e, zrn_bind_prop_t prop, ZThemeResource key);
/** A direct setter was called: the property keeps its value and stops following a key. */
void zrn_unbind(zrn_element_t* e, zrn_bind_prop_t prop);
/** Applies every binding of `e` (when it enters a root). */
void zrn_bindings_apply(zrn_element_t* e);
/** Frees the binding list of `e` (zrn_destroy). */
void zrn_bindings_free(zrn_element_t* e);
/** The theme, the accent or an override changed: every bound property of the tree of the root is resolved again. */
void zrn_root_resources_changed(zrn_root_t* r);
/** Frees the overrides of the root (Zroot_destroy). */
void zrn_root_resources_free(zrn_root_t* r);
