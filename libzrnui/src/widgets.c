// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * What Windows Phone 8.1 had and the first libzrnui did not: check boxes and radio
 * buttons, a password box and suggestions under a field, the progress ring, the
 * list picker (in place, as ZrnMetroCore's zmc_picker opens it), date and time
 * pickers, tiles that turn over, the Hub, press and hold, and what goes over a
 * page: a menu, a dialog, the jump list (ZrnMetroCore's letter board).
 *
 * Nothing here knows which style is in use. It draws with the palette and the
 * metrics, and the renderer turns every fill into dots in DotMatrix; where a
 * fill would drown its own text there (a lit panel under dotted type) the
 * widget draws a frame instead (`dotted`).
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zui-internal.h"

#define M (app->style->m)

static bool dotted(const zui_app *app) { return app->style == &zui_style_dotmatrix; }
static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static char *dupstr(const char *s) { return strdup(s ? s : ""); }

static void text_at(struct zt_slot *slot, const char *text, int font, float size, float x, float y,
		uint32_t color, float max_w) {
	struct zt_layout *L = zt_slot_get(slot, text, font, size, max_w, max_w > 0 ? 1 : 0);
	zt_draw(L, x, y, color);
}

/* one line of text drawn now, not cached (overlays: few and short) */
static float text_now(const char *text, int font, float size, float x, float y, uint32_t color, float max_w) {
	struct zt_layout *L = zt_layout_make(text, font, size, max_w, max_w > 0 ? 1 : 0);
	zt_draw(L, x, y, color);
	float w = L->w;
	zt_layout_free(L);
	return w;
}

static bool in_footer(const zui_node *n) {
	for (; n; n = n->parent) if (n->kind == ZK_FOOTER) return true;
	return false;
}

/* a node's top in window points, whatever scrolls */
static float screen_y(const zui_node *n) {
	zui_app *app = n->app;
	return n->y - (in_footer(n) || !app->page ? 0 : app->page->scroll);
}

float zui_node_hub_shift(const zui_node *n) {
	for (const zui_node *p = n->parent; p; p = p->parent)
		if (p->kind == ZK_HUB) return zui_hub_offset(p);
	return 0;
}

/* ============================================================ check, radio, ring == */

ZUI_EXPORT zui_node *zui_check(zui_node *parent, const char *title, bool on, zui_cb cb, void *data) {
	zui_node *n = zui_node_new_kind(parent, ZK_CHECK);
	zui_set_text(n, title);
	n->on = on;
	zanim_set(&n->knob, on ? 1 : 0);
	n->cb = cb;
	n->data = data;
	return n;
}

ZUI_EXPORT zui_node *zui_radio(zui_node *parent, const char *title, bool on, zui_cb cb, void *data) {
	zui_node *n = zui_check(parent, title, on, cb, data);
	n->kind = ZK_RADIO;
	return n;
}

void zui_set_checked(zui_node *n, bool on) {
	if (n->on == on) return;
	n->on = on;
	zanim_go(&n->knob, on ? 1 : 0, 120, zui_now());
	n->app->dirty = true;
}

ZUI_EXPORT zui_node *zui_ring(zui_node *parent) { return zui_node_new_kind(parent, ZK_RING); }

static void draw_check(zui_app *app, zui_node *n, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	float k = zanim_value(&n->knob, now, NULL);
	float x = n->x, h = n->h, y = n->y + (h - 26) / 2;
	uint32_t ink = n->enabled ? pal.text : pal.text2;
	bool radio = n->kind == ZK_RADIO;
	if (radio) {
		/* WP's radio: a ring, a disc in it when chosen */
		zg_ring(x + 13, y + 13, 13, 2, ink);
		if (k > 0.01f) zg_circle(x + 13, y + 13, 6.5f * k, ink);
	} else {
		zg_border(x, y, 26, 26, 0, 2, ink);
		if (k > 0.01f) {
			if (dotted(app)) zui_icon("check", x + 2, y + 2, 22, 2, zc_alpha(ink, k));
			else {
				zg_rect(x + 4, y + 4, 18, 18, 0, zc_alpha(ink, k));
				zui_icon("check", x + 1, y + 1, 24, 2.2f, zc_alpha(pal.bg, k));
			}
		}
	}
	text_at(&n->t[0], n->text, ZT_LIGHT, 20, x + 26 + 14, n->y + (h - zt_line_height(20)) / 2, ink,
		n->w - 26 - 14);
}

static void draw_ring(zui_app *app, zui_node *n, uint32_t now) {
	uint32_t acc = zc_rgb(app->accent);
	float cx = n->x + n->w / 2, cy = n->y + n->h / 2, r = 13;
	for (int i = 0; i < 5; i++) {
		/* each dot starts a little after the one before, quick through the
		 * bottom and slow over the top as WP's ring is */
		float u = (float)(now % 2400) / 2400.0f - i * 0.055f;
		u -= floorf(u);
		float a = (float)M_PI * 2 * (u < 0.5f ? 2 * u * u : 1 - 2 * (1 - u) * (1 - u));
		float fade = u > 0.02f && u < 0.98f ? 1 : 0;
		if (fade) zg_circle(cx + r * sinf(a), cy - r * cosf(a), 2.4f, acc);
	}
}

/* radios that share a parent are one group */
static void radio_chosen(zui_node *n) {
	for (zui_node *s = n->parent ? n->parent->first : NULL; s; s = s->next)
		if (s != n && s->kind == ZK_RADIO && s->on) {
			zui_set_checked(s, false);
			zui_fire(s, ZUI_EVENT_CHANGE);
		}
}

/* ============================================================ password, suggestions == */

struct suggest { char **item; int n; };

ZUI_EXPORT void zui_field_set_password(zui_node *f, bool on) {
	f->password = on;
	f->app->dirty = true;
}

ZUI_EXPORT void zui_field_set_suggestions(zui_node *f, const char *const *items, int count) {
	struct suggest *s = f->ext;
	if (s) {
		for (int i = 0; i < s->n; i++) free(s->item[i]);
		free(s->item);
		free(s);
	}
	f->ext = NULL;
	if (!items || count <= 0) return;
	s = calloc(1, sizeof(*s));
	s->item = calloc((size_t)count, sizeof(char *));
	s->n = count;
	for (int i = 0; i < count; i++) s->item[i] = dupstr(items[i]);
	f->ext = s;
}

static bool contains_nocase(const char *hay, const char *needle) {
	size_t n = strlen(needle);
	if (!n) return true;
	for (; *hay; hay++) {
		size_t i = 0;
		while (i < n && hay[i] && tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i])) i++;
		if (i == n) return true;
	}
	return false;
}

#define SUGGEST_MAX 4
#define SUGGEST_ROW 40
/* what hangs under the field with the keyboard: the matches, and where */
static int suggestions_for(zui_app *app, int out[SUGGEST_MAX], float *x, float *y, float *w) {
	zui_node *f = app->focus;
	if (!f || f->kind != ZK_FIELD || !f->ext || !f->text || !*f->text) return 0;
	struct suggest *s = f->ext;
	int c = 0;
	for (int i = 0; i < s->n && c < SUGGEST_MAX; i++)
		if (contains_nocase(s->item[i], f->text) && strcasecmp(s->item[i], f->text) != 0) out[c++] = i;
	if (!c) return 0;
	float top = screen_y(f) + (f->inline_hint ? 0 : 22) + M.field_h;
	float h = c * SUGGEST_ROW;
	if (top + h > app->height - 4) top = screen_y(f) + (f->inline_hint ? 0 : 22) - h;	/* over it instead */
	*x = f->x;
	*y = top;
	*w = f->w;
	return c;
}

