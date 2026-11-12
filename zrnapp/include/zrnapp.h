/**
 * zrnapp
 * zrnapp.h : the app cycle of a zrnui application and its integration in the OS.
 *
 * zrnui (the UI core) draws a tree and knows no window. zrnapp owns the rest: a window on Wayland, the event loop,
 * pointer, touch, wheel, keyboard and text going into the Zroot_inject_* functions, the scale of the shell, and the
 * frames of the root going to the compositor (wl_shm buffers, or OpenGL ES 2 through EGL). It talks to the core
 * only through zrnui/platform.h, so a shell that embeds zrnui does what zrnapp does, and links zrnui alone.
 *
 * Built when wayland-client and xkbcommon are found. Without them the functions exist and fail.
 */

#pragma once

#include <zrnui/id.h>

#if defined(_WIN32) && defined(ZRNAPP_BUILD_DLL)
	#define ZRNAPP_API __declspec(dllexport)
#elif defined(_WIN32) && defined(ZRNAPP_USE_DLL)
	#define ZRNAPP_API __declspec(dllimport)
#elif defined(__GNUC__) && defined(ZRNAPP_BUILD_DLL)
	#define ZRNAPP_API __attribute__((visibility("default")))
#else
	#define ZRNAPP_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

	/** What the window says about itself. Zero fields mean the default. */
	typedef struct ZWindowOptions {
		/** The title in the title bar and the task switcher. NULL: the app id. */
		const char* title;
		/**
		 * The app id: the identity by which the shell finds the .desktop file, the icon and the grouping of windows, so it is the
		 * name of the .desktop file without the suffix (`org.example.Notes` for `org.example.Notes.desktop`). NULL or empty:
		 * the one the launcher gave in $ZRNAPP_APP_ID (the shell sets it to the .desktop file it starts, and the variable is
		 * removed from the environment once read), else the file name of the executable, as GTK does; a program installed as
		 * `notes` with `notes.desktop` needs nothing here.
		 */
		const char* app_id;
		/**
		 * Called once with the Wayland connection (a `struct wl_display*`, as void* so this header needs no Wayland), after
		 * the window objects exist and before its first commit, so no configure arrives while it runs. A shell uses it to
		 * bind a global of its own (a private protocol) on the same connection: its events then come in the loop of the
		 * window, in the same thread, and need no locking. The callback may roundtrip on the display. NULL: nothing.
		 */
		void (*on_connect)(void* wl_display, void* user_data);
		void* user_data;
	} ZWindowOptions;

	/**
	 * @brief Opens a window for `root` and runs its event loop until the window
	 * is closed, the compositor goes away, or Zwindow_quit is called.
	 * The window is as big as the root (in logical pixels; a root with no size gets 480 x 800); resizing it
	 * resizes the root. The scale is the one of the compositor ($ZRNAPP_SCALE overrides it, for debugging).
	 * Zroot_tick is called at least every ~16 ms: animations, caret, repeat buttons.
	 * It draws with the software painter (wl_shm), which commits a frame only when something changed, or with OpenGL ES 2
	 * (EGL), as Zroot_get_renderer says. The loop also ends when the root is destroyed. The root is not destroyed on return.
	 * Call from the thread that made the root.
	 * @param root the root to show.
	 * @param options NULL: the defaults of ZWindowOptions.
	 * @return true after a normal close; false when no window could be opened: no Wayland, an invalid root,
	 * Z_RENDERER_GPU without OpenGL ES, or a build without Wayland (see Zget_error).
	 */
	ZRNAPP_API bool Zwindow_run_ex(Zid root, const ZWindowOptions* options);

	/**
	 * @brief Zwindow_run_ex with only a title. The app id is the one of the executable (see ZWindowOptions.app_id): the title
	 * is no longer used as the app id, which it was before and which made "Hello World" the identity of a window.
	 */
	ZRNAPP_API bool Zwindow_run(Zid root, const char* title);

	/** @brief Ends the loop of Zwindow_run / Zwindow_run_ex, from a callback of the UI. Does nothing when no window is open. */
	ZRNAPP_API void Zwindow_quit(void);

#ifdef __cplusplus
}
#endif
