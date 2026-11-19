/**
 * zrnui - painter.h
 * The painter: state of a frame and the drawing calls every control uses.
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_backend zrn_backend_t;
typedef struct zrn_painter zrn_painter_t;
typedef struct zrn_image zrn_image_t;

struct zrn_painter {
	/** The transform of what is being painted, column-major 4x4 in device pixels (perspective is the renderer's), and whether it is not the identity. */
	float xf[16];
	bool b_has_xf;
	const zrn_backend_t* be;
	void* backend_data;
	/** The target of the software backend, owned by the caller; NULL for a GPU painter. stride is in bytes. */
	uint8_t* pixels;
	int width, height, stride;
	/** Device position of the local origin and device pixels per logical pixel. */
	float tx, ty, scale;
	/** The opacity of what is being painted: the product of the opacities of the elements above. */
	float alpha;
	/** The clip in device pixels, x0, y0, x1, y1 (the end excluded); clip_stack holds the saved ones, 32 deep. */
	int clip[4];
	int clip_stack[32][4];
	int clip_depth;
	/** Pushes that were ignored because the stack was full: their pops are ignored too. */
	int clip_skipped;
};

/** One glyph bitmap at device pixel x, y (logical coordinates and tx, ty do not apply): `alpha` is w * h coverage with `stride` bytes a row. */
void zrn_blit_alpha(zrn_painter_t* p, int x, int y, int w, int h, const uint8_t* alpha, int stride, ZColor c);
/**
 * A software painter on the caller's RGBA8 buffer of width x height (stride width * 4); `scale` is device pixels per logical pixel. The clip
 * is the whole buffer.
 */
void zrn_painter_init(zrn_painter_t* p, uint8_t* pixels, int width, int height, float scale);
/** Fills the whole target with an opaque color, ignoring clip and transform. */
void zrn_paint_clear(zrn_painter_t* p, ZColor color);
/**
 * The shapes: coordinates are logical and local to the element being painted (tx, ty and scale make them device pixels); the clip and `alpha`
 * apply.
 */
void zrn_fill_rect(zrn_painter_t* p, ZRect r, ZColor c);
/** The outline of a rectangle, drawn inside it. */
void zrn_stroke_rect(zrn_painter_t* p, ZRect r, ZThickness t, ZColor c);
void zrn_fill_rrect(zrn_painter_t* p, ZRect r, ZCornerRadius radius, ZColor c);
void zrn_fill_ellipse(zrn_painter_t* p, ZRect r, ZColor c);
void zrn_stroke_ellipse(zrn_painter_t* p, ZRect r, float thickness, ZColor c);
/** `xy` holds `count` points as x, y pairs; fewer than 3 draw nothing. */
void zrn_fill_polygon(zrn_painter_t* p, const float* xy, int count, ZColor c);
void zrn_draw_line(zrn_painter_t* p, float x0, float y0, float x1, float y1, float thickness, ZColor c);
/** Draws `source` (in image pixels) of the image stretched into `dest`. */
void zrn_draw_image(zrn_painter_t* p, const zrn_image_t* image, ZRect dest, ZRect source);
/** Clips to a local rectangle until zrn_pop_clip; see zrn_clip_push for the limit of 32 levels. */
void zrn_push_clip(zrn_painter_t* p, ZRect r);
void zrn_pop_clip(zrn_painter_t* p);
/** True when the local rectangle is entirely outside the clip. */
int zrn_clipped_out(const zrn_painter_t* p, ZRect r);
