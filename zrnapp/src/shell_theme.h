/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * zrnapp - shell_theme.h
 * The theme and the accent of the shell (ztMETRO): the lines `theme = dark|light` and `accent = <name>` of
 * ~/.config/ztmetro/config, which the shell's Settings writes (a temporary file renamed over it). The window reads it at
 * start and again whenever the file changes (inotify on its directory): no polling.
 */

#pragma once

#include <zrnui/core.h>

#include <stddef.h>

/** What the config file says. A value the file does not give is left out (its b_has_ flag false). */
typedef struct zrnapp_shell_theme {
	bool b_has_theme;
	ZTheme theme;
	bool b_has_accent;
	ZColor accent;
	/** The height of the shell's keyboard in its points, upright: 3 + 4 rows of `key_height` (56 when the file gives none, 42..72), as ztMETRO lays it out. */
	float keyboard_height;
	/** `frost = 1|0` (also on/off): zrn-frosted-diffusion on system-wide (Zset_frosted_diffusion_enabled); true when absent. */
	bool b_frost;
} zrnapp_shell_theme_t;

/**
 * Reads `path` into `out`. `theme = light` (or 1) is light, any other theme is dark; `accent` is one of the 20 names of
 * Windows Phone 8 (lime ... taupe) or #rrggbb. The theme of the session ($ZTRF_SHELL_THEME) stands in when the file
 * gives none. false when the file cannot be read (the session theme is still filled in).
 */
bool zrnapp_shell_theme_read(const char* path, zrnapp_shell_theme_t* out);

/** The path of the config file of the shell: $ZRNAPP_SHELL_CONFIG when set, else $HOME/.config/ztmetro/config. Written into `out`; false when there is no HOME. */
bool zrnapp_shell_theme_path(char* out, size_t capacity);

/**
 * The keyboard as the shell publishes it: $XDG_RUNTIME_DIR/ztmetro-osk, one line `<height in points> <visible 0|1>`,
 * renamed over whenever the keyboard shows, hides or changes height (orientation, terminal row). false when the file
 * is absent or not in that form (fall back to keyboard_height of the config).
 */
bool zrnapp_shell_osk_read(const char* path, float* height, bool* b_visible);

/** The path of that file ($XDG_RUNTIME_DIR/ztmetro-osk); false without XDG_RUNTIME_DIR. */
bool zrnapp_shell_osk_path(char* out, size_t capacity);
