// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * What a messaging app needs beyond rows and buttons: pictures, long lists of
 * rows of differing heights, tabs that swipe, a bar fixed at the bottom, and
 * speech bubbles. Their look is in the styles; only behaviour is here.
 *
 * A list keeps no texture per row. Rows are ordinary nodes, made when they come
 * near the screen and kept in a small pool; glyphs and shapes are quads anyway,
 * so a row that moves costs nothing but its vertices. Inserting at the start
 * renumbers the pool and moves the scroll: nothing is made again.
 */
#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "../third_party/stb_image.h"
#pragma GCC diagnostic pop

#include <stdio.h>
#include "zui-internal.h"

/* ---- pictures ---- */
static zui_pixmap *pix_new(int w, int h, uint8_t *owned) {
	zui_pixmap *p = calloc(1, sizeof(*p));
	p->w = w;
	p->h = h;
	p->rgba = owned;
	p->refs = 1;
	return p;
}

/* box filter down to max_side: thumbnails stay small in memory and in the GPU */
static uint8_t *shrink(const uint8_t *src, int w, int h, int *ow, int *oh, int max_side) {
	int big = w > h ? w : h;
	if (max_side <= 0 || big <= max_side) return NULL;
	int f = (big + max_side - 1) / max_side;
	int nw = w / f > 0 ? w / f : 1, nh = h / f > 0 ? h / f : 1;
	uint8_t *d = malloc((size_t)nw * nh * 4);
	for (int y = 0; y < nh; y++)
		for (int x = 0; x < nw; x++) {
			unsigned s[4] = {0};
			for (int j = 0; j < f && y * f + j < h; j++) {
				const uint8_t *row = src + ((size_t)(y * f + j) * w + (size_t)x * f) * 4;
				for (int i = 0; i < f && x * f + i < w; i++)
					for (int c = 0; c < 4; c++) s[c] += row[i * 4 + c];
			}
			for (int c = 0; c < 4; c++) d[((size_t)y * nw + x) * 4 + c] = (uint8_t)(s[c] / (unsigned)(f * f));
		}
	*ow = nw;
	*oh = nh;
	return d;
}

ZUI_EXPORT zui_pixmap *zui_pixmap_decode(const void *bytes, size_t size, int max_side) {
	int w, h, n;
	uint8_t *px = stbi_load_from_memory(bytes, (int)size, &w, &h, &n, 4);
	if (!px) return NULL;
	int nw, nh;
	uint8_t *small = shrink(px, w, h, &nw, &nh, max_side);
	if (small) {
		stbi_image_free(px);
		return pix_new(nw, nh, small);
	}
	uint8_t *copy = malloc((size_t)w * h * 4);
	memcpy(copy, px, (size_t)w * h * 4);
	stbi_image_free(px);
	return pix_new(w, h, copy);
}

ZUI_EXPORT zui_pixmap *zui_pixmap_load(const char *path, int max_side) {
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *b = size > 0 ? malloc((size_t)size) : NULL;
	zui_pixmap *p = NULL;
	if (b && fread(b, 1, (size_t)size, f) == (size_t)size) p = zui_pixmap_decode(b, (size_t)size, max_side);
	free(b);
	fclose(f);
	return p;
}

ZUI_EXPORT zui_pixmap *zui_pixmap_rgba(int w, int h, const void *rgba) {
	uint8_t *copy = malloc((size_t)w * h * 4);
	memcpy(copy, rgba, (size_t)w * h * 4);
	return pix_new(w, h, copy);
}

ZUI_EXPORT void zui_pixmap_free(zui_pixmap *p) {
	if (!p || --p->refs > 0) return;
	if (p->tex && p->gen == zg_generation()) zg_tex_free(p->tex);
	free(p->rgba);
	free(p);
}

ZUI_EXPORT void zui_pixmap_size(const zui_pixmap *p, int *w, int *h) {
	if (w) *w = p->w;
	if (h) *h = p->h;
}

/* A frame uploads at most this many pixels: pictures arrive over a few frames
 * instead of one frame arriving late. */
#define UPLOAD_BUDGET (1024 * 1024)
static long frame_uploaded;
void zui_frame_begin(void) { frame_uploaded = 0; }

bool zui_pixmap_ready(zui_app *app, zui_pixmap *p, bool nearest) {
	if (p->tex && p->gen == zg_generation()) return true;
	long cost = (long)p->w * p->h;
	if (frame_uploaded > 0 && frame_uploaded + cost > UPLOAD_BUDGET) {
		app->dirty = true;	/* next frame */
		return false;
	}
	frame_uploaded += cost;
	p->tex = zg_tex_new(p->w, p->h, p->rgba, nearest);
	p->gen = zg_generation();
	return true;
}

