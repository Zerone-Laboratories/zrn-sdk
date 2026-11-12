/**
 * zrnui - theme_resources.h
 * The generated table of the ThemeResource keys (theme_resources.c), read by resources.c.
 */

#pragma once

#include "zrnui/element.h"

/** The type of the value of a key; ZRN_RT_NONE for a key with no value. */
typedef enum zrn_res_type {
	ZRN_RT_NONE = 0,
	ZRN_RT_COLOR,
	ZRN_RT_DOUBLE,
	ZRN_RT_INT,
	ZRN_RT_THICKNESS,
	ZRN_RT_FONT_FAMILY
} zrn_res_type_t;

typedef union zrn_res_value {
	/** A color: ARGB; of an accent color only the alpha counts. */
	uint32_t argb;
	/** A double in f[0], a thickness (left, top, right, bottom) in f. */
	float f[4];
	/** An int, a font weight, a ZFontFamily. */
	int i;
} zrn_res_value_t;

/** One generated key: the table is indexed by key - Z_RES_FIRST_. */
typedef struct zrn_res_def {
	/** The XAML key name. */
	const char* name;
	/** A zrn_res_type_t. */
	uint8_t type;
	/** 1: the Dark color is the accent of the root (with the alpha of `dark`), 2: the Light one. */
	uint8_t accent;
	zrn_res_value_t dark, light;
} zrn_res_def_t;

/** Generated, one entry per built-in key from Z_RES_FIRST_ up to Z_RES_LAST_; do not edit it by hand. */
extern const zrn_res_def_t zrn_res_table[];
