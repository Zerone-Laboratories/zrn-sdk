/**
 * zrnui
 * platform.h : what a platform layer needs from the UI core.
 *
 * zrnui draws and lays out a tree; it does not own a window, an event loop or the OS. The platform layer does:
 * zrnapp (a Wayland window, ../zrnapp) or a shell that embeds zrnui. This header is the whole seam between
 * them: the size, density and invalidation of a root, the report of the renderer in use, and a GPU target that draws
 * in an OpenGL ES 2 context that the platform created. Input and time go in through Zroot_inject_* and Zroot_tick
 * (host.h), pixels come out of Zroot_render (host.h) or of Zgl_target_paint.
 */

#pragma once

#include "host.h"

#ifdef __cplusplus
extern "C" {
#endif

	/** @brief The size of the root in logical pixels (0 x 0 until set). Either pointer may be NULL. @return false for an invalid root, with nothing written. */
	ZRNUI_API bool Zroot_get_size(Zid root, int* width, int* height);

	/** @brief The next frame is painted whole, though nothing changed: the buffer the platform draws into is new or blank. */
	ZRNUI_API void Zroot_invalidate(Zid root);

	/**
	 * @brief The density of the buffer against the logical size: device pixels per logical pixel. Only the crispness
	 * of the picture, what the compositor needs to show the window sharply; the size of the content is Zroot_set_scale.
	 */
	ZRNUI_API void Zroot_set_density(Zid root, float density);

	/** @brief A first page that went into the Frame before the window existed swings in now (an animation, when animations run). Call when the surface is up. */
	ZRNUI_API void Zroot_start_intro(Zid root);

	/**
	 * @brief The platform says how it presents the root: the name Zroot_get_renderer_name returns ("gles2", "software")
	 * and whether the GPU draws (animations that need it run only then).
	 */
	ZRNUI_API void Zroot_report_presenter(Zid root, const char* renderer_name, bool is_gpu);

	/** @brief Reports an error through Zset_error_callback and remembers it (Zget_error, Zget_error_message). */
	ZRNUI_API void Zreport_error(ZError error, const char* message);

	/** A renderer that draws in an OpenGL ES 2 context of the platform. Opaque; made by Zgl_target_create, freed by Zgl_target_destroy. */
	typedef struct ZGlTarget ZGlTarget;

	/**
	 * @brief Makes a GPU target. The context of the platform must be current. `make_current` makes it current again:
	 * it is called before the target does any GL work (a frame, the deletion of a texture), possibly from another place
	 * than the frame. NULL when the context stays current.
	 * @return the target, or NULL when there is no OpenGL ES in this build or the GL setup failed.
	 */
	ZRNUI_API ZGlTarget* Zgl_target_create(void (*make_current)(void* user), void* user);

	/**
	 * @brief Paints the root into the framebuffer 0 of the current context, `width` x `height` device pixels, when
	 * something changed (a new size paints everything). The platform swaps the buffers afterwards.
	 * @return true when a frame was painted; false when nothing changed, and for an invalid root or a NULL target.
	 */
	ZRNUI_API bool Zgl_target_paint(Zid root, ZGlTarget* target, int width, int height);

	/**
	 * @brief Whether Zroot_render would draw something new (a change of the tree, the theme, the size or the density, or the
	 * first frame). A platform that copies the pixels to a window can skip the frame when it is false. It is true until
	 * Zroot_render has made a frame of the current size, so it is for the Zroot_render path, not for Zgl_target_paint (which returns the answer itself).
	 */
	ZRNUI_API bool Zroot_needs_render(Zid root);

	/** @brief Frees the target and its textures; the context must be current. NULL is ignored. */
	ZRNUI_API void Zgl_target_destroy(ZGlTarget* target);

#ifdef __cplusplus
}
#endif
