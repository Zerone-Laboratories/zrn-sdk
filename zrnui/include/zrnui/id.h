/**
 * zrnui - id.h
 * The handle of a zrnui object. It has no dependency, so a program that only passes handles around (zrnapp) includes this.
 */

#pragma once

#include <stdint.h>

/**
 * Opaque handle to any zrnui object: a root, an element, a control. A
 * handle carries an index and a generation counter, so using a destroyed
 * handle is detected and reported instead of crashing.
 * Both Z_INVALID_ID and 0 are never valid.
 */
typedef uint32_t Zid;

#define Z_INVALID_ID ((Zid)0)