ZUI_EXPORT zui_node *zui_image(zui_node *parent, zui_pixmap *pix, double w, double h) {
	zui_node *n = zui_node_new_kind(parent, ZK_IMAGE);
	n->space = w;
	n->value = h;
	zui_set_image(n, pix);
	return n;
}

ZUI_EXPORT void zui_set_image(zui_node *n, zui_pixmap *pix) {
	if (pix) pix->refs++;
	zui_pixmap_free(n->pix);
	n->pix = pix;
	n->app->laid_out = false;
	n->app->dirty = true;
}

ZUI_EXPORT void zui_set_image_sharp(zui_node *n, bool sharp) { n->sharp = sharp; n->app->dirty = true; }
ZUI_EXPORT void zui_set_radius(zui_node *n, double r) { n->radius = (float)r; n->app->dirty = true; }
ZUI_EXPORT void zui_set_align(zui_node *n, int a) { n->align = a; n->app->dirty = true; }
ZUI_EXPORT void zui_set_badge(zui_node *n, const char *t) {
	free(n->badge);
	n->badge = t && *t ? strdup(t) : NULL;
	n->app->dirty = true;
}
ZUI_EXPORT void zui_set_monogram(zui_node *n, const char *t) {
	free(n->monogram);
	n->monogram = t && *t ? strdup(t) : NULL;
	n->app->laid_out = false;
	n->app->dirty = true;
}
ZUI_EXPORT void zui_set_emphasis(zui_node *n, bool on) { n->emphasis = on; n->app->dirty = true; }

/* a picture filling (x,y,w,h), cropped to keep its shape */
void zui_draw_pixmap(zui_app *app, zui_pixmap *p, float x, float y, float w, float h, float r, bool sharp, uint32_t back) {
	if (!p || !zui_pixmap_ready(app, p, sharp)) {
		zg_rect(x, y, w, h, r, back);
		return;
	}
	float sa = w / h, ia = (float)p->w / p->h, cx = 1, cy = 1;
	if (ia > sa) cx = sa / ia;
	else cy = ia / sa;
	zg_image(p->tex, x, y, w, h, r, cx, cy);
}

/* an avatar: the person's picture, or their initial on the accent */
void zui_draw_leading(zui_app *app, zui_node *n, float x, float y, float size, float r, uint32_t fg) {
	if (n->pix) {
		zui_draw_pixmap(app, n->pix, x, y, size, size, r, false, zc_rgb(app->accent));
		return;
	}
	zg_rect(x, y, size, size, r, n->tint ? zc_rgb(n->tint & 0xffffff) : zc_rgb(app->accent));
	if (n->monogram) {
		struct zt_layout *L = zt_slot_get(&n->t[2], n->monogram, ZT_MEDIUM, size * 0.42f, 0, 0);
		zt_draw(L, x + (size - L->w) / 2, y + (size - L->line_h) / 2, fg);
	}
}

/* ---- long lists ---- */
static void rebuild_prefix(struct zui_list_state *L) {
	free(L->y);
	L->y = malloc(sizeof(float) * (size_t)(L->count + 1));
	float at = 0;
	for (int i = 0; i < L->count; i++) {
		L->y[i] = at;
		at += L->height ? (float)L->height(i, L->data) : L->row_h;
	}
	L->y[L->count] = at;
}

ZUI_EXPORT zui_node *zui_list(zui_node *parent, int count, zui_list_height height, zui_list_bind bind, void *data) {
	zui_node *n = zui_node_new_kind(parent, ZK_LIST);
	struct zui_list_state *L = calloc(1, sizeof(*L));
	n->list = L;
	L->count = count < 0 ? 0 : count;
	L->height = height;
	L->bind = bind;
	L->data = data;
	L->row_h = 64;
	rebuild_prefix(L);
	return n;
}

ZUI_EXPORT void zui_list_set_row_height(zui_node *n, double h) { n->list->row_h = (float)h; rebuild_prefix(n->list); n->app->laid_out = false; }

static void slot_drop(zui_node *list, int i) {
	struct zui_list_state *L = list->list;
	if (L->slot[i]) zui_destroy(L->slot[i]);
	L->slot[i] = NULL;
	L->slot_row[i] = -1;
}

void zui_list_free(zui_node *n) {
	if (!n->list) return;
	free(n->list->y);
	free(n->list);
	n->list = NULL;
}

/* where the page's scroll, for this list, is now */
static float list_scroll(zui_node *n, zui_node **page_out) {
	zui_node *p = n, *tab = NULL;
	while (p->parent) {
		if (p->parent->kind == ZK_TABS) tab = p;
		p = p->parent;
	}
	if (page_out) *page_out = p;
	if (tab && tab->parent->tab_cur != zui_child_index(tab)) return tab->scroll;
	return p->scroll;
}

