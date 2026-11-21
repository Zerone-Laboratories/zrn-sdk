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
#include "zrnui/internal/anim.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SV(e) ZRN_AS(zrn_scroll_viewer_t, e)

/* a drag starts after the finger moved this far */
static constexpr float DRAG_THRESHOLD = 10.0f;
/* the speed of a fling: measured over this much of the end of the drag; a finger still this long before lifting flings
 * nothing; and never faster than this (points a second) */
static constexpr double FLING_WINDOW_MS = 100.0;
static constexpr double FLING_STILL_MS = 60.0;
static constexpr float FLING_MAX = 4500.0f;
static constexpr int SAMPLES = 8;

static double sv_now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec * 1e3 + (double)t.tv_nsec / 1e6;
}

static void sv_sample_reset(zrn_scroll_viewer_t* const s) { s->sample_count = s->sample_next = 0; }

static void sv_sample(zrn_scroll_viewer_t* const s, const float x, const float y) {
	s->sample_t[s->sample_next] = sv_now_ms();
	s->sample_x[s->sample_next] = x;
	s->sample_y[s->sample_next] = y;
	s->sample_next = (s->sample_next + 1) % SAMPLES;
	if (s->sample_count < SAMPLES) s->sample_count++;
}

/* The finger's speed at the end of the drag, in points a second of the content (the opposite of the finger): from the
 * oldest sample in the window to the newest, by their real times (moves do not come every 16 ms: the panel reports at
 * its own rate and the loop coalesces them). Zero when the finger had stopped. */
static void sv_release_velocity(zrn_scroll_viewer_t* const s, const float up_x, const float up_y) {
	s->velocity_x = s->velocity_y = 0;
	sv_sample(s, up_x, up_y);
	const int newest = (s->sample_next + SAMPLES - 1) % SAMPLES;
	const double t1 = s->sample_t[newest];
	int oldest = newest;
	for (int k = 1; k < s->sample_count; k++) {
		const int i = (newest + SAMPLES - k) % SAMPLES;
		if (t1 - s->sample_t[i] > FLING_WINDOW_MS) break;
		oldest = i;
	}
	if (oldest == newest) return;
	/* the last move before the lift: the finger rested if it was long before */
	const int before = (newest + SAMPLES - 1) % SAMPLES;
	if (t1 - s->sample_t[before] > FLING_STILL_MS) return;
	const double span = (t1 - s->sample_t[oldest]) / 1000.0;
	if (span < 0.008) return;
	s->velocity_x = zrn_clampf(-(float)((double)(s->sample_x[newest] - s->sample_x[oldest]) / span), -FLING_MAX, FLING_MAX);
	s->velocity_y = zrn_clampf(-(float)((double)(s->sample_y[newest] - s->sample_y[oldest]) / span), -FLING_MAX, FLING_MAX);
}

/* the bars stay in view this long after the last scroll */
static constexpr float BAR_HOLD_MS = 800.0f;


static void sv_limits(const zrn_scroll_viewer_t* const s, double* const max_x, double* const max_y) {
	*max_x = s->extent_w > s->viewport_w ? s->extent_w - s->viewport_w : 0;
	*max_y = s->extent_h > s->viewport_h ? s->extent_h - s->viewport_h : 0;
}

/* WP overscroll: the content goes on past an end by as much as it is pulled (with resistance, never more than a fifth of
 * the viewport) and eases back, critically damped: no bounce, no squeeze. */
static constexpr float OVER_SHARE = 0.2f;
static constexpr float OVER_OMEGA = 22.0f;

static float sv_over_max(const zrn_scroll_viewer_t* const s) { return (float)s->viewport_h * OVER_SHARE; }

static float sv_rubber(const zrn_scroll_viewer_t* const s, const float pull) {
	const float m = sv_over_max(s);
	if (m <= 0) return 0;
	const float a = m * (1.0f - expf(-fabsf(pull) / (m * 2.0f)));
	return pull < 0 ? -a : a;
}

static float sv_unrubber(const zrn_scroll_viewer_t* const s, const float over) {
	const float m = sv_over_max(s);
	if (m <= 0 || over == 0) return 0;
	const float a = zrn_minf(fabsf(over), m * 0.98f);
	const float pull = -m * 2.0f * logf(1.0f - a / m);
	return over < 0 ? -pull : pull;
}

