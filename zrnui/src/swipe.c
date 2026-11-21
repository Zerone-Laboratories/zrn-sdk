// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Horizontal swipe on any element (Zelement_on_swipe): the press goes on as usual (tilt, Holding, a list's scroll) until the
 * finger has moved far enough sideways and more sideways than up or down; then the element takes the pointer from whoever had
 * it and reports the swipe until the lift. Driven from the preview pass of the root (host.c), so the element needs no class
 * input of its own. A small table: few elements on screen have one.
 */
#include "zrnui/element.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/input.h"
#include "zrnui/internal/element.h"
#include "zrnui/internal/anim.h"
#include "zrnui/internal/classes.h"
#include "zrnui/internal/swipe.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SWIPE_THRESHOLD 14.0f

typedef struct {
	Zid id;
	ZSwipeCallback callback;
	void* user_data;
	float x0, y0;
	bool b_down, b_active, b_blocked_before;
} swipe_t;

static swipe_t* g_swipes;
static int g_count, g_cap;

static swipe_t* find(const Zid id) {
	for (int i = 0; i < g_count; i++)
		if (g_swipes[i].id == id) return &g_swipes[i];
	return nullptr;
}

void Zelement_on_swipe(const Zid element, const ZSwipeCallback callback, void* const user_data) {
	if (!zrn_lookup(element)) return;
	/* drop the rows of elements that are gone */
	for (int i = 0; i < g_count;)
		if (!zrn_lookup(g_swipes[i].id)) g_swipes[i] = g_swipes[--g_count];
		else i++;
	swipe_t* s = find(element);
	if (!callback) {
		if (s) *s = g_swipes[--g_count];
		return;
	}
	if (!s) {
		if (g_count == g_cap) {
			const int cap = g_cap ? g_cap * 2 : 32;
			swipe_t* const grown = realloc(g_swipes, sizeof *grown * (size_t)cap);
			if (!grown) return;
			g_swipes = grown;
			g_cap = cap;
		}
		s = &g_swipes[g_count++];
	}
	memset(s, 0, sizeof *s);
	s->id = element;
	s->callback = callback;
	s->user_data = user_data;
}

void zrn_swipe_preview(zrn_element_t* const target, const int type, const float x, const float y) {
	if (!g_count || !target) return;
	for (zrn_element_t* e = target; e; e = e->parent) {
		swipe_t* s = find(e->id);
		if (!s) continue;
		const Zid id = e->id;
		zrn_root_t* const r = e->root;
		if (type == ZRN_IN_PREVIEW_DOWN) {
			s->b_down = true;
			s->b_active = false;
			s->x0 = x;
			s->y0 = y;
		} else if (type == ZRN_IN_PREVIEW_MOVE && s->b_down) {
			const float dx = x - s->x0, dy = y - s->y0;
			if (!s->b_active) {
				/* a scroll viewer already dragging keeps it */
				if (r && r->capture && r->capture != e && r->capture->cls == &zrn_class_scroll_viewer && fabsf(dy) > 1.0f) {
					s->b_down = false;
					return;
				}
				/* a slider, a thumb or a text box inside keeps its own drag */
				if (r && r->capture && r->capture != e && r->capture->b_blocks_scroll) return;
				if (fabsf(dx) < SWIPE_THRESHOLD || fabsf(dx) < 1.5f * fabsf(dy)) return;
				s->b_active = true;
				s->b_blocked_before = e->b_blocks_scroll;
				e->b_blocks_scroll = true;
				if (r) {
					zrn_tilt_release(r);
					r->b_hold_armed = false;
					zrn_set_capture(r, e);
				}
				s = find(id);
				if (!s || !zrn_lookup(id)) return;
				s->callback(id, Z_SWIPE_STARTED, dx, s->user_data);
				return;
			}
			s->callback(id, Z_SWIPE_DELTA, dx, s->user_data);
		} else if (type == ZRN_IN_PREVIEW_UP && s->b_down) {
			const bool b_active = s->b_active;
			s->b_down = s->b_active = false;
			if (!b_active) return;
			e->b_blocks_scroll = s->b_blocked_before;
			if (r) zrn_release_capture(r, e);
			s->callback(id, Z_SWIPE_COMPLETED, x - s->x0, s->user_data);
		}
		return; /* the innermost one only */
	}
}
