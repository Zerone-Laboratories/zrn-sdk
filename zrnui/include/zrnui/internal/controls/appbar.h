/**
 * zrnui - appbar.h
 * AppBar and what the page and the buttons ask of it (controls/appbar.c).
 */

#pragma once

#include "zrnui/controls.h"
#include "zrnui/internal/controls/control.h"

/** AppBar: closed it is as tall as ClosedDisplayMode says, open as tall as its content (a CommandBar: its commands with their labels, and the secondary ones above). */
typedef struct zrn_app_bar {
	zrn_content_control_t base;
	/** What the program asked for; open_t follows it with an animation. */
	bool b_is_open;
	/** A sticky bar is not closed by a press elsewhere or by Back. */
	bool b_is_sticky;
	ZAppBarClosedDisplayMode closed_mode;
	/** The opening animation: 0 closed, 1 open. */
	float open_t;
	/** Set by the Page that shows it as its TopAppBar. */
	bool b_on_top;
	/** A press began on the three dots. */
	int ellipsis_pressed;
} zrn_app_bar_t;

/** A page (frame.c) asks the bars it shows how tall they are closed (Minimal or Compact, whatever open_t is) and tells a bar that it is the top bar. */
float zrn_app_bar_closed_height(zrn_element_t* bar);
/** Marks the bar as the top bar of its page (its border is then on the lower edge) and lays it out again when that changes. */
void zrn_app_bar_set_on_top(zrn_element_t* bar, int on_top);
/** A command of an app bar was clicked (appbar.c): a secondary one closes the bar. */
void zrn_app_bar_item_clicked(zrn_element_t* item);
