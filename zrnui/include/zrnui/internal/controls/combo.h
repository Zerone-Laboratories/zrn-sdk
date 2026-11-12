/**
 * zrnui - combo.h
 * ComboBox and AutoSuggestBox (controls/combo.c).
 */

#pragma once

#include "zrnui/controls.h"

typedef struct zrn_items_control zrn_items_control_t;

/** The tap on an item of a ComboBox: selects it and closes the list. A SelectionChanged handler may destroy the control. */
void zrn_combo_item_tapped(zrn_items_control_t* o, int index);
