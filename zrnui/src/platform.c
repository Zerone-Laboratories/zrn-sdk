/**
 * zrnui - platform.c
 * The seam to the platform layer (include/zrnui/platform.h): zrnapp or a shell that embeds zrnui.
 */

#include "zrnui/platform.h"

#include <math.h>
#include "zrnui/internal/util.h"
#include "zrnui/internal/render/painter.h"
#include "zrnui/internal/render/gles.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/controls/frame.h"

bool Zroot_get_size(const Zid root, int* const width, int* const height) {
	const zrn_root_t* const r = zrn_root_lookup(root);
	if (!r) return false;
	if (width) *width = r->width;
	if (height) *height = r->height;
	return true;
}

void Zroot_invalidate(const Zid root) {
	zrn_root_t* const r = zrn_root_lookup(root);
	if (r) r->b_paint_dirty = true;
}

bool Zroot_needs_render(const Zid root) {
	const zrn_root_t* const r = zrn_root_lookup(root);
	if (!r) return false;
	/* the same size as Zroot_render gives its buffer */
	const int w = (int)ceilf((float)r->width * r->density), h = (int)ceilf((float)r->height * r->density);
	return r->b_layout_dirty || r->b_paint_dirty || r->b_first_frame || r->pixels_w != w || r->pixels_h != h;
}

void Zroot_start_intro(const Zid root) {
	zrn_root_t* const r = zrn_root_lookup(root);
	if (r) zrn_root_start_intro(r);
}

void Zroot_report_presenter(const Zid root, const char* const renderer_name, const bool is_gpu) {
	zrn_root_t* const r = zrn_root_lookup(root);
	zrn_note_renderer(renderer_name);
	if (r) r->b_gpu_window = is_gpu != 0;
}

void Zreport_error(const ZError error, const char* const message) {
	zrn_error(error, "%s", message ? message : "");
}

ZGlTarget* Zgl_target_create(void (*const make_current)(void* user), void* const user) {
	return (ZGlTarget*)zrn_gl_create_external(make_current, user);
}

bool Zgl_target_paint(const Zid root, ZGlTarget* const target, const int width, const int height) {
	zrn_root_t* const r = zrn_root_lookup(root);
	if (!r || !target) return false;
	zrn_gl_t* const gl = (zrn_gl_t*)target;
	zrn_painter_t p;
	/* the buffer of a new size is blank: everything is drawn again */
	if (zrn_gl_width(gl) != width || zrn_gl_height(gl) != height) r->b_paint_dirty = true;
	zrn_gl_begin(gl, &p, width, height, r->scale);
	const bool b_painted = zrn_root_paint(r, &p);
	zrn_gl_end(gl, 0);   /* flushed, not swapped: the buffers are the platform's */
	return b_painted;
}

void Zgl_target_destroy(ZGlTarget* const target) {
	zrn_gl_destroy((zrn_gl_t*)target);
}
