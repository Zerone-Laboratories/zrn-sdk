/**
 * zrnapp - egl_window.h
 * An OpenGL ES 2 context on the EGL window of a Wayland surface, and the frames of a zrnui root drawn in it.
 * Without EGL in the build (ZRNAPP_HAVE_EGL) the window has no GPU path: the functions below are empty and
 * zrnapp_gl_create fails, so the window draws with wl_shm.
 */

#pragma once

#include <zrnui/platform.h>

typedef struct zrnapp_gl zrnapp_gl_t;

#ifdef ZRNAPP_HAVE_EGL

/**
 * An OpenGL ES 2 context on a wl_display and a wl_surface (the wayland pointers, as void*), `width` x `height` in buffer pixels. NULL when there is no EGL / OpenGL ES 2
 * for this surface (the message is in Zget_error_message, except when the wl_egl_window cannot be made). Free with zrnapp_gl_destroy.
 */
zrnapp_gl_t* zrnapp_gl_create(void* wl_display, void* wl_surface, int width, int height);
/** Resizes the wl_egl_window to the new buffer size. */
void zrnapp_gl_resize(zrnapp_gl_t* gl, int width, int height);
/** Paints the root into the back buffer when something changed; resizes first when the size differs. @return true when a frame was painted, to be shown by zrnapp_gl_swap. */
bool zrnapp_gl_paint(zrnapp_gl_t* gl, Zid root, int width, int height);
/** Shows the frame zrnapp_gl_paint painted (eglSwapBuffers: the commit of the surface). */
void zrnapp_gl_swap(zrnapp_gl_t* gl);
/** Frees the context, the EGL window and the renderer; NULL is allowed. Call it before the wl_surface goes away. */
void zrnapp_gl_destroy(zrnapp_gl_t* gl);

#else

static inline zrnapp_gl_t* zrnapp_gl_create(void* wl_display, void* wl_surface, int width, int height) {
	(void)wl_display; (void)wl_surface; (void)width; (void)height;
	Zreport_error(Z_ERROR_IO, "zrnapp was built without EGL");
	return nullptr;
}
static inline void zrnapp_gl_resize(zrnapp_gl_t* gl, int width, int height) { (void)gl; (void)width; (void)height; }
static inline bool zrnapp_gl_paint(zrnapp_gl_t* gl, Zid root, int width, int height) { (void)gl; (void)root; (void)width; (void)height; return false; }
static inline void zrnapp_gl_swap(zrnapp_gl_t* gl) { (void)gl; }
static inline void zrnapp_gl_destroy(zrnapp_gl_t* gl) { (void)gl; }

#endif