static void draw_suggestions(zui_app *app) {
	int idx[SUGGEST_MAX];
	float x, y, w;
	int c = suggestions_for(app, idx, &x, &y, &w);
	if (!c) return;
	struct suggest *s = app->focus->ext;
	struct zui_palette pal = zui_pal(app);
	bool dots = dotted(app);
	zg_set_xf(NULL);
	zg_rect(x, y, w, c * SUGGEST_ROW, 0, dots ? pal.bg : 0xffffffff);
	zg_border(x, y, w, c * SUGGEST_ROW, 0, dots ? 1.2f : 1, dots ? pal.text : 0x999999ff);
	for (int i = 0; i < c; i++)
		text_now(s->item[idx[i]], ZT_REGULAR, 17, x + 10, y + i * SUGGEST_ROW + 9, dots ? pal.text : 0x000000ff, w - 20);
}

bool zui_suggest_touch(zui_app *app, float x, float y, float scroll) {
	(void)scroll;
	int idx[SUGGEST_MAX];
	float fx, fy, fw;
	int c = suggestions_for(app, idx, &fx, &fy, &fw);
	if (!c || x < fx || x >= fx + fw || y < fy || y >= fy + c * SUGGEST_ROW) return false;
	zui_node *f = app->focus;
	struct suggest *s = f->ext;
	int i = (int)((y - fy) / SUGGEST_ROW);
	zui_set_text(f, s->item[idx[i]]);
	zui_haptic("tap");
	zui_fire(f, ZUI_EVENT_SUBMIT);
	return true;
}

/* ============================================================ list picker == */

struct picker { char **opt; int count, sel; };
#define PICK_ROW 40
#define PICK_OPEN_MS 250
#define PICK_CLOSE_MS 200

ZUI_EXPORT zui_node *zui_picker(zui_node *parent, const char *caption, const char *const *options, int count,
		int selected, zui_cb cb, void *data) {
	zui_node *n = zui_node_new_kind(parent, ZK_PICKER);
	zui_set_text(n, caption);
	struct picker *p = calloc(1, sizeof(*p));
	p->count = count < 0 ? 0 : count;
	p->opt = calloc((size_t)(p->count ? p->count : 1), sizeof(char *));
	for (int i = 0; i < p->count; i++) p->opt[i] = dupstr(options[i]);
	p->sel = selected < 0 || selected >= p->count ? 0 : selected;
	n->ext = p;
	n->cb = cb;
	n->data = data;
	n->app->n_pickers++;
	return n;
}

ZUI_EXPORT int zui_picker_selected(zui_node *n) {
	return n->kind == ZK_PICKER && n->ext ? ((struct picker *)n->ext)->sel : -1;
}

ZUI_EXPORT void zui_picker_select(zui_node *n, int index) {
	struct picker *p = n->kind == ZK_PICKER ? n->ext : NULL;
	if (!p || index < 0 || index >= p->count) return;
	p->sel = index;
	n->app->dirty = true;
}

static float picker_open(const zui_node *n, uint32_t now) { return clamp01(zanim_value(&n->knob, now, NULL)); }

static float layout_picker(zui_node *n) {
	struct picker *p = n->ext;
	/* (a file render has no motion: the end state) */
	float o = n->app->wl ? picker_open(n, zui_now()) : (n->on ? 1 : 0);
	return 24 + PICK_ROW + ((p->count - 1) * PICK_ROW + 8) * o + 18;
}

static void picker_set_open(zui_node *n, bool open) {
	if (n->on == open) return;
	n->on = open;
	zanim_go(&n->knob, open ? 1 : 0, open ? PICK_OPEN_MS : PICK_CLOSE_MS, zui_now());
	n->app->laid_out = false;
	n->app->dirty = true;
	if (open) n->app->open_picker = n;
	else if (n->app->open_picker == n) n->app->open_picker = NULL;
}

static void draw_picker(zui_app *app, zui_node *n, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	struct picker *p = n->ext;
	float o = app->wl ? picker_open(n, now) : (n->on ? 1 : 0);
	float x = n->x, w = n->w, y = n->y + 24, h = PICK_ROW + ((p->count - 1) * PICK_ROW + 8) * o;
	bool dots = dotted(app);
	text_at(&n->t[0], n->text, ZT_REGULAR, 15, n->x, n->y, pal.text2, w);
	/* the box: an outline closed, the edit background open */
	uint32_t paper = dots ? pal.bg : 0xffffffff;
	if (!dots) zg_rect(x, y, w, h, 0, zc_alpha(paper, o));
	zg_border(x, y, w, h, 0, 2, pal.text);
	/* (no clip: an option never leaves the box, from closed to open) */
	for (int i = 0; i < p->count; i++) {
		/* each option slides from where the chosen one sits to its own place */
		float top = y + 4 * o + i * PICK_ROW * o;
		uint32_t ink;
		if (i == p->sel) ink = zc_mix(pal.text, pal.accent_text, o);
		else {
			if (o <= 0.001f) continue;
			ink = zc_alpha(dots ? pal.text : 0x000000ff, o);
		}
		if (i == p->sel && !dots) ink = zc_mix(0xffffffff, zc_rgb(app->accent), o);
		text_now(p->opt[i], ZT_REGULAR, 17, x + 10, top + 9, ink, w - 20);
	}
}

static void picker_tapped(zui_app *app, zui_node *n) {
	struct picker *p = n->ext;
	if (!n->on) {
		picker_set_open(n, true);
		return;
	}
	float py = app->down_y + (in_footer(n) || !app->page ? 0 : app->page->scroll);
	int i = (int)floorf((py - (n->y + 24 + 4)) / PICK_ROW);
	if (i >= 0 && i < p->count) {
		bool changed = i != p->sel;
		p->sel = i;
		picker_set_open(n, false);
		zui_haptic("tick");
		if (changed) zui_fire(n, ZUI_EVENT_CHANGE);
	} else {
		picker_set_open(n, false);
	}
}

/* a tap anywhere but on the open picker shuts it */
void zui_pickers_close_except(zui_app *app, zui_node *keep) {
	zui_node *o = app->open_picker;
	if (o && o != keep) picker_set_open(o, false);
}

static bool pickers_moving(zui_node *n, uint32_t now) {
	for (zui_node *c = n->first; c; c = c->next) {
		if (n->kind == ZK_LIST) break;
		if (c->kind == ZK_PICKER) {
			bool running = false;
			zanim_value(&c->knob, now, &running);
			if (running) return true;
		}
		if (pickers_moving(c, now)) return true;
	}
	return false;
}

/* ============================================================ tiles == */

struct tile { bool wide; char *back; int timer, period; };
#define TILE_GAP 6

ZUI_EXPORT zui_node *zui_tiles(zui_node *parent) { return zui_node_new_kind(parent, ZK_TILES); }

ZUI_EXPORT zui_node *zui_tile(zui_node *tiles, const char *title, const char *icon, bool wide, zui_cb cb, void *data) {
	zui_node *n = zui_node_new_kind(tiles, ZK_TILE);
	zui_set_text(n, title);
	zui_set_icon(n, icon);
	struct tile *t = calloc(1, sizeof(*t));
	t->wide = wide;
	n->ext = t;
	n->cb = cb;
	n->data = data;
	return n;
}

static void tile_flip(void *data) {
	zui_node *n = data;
	struct tile *t = n->ext;
	if (!t || !t->back) return;
	n->on = !n->on;
	zanim_go(&n->knob, n->on ? 1 : 0, 700, zui_now());
	n->app->dirty = true;
}

ZUI_EXPORT void zui_tile_set_live(zui_node *n, const char *back, int period_ms) {
	struct tile *t = n->kind == ZK_TILE ? n->ext : NULL;
	if (!t) return;
	if (t->timer) zui_timer_cancel(n->app, t->timer);
	t->timer = 0;
	free(t->back);
	t->back = back && *back ? strdup(back) : NULL;
	t->period = period_ms;
	if (n->on) {
		n->on = false;
		zanim_set(&n->knob, 0);
	}
	if (t->back && period_ms > 0) {
		/* staggered, so a screenful of tiles does not turn as one */
		static int stagger;
		t->timer = zui_timer(n->app, period_ms + (stagger++ % 5) * 700, true, tile_flip, n);
	}
	n->app->dirty = true;
}