static void changed_height(zui_node *n, int row, float delta) {
	struct zui_list_state *L = n->list;
	if (fabsf(delta) < 0.01f) return;
	for (int i = row + 1; i <= L->count; i++) L->y[i] += delta;
	zui_node *page;
	float scroll = list_scroll(n, &page);
	n->h += delta;
	for (int i = 0; i < ZUI_LIST_SLOTS; i++)
		if (L->slot[i] && L->slot_row[i] > row) L->slot[i]->y += delta, zui_move_children(L->slot[i], delta);
	page->content_h += delta;
	/* what is on the screen stays where it is, when a row above it turned out
	 * another height than guessed */
	if (n->y + L->y[row] < scroll + page->header_h && !page->stick_end) {
		page->scroll += delta;
		page->down_fix += delta;
	}
}

static void bind_row(zui_node *n, int row) {
	struct zui_list_state *L = n->list;
	int at = -1;
	float best = -1;
	for (int i = 0; i < ZUI_LIST_SLOTS; i++) {
		if (!L->slot[i]) { at = i; break; }
		if (L->slot_row[i] >= L->want0 && L->slot_row[i] <= L->want1) continue;
		float far = (float)(L->slot_row[i] < L->want0 ? L->want0 - L->slot_row[i] : L->slot_row[i] - L->want1);
		if (far > best) { best = far; at = i; }
	}
	if (at < 0) return;
	if (L->slot[at]) slot_drop(n, at);
	zui_app *app = n->app;
	bool was = app->laid_out;
	zui_node *s = zui_column(n);
	L->slot[at] = s;
	L->slot_row[at] = row;
	if (L->bind) L->bind(row, s, L->data);
	float h = zui_layout_at(s, n->x, n->y + L->y[row], n->w);
	app->laid_out = was;
	changed_height(n, row, h - (L->y[row + 1] - L->y[row]));
}

/* the rows in sight are made now, and one more ahead each frame */
void zui_list_ensure(zui_node *n, uint32_t now) {
	struct zui_list_state *L = n->list;
	if (!L->count) return;
	zui_node *page;
	float scroll = list_scroll(n, &page), H = n->app->height;
	float top = scroll - n->y, bot = top + H;
	int lo = 0, hi = L->count;
	while (lo < hi) {
		int m = (lo + hi) / 2;
		if (L->y[m + 1] <= top) lo = m + 1; else hi = m;
	}
	int first = lo;
	int last = first;
	while (last + 1 < L->count && L->y[last + 1] < bot) last++;
	L->want0 = first;
	L->want1 = last;
	bool have[ZUI_LIST_SLOTS * 2];
	(void)have;
	for (int r = first; r <= last; r++) {
		bool found = false;
		for (int i = 0; i < ZUI_LIST_SLOTS && !found; i++) found = L->slot[i] && L->slot_row[i] == r;
		if (!found) bind_row(n, r);
	}
	/* ahead, the way it is going: at most one a frame */
	int dir = first >= L->last_first ? 1 : -1;
	L->last_first = first;
	for (int k = 1; k <= 6; k++) {
		int r = dir > 0 ? last + k : first - k;
		if (r < 0 || r >= L->count) continue;
		bool found = false;
		for (int i = 0; i < ZUI_LIST_SLOTS && !found; i++) found = L->slot[i] && L->slot_row[i] == r;
		if (!found) {
			bind_row(n, r);
			n->app->dirty = true;
			break;
		}
	}
	/* near the start: the app may have more to put there */
	if (first < 15 && !L->asked && n->cb) {
		L->asked = true;
		zui_fire(n, ZUI_EVENT_CHANGE);
	}
}

float zui_list_layout(zui_node *n, float x, float y, float w) {
	struct zui_list_state *L = n->list;
	for (int i = 0; i < ZUI_LIST_SLOTS; i++) {
		if (!L->slot[i]) continue;
		int r = L->slot_row[i];
		float h = zui_layout_at(L->slot[i], x, y + L->y[r], w);
		float was = L->y[r + 1] - L->y[r];
		if (fabsf(h - was) > 0.01f) {
			for (int k = r + 1; k <= L->count; k++) L->y[k] += h - was;
		}
	}
	/* (rows moved by that) */
	for (int i = 0; i < ZUI_LIST_SLOTS; i++)
		if (L->slot[i]) zui_layout_at(L->slot[i], x, y + L->y[L->slot_row[i]], w);
	return L->y[L->count];
}

ZUI_EXPORT void zui_list_set_count(zui_node *n, int count) {
	struct zui_list_state *L = n->list;
	for (int i = 0; i < ZUI_LIST_SLOTS; i++) if (L->slot[i]) slot_drop(n, i);
	L->count = count < 0 ? 0 : count;
	L->asked = false;
	rebuild_prefix(L);
	n->app->laid_out = false;
	n->app->dirty = true;
}