/* past an end the content slides on by over_y with the finger (rows keep their size: nothing is squeezed) and eases back */
static void sv_apply_over(zrn_scroll_viewer_t* const s) {
	zrn_element_t* const c = s->base.content;
	if (!c) return;
	const float t = fabsf(s->over_y) < 0.25f ? 0.0f : -s->over_y;
	if (c->translate_y == t) return;
	c->translate_y = t;
	zrn_update_transformed(c);
	zrn_changed_render(c);
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
	zrn_want_tick(e);
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

/* Where the anchor is in the content (y from its top); false when it is gone or not inside the content. */
static bool anchor_in_content(const zrn_scroll_viewer_t* const s, double* const y) {
	const zrn_element_t* const content = s->base.content;
	const zrn_element_t* a = s->anchor != Z_INVALID_ID ? zrn_lookup(s->anchor) : nullptr;
	double sum = 0;
	if (!content) return false;
	for (; a && a != content; a = a->parent) sum += a->rect.y;
	if (a != content) return false;
	*y = sum;
	return true;
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
		/* room at the ends for what is laid over them with a backdrop effect (a header, a composer, an app bar): the first
		 * and last items scroll clear of it */
		float inset_top = 0, inset_bottom = 0;
		if (s->v_visibility != Z_SCROLLBAR_DISABLED) zrn_backdrop_insets(e, &inset_top, &inset_bottom);
		/* the elements over the ends may be arranged after this viewer (a composer, an app bar): their places were last
		 * pass's, so a change takes another pass, with the new ones */
		const float d_top = inset_top - s->inset_top, d_bottom = inset_bottom - s->inset_bottom;
		if (fabsf(d_top) > 0.25f || fabsf(d_bottom) > 0.25f) zrn_changed_arrange(e);
		s->inset_top = inset_top;
		s->inset_bottom = inset_bottom;
		s->extent_h = s->v_visibility == Z_SCROLLBAR_DISABLED ? final.height : d.height + inset_top + inset_bottom;
		/* the offset as it was, before the new sizes clamp it: the anchor's shift is from there (a viewport that grows at the
		 * end would otherwise lose its growth twice, to the clamp and to the shift, and the list stop short of its end) */
		const double unclamped = s->offset_y;
		sv_clamp(s);
		zrn_arrange(cc->content, zrn_rect((float)-s->offset_x, (float)(s->inset_top - s->offset_y), w, h));
		double y;
		if (s->anchor != Z_INVALID_ID && anchor_in_content(s, &y)) {
			/* what changed above the anchor moves the offset by as much; a viewport of another height keeps it as far from the bottom */
			/* ...and keeps its place over what is laid over the ends: room that grows at the bottom (the composer coming up
			 * with the keyboard) lifts it by as much, room that grows at the top keeps it where it was on screen */
			const double shift = s->b_anchor_known ? (y - s->anchor_y) + (s->anchor_viewport_h - s->viewport_h) + d_top + d_bottom : 0;
			if (shift != 0) {
				const double before = s->offset_y;
				s->offset_y = unclamped + shift;
				sv_clamp(s);
				if (s->offset_y != before) zrn_arrange(cc->content, zrn_rect((float)-s->offset_x, (float)(s->inset_top - s->offset_y), w, h));
			}
			s->anchor_y = y;
			s->anchor_viewport_h = s->viewport_h;
			s->b_anchor_known = true;
		} else if (s->anchor != Z_INVALID_ID && !zrn_lookup(s->anchor)) {
			s->anchor = Z_INVALID_ID;
			s->b_anchor_known = false;
		}
	} else {
		s->extent_w = final.width;
		s->extent_h = final.height;
	}
	if (s->over_y != 0) sv_apply_over(s);
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
		const double want_y = s->offset_y;
		sv_clamp(s);
		if (s->offset_x == old_x) s->velocity_x = 0;
		/* a fling that reaches an end goes on into the overscroll and springs back */
		if (s->offset_y != want_y && s->over_v == 0 && s->over_y == 0 && fabsf(s->velocity_y) > 300.0f) {
			s->over_v = s->velocity_y * 0.25f;
			s->velocity_y = 0;
		}
		if (s->offset_y == old_y) s->velocity_y = 0;
		zrn_changed_arrange(e);
		s->bar_visible = BAR_HOLD_MS;
		b_busy = true;
		b_fire = s->offset_y != old_y || s->offset_x != old_x;
	} else if (!s->b_dragging) {
		s->velocity_x = s->velocity_y = 0;
	}
	if (!s->b_dragging && (s->over_y != 0 || s->over_v != 0)) {
		/* the spring, critically damped, in steps of at most 8 ms */
		float left = dt / 1000.0f;
		const float m = sv_over_max(s);
		while (left > 0) {
			const float h = left > 0.008f ? 0.008f : left;
			s->over_v += (-OVER_OMEGA * OVER_OMEGA * s->over_y - 2.0f * OVER_OMEGA * s->over_v) * h;
			s->over_y += s->over_v * h;
			left -= h;
		}
		if (s->over_y > m) { s->over_y = m; if (s->over_v > 0) s->over_v = 0; }
		if (s->over_y < -m) { s->over_y = -m; if (s->over_v < 0) s->over_v = 0; }
		if (fabsf(s->over_y) < 0.3f && fabsf(s->over_v) < 8.0f) s->over_y = s->over_v = 0;
		sv_apply_over(s);
		b_busy = true;
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
		case ZRN_IN_PREVIEW_DOWN: {
			/* a press while the list coasts (a fling, the overscroll easing back) stops it and is not a tap: the viewer
			 * takes the pointer, so nothing under the finger is pressed or clicked (WP; the shell's "coasting: stopped") */
			const bool b_coasting = fabsf(s->velocity_y) > 60.0f || fabsf(s->velocity_x) > 60.0f || fabsf(s->over_v) > 60.0f;
			s->b_pressed = true;
			s->drag_start_x = in->x;
			s->drag_start_y = in->y;
			s->drag_offset_x = s->offset_x;
			s->drag_offset_y = s->offset_y;
			s->over_pull = sv_unrubber(s, s->over_y);
			s->last_move_x = in->x;
			s->last_move_y = in->y;
			s->b_drag_moved = false;
			s->velocity_x = s->velocity_y = 0;
			s->b_dragging = false;
			sv_sample_reset(s);
			sv_sample(s, in->x, in->y);
			if (b_coasting && e->root) {
				s->b_dragging = true;
				zrn_set_capture(e->root, e);
				zrn_tilt_release(e->root);
				e->root->b_hold_armed = false;
				zrn_want_tick(e);
				return 1;
			}
			if (s->bar_visible > 0) zrn_want_tick(&s->base.base.base);
			else s->base.base.base.b_wants_tick = false;
			return 0;
		}
		case ZRN_IN_PREVIEW_MOVE:
			if (!s->b_pressed || s->b_dragging || e->root == nullptr || e->root->capture == e) return 0;
			/* past the slop and mostly along the axis the drag is the list's, whoever was pressed (a bubble that holds no
			 * pointer included); only a child that keeps its own drag (a slider, a swipe already going) is left alone */
			if (s->b_drag_moved == 0 && !(e->root->capture && e->root->capture->b_blocks_scroll)) {
				const float dx = in->x - s->drag_start_x, dy = in->y - s->drag_start_y;
				const bool b_along_y = sv_can_scroll_y(s) && fabsf(dy) > DRAG_THRESHOLD && fabsf(dy) >= fabsf(dx);
				const bool b_along_x = sv_can_scroll_x(s) && fabsf(dx) > DRAG_THRESHOLD && fabsf(dx) > fabsf(dy);
				if (b_along_y || b_along_x) {
					/* the finger moved far enough: the drag is ours, whoever had the pointer loses it */
					s->b_drag_moved = true;
					s->b_dragging = true;
					zrn_set_capture(e->root, e);
					/* the row under the finger stops leaning: the list moves, not the row */
					zrn_tilt_release(e->root);
					e->root->b_hold_armed = false; /* nor is it a long press any more */
					zrn_want_tick(e);
				}
			}
			return 0;
		case ZRN_IN_PREVIEW_UP:
			s->b_pressed = false;
			return 0;
		case ZRN_IN_POINTER_DOWN:
			/* the viewer itself was hit (no child took it): a drag can start here too */
			s->b_pressed = true;
			s->drag_start_x = in->x;
			s->drag_start_y = in->y;
			s->drag_offset_x = s->offset_x;
			s->drag_offset_y = s->offset_y;
			s->over_pull = sv_unrubber(s, s->over_y);
			s->last_move_x = in->x;
			s->last_move_y = in->y;
			s->velocity_x = s->velocity_y = 0;
			sv_sample_reset(s);
			sv_sample(s, in->x, in->y);
			if (e->root && (sv_can_scroll_y(s) || sv_can_scroll_x(s))) {
				zrn_set_capture(e->root, e);
				s->b_dragging = true;
				s->b_drag_moved = false;
				zrn_want_tick(e);
			}
			return 1;
		case ZRN_IN_POINTER_MOVE:
			if (!s->b_dragging) return 0;
			{
				const float dx = in->x - s->drag_start_x, dy = in->y - s->drag_start_y;
				if (!s->b_drag_moved && fabsf(dx) < DRAG_THRESHOLD && fabsf(dy) < DRAG_THRESHOLD) return 1;
				s->b_drag_moved = true;
				if (sv_can_scroll_y(s)) {
					double mx, my;
					sv_limits(s, &mx, &my);
					const double raw = s->drag_offset_y - dy + s->over_pull;
					s->offset_y = raw;
					/* past an end the finger pulls the content into the squeeze, with resistance */
					s->over_y = raw < 0 ? sv_rubber(s, (float)raw) : raw > my ? sv_rubber(s, (float)(raw - my)) : 0;
					s->over_v = 0;
					sv_apply_over(s);
				}
				if (sv_can_scroll_x(s)) s->offset_x = s->drag_offset_x - dx;
				/* the content follows the finger 1:1; the speed for the fling is worked out at the lift (sv_release_velocity) */
				sv_sample(s, in->x, in->y);
				s->last_move_x = in->x;
				s->last_move_y = in->y;
				sv_changed(s, 1);
			}
			return 1;
		case ZRN_IN_POINTER_UP:
			s->b_pressed = false;
			if (!s->b_dragging) return 0;
			s->b_dragging = false;
			if (e->root) zrn_release_capture(e->root, e);
			if (s->b_drag_moved) sv_release_velocity(s, in->x, in->y);
			else s->velocity_x = s->velocity_y = 0;
			/* let go past an end: the spring brings it back, nothing flings */
			if (s->over_y != 0) s->velocity_y = 0;
			s->over_pull = 0;
			if (!sv_can_scroll_x(s)) s->velocity_x = 0;
			if (!sv_can_scroll_y(s)) s->velocity_y = 0;
			zrn_want_tick(e);
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
void Zscroll_viewer_get_content_insets(const Zid scroll_viewer, float* const top, float* const bottom) {
	const zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (top) *top = s ? s->inset_top : 0;
	if (bottom) *bottom = s ? s->inset_bottom : 0;
}
double Zscroll_viewer_get_viewport_width(Zid scroll_viewer) { const zrn_scroll_viewer_t* s = get_sv(scroll_viewer); return s ? s->viewport_w : 0; }

void Zscroll_viewer_on_scroll(const Zid scroll_viewer, const ZScrollCallback callback, void* const user_data) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	s->on_scroll = callback;
	s->on_scroll_data = user_data;
}

void Zscroll_viewer_set_anchor(const Zid scroll_viewer, const Zid element) {
	zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	if (!s) return;
	if (element != Z_INVALID_ID && !zrn_get(element, Z_CLASS_ELEMENT)) return;
	s->anchor = element;
	s->b_anchor_known = false;
	/* where it is now is where it stays: known at the next arrange */
	zrn_changed_arrange(&s->base.base.base);
}

Zid Zscroll_viewer_get_anchor(const Zid scroll_viewer) {
	const zrn_scroll_viewer_t* const s = get_sv(scroll_viewer);
	return s && s->anchor != Z_INVALID_ID && zrn_lookup(s->anchor) ? s->anchor : Z_INVALID_ID;
}