static float layout_tiles(zui_node *g, float x, float y, float w) {
	float side = (w - TILE_GAP) / 2, cx = 0, cy = 0;
	for (zui_node *c = g->first; c; c = c->next) {
		if (!c->visible) continue;
		struct tile *t = c->ext;
		bool wide = t && t->wide;
		/* a wide one has a row to itself; a square goes where there is room */
		if (cx > 0 && (wide || cx + side > w + 0.5f)) { cy += side + TILE_GAP; cx = 0; }
		c->x = x + cx;
		c->y = y + cy;
		c->w = wide ? w : side;
		c->h = side;
		cx += c->w + TILE_GAP;
		if (cx > w - 1) { cy += side + TILE_GAP; cx = 0; }
	}
	return cx > 0 ? cy + side : cy > 0 ? cy - TILE_GAP : 0;
}

static void draw_tile(zui_app *app, zui_node *n, const struct zg_xf *base, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	struct tile *t = n->ext;
	float k = zanim_value(&n->knob, now, NULL);
	struct zg_xf xf = *base;
	bool back = k > 0.5f;
	if (t->back && k > 0.001f && k < 0.999f) {
		/* turning over about its middle: the other face once it is edge on */
		zg_xf_rotate_x(&xf, back ? (k - 1) * 180 : k * 180, n->y + n->h / 2);
		app->dirty = true;
	}
	zg_set_xf(&xf);
	float x = n->x, y = n->y, w = n->w, h = n->h;
	uint32_t base_c = n->tint ? zc_rgb(n->tint & 0xffffff) : zc_rgb(app->accent);
	bool dots = dotted(app);
	uint32_t face = back ? zc_mix(base_c, 0x000000ff, 0.35f) : base_c;
	uint32_t ink = dots ? (back ? pal.text2 : pal.text) : 0xffffffff;
	if (dots) zg_border(x, y, w, h, 0, 1.2f, back ? pal.text2 : pal.text);
	else zg_rect(x, y, w, h, 0, face);
	if (back) {
		struct zt_layout *L = zt_layout_make(t->back, ZT_REGULAR, 15, w - 20, 0);
		zt_draw(L, x + 10, y + 10, ink);
		zt_layout_free(L);
		return;
	}
	if (n->icon) zui_icon(n->icon, x + (w - 44) / 2, y + (h - 44) / 2 - 8, 44, 2.2f, ink);
	text_at(&n->t[0], n->text, ZT_REGULAR, 14.5f, x + 9, y + h - 9 - zt_line_height(14.5f), ink, w - 18 - (n->badge ? 30 : 0));
	if (n->badge) {
		struct zt_layout *B = zt_slot_get(&n->t[4], n->badge, ZT_LIGHT, 22, 0, 0);
		zt_draw(B, x + w - 9 - B->w, y + h - 6 - B->line_h, ink);
	}
}

/* ============================================================ hub == */

#define HUB_PEEK 44	/* how much of the next section shows */
#define HUB_GAP 12
#define HUB_HEAD 46

struct hub { zui_pixmap *bg; };

static float hub_stride(const zui_node *hub) {
	zui_app *app = hub->app;
	return app->width - M.margin - HUB_PEEK + HUB_GAP;
}

static int hub_count(const zui_node *hub) {
	int c = 0;
	for (const zui_node *s = hub->first; s; s = s->next) c += s->visible;
	return c;
}

float zui_hub_offset(const zui_node *hub) {
	return zanim_value(&hub->knob, zui_now(), NULL);
}

ZUI_EXPORT zui_node *zui_hub(zui_node *page) {
	zui_node *n = zui_node_new_kind(page, ZK_HUB);
	n->ext = calloc(1, sizeof(struct hub));
	page->hub = n;
	return n;
}

ZUI_EXPORT zui_node *zui_hub_section(zui_node *hub, const char *title) {
	zui_node *s = zui_node_new_kind(hub, ZK_HUBSEC);
	zui_set_text(s, title);
	return s;
}

ZUI_EXPORT void zui_hub_set_background(zui_node *hub, zui_pixmap *pix) {
	struct hub *h = hub->ext;
	if (!h) return;
	zui_pixmap_free(h->bg);
	h->bg = pix;
	if (pix) pix->refs++;
	hub->app->dirty = true;
}

static float hub_max(const zui_node *hub) {
	int c = hub_count(hub);
	return c > 1 ? (c - 1) * hub_stride(hub) : 0;
}

ZUI_EXPORT int zui_hub_current(zui_node *hub) {
	float s = hub_stride(hub);
	return s > 0 ? (int)lroundf(zui_hub_offset(hub) / s) : 0;
}

ZUI_EXPORT void zui_hub_show(zui_node *hub, int section) {
	int c = hub_count(hub);
	if (section < 0) section = 0;
	if (section >= c) section = c - 1;
	zanim_go(&hub->knob, section * hub_stride(hub), hub->app->wl ? 420 : 0, zui_now());
	hub->app->dirty = true;
}

void zui_hub_drag(zui_app *app, zui_node *hub, float dx) {
	float raw = app->hub_from - dx, max = hub_max(hub);
	/* past either end it follows the finger less and less */
	if (raw < 0) raw *= 0.4f;
	else if (raw > max) raw = max + (raw - max) * 0.4f;
	zanim_set(&hub->knob, raw);
	app->dirty = true;
}

void zui_hub_release(zui_app *app, zui_node *hub, float v) {
	float s = hub_stride(hub), off = zui_hub_offset(hub);
	int from = (int)lroundf(app->hub_from / s), c = hub_count(hub), to = from;
	float moved = off - app->hub_from;
	/* as pivots: 50 points, or 0.3 points a ms, go on to the next */
	if (v > 0.3f || (moved > 50 && v >= -0.3f)) to = from + 1;
	else if (v < -0.3f || (moved < -50 && v <= 0.3f)) to = from - 1;
	if (to < 0) to = 0;
	if (to >= c) to = c - 1;
	zanim_go(&hub->knob, to * s, 420, zui_now());
	hub->knob.v0 = v;
	hub->knob.w = 0.018f;
	app->dirty = true;
}

void zui_hub_title_shift(zui_app *app, zui_node *page, float *shift) {
	(void)app;
	*shift = page->hub ? -zui_hub_offset(page->hub) * 0.35f : 0;
}

static float layout_hub(zui_node *hub) {
	zui_app *app = hub->app;
	float stride = hub_stride(hub), sw = stride - HUB_GAP, h = 0;
	hub->x = 0;
	hub->w = app->width;
	int i = 0;
	for (zui_node *s = hub->first; s; s = s->next) {
		if (!s->visible) continue;
		float sx = M.margin + i++ * stride, at = hub->y + HUB_HEAD;
		for (zui_node *c = s->first; c; c = c->next) {
			if (!c->visible) continue;
			at += zui_layout_at(c, sx, at, sw) + M.gap;
		}
		s->x = sx;
		s->y = hub->y;
		s->w = sw;
		s->h = at - hub->y;
		if (s->h > h) h = s->h;
	}
	/* it fills the screen under the title, so the picture behind has somewhere to be */
	float fill = app->height - hub->y;
	return h > fill ? h : fill;
}

static void draw_hub(zui_app *app, zui_node *hub, const struct zg_xf *base, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	struct hub *hb = hub->ext;
	float off = zanim_value(&hub->knob, now, NULL), W = app->width;
	if (hb && hb->bg) {
		/* the picture moves a quarter as fast as the sections, and is dimmed to read over */
		float span = W + hub_max(hub) * 0.25f, x = -off * 0.25f;
		struct zg_xf id = *base;
		zg_set_xf(&id);
		zui_draw_pixmap(app, hb->bg, x, hub->y, span, hub->h, 0, false, pal.surface2);
		if (!dotted(app)) zg_rect(0, hub->y, W, hub->h, 0, zc_alpha(pal.bg, 0.6f));
	}
	struct zg_xf xf = *base;
	zg_xf_translate(&xf, -off, 0, 0);
	for (zui_node *s = hub->first; s; s = s->next) {
		if (!s->visible) continue;
		float sx = s->x - off;
		if (sx > W || sx + s->w < 0) continue;
		zg_set_xf(&xf);
		text_at(&s->t[0], s->text, ZT_LIGHT, 27, s->x - 1, hub->y + 2, pal.text, s->w);
		for (zui_node *c = s->first; c; c = c->next)
			if (c->visible) zui_draw_node(app, c, &xf, now);
	}
}

