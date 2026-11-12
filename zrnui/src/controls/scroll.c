/**
 * zrnui - controls/scroll.c
 * ScrollViewer: touch scrolling with inertia, the wheel, and the thin
 * transient scroll bars of Windows Phone.
 */

#include "zrnui/internal/controls/scroll.h"
#include "zrnui/internal/util.h"
#include "zrnui/internal/input.h"
#include "zrnui/internal/class.h"
#include "zrnui/internal/element.h"
#include "zrnui/internal/theme.h"
#include "zrnui/internal/render/painter.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/controls/control.h"

#include <math.h>

#define SV(e) ZRN_AS(zrn_scroll_viewer_t, e)

/* a drag starts after the finger moved this far */
static constexpr float DRAG_THRESHOLD = 10.0f;
/* the bars stay in view this long after the last scroll */
static constexpr float BAR_HOLD_MS = 800.0f;


static void sv_limits(const zrn_scroll_viewer_t* const s, double* const max_x, double* const max_y) {
	*max_x = s->extent_w > s->viewport_w ? s->extent_w - s->viewport_w : 0;
	*max_y = s->extent_h > s->viewport_h ? s->extent_h - s->viewport_h : 0;
}

static void sv_clamp(zrn_scroll_viewer_t* const s) {
	double mx, my;
	sv_limits(s, &mx, &my);
	if (s->offset_x < 0) s->offset_x = 0;
	if (s->offset_x > mx) s->offset_x = mx;
	if (s->offset_y < 0) s->offset_y = 0;
	if (s->offset_y > my) s->offset_y = my;
}

/** The position that OnScroll reports: along the axis that scrolls (a horizontal viewer, a Hub, reports its horizontal offset). */
static double sv_reported_offset(const zrn_scroll_viewer_t* const s);

static void sv_changed(zrn_scroll_viewer_t* const s, const bool b_fire) {
	zrn_element_t* const e = &s->base.base.base;
	sv_clamp(s);
	s->bar_visible = BAR_HOLD_MS;
	e->b_wants_tick = true;
	zrn_changed_arrange(e);
	if (b_fire && s->on_scroll) s->on_scroll(e->id, Z_SCROLL_THUMB_POSITION, sv_reported_offset(s), s->on_scroll_data);
}

static ZSize sv_measure(zrn_element_t* const e, const ZSize available) {
	auto s = SV(e);
	const zrn_content_control_t* const cc = &s->base;
	ZSize room = available, content = { 0, 0 };
	if (s->h_visibility != Z_SCROLLBAR_DISABLED) room.width = Z_INFINITE;
	if (s->v_visibility != Z_SCROLLBAR_DISABLED) room.height = Z_INFINITE;
	if (cc->content) {
		zrn_measure(cc->content, room);
		content = cc->content->desired;
	}
	return zrn_size(zrn_minf(content.width, available.width), zrn_minf(content.height, available.height));
}

static ZSize sv_arrange(zrn_element_t* const e, const ZSize final) {
	const auto s = SV(e);
	const zrn_content_control_t* const cc = &s->base;
	s->viewport_w = final.width;
	s->viewport_h = final.height;
	if (cc->content) {
		const ZSize d = cc->content->desired;
		const float w = s->h_visibility == Z_SCROLLBAR_DISABLED ? final.width : zrn_maxf(d.width, final.width);
		const float h = s->v_visibility == Z_SCROLLBAR_DISABLED ? final.height : zrn_maxf(d.height, final.height);
		s->extent_w = s->h_visibility == Z_SCROLLBAR_DISABLED ? final.width : d.width + s->extra_extent_x;
		s->extent_h = s->v_visibility == Z_SCROLLBAR_DISABLED ? final.height : d.height;
		sv_clamp(s);
		zrn_arrange(cc->content, zrn_rect((float)-s->offset_x, (float)-s->offset_y, w, h));
	} else {
		s->extent_w = final.width;
		s->extent_h = final.height;
	}
	s->b_show_v = s->v_visibility != Z_SCROLLBAR_DISABLED && s->v_visibility != Z_SCROLLBAR_HIDDEN && s->extent_h > s->viewport_h + 0.5;
	s->b_show_h = s->h_visibility != Z_SCROLLBAR_DISABLED && s->h_visibility != Z_SCROLLBAR_HIDDEN && s->extent_w > s->viewport_w + 0.5;
	if (s->v_visibility == Z_SCROLLBAR_VISIBLE) s->b_show_v = true;
	if (s->h_visibility == Z_SCROLLBAR_VISIBLE) s->b_show_h = true;
	return final;
}

