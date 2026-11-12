/**
 * zrnapp - app_id.h
 * The app_id of a window: the identity by which the shell finds the .desktop file, the icon and the grouping of the windows.
 */

#pragma once

#include <stddef.h>

/**
 * Chooses the app_id and writes it, NUL-terminated, into `out` (`capacity` bytes, at least 1).
 *   1. `explicit_id`, when it is given and not empty (the .desktop file name without the suffix, as the program says;
 *      window.c passes the app_id of ZWindowOptions, else the one of zrnapp_take_launcher_app_id);
 *   2. else the file name of `program` (the path of the executable, or its name), as GTK takes the program name;
 *   3. else "zrnui".
 * The text is cut to fit; nothing is written when `capacity` is 0. The title of the window has nothing to do with it.
 */
void zrnapp_choose_app_id(const char* explicit_id, const char* program, char* out, size_t capacity);

/** The path of the running executable, or NULL when the system does not tell. Valid until the next call. */
const char* zrnapp_program_path(void);

/**
 * The app_id the launcher gave in $ZRNAPP_APP_ID (the shell knows which .desktop file it started), or NULL when there is none or
 * it is empty. The variable is removed from the environment on the first call, so the programs this one starts do not inherit
 * an identity that is not theirs: only the first call returns the id, every later one returns NULL. The text is valid until the next call.
 */
const char* zrnapp_take_launcher_app_id(void);