/* ============================================================ hold == */

ZUI_EXPORT void zui_set_hold(zui_node *n, zui_cb cb, void *data, zui_free_fn free_data) {
	if (n->hold_free) n->hold_free(n->hold_data);
	n->hold_cb = cb;
	n->hold_data = data;
	n->hold_free = free_data;
}

static void hold_fire(void *data) {
	zui_app *app = data;
	app->hold_timer = 0;
	zui_node *n = app->hold_node;
	if (!n || !app->touching || app->dragging || app->swiping || app->hub_drag || app->pressed != n) return;
	app->hold_fired = true;
	zui_haptic("heavy");
	zanim_go(&app->press, 0, 160, zui_now());
	app->firing++;
	n->event = ZUI_EVENT_HOLD;
	if (n->hold_cb) n->hold_cb(n, n->hold_data);
	n->event = ZUI_EVENT_NONE;
	app->dirty = true;
	zui_cb_end(app);
}

void zui_hold_start(zui_app *app, zui_node *n, uint32_t time) {
	zui_hold_cancel(app);
	app->hold_fired = false;
	if (!n || !n->hold_cb) { app->hold_node = NULL; return; }
	app->hold_node = n;
	app->hold_since = time;
	app->hold_timer = zui_timer(app, 500, false, hold_fire, app);
}

void zui_hold_cancel(zui_app *app) {
	if (app->hold_timer) zui_timer_cancel(app, app->hold_timer);
	app->hold_timer = 0;
}

/* a lift that comes after half a second on one spot is a hold, even if the timer did not wake us */
bool zui_hold_on_lift(zui_app *app, uint32_t time, uint32_t down_time) {
	if (app->hold_fired) { app->hold_fired = false; app->hold_node = NULL; return true; }
	zui_node *n = app->hold_node;
	app->hold_node = NULL;
	zui_hold_cancel(app);
	if (n && n == app->pressed && (int32_t)(time - down_time) >= 500) {
		app->hold_node = n;
		app->touching = true;	/* hold_fire checks it */
		hold_fire(app);
		app->touching = false;
		app->hold_fired = false;
		app->hold_node = NULL;
		return true;
	}
	return false;
}

/* ============================================================ the overlay == */

enum { OV_MENU, OV_DIALOG, OV_BOARD, OV_DATE, OV_TIME };

struct col {
	int n, lo, pad;		/* n values from lo, shown with at least pad digits */
	const char *const *names;	/* or these */
	bool loop;
	float pos, target, v;	/* pos: which value is in the middle, between values while it moves */
	float x, w;
};

struct ov {
	int kind;
	uint32_t t0, t_close, last;
	bool closing;
	zui_choice_fn cb;
	void *data;
	char *title, *msg;
	char **item;
	bool *live;
	int count;
	float y;			/* a menu's top; a board's top */
	int pressed;			/* what the finger is on: item, button, cell */
	float down_x, down_y;
	bool moved;
	/* pickers */
	struct col col[3];
	int ncol, drag;
	float drag_y0, drag_pos0, last_y;
	uint32_t last_t;
	zui_node *source;
};

static struct ov *OV(zui_app *app) { return app->ov; }

static void ov_free(struct ov *o) {
	if (!o) return;
	free(o->title);
	free(o->msg);
	for (int i = 0; i < o->count; i++) if (o->item) free(o->item[i]);
	free(o->item);
	free(o->live);
	free(o);
}

void zui_widgets_free(zui_app *app) {
	ov_free(app->ov);
	app->ov = NULL;
}

static void drop_focus(zui_app *app) {
	if (app->focus) {
		app->focus->focused = false;
		app->focus = NULL;
		zui_osk(app, false);
	}
}

static struct ov *ov_open(zui_app *app, int kind) {
	ov_free(app->ov);
	struct ov *o = calloc(1, sizeof(*o));
	o->kind = kind;
	o->t0 = zui_now();
	o->pressed = -1;
	o->drag = -1;
	app->ov = o;
	drop_focus(app);
	zui_pickers_close_except(app, NULL);
	app->pressed = NULL;
	app->dirty = true;
	return o;
}

static void ov_items(struct ov *o, const char *const *items, int count) {
	o->count = count < 0 ? 0 : count;
	o->item = calloc((size_t)(o->count ? o->count : 1), sizeof(char *));
	for (int i = 0; i < o->count; i++) o->item[i] = dupstr(items[i]);
}

ZUI_EXPORT bool zui_overlay_open(zui_app *app) { return app->ov && !OV(app)->closing; }

ZUI_EXPORT void zui_overlay_close(zui_app *app) {
	struct ov *o = app->ov;
	if (!o || o->closing) return;
	o->closing = true;
	o->t_close = zui_now();
	app->dirty = true;
}

bool zui_widgets_back(zui_app *app) {
	if (!zui_overlay_open(app)) return false;
	zui_overlay_close(app);
	return true;
}

ZUI_EXPORT void zui_menu(zui_app *app, zui_node *anchor, const char *const *items, int count,
		zui_choice_fn cb, void *data) {
	if (count <= 0) return;
	struct ov *o = ov_open(app, OV_MENU);
	ov_items(o, items, count);
	o->cb = cb;
	o->data = data;
	float h = count * 50 + 16;
	float y = anchor ? screen_y(anchor) + anchor->h : app->height - h;
	if (y + h > app->height - 8) y = app->height - 8 - h;	/* all of it on the screen */
	if (y < 0) y = 0;
	o->y = y;
}

ZUI_EXPORT void zui_dialog(zui_app *app, const char *title, const char *message, const char *const *buttons,
		int count, zui_choice_fn cb, void *data) {
	if (count <= 0) return;
	if (count > 3) count = 3;
	struct ov *o = ov_open(app, OV_DIALOG);
	ov_items(o, buttons, count);
	o->title = dupstr(title);
	o->msg = dupstr(message);
	o->cb = cb;
	o->data = data;
}

ZUI_EXPORT void zui_jump_list(zui_app *app, const char *const *labels, const bool *live, int count,
		zui_choice_fn cb, void *data) {
	if (count <= 0) return;
	struct ov *o = ov_open(app, OV_BOARD);
	ov_items(o, labels, count);
	o->live = calloc((size_t)count, sizeof(bool));
	for (int i = 0; i < count; i++) o->live[i] = live ? live[i] : true;
	o->cb = cb;
	o->data = data;
}

/* ---- the geometry of each ---- */
#define BOARD_COLS 4
#define BOARD_GAP 9
#define BOARD_FONT 27
#define BOARD_INSET 9
#define BOARD_STAGGER 12.0f
#define SWIVEL_IN_MS 300.0f
#define SWIVEL_OUT_MS 200.0f

static void board_geometry(zui_app *app, struct ov *o, float *x0, float *top, float *side) {
	float W = app->width;
	float header = app->page ? app->page->header_h : 0;
	float s = (W - 2 * M.margin - (BOARD_COLS - 1) * BOARD_GAP) / BOARD_COLS;
	int rows = (o->count + BOARD_COLS - 1) / BOARD_COLS;
	float avail = app->height - header - 2 * BOARD_GAP;
	if (rows * (s + BOARD_GAP) > avail) s = avail / rows - BOARD_GAP;
	*x0 = M.margin;
	*top = header + BOARD_GAP;
	*side = s;
}

static int board_cell_at(zui_app *app, struct ov *o, float x, float y) {
	float x0, top, side;
	board_geometry(app, o, &x0, &top, &side);
	for (int i = 0; i < o->count; i++) {
		float cx = x0 + (i % BOARD_COLS) * (side + BOARD_GAP), cy = top + (i / BOARD_COLS) * (side + BOARD_GAP);
		if (x >= cx && x < cx + side && y >= cy && y < cy + side) return i;
	}
	return -1;
}