static void sv_paint_over(zrn_element_t* const e, zrn_painter_t* const p) {
	const zrn_scroll_viewer_t* const s = SV(e);
	double mx, my;
	const bool b_always_v = s->v_visibility == Z_SCROLLBAR_VISIBLE;
	const bool b_always_h = s->h_visibility == Z_SCROLLBAR_VISIBLE;
	sv_limits(s, &mx, &my);
	float alpha = zrn_clampf(s->bar_visible / 250.0f, 0, 1);
	ZColor color = zrn_theme_color(e, ZRN_TC_DISABLED);
	if (s->b_show_v && (alpha > 0 || b_always_v)) {
		const float length = e->rect.height - (s->b_show_h ? 7.0f : 0);
		const float thumb = zrn_clampf((float)(s->viewport_h / s->extent_h) * length, zrn_minf(72.0f, length), length);
		const float pos = my > 0 ? (float)(s->offset_y / my) * (length - thumb) : 0;
		zrn_fill_rect(p, zrn_rect(e->rect.width - 6, pos + 1, 5, thumb - 2), zrn_color_alpha(color, b_always_v ? 1.0f : alpha));
	}
	if (s->b_show_h && (alpha > 0 || b_always_h)) {
		const float length = e->rect.width - (s->b_show_v ? 7.0f : 0);
		const float thumb = zrn_clampf((float)(s->viewport_w / s->extent_w) * length, zrn_minf(72.0f, length), length);
		const float pos = mx > 0 ? (float)(s->offset_x / mx) * (length - thumb) : 0;
		zrn_fill_rect(p, zrn_rect(pos + 1, e->rect.height - 6, thumb - 2, 5), zrn_color_alpha(color, b_always_h ? 1.0f : alpha));
	}
}

static void sv_tick(zrn_element_t* const e, const float dt) {
	auto s = SV(e);
	bool b_busy = false;
	bool b_fire = false;
	if (!s->b_dragging && (fabsf(s->velocity_x) > 20.0f || fabsf(s->velocity_y) > 20.0f)) {
		/* inertia: exponential decay, 1/3 of the speed left after 0.35 s */
		const float decay = expf(-dt / 320.0f);
		const double old_x = s->offset_x, old_y = s->offset_y;
		s->offset_x += s->velocity_x * dt / 1000.0f;
		s->offset_y += s->velocity_y * dt / 1000.0f;
		s->velocity_x *= decay;
		s->velocity_y *= decay;
		sv_clamp(s);
		if (s->offset_x == old_x) s->velocity_x = 0;
		if (s->offset_y == old_y) s->velocity_y = 0;
		zrn_changed_arrange(e);
		s->bar_visible = BAR_HOLD_MS;
		b_busy = true;
		b_fire = s->offset_y != old_y || s->offset_x != old_x;
	} else if (!s->b_dragging) {
		s->velocity_x = s->velocity_y = 0;
	}
	if (s->bar_visible > 0 && !s->b_dragging) {
		const float before = s->bar_visible;
		s->bar_visible = zrn_maxf(0, s->bar_visible - dt);
		if (before <= 250.0f || s->bar_visible <= 250.0f) zrn_changed_paint(e);
		b_busy = b_busy || s->bar_visible > 0;
	}
	if (!b_busy && !s->b_dragging) e->b_wants_tick = false;
	/* last: the handler may destroy the viewer */
	if (b_fire && s->on_scroll) s->on_scroll(e->id, Z_SCROLL_THUMB_POSITION, sv_reported_offset(s), s->on_scroll_data);
}

static bool sv_can_scroll_y(const zrn_scroll_viewer_t* s) { return s->v_visibility != Z_SCROLLBAR_DISABLED && s->extent_h > s->viewport_h; }
static bool sv_can_scroll_x(const zrn_scroll_viewer_t* s) { return s->h_visibility != Z_SCROLLBAR_DISABLED && s->extent_w > s->viewport_w; }

static double sv_reported_offset(const zrn_scroll_viewer_t* const s) {
	return sv_can_scroll_x(s) && !sv_can_scroll_y(s) ? s->offset_x : s->offset_y;
}

