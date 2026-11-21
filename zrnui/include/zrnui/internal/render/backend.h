/**
 * zrnui - backend.h
 * What a renderer implements, and the clip every renderer shares (render/painter.c).
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_painter zrn_painter_t;
typedef struct zrn_image zrn_image_t;

/**
 * A renderer. Every call has the signature of the zrn_* function of the same name, coordinates
 * logical and local as the painter's tx, ty, scale, alpha and clip say. The software backend
 * (render/software.c) draws into an RGBA8 buffer; others (OpenGL) draw on the GPU.
 */
typedef struct zrn_backend {
	/** "software" or "gles2". */
	const char* name;
	void (*paint_clear)(zrn_painter_t* p, ZColor c);
	void (*fill_rect)(zrn_painter_t* p, ZRect r, ZColor c);
	void (*fill_rrect)(zrn_painter_t* p, ZRect r, ZCornerRadius radius, ZColor c);
	void (*fill_ellipse)(zrn_painter_t* p, ZRect r, ZColor c);
	void (*stroke_ellipse)(zrn_painter_t* p, ZRect r, float thickness, ZColor c);
	/** `xy` holds `count` points as x, y pairs; at least 3, filled even-odd. */
	void (*fill_polygon)(zrn_painter_t* p, const float* xy, int count, ZColor c);
	void (*draw_line)(zrn_painter_t* p, float x0, float y0, float x1, float y1, float thickness, ZColor c);
	/** One glyph bitmap: 8 bit coverage (`stride` bytes a row) at a device position, not logical and not moved by tx, ty; the clip applies. */
	void (*blit_alpha)(zrn_painter_t* p, int x, int y, int w, int h, const uint8_t* alpha, int stride, ZColor c);
	/** `source` is in image pixels, `dest` in logical units. */
	void (*draw_image)(zrn_painter_t* p, const zrn_image_t* image, ZRect dest, ZRect source);
	void (*push_clip)(zrn_painter_t* p, ZRect r);
	void (*pop_clip)(zrn_painter_t* p);
	/** Nonzero when the local rectangle is entirely outside the clip. */
	int (*clipped_out)(const zrn_painter_t* p, ZRect r);
	/** What follows is drawn under this affine transform (column-major 4x4, device pixels), NULL for none. Backends that cannot, draw it untransformed. */
	void (*set_transform)(zrn_painter_t* p, const float* m);
	/**
	 * Layers (CacheMode): what follows layer_begin is painted into `*layer` (made or remade at width x height device pixels), from
	 * a clear transparent start, with the painter at its origin, unclipped and opaque; layer_end puts the painter and the target back as
	 * `saved` (opaque storage of ZRN_LAYER_SAVE bytes) holds them. draw_layer draws a layer with its top left at the painter's origin,
	 * under its transform, alpha and clip. NULL in a renderer that has none (software): the subtree is painted directly then.
	 * layer_begin returns false when it cannot make the layer, and then nothing changed.
	 */
	bool (*layer_begin)(zrn_painter_t* p, zrn_image_t** layer, int width, int height, void* saved);
	void (*layer_end)(zrn_painter_t* p, const void* saved);
	void (*draw_layer)(zrn_painter_t* p, const zrn_image_t* layer);
	/** Whether draw_layer can draw this layer now (a layer that shares a texture with the target cannot); NULL is always. */
	bool (*layer_drawable)(const zrn_painter_t* p, const zrn_image_t* layer);
	/**
	 * zrn-frosted-diffusion (host.c). backdrop_begin: what follows is painted into `*strip` (made or remade at width x height
	 * device pixels, cleared to `under`), with the painter at the strip's origin, as the frame would be (bitmap caches drawn
	 * from their textures: it is not a layer); false when it cannot, and then nothing changed. backdrop_end: the strip, cut
	 * to a quarter each way and blurred in one pass, into `*blur`; the painter and the target are put back from `saved`
	 * (ZRN_LAYER_SAVE bytes). backdrop_draw: the effect over `r` (local), evenly or fading as `mode` (a ZFrostedDiffusion) says: `blur` (its part `uv`: u, v of the element's top left, then its width and height in texture units, v up) with grain under `tint` (`veil`
	 * of it), or where `blur` is NULL (nothing under it) `under` under the same tint. NULL in a renderer without them: the
	 * element is filled with the tint.
	 */
	bool (*backdrop_begin)(zrn_painter_t* p, zrn_image_t** strip, int width, int height, ZColor under, void* saved);
	void (*backdrop_end)(zrn_painter_t* p, zrn_image_t* strip, zrn_image_t** blur, const void* saved);
	void (*backdrop_draw)(zrn_painter_t* p, const zrn_image_t* blur, const float uv[4], ZRect r, int mode, ZColor tint, ZColor under, float veil);
} zrn_backend_t;

/** The room layer_begin needs to save the state of a painter and its target. */
#define ZRN_LAYER_SAVE 2048

extern const zrn_backend_t zrn_backend_software;

/** The clip is state of the painter, the same for every backend. Device pixels, [x0, x1) by [y0, y1). */
/**
 * Intersects the clip with `r` and saves the old one. 32 levels: a push beyond that is ignored (reported once, Z_ERROR_CYCLE) and its pop then
 * undoes the previous push. Under a transform nothing is clipped.
 */
void zrn_clip_push(zrn_painter_t* p, ZRect r);
/** Restores the clip saved by the last push; does nothing at depth 0. */
void zrn_clip_pop(zrn_painter_t* p);
/** Nonzero when `r` is entirely outside the clip; never under a transform. */
int zrn_clip_out(const zrn_painter_t* p, ZRect r);
