/**
 * zrnui - frame.h
 * Frame and Page (controls/frame.c).
 */

#pragma once

#include "zrnui/controls.h"

typedef struct zrn_root zrn_root_t;

/** The window has decided on its renderer: a first page that waited swings in now, when animations run. Does nothing unless the visual of the root is a Frame in that state. */
void zrn_root_start_intro(zrn_root_t* r);