static int sv_input(zrn_element_t* const e, zrn_input_t* const in) {
	const auto s = SV(e);
	switch (in->type) {
		case ZRN_IN_PREVIEW_DOWN:
			s->drag_start_x = in->x;
			s->drag_start_y = in->y;
			s->drag_offset_x = s->offset_x;
			s->drag_offset_y = s->offset_y;
			s->last_move_x = in->x;
			s->last_move_y = in->y;
			s->b_drag_moved = false;
			s->velocity_x = s->velocity_y = 0;
			s->b_dragging = false;
			s->base.base.base.b_wants_tick = s->bar_visible > 0;
			return 0;
		case ZRN_IN_PREVIEW_MOVE:
			if (s->b_dragging || e->root == nullptr || e->root->capture == e) return 0;
			if (s->b_drag_moved == 0 && e->root->capture && !e->root->capture->b_blocks_scroll) {
				const float dx = in->x - s->drag_start_x, dy = in->y - s->drag_start_y;
				const bool b_along_y = sv_can_scroll_y(s) && fabsf(dy) > DRAG_THRESHOLD && fabsf(dy) >= fabsf(dx);
				const bool b_along_x = sv_can_scroll_x(s) && fabsf(dx) > DRAG_THRESHOLD && fabsf(dx) > fabsf(dy);
				if (b_along_y || b_along_x) {
					/* the finger moved far enough: the drag is ours, whoever had the pointer loses it */
					s->b_drag_moved = true;
					s->b_dragging = true;
					zrn_set_capture(e->root, e);
					e->b_wants_tick = true;
				}
			}
			return 0;
		case ZRN_IN_PREVIEW_UP:
			return 0;
		case ZRN_IN_POINTER_DOWN:
			/* the viewer itself was hit (no child took it): a drag can start here too */
			s->drag_start_x = in->x;
			s->drag_start_y = in->y;
			s->drag_offset_x = s->offset_x;
			s->drag_offset_y = s->offset_y;
			s->last_move_x = in->x;
			s->last_move_y = in->y;
			s->velocity_x = s->velocity_y = 0;
			if (e->root && (sv_can_scroll_y(s) || sv_can_scroll_x(s))) {
				zrn_set_capture(e->root, e);
				s->b_dragging = true;
				s->b_drag_moved = false;
				e->b_wants_tick = true;
			}
			return 1;
		case ZRN_IN_POINTER_MOVE:
			if (!s->b_dragging) return 0;
			{
				const float dx = in->x - s->drag_start_x, dy = in->y - s->drag_start_y;
				if (!s->b_drag_moved && fabsf(dx) < DRAG_THRESHOLD && fabsf(dy) < DRAG_THRESHOLD) return 1;
				s->b_drag_moved = true;
				if (sv_can_scroll_y(s)) s->offset_y = s->drag_offset_y - dy;
				if (sv_can_scroll_x(s)) s->offset_x = s->drag_offset_x - dx;
				/* velocity in px/s of the content, smoothed; the root ticks in between, so this uses a fixed 16 ms step */
				s->velocity_y = s->velocity_y * 0.5f + (-(in->y - s->last_move_y)) * (1000.0f / 16.0f) * 0.5f;
				s->velocity_x = s->velocity_x * 0.5f + (-(in->x - s->last_move_x)) * (1000.0f / 16.0f) * 0.5f;
				s->last_move_x = in->x;
				s->last_move_y = in->y;
				sv_changed(s, 1);
			}
			return 1;
		case ZRN_IN_POINTER_UP:
			if (!s->b_dragging) return 0;
			s->b_dragging = false;
			if (e->root) zrn_release_capture(e->root, e);
			if (!s->b_drag_moved) s->velocity_x = s->velocity_y = 0;
			e->b_wants_tick = true;
			return 1;
		case ZRN_IN_CAPTURE_LOST:
			s->b_dragging = false;
			return 0;
		case ZRN_IN_WHEEL:
			if (!sv_can_scroll_y(s) && !sv_can_scroll_x(s)) return 0;
			if (sv_can_scroll_y(s)) s->offset_y -= in->delta * 48.0f;
			else s->offset_x -= in->delta * 48.0f;
			s->velocity_x = s->velocity_y = 0;
			sv_changed(s, 1);
			return 1;
		default:
			return 0;
	}
}