struct dlg { float y, h, ty, my, by, bw; struct zt_layout *T, *L; };

static void dlg_geometry(zui_app *app, struct ov *o, struct dlg *d) {
	float W = app->width, pad = M.margin;
	d->T = zt_layout_make(o->title, ZT_LIGHT, 26, W - 2 * pad, 0);
	d->L = zt_layout_make(o->msg, ZT_REGULAR, 16, W - 2 * pad, 0);
	d->ty = 22;
	d->my = d->ty + d->T->h + 10;
	d->by = d->my + (o->msg && *o->msg ? d->L->h : 0) + 24;
	d->h = d->by + 40 + 22;
	d->y = (app->height - d->h) / 3;
	if (d->y < 0) d->y = 0;
	d->bw = (W - 2 * pad - 12 * (o->count - 1)) / o->count;
}

static void dlg_free(struct dlg *d) {
	zt_layout_free(d->T);
	zt_layout_free(d->L);
}

static int dlg_button_at(zui_app *app, struct ov *o, float x, float y) {
	struct dlg d;
	dlg_geometry(app, o, &d);
	int hit = -1;
	for (int i = 0; i < o->count; i++) {
		float bx = M.margin + i * (d.bw + 12), by = d.y + d.by;
		if (x >= bx && x < bx + d.bw && y >= by && y < by + 40) hit = i;
	}
	dlg_free(&d);
	return hit;
}

#define MENU_ROW 50
static int menu_row_at(zui_app *app, struct ov *o, float x, float y) {
	(void)x;
	float top = o->y + 8;
	if (y < top || y >= top + o->count * MENU_ROW) return -1;
	return (int)((y - top) / MENU_ROW);
}

/* pickers: a bar of two round buttons at the bottom, columns over it */
#define BAR_H 70
#define ROW_H 62
static int pick_button_at(zui_app *app, float x, float y) {
	if (y < app->height - BAR_H) return -1;
	return x < app->width / 2 ? 0 : 1;	/* 0 the tick, 1 the cross */
}

static const char *const MONTHS[12] = { "January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December" };

