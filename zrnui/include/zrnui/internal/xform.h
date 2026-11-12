/**
 * zrnui - xform.h
 * 4x4 matrices, column-major (anim.c).
 */

#pragma once

#include "zrnui/element.h"

/** Sets `m` to the identity. */
void zrn_xf_identity(float m[16]);
/** out = a * b: b is applied first. `out` may be `a` or `b`. */
void zrn_xf_mul(float out[16], const float a[16], const float b[16]);