static void sv_init(zrn_element_t* const e) {
	const auto s = SV(e);
	s->v_visibility = Z_SCROLLBAR_AUTO;
	s->h_visibility = Z_SCROLLBAR_DISABLED;
	e->b_clip_to_bounds = true;
	e->b_focusable = false;
	e->b_observes_pointer = true;
	s->base.base.h_content = Z_HALIGN_STRETCH;
	s->base.base.v_content = Z_VALIGN_STRETCH;
}

zrn_class_t zrn_class_scroll_viewer = {
	.base = &zrn_class_content_control, .name = "ScrollViewer", .id = Z_CLASS_SCROLL_VIEWER, .size = sizeof(zrn_scroll_viewer_t),
	.init = sv_init, .measure = sv_measure, .arrange = sv_arrange, .paint_over = sv_paint_over, .input = sv_input, .tick = sv_tick
};

void zrn_scroll_viewer_shift_x(zrn_element_t* const e, const double delta) {
	zrn_scroll_viewer_t* const s = SV(e);
	s->offset_x += delta;
	s->drag_offset_x += delta;
}

void zrn_scroll_viewer_set_extra_extent_x(zrn_element_t* const e, const double extra) {
	zrn_scroll_viewer_t* const s = SV(e);
	if (s->extra_extent_x == extra) return;
	s->extra_extent_x = extra;
	zrn_changed_arrange(e);
}

Zid Zscroll_viewer_create(void) {
	const zrn_element_t* const e = zrn_create(Z_CLASS_SCROLL_VIEWER);
	return e ? e->id : Z_INVALID_ID;
}

static zrn_scroll_viewer_t* get_sv(const Zid id) { return SV(zrn_get(id, Z_CLASS_SCROLL_VIEWER)); }

void Zscroll_viewer_set_content(const Zid scroll_viewer, const Zid content) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	zrn_element_t* const c = content != Z_INVALID_ID ? zrn_get(content, Z_CLASS_ELEMENT) : nullptr;
	if (!s || (content != Z_INVALID_ID && !c)) return;
	zrn_content_set(&s->base, c);
}

void Zscroll_viewer_set_vertical_scrollbar_visibility(const Zid scroll_viewer, const ZScrollBarVisibility visibility) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	s->v_visibility = visibility;
	zrn_changed_layout(&s->base.base.base);
}

void Zscroll_viewer_set_horizontal_scrollbar_visibility(const Zid scroll_viewer, const ZScrollBarVisibility visibility) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	s->h_visibility = visibility;
	zrn_changed_layout(&s->base.base.base);
}

void Zscroll_viewer_scroll_to_vertical_offset(const Zid scroll_viewer, const double offset) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	s->offset_y = offset;
	s->velocity_y = 0;
	sv_changed(s, 1);
}

void Zscroll_viewer_scroll_to_horizontal_offset(const Zid scroll_viewer, const double offset) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	s->offset_x = offset;
	s->velocity_x = 0;
	sv_changed(s, 1);
}

double Zscroll_viewer_get_vertical_offset(Zid id) { const zrn_scroll_viewer_t* s = get_sv(id); return s ? s->offset_y : 0; }
double Zscroll_viewer_get_horizontal_offset(Zid scroll_viewer) { const zrn_scroll_viewer_t* s = get_sv(scroll_viewer); return s ? s->offset_x : 0; }
double Zscroll_viewer_get_extent_height(Zid scroll_viewer) { const zrn_scroll_viewer_t* s = get_sv(scroll_viewer); return s ? s->extent_h : 0; }
double Zscroll_viewer_get_extent_width(Zid scroll_viewer) { const zrn_scroll_viewer_t* s = get_sv(scroll_viewer); return s ? s->extent_w : 0; }
double Zscroll_viewer_get_viewport_height(Zid scroll_viewer) { const zrn_scroll_viewer_t* s = get_sv(scroll_viewer); return s ? s->viewport_h : 0; }
double Zscroll_viewer_get_viewport_width(Zid scroll_viewer) { const zrn_scroll_viewer_t* s = get_sv(scroll_viewer); return s ? s->viewport_w : 0; }

void Zscroll_viewer_on_scroll(const Zid scroll_viewer, const ZScrollCallback callback, void* const user_data) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	s->on_scroll = callback;
	s->on_scroll_data = user_data;
}