static int days_in(int year, int month) {
	static const int d[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
	return d[(month - 1 + 12) % 12];
}

static int col_value(const struct col *c) {
	int i = (int)lroundf(c->pos);
	if (c->loop) return ((i % c->n) + c->n) % c->n;
	return i < 0 ? 0 : i >= c->n ? c->n - 1 : i;
}

#define YEAR_LO 1900
#define YEAR_N 201

/* what the date/time overlay is editing is kept in the picker node */
struct stamp { int year, month, day, hour, minute; bool time; zui_cb cb; void *data; };

static void pick_layout_cols(zui_app *app, struct ov *o) {
	float W = app->width, avail = W - 2 * M.margin;
	if (o->kind == OV_DATE) {
		float widths[3] = { 140, 70, 100 }, total = 310 + 18, sc = avail < total ? avail / total : 1, x = (W - total * sc) / 2;
		for (int i = 0; i < 3; i++) {
			o->col[i].x = x;
			o->col[i].w = widths[i] * sc;
			x += widths[i] * sc + 9 * sc;
		}
	} else {
		float w = 110, x = (W - 2 * w - 20) / 2;
		for (int i = 0; i < 2; i++) { o->col[i].x = x; o->col[i].w = w; x += w + 20; }
	}
}

static void open_picker(zui_node *n) {
	zui_app *app = n->app;
	struct stamp *s = n->ext;
	struct ov *o = ov_open(app, s->time ? OV_TIME : OV_DATE);
	o->source = n;
	o->title = dupstr(n->text);
	if (!s->time) {
		/* month, day, year: as the phone's English has them */
		o->ncol = 3;
		o->col[0] = (struct col){ .n = 12, .names = MONTHS, .loop = true, .pos = (float)(s->month - 1) };
		o->col[1] = (struct col){ .n = days_in(s->year, s->month), .lo = 1, .pad = 1, .loop = true, .pos = (float)(s->day - 1) };
		o->col[2] = (struct col){ .n = YEAR_N, .lo = YEAR_LO, .pad = 4, .loop = false, .pos = (float)(s->year - YEAR_LO) };
	} else {
		o->ncol = 2;
		o->col[0] = (struct col){ .n = 24, .lo = 0, .pad = 2, .loop = true, .pos = (float)s->hour };
		o->col[1] = (struct col){ .n = 60, .lo = 0, .pad = 2, .loop = true, .pos = (float)s->minute };
	}
	for (int i = 0; i < o->ncol; i++) o->col[i].target = o->col[i].pos;
	pick_layout_cols(app, o);
}

static void stamp_text(zui_node *n) {
	struct stamp *s = n->ext;
	char buf[48];
	if (s->time) snprintf(buf, sizeof(buf), "%02d:%02d", s->hour, s->minute);
	else snprintf(buf, sizeof(buf), "%d %s %d", s->day, MONTHS[(s->month - 1 + 12) % 12], s->year);
	zui_set_detail(n, buf);
}

static void picker_row_tapped(zui_node *n, void *data) { (void)data; open_picker(n); }

static zui_node *stamp_row(zui_node *parent, const char *title, struct stamp *init, zui_cb cb, void *data) {
	zui_node *n = zui_item(parent, title, NULL, picker_row_tapped, NULL);
	struct stamp *s = malloc(sizeof(*s));
	*s = *init;
	s->cb = cb;
	s->data = data;
	n->ext = s;
	stamp_text(n);
	return n;
}

ZUI_EXPORT zui_node *zui_date_picker(zui_node *parent, const char *title, int year, int month, int day,
		zui_cb cb, void *data) {
	struct stamp s = { .year = year, .month = month < 1 ? 1 : month > 12 ? 12 : month, .day = day < 1 ? 1 : day, .time = false };
	if (s.day > days_in(year, s.month)) s.day = days_in(year, s.month);
	if (s.year < YEAR_LO) s.year = YEAR_LO;
	if (s.year >= YEAR_LO + YEAR_N) s.year = YEAR_LO + YEAR_N - 1;
	return stamp_row(parent, title, &s, cb, data);
}

ZUI_EXPORT zui_node *zui_time_picker(zui_node *parent, const char *title, int hour, int minute, zui_cb cb, void *data) {
	struct stamp s = { .hour = ((hour % 24) + 24) % 24, .minute = ((minute % 60) + 60) % 60, .time = true };
	return stamp_row(parent, title, &s, cb, data);
}

ZUI_EXPORT void zui_date_picker_get(zui_node *n, int *y, int *m, int *d) {
	struct stamp *s = n->ext;
	if (!s || s->time) return;
	if (y) *y = s->year;
	if (m) *m = s->month;
	if (d) *d = s->day;
}

ZUI_EXPORT void zui_date_picker_set(zui_node *n, int y, int m, int d) {
	struct stamp *s = n->ext;
	if (!s || s->time) return;
	s->year = y < YEAR_LO ? YEAR_LO : y >= YEAR_LO + YEAR_N ? YEAR_LO + YEAR_N - 1 : y;
	s->month = m < 1 ? 1 : m > 12 ? 12 : m;
	s->day = d < 1 ? 1 : d > days_in(s->year, s->month) ? days_in(s->year, s->month) : d;
	stamp_text(n);
}

ZUI_EXPORT void zui_time_picker_get(zui_node *n, int *h, int *m) {
	struct stamp *s = n->ext;
	if (!s || !s->time) return;
	if (h) *h = s->hour;
	if (m) *m = s->minute;
}

ZUI_EXPORT void zui_time_picker_set(zui_node *n, int h, int m) {
	struct stamp *s = n->ext;
	if (!s || !s->time) return;
	s->hour = ((h % 24) + 24) % 24;
	s->minute = ((m % 60) + 60) % 60;
	stamp_text(n);
}

static void pick_commit(zui_app *app, struct ov *o) {
	zui_node *n = o->source;
	if (!n) return;
	struct stamp *s = n->ext;
	if (o->kind == OV_DATE) {
		s->month = col_value(&o->col[0]) + 1;
		s->year = YEAR_LO + col_value(&o->col[2]);
		int d = col_value(&o->col[1]) + 1, dim = days_in(s->year, s->month);
		s->day = d > dim ? dim : d;
	} else {
		s->hour = col_value(&o->col[0]);
		s->minute = col_value(&o->col[1]);
	}
	stamp_text(n);
	if (s->cb) {
		app->firing++;
		n->event = ZUI_EVENT_CHANGE;
		s->cb(n, s->data);
		n->event = ZUI_EVENT_NONE;
		zui_cb_end(app);
	}
}

/* the day column is as long as its month */
static void pick_fit_days(struct ov *o) {
	if (o->kind != OV_DATE) return;
	int month = col_value(&o->col[0]) + 1, year = YEAR_LO + col_value(&o->col[2]);
	struct col *d = &o->col[1];
	int dim = days_in(year, month);
	if (dim == d->n) return;
	int v = col_value(d);
	d->n = dim;
	int nv = v >= dim ? dim - 1 : v;
	d->pos = d->target = (float)nv;
}

/* ---- touch ---- */
static void ov_choose(zui_app *app, struct ov *o, int index) {
	zui_choice_fn cb = o->cb;
	void *data = o->data;
	zui_overlay_close(app);
	zui_haptic("tap");
	if (cb) {
		app->firing++;
		cb(app, index, data);
		app->dirty = true;
		zui_cb_end(app);
	}
}

bool zui_widgets_touch(zui_app *app, int phase, float x, float y, uint32_t time) {
	struct ov *o = app->ov;
	if (!o) return false;
	uint32_t now = zui_now();
	if (o->closing) {
		if ((int32_t)(now - o->t_close) < 260) return true;	/* going: it takes the touch and ignores it */
		ov_free(o);	/* gone, though no frame has said so */
		app->ov = NULL;
		return false;
	}
	app->dirty = true;
	if (phase == 0) {
		o->down_x = x;
		o->down_y = y;
		o->moved = false;
		o->pressed = -1;
		switch (o->kind) {
		case OV_MENU: o->pressed = menu_row_at(app, o, x, y); break;
		case OV_DIALOG: o->pressed = dlg_button_at(app, o, x, y); break;
		case OV_BOARD: {
			int c = board_cell_at(app, o, x, y);
			o->pressed = c >= 0 && o->live[c] ? c : -1;
			break;
		}
		default:
			o->pressed = pick_button_at(app, x, y);
			o->drag = -1;
			if (o->pressed < 0)
				for (int i = 0; i < o->ncol; i++)
					if (x >= o->col[i].x - 6 && x < o->col[i].x + o->col[i].w + 6) o->drag = i;
			if (o->drag >= 0) {
				struct col *c = &o->col[o->drag];
				o->drag_y0 = o->last_y = y;
				o->drag_pos0 = c->pos;
				c->v = 0;
				c->target = c->pos;
				o->last_t = time;
			}
			break;
		}
		return true;
	}
	if (phase == 1) {
		if (fabsf(y - o->down_y) > 8 || fabsf(x - o->down_x) > 8) o->moved = true;
		if (o->drag >= 0 && o->moved) {
			struct col *c = &o->col[o->drag];
			float raw = o->drag_pos0 - (y - o->drag_y0) / ROW_H;
			if (!c->loop) raw = raw < -0.4f ? -0.4f : raw > c->n - 0.6f ? c->n - 0.6f : raw;
			float dt = (float)(int32_t)(time - o->last_t);
			if (dt > 0) c->v = c->v * 0.3f + (-(y - o->last_y) / ROW_H / dt) * 0.7f;
			o->last_y = y;
			o->last_t = time;
			c->pos = c->target = raw;
			pick_fit_days(o);
		}
		return true;
	}
	/* up or cancel */
	int pressed = o->pressed;
	o->pressed = -1;
	if (phase != 2) {
		if (o->drag >= 0) {
			struct col *c = &o->col[o->drag];
			c->target = roundf(c->pos);
		}
		o->drag = -1;
		return true;
	}
	switch (o->kind) {
	case OV_MENU: {
		int hit = menu_row_at(app, o, x, y);
		if (!o->moved && hit >= 0 && hit == pressed) ov_choose(app, o, hit);
		else if (!o->moved && hit < 0 && pressed < 0) zui_overlay_close(app);
		break;
	}
	case OV_DIALOG: {
		int hit = dlg_button_at(app, o, x, y);
		if (!o->moved && hit >= 0 && hit == pressed) ov_choose(app, o, hit);
		break;
	}
	case OV_BOARD: {
		int hit = board_cell_at(app, o, x, y);
		if (!o->moved && hit >= 0 && hit == pressed) ov_choose(app, o, hit);
		else if (!o->moved && hit < 0) zui_overlay_close(app);
		break;
	}
	default: {
		if (pressed >= 0 && pick_button_at(app, x, y) == pressed && !o->moved) {
			zui_haptic("tap");
			if (pressed == 0) pick_commit(app, o);
			zui_overlay_close(app);
			break;
		}
		if (o->drag >= 0) {
			struct col *c = &o->col[o->drag];
			if (!o->moved) {
				/* a tap on a row above or under the middle brings it there */
				float cy = (app->height - BAR_H) / 2 + 10;
				int k = (int)lroundf((y - cy) / ROW_H);
				c->target = roundf(c->pos) + (float)k;
				if (!c->loop) c->target = c->target < 0 ? 0 : c->target > c->n - 1 ? c->n - 1 : c->target;
			} else {
				/* it goes on a little with the speed it was let go at, then settles on a row */
				float to = roundf(c->pos + c->v * 160);
				if (!c->loop) to = to < 0 ? 0 : to > c->n - 1 ? c->n - 1 : to;
				c->target = to;
			}
		}
		break;
	}
	}
	o->drag = -1;
	(void)now;
	return true;
}

/* ---- motion ---- */
static bool ov_moving(const struct ov *o, uint32_t now) {
	if (o->closing) return (int32_t)(now - o->t_close) < 260;
	uint32_t age = now - o->t0;
	if (age < 420 + (o->kind == OV_BOARD ? (uint32_t)(o->count * BOARD_STAGGER) : 0)) return true;
	for (int i = 0; i < o->ncol; i++)
		if (fabsf(o->col[i].target - o->col[i].pos) > 0.003f || o->drag == i) return true;
	return false;
}

static void ov_step(zui_app *app, struct ov *o, uint32_t now) {
	float dt = o->last ? (float)(int32_t)(now - o->last) : 16;
	if (dt > 64) dt = 64;
	if (dt < 0) dt = 0;
	o->last = now;
	for (int i = 0; i < o->ncol; i++) {
		struct col *c = &o->col[i];
		if (o->drag == i) continue;
		float d = c->target - c->pos;
		if (fabsf(d) > 0.003f) {
			c->pos += d * (1 - expf(-dt / 80));
			if (fabsf(c->target - c->pos) <= 0.003f) c->pos = c->target;
			app->dirty = true;
		}
	}
	if (o->kind == OV_DATE) pick_fit_days(o);
}

bool zui_widgets_animating(zui_app *app, uint32_t now) {
	struct ov *o = app->ov;
	if (o && ov_moving(o, now)) return true;
	if (app->n_pickers && app->page && pickers_moving(app->page, now)) return true;
	zui_node *hub = app->page ? app->page->hub : NULL;
	if (hub) {
		bool running = false;
		zanim_value(&hub->knob, now, &running);
		if (running || app->hub_drag) return true;
	}
	return false;
}

static bool has_ring(zui_node *n) {
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		if (c->kind == ZK_RING) return true;
		if (c->kind != ZK_LIST && has_ring(c)) return true;
	}
	return false;
}

