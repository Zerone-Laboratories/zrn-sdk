/**
 * zrnui - util.h
 * Small helpers, strings and errors, used by every source.
 */

#pragma once

#include "zrnui/host.h"

/** Silences an unused parameter. */
#define ZRN_UNUSED(x) ((void)(x))
static constexpr float ZRN_PI = 3.14159265358979f;

static inline float zrn_minf(const float a, const float b) { return a < b ? a : b; }
static inline float zrn_maxf(const float a, const float b) { return a > b ? a : b; }
static inline float zrn_clampf(const float v, const float lo, const float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int zrn_mini(const int a, const int b) { return a < b ? a : b; }
static inline int zrn_maxi(const int a, const int b) { return a > b ? a : b; }
static inline ZSize zrn_size(const float w, const float h) { const ZSize s = { .width = w, .height = h }; return s; }
static inline ZRect zrn_rect(const float x, const float y, const float w, const float h) {
 const ZRect r = { .x = x, .y = y, .width = w, .height = h }; return r; }
static inline ZColor zrn_color(const uint32_t argb) { return Zcolor_argb(argb); }
/** `c` with its alpha multiplied by `a`. */
static inline ZColor zrn_color_alpha(ZColor c, const float a) { c.a *= a; return c; }
static inline ZThickness zrn_thickness_zero(void) { const ZThickness t = { 0, 0, 0, 0 }; return t; }
static inline float zrn_thickness_h(const ZThickness t) { return t.left + t.right; }
static inline float zrn_thickness_v(const ZThickness t) { return t.top + t.bottom; }

/** A malloc'd copy of `s` (NULL counts as ""); NULL on no memory, without zrn_error. Free with free(). */
char* zrn_strdup(const char* s);
/** What the library was started with (Zinit_ex); never NULL. The environment is read only by Zconfig_from_environment. */
const ZConfig* zrn_config(void);
/**
 * Replaces *dest with a malloc'd copy of `s` and frees the old string; a NULL `s` leaves *dest NULL. On no memory *dest also ends NULL: the
 * old value is lost.
 */
void zrn_set_string(char** dest, const char* s);
/** Reports an error through Zset_error_callback (stderr when there is none) and remembers it for Zget_error and Zget_error_message; the message is cut at 255 characters. */
[[gnu::format(printf, 2, 3)]] void zrn_error(ZError error, const char* fmt, ...);

/** An unchecked cast of an element to the struct of its class: the caller has checked the class. */
#define ZRN_AS(type, e) ((type*)(e))
