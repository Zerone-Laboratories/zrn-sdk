/**
 * zrnui - gles.h
 * The OpenGL ES 2 renderer (render/gles.c): a context in a window or off-screen.
 */

#pragma once

#include "zrnui/element.h"
#include "zrnui/internal/render/backend.h"

typedef struct zrn_painter zrn_painter_t;
typedef struct zrn_image zrn_image_t;

/* render/gles.c: an OpenGL ES 2 context, in a window or off-screen */
typedef struct zrn_gl zrn_gl_t;
/** false in a build without OpenGL ES: every creating call then returns NULL and the others do nothing. */
bool zrn_gl_available(void);
/** The size the renderer last drew: 0 before the first frame. */
int zrn_gl_width(const zrn_gl_t* gl);
int zrn_gl_height(const zrn_gl_t* gl);
/** A context with a framebuffer object of that size; NULL, with zrn_error, when there is no EGL or OpenGL ES 2. Free with zrn_gl_destroy. */
zrn_gl_t* zrn_gl_create_offscreen(int width, int height);
/** A renderer that draws in a context of the platform; `current` makes that context current (called before GL work). NULL when the renderer cannot start. */
zrn_gl_t* zrn_gl_create_external(void (*current)(void* user), void* user);
/** Resizes the framebuffer of an off-screen renderer; zrn_gl_begin does it when the size differs. */
void zrn_gl_resize(zrn_gl_t* gl, int width, int height);
/** Makes the context current and prepares `p` to draw on it; zrn_gl_end draws what is left and swaps. */
void zrn_gl_begin(zrn_gl_t* gl, zrn_painter_t* p, int width, int height, float scale);
/** Draws what is batched; swaps only with `b_swap` and a surface the renderer made itself. */
void zrn_gl_end(zrn_gl_t* gl, bool b_swap);
/** Swaps the buffer of an on-screen surface that the renderer made itself; nothing on an offscreen one or one of the platform. */
void zrn_gl_swap(zrn_gl_t* gl);
/** Reads the frame into `rgba` (width * height * 4 bytes), top row first; false on no memory. */
bool zrn_gl_read(zrn_gl_t* gl, uint8_t* rgba, int width, int height);
/** Frees the renderer and the textures of its images; NULL is allowed. */
void zrn_gl_destroy(zrn_gl_t* gl);
/** Deletes the texture of an image in its renderer; called by zrn_image_free. */
void zrn_gl_forget_image(zrn_image_t* image);

extern const zrn_backend_t zrn_backend_gles;
