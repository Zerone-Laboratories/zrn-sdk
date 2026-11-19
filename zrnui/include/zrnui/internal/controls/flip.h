/**
 * zrnui - flip.h
 * FlipView (controls/flip.c).
 */

#pragma once

#include "zrnui/controls.h"

typedef struct zrn_items_control zrn_items_control_t;

/** flip.c: what a FlipView does when its items or its selection change. Items came or went: one item stays selected and shown, with no animation. */
void zrn_flip_items_changed(zrn_items_control_t* o);
/** The selection changed: the view slides (or jumps, without animations) to selected_index. */
void zrn_flip_selected(zrn_items_control_t* o);
