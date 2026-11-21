#pragma once
#include "zrnui/internal/element.h"
/** The swipe of Zelement_on_swipe sees each preview of the pointer (ZRN_IN_PREVIEW_*) at `target`, before the ancestors that observe it. */
void zrn_swipe_preview(zrn_element_t* target, int type, float x, float y);
