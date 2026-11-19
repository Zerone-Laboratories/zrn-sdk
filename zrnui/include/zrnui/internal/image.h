/**
 * zrnui - image.h
 * An image: RGBA pixels (render/painter.c).
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_image zrn_image_t;

struct zrn_image {
	int width, height;
	/** width * height * 4 bytes, owned by the image. */
	uint8_t* rgba;
	/** The texture of the OpenGL renderer that has drawn it (render/gles.c); 0 and NULL otherwise. */
	unsigned gl_texture;
	void* gl_owner;
};

/**
 * Reads a PNG file as RGBA8 (at most 16384 px a side); NULL when it cannot be opened or decoded. Only a too large image reports zrn_error.
 * Free with zrn_image_free.
 */
zrn_image_t* zrn_image_load_png(const char* path);
/** Copies w * h * 4 bytes into a new image; NULL for a size of 0 or less, no pixels or no memory. Free with zrn_image_free. */
zrn_image_t* zrn_image_from_rgba(int w, int h, const uint8_t* rgba);
/** Frees the image and its OpenGL texture; NULL is allowed. */
void zrn_image_free(zrn_image_t* image);