/* k more rows at the end */
ZUI_EXPORT void zui_list_append(zui_node *n, int k) {
	struct zui_list_state *L = n->list;
	int old = L->count;
	L->y = realloc(L->y, sizeof(float) * (size_t)(old + k + 1));
	L->count = old + k;
	for (int i = old; i < old + k; i++)
		L->y[i + 1] = L->y[i] + (L->height ? (float)L->height(i, L->data) : L->row_h);
	n->app->laid_out = false;
	n->app->dirty = true;
}

/* k more rows at the start; nothing on the screen moves or is made again */
ZUI_EXPORT void zui_list_prepend(zui_node *n, int k) {
	struct zui_list_state *L = n->list;
	int old = L->count;
	float *y = malloc(sizeof(float) * (size_t)(old + k + 1));
	float at = 0;
	for (int i = 0; i < k; i++) {
		y[i] = at;
		at += L->height ? (float)L->height(i, L->data) : L->row_h;
	}
	for (int i = 0; i <= old; i++) y[k + i] = at + L->y[i];
	free(L->y);
	L->y = y;
	L->count = old + k;
	L->asked = false;
	for (int i = 0; i < ZUI_LIST_SLOTS; i++)
		if (L->slot[i]) L->slot_row[i] += k;
	zui_node *page;
	list_scroll(n, &page);
	page->scroll += at;
	n->app->down_scroll += at;
	n->app->laid_out = false;
	n->app->dirty = true;
}

/* this row's content changed */
ZUI_EXPORT void zui_list_refresh(zui_node *n, int row) {
	struct zui_list_state *L = n->list;
	for (int i = 0; i < ZUI_LIST_SLOTS; i++)
		if (L->slot[i] && L->slot_row[i] == row) slot_drop(n, i);
	n->app->dirty = true;
}

ZUI_EXPORT void zui_list_allow_more(zui_node *n) { n->list->asked = false; }

/* ---- the end of a page ---- */
ZUI_EXPORT void zui_page_scroll_end(zui_node *page, bool glide) {
	page->stick_end = true;
	page->glide = glide;
	if (glide) page->glide_at = zui_now();
	page->app->dirty = true;
}
ZUI_EXPORT bool zui_page_at_end(zui_node *page, double within) {
	return page->stick_end || page->content_h - page->app->height - page->scroll <= within;
}

/* ---- tabs ---- */
ZUI_EXPORT zui_node *zui_tabs(zui_node *page) {
	zui_node *n = zui_node_new_kind(page, ZK_TABS);
	page->tabs = n;
	return n;
}
ZUI_EXPORT zui_node *zui_tab(zui_node *tabs, const char *title) {
	zui_node *c = zui_column(tabs);
	c->text = strdup(title ? title : "");
	return c;
}
int zui_child_index(const zui_node *c) {
	int i = 0;
	for (const zui_node *p = c->prev; p; p = p->prev) i++;
	return i;
}
zui_node *zui_tab_at(zui_node *tabs, int i) {
	zui_node *c = tabs->first;
	while (c && i-- > 0) c = c->next;
	return c;
}
int zui_tab_count(const zui_node *tabs) {
	int k = 0;
	for (zui_node *c = tabs->first; c; c = c->next) k++;
	return k;
}
ZUI_EXPORT int zui_tabs_current(zui_node *tabs) { return tabs->tab_cur; }
ZUI_EXPORT void zui_tabs_select(zui_node *tabs, int i) {
	int count = zui_tab_count(tabs);
	if (i < 0 || i >= count) i = tabs->tab_cur;
	zui_app *app = tabs->app;
	zui_node *page = tabs->parent;
	uint32_t now = zui_now();
	/* the slide carries on from where the finger left it */
	float from = (float)tabs->tab_cur + zanim_value(&tabs->knob, now, NULL);
	if (i != tabs->tab_cur) {
		zui_node *old = zui_tab_at(tabs, tabs->tab_cur), *nw = zui_tab_at(tabs, i);
		old->scroll = page->scroll;
		tabs->tab_cur = i;
		page->scroll = nw->scroll;
		page->vel = 0;
		app->fling = false;
		tabs->tab_prev = zui_child_index(old);
		app->laid_out = false;
		zui_fire(tabs, ZUI_EVENT_CHANGE);
	}
	tabs->knob.from = from - (float)i;
	tabs->knob.to = 0;
	tabs->knob.start = now;
	tabs->knob.ms = 240;
	zanim_go(&tabs->focus_anim, 0, 240, now);	/* (keeps frames coming) */
	app->dirty = true;
}