static bool has_flip(zui_node *n, uint32_t now) {
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		if (c->kind == ZK_TILE) {
			bool running = false;
			zanim_value(&c->knob, now, &running);
			if (running) return true;
		} else if (c->kind != ZK_LIST && has_flip(c, now)) return true;
	}
	return false;
}

/* what moves with no one touching: rings and tiles, only while they are on this page */
bool zui_widgets_page_moving(zui_app *app, zui_node *page, uint32_t now) {
	(void)app;
	return has_ring(page) || has_flip(page, now);
}

void zui_widgets_step(zui_app *app, uint32_t now) {
	struct ov *o = app->ov;
	if (o) {
		ov_step(app, o, now);
		if (o->closing && (int32_t)(now - o->t_close) >= 260) {
			ov_free(o);
			app->ov = NULL;
			app->dirty = true;
		}
	}
	/* a picker opening changes the page's layout every frame until it has opened */
	if (app->n_pickers && app->page && pickers_moving(app->page, now)) {
		app->laid_out = false;
		app->dirty = true;
	} else if (app->n_pickers && app->wl) {
		app->laid_out = false;
	}
	if (app->page && app->page->hub) {
		bool running = false;
		zanim_value(&app->page->hub->knob, now, &running);
		if (running) app->dirty = true;
	}
}

/* ---- drawing the overlays ---- */
static float ov_alpha(const struct ov *o, uint32_t now) {
	if (getenv("ZUI_NO_OPEN") && !o->closing) return 1;
	if (o->closing) return 1 - clamp01((float)(int32_t)(now - o->t_close) / 200);
	return clamp01((float)(int32_t)(now - o->t0) / 120);
}

static void scrim(zui_app *app, float a, bool solid) {
	struct zui_palette pal = zui_pal(app);
	float al = solid ? 1 : dotted(app) ? 0.97f : (float)(pal.scrim & 0xff) / 255.0f;
	zg_rect(0, 0, app->width, app->height, 0, zc_alpha(pal.bg, al * a));
}

static void draw_menu(zui_app *app, struct ov *o, float a) {
	struct zui_palette pal = zui_pal(app);
	bool dots = dotted(app);
	float W = app->width, h = o->count * MENU_ROW + 16, y = o->y;
	uint32_t paper = dots ? pal.bg : pal.text, ink = dots ? pal.text : pal.bg;
	zg_rect(0, y, W, h, 0, zc_alpha(paper, a));
	if (dots) zg_border(0, y, W, h, 0, 1.2f, zc_alpha(pal.text, a));
	for (int i = 0; i < o->count; i++) {
		float top = y + 8 + i * MENU_ROW;
		bool down = o->pressed == i;
		if (down) zg_rect(0, top, W, MENU_ROW, 0, zc_alpha(zc_rgb(app->accent), a));
		text_now(o->item[i], ZT_LIGHT, 24, M.margin, top + 8, zc_alpha(down ? 0xffffffff : ink, a), W - 2 * M.margin);
	}
}

static void draw_dialog(zui_app *app, struct ov *o, float a, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	struct dlg d;
	dlg_geometry(app, o, &d);
	float W = app->width, e = zui_ease_out(a);
	struct zg_xf xf;
	zg_xf_identity(&xf);
	zg_xf_translate(&xf, 0, (1 - e) * -24, 0);
	zg_set_xf(&xf);
	bool dots = dotted(app);
	zg_rect(0, d.y, W, d.h, 0, zc_alpha(dots ? pal.bg : pal.surface2, a));
	if (dots) zg_border(0, d.y, W, d.h, 0, 1.2f, zc_alpha(pal.text, a));
	zt_draw(d.T, M.margin, d.y + d.ty, zc_alpha(pal.text, a));
	if (o->msg && *o->msg) zt_draw(d.L, M.margin, d.y + d.my, zc_alpha(pal.text2, a));
	for (int i = 0; i < o->count; i++) {
		float bx = M.margin + i * (d.bw + 12), by = d.y + d.by;
		bool down = o->pressed == i;
		if (down) zg_rect(bx, by, d.bw, 40, 0, zc_alpha(pal.text, a));
		else zg_border(bx, by, d.bw, 40, 0, 2, zc_alpha(pal.text, a));
		struct zt_layout *L = zt_layout_make(o->item[i], ZT_STRONG, M.button_text, d.bw - 16, 1);
		zt_draw(L, bx + (d.bw - L->w) / 2, by + (40 - L->line_h) / 2, zc_alpha(down ? pal.bg : pal.text, a));
		zt_layout_free(L);
	}
	dlg_free(&d);
	zg_set_xf(NULL);
	(void)now;
}

static float swivel_ease(float t, bool in) {
	t = clamp01(t);
	float k = (in ? 200.0f : 150.0f) / 30.0f;
	return in ? 1 - (expf(k * (1 - t)) - 1) / (expf(k) - 1) : (expf(k * t) - 1) / (expf(k) - 1);
}

static void draw_board(zui_app *app, struct ov *o, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	float x0, top, side;
	board_geometry(app, o, &x0, &top, &side);
	bool nomove = getenv("ZUI_NO_OPEN") != NULL;
	float age = (float)(int32_t)(now - (o->closing ? o->t_close : o->t0));
	bool dots = dotted(app);
	for (int i = 0; i < o->count; i++) {
		float own = age - i * BOARD_STAGGER;
		float angle, alpha = 1;
		if (o->closing) {
			if (own >= SWIVEL_OUT_MS) continue;
			float t = own / SWIVEL_OUT_MS;
			angle = own <= 0 ? 0 : 60 * swivel_ease(t, false);
			alpha = own <= 0 ? 1 : 1 - clamp01(t);
		} else {
			if (nomove) own = 1e6f;
			if (own < 0) continue;	/* its turn has not come */
			float t = own / SWIVEL_IN_MS, e = swivel_ease(t, true);
			angle = -45 * (1 - e);
			alpha = t >= 1 ? 1 : clamp01(e * 1.6f);
		}
		float cx = x0 + (i % BOARD_COLS) * (side + BOARD_GAP), cy = top + (i / BOARD_COLS) * (side + BOARD_GAP);
		struct zg_xf xf;
		zg_xf_identity(&xf);
		bool pressed = o->pressed == i;
		if (angle != 0) zg_xf_rotate_x(&xf, angle, cy + side / 2);
		if (pressed) zg_xf_scale_about(&xf, 0.94f, cx + side / 2, cy + side / 2);
		zg_set_xf(&xf);
		bool live = o->live[i];
		uint32_t fill = live ? zc_rgb(app->accent) : 0x262626ff;
		uint32_t ink = live ? 0xffffffff : 0xd9d9d9ff;
		if (dots) {
			fill = live ? zc_rgb(app->accent) : pal.text3;
			zg_border(cx, cy, side, side, 0, 1.2f, zc_alpha(fill, alpha));
			ink = live ? pal.text : pal.text3;
		} else {
			zg_rect(cx, cy, side, side, 0, zc_alpha(fill, alpha));
		}
		struct zt_layout *L = zt_layout_make(o->item[i], ZT_LIGHT, BOARD_FONT, side - BOARD_INSET, 1);
		zt_draw(L, cx + BOARD_INSET, cy + side - BOARD_INSET - L->line_h, zc_alpha(ink, alpha));
		zt_layout_free(L);
	}
	zg_set_xf(NULL);
}

static void draw_pick_button(zui_app *app, float cx, float y, const char *icon, const char *word, bool down, float a) {
	struct zui_palette pal = zui_pal(app);
	uint32_t c = zc_alpha(pal.text, a);
	if (down) zg_circle(cx, y + 22, 18, c);
	else zg_ring(cx, y + 22, 18, 2, c);
	zui_icon(icon, cx - 10, y + 12, 20, 2, down ? zc_alpha(pal.bg, a) : c);
	struct zt_layout *L = zt_layout_make(word, ZT_REGULAR, 11.5f, 0, 0);
	zt_draw(L, cx - L->w / 2, y + 42, c);
	zt_layout_free(L);
}

