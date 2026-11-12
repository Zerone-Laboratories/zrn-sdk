/**
 * zrnui - anim.h
 * Animations, render transforms and tilt of elements (anim.c).
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_element zrn_element_t;
typedef struct zrn_root zrn_root_t;

/** Whether animations run for the root: false for NULL; Z_ANIMATIONS_AUTO means only with the GPU renderer or a GPU window. */
bool zrn_animations_enabled(const zrn_root_t* r);
/** Easing curve: `t` is clamped to [0, 1] and so is the result; an unknown easing is linear. */
float zrn_ease(ZEasing easing, float t);
/**
 * Advances the Zelement_animate animations of `e` to the clock of its root (a detached element finishes them at once); frees the animation
 * block when none is left.
 */
void zrn_tweens_tick(zrn_element_t* e);
/** Drops every running animation of `e` without finishing it: the properties keep their current values. Also called by zrn_destroy. */
void zrn_tweens_free(zrn_element_t* e);
/** Recomputes b_transformed after rot_x, rot_y, render_scale, translate_x or translate_y changed. */
void zrn_update_transformed(zrn_element_t* e);

/**
 * The transform of an element, in device pixels, about its centre of projection. `origin` and `width`/`height` are the element's own, in device pixels;
 * `scale` turns translate_x/translate_y (logical) into device pixels. `out` is column-major (xform.h).
 */
void zrn_xf_of_element(const zrn_element_t* e, float origin_x, float origin_y, float width, float height, float scale, float out[16]);
/**
 * Starts the press lean of `e` (not NULL: zrn_tilt_release lets go) with the finger at x, y in root coordinates; the element that leaned before is reset.
 * Does nothing when animations are off. It does not look at e->tilt: the caller decides.
 */
void zrn_tilt_press(zrn_root_t* r, zrn_element_t* e, float x, float y);
/** The finger moved (root coordinates): the lean follows it. Ignored once the lean is being released. */
void zrn_tilt_move(zrn_root_t* r, float x, float y);
/** The finger lifted: the element leans back to flat and is then let go. */
void zrn_tilt_release(zrn_root_t* r);
/** Advances the lean by `dt` ms and writes rot_x, rot_y and render_scale of the element (over any Zelement_animate on them). Called by Zroot_tick. */
void zrn_tilt_tick(zrn_root_t* r, float dt);