static void col_label(const struct col *c, int i, char *out, size_t size) {
	int v = ((i % c->n) + c->n) % c->n;
	if (c->names) snprintf(out, size, "%s", c->names[v]);
	else snprintf(out, size, "%0*d", c->pad, c->lo + v);
}

static void draw_pick(zui_app *app, struct ov *o, float a, uint32_t now) {
	struct zui_palette pal = zui_pal(app);
	float W = app->width, H = app->height;
	/* the page is gone behind black: the screen belongs to the picker */
	zg_rect(0, 0, W, H, 0, zc_alpha(pal.bg, a));
	char *caps = dupstr(app->title);
	for (char *p = caps; *p; p++) if ((unsigned char)*p < 0x80) *p = (char)toupper((unsigned char)*p);
	text_now(caps, ZT_STRONG, 11.5f, M.margin, 12, zc_alpha(pal.text, a), W - 2 * M.margin);
	free(caps);
	char *lower = dupstr(o->title);
	for (char *p = lower; *p; p++) if ((unsigned char)*p < 0x80) *p = (char)tolower((unsigned char)*p);
	text_now(lower, ZT_LIGHT, 38, M.margin - 2, 28, zc_alpha(pal.text, a), W - 2 * M.margin);
	free(lower);
	float cy = (H - BAR_H) / 2 + 10;
	for (int ci = 0; ci < o->ncol; ci++) {
		struct col *c = &o->col[ci];
		int mid = (int)floorf(c->pos);
		zg_clip(c->x - 4, cy - 2.6f * ROW_H, c->w + 8, 5.2f * ROW_H);
		for (int i = mid - 3; i <= mid + 4; i++) {
			if (!c->loop && (i < 0 || i >= c->n)) continue;
			float d = fabsf((float)i - c->pos), y = cy + ((float)i - c->pos) * ROW_H;
			if (d > 2.6f) continue;
			char buf[24];
			col_label(c, i, buf, sizeof(buf));
			/* the one in the middle in the accent; the rest grey and fading */
			uint32_t colour = zc_mix(pal.text3, pal.accent_text, clamp01(1.2f - d));
			float fade = d < 0.5f ? 1 : clamp01(0.75f - (d - 0.5f) * 0.32f);
			struct zt_layout *L = zt_layout_make(buf, ZT_LIGHT, c->names ? 28 : 34, c->w, 1);
			zt_draw(L, c->x + (c->w - L->w) / 2, y - L->line_h / 2, zc_alpha(colour, a * fade));
			zt_layout_free(L);
		}
		zg_noclip();
	}
	if (o->kind == OV_TIME)
		text_now(":", ZT_LIGHT, 34, o->col[0].x + o->col[0].w + 4, cy - 24, zc_alpha(pal.text3, a), 0);
	/* the bar: a tick and a cross */
	zg_rect(0, H - BAR_H, W, BAR_H, 0, zc_alpha(dotted(app) ? pal.bg : pal.surface2, a));
	draw_pick_button(app, W * 0.25f, H - BAR_H + 8, "check", "done", o->pressed == 0, a);
	draw_pick_button(app, W * 0.75f, H - BAR_H + 8, "close", "cancel", o->pressed == 1, a);
	(void)now;
}

void zui_widgets_draw_over(zui_app *app, uint32_t now) {
	draw_suggestions(app);
	struct ov *o = app->ov;
	if (!o) return;
	zg_noclip();
	zg_set_xf(NULL);
	zg_set_alpha(1);
	float a = ov_alpha(o, now);
	switch (o->kind) {
	case OV_MENU:
		scrim(app, a, false);
		draw_menu(app, o, a);
		break;
	case OV_DIALOG:
		scrim(app, a, false);
		draw_dialog(app, o, a, now);
		break;
	case OV_BOARD:
		/* the page stays faintly there under the tiles, ZrnMetroCore's 0.85 */
		zg_rect(0, 0, app->width, app->height, 0, zc_alpha(zui_pal(app).bg, (dotted(app) ? 0.97f : 0.85f) * (o->closing || getenv("ZUI_NO_OPEN") ? a : clamp01((float)(int32_t)(now - o->t0) / 100))));
		draw_board(app, o, now);
		break;
	default:
		draw_pick(app, o, a, now);
		break;
	}
	zg_set_xf(NULL);
	zg_set_alpha(1);
}

/* ============================================================ what core.c asks == */

float zui_widget_layout(zui_node *n, float x, float y, float w) {
	zui_app *app = n->app;
	(void)x; (void)y; (void)w;
	switch (n->kind) {
	case ZK_CHECK: case ZK_RADIO: return M.item_h;
	case ZK_RING: return 40;
	case ZK_PICKER: return layout_picker(n);
	case ZK_TILES: return layout_tiles(n, x, y, w);
	case ZK_TILE: return n->h;
	case ZK_HUB: return layout_hub(n);
	case ZK_HUBSEC: return n->h;
	default: return -1;
	}
}

bool zui_widget_draw(zui_app *app, zui_node *n, const struct zg_xf *xf, uint32_t now) {
	switch (n->kind) {
	case ZK_CHECK: case ZK_RADIO: draw_check(app, n, now); return true;
	case ZK_RING: draw_ring(app, n, now); return true;
	case ZK_PICKER: draw_picker(app, n, now); return true;
	case ZK_TILES:
		for (zui_node *c = n->first; c; c = c->next) if (c->visible) zui_draw_node(app, c, xf, now);
		return true;
	case ZK_TILE: draw_tile(app, n, xf, now); return true;
	case ZK_HUB: draw_hub(app, n, xf, now); return true;
	default: return false;
	}
}

bool zui_widget_interactive(const zui_node *n) {
	switch (n->kind) {
	case ZK_CHECK: case ZK_RADIO: case ZK_PICKER: return true;
	case ZK_TILE: return n->cb != NULL || n->hold_cb != NULL;
	default: return false;
	}
}

bool zui_widget_activate(zui_app *app, zui_node *n) {
	switch (n->kind) {
	case ZK_CHECK:
		zui_set_checked(n, !n->on);
		zui_haptic("click");
		zui_fire(n, ZUI_EVENT_CHANGE);
		return true;
	case ZK_RADIO:
		if (!n->on) {
			zui_set_checked(n, true);
			radio_chosen(n);
			zui_haptic("click");
			zui_fire(n, ZUI_EVENT_CHANGE);
		}
		return true;
	case ZK_PICKER:
		picker_tapped(app, n);
		return true;
	case ZK_TILE:
		zui_haptic("tap");
		zui_fire(n, ZUI_EVENT_CLICK);
		return true;
	default: return false;
	}
}

void zui_widget_free(zui_node *n) {
	zui_app *app = n->app;
	if (app->ov && OV(app)->source == n) OV(app)->source = NULL;
	if (app->open_picker == n) app->open_picker = NULL;
	if (app->hold_node == n) app->hold_node = NULL;
	if (n->hold_free) n->hold_free(n->hold_data);
	n->hold_free = NULL;
	switch (n->kind) {
	case ZK_PICKER: {
		struct picker *p = n->ext;
		if (p) {
			for (int i = 0; i < p->count; i++) free(p->opt[i]);
			free(p->opt);
			free(p);
		}
		if (app->n_pickers > 0) app->n_pickers--;
		break;
	}
	case ZK_TILE: {
		struct tile *t = n->ext;
		if (t) {
			if (t->timer) zui_timer_cancel(app, t->timer);
			free(t->back);
			free(t);
		}
		break;
	}
	case ZK_HUB: {
		struct hub *h = n->ext;
		if (h) {
			zui_pixmap_free(h->bg);
			free(h);
		}
		if (n->parent && n->parent->hub == n) n->parent->hub = NULL;
		break;
	}
	case ZK_ITEM:
		if (n->ext && n->cb == picker_row_tapped) free(n->ext);
		break;
	case ZK_FIELD: {
		struct suggest *s = n->ext;
		if (s) {
			for (int i = 0; i < s->n; i++) free(s->item[i]);
			free(s->item);
			free(s);
		}
		break;
	}
	default: break;
	}
	n->ext = NULL;
}
