// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * zrnCoolStyle: calm and readable. Graphite (or paper) background, rounded
 * cards that group rows, Inter, one accent colour from the system, pill
 * buttons and switches. Pages fade through a short slide; a press dims a row
 * or sinks a button a little. Nothing moves that the finger did not move.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "zui-internal.h"

static struct zui_palette cool_palette(const zui_app *app) {
	uint32_t acc = zc_rgb(app->accent);
	struct zui_palette p;
	if (app->light) {
		p = (struct zui_palette){
			.bg = 0xf3f4f7ff, .surface = 0xffffffff, .surface2 = 0xeceef2ff,
			.text = 0x14161bff, .text2 = 0x5d6470ff, .text3 = 0x9aa0aaff,
			.divider = 0x14161b17, .track = 0xd3d7deff, .knob = 0xffffffff,
			.shadow = 0x1a1f2a1f, .danger = 0xd93a3fff, .scrim = 0x00000059,
		};
		p.accent_text = zc_mix(acc, 0x000000ff, 0.12f);
	} else {
		p = (struct zui_palette){
			.bg = 0x0e0f12ff, .surface = 0x1a1c21ff, .surface2 = 0x24272eff,
			.text = 0xf2f3f5ff, .text2 = 0x9aa1acff, .text3 = 0x666c77ff,
			.divider = 0xffffff14, .track = 0x3a3e47ff, .knob = 0xffffffff,
			.shadow = 0x00000080, .danger = 0xff5c5cff, .scrim = 0x00000099,
		};
		/* the accent as text on dark: lifted, so cobalt is still readable */
		p.accent_text = zc_luma(acc) < 0.45f ? zc_mix(acc, 0xffffffff, 0.38f) : acc;
	}
	p.on_accent = zc_luma(acc) > 0.62f ? 0x111111ff : 0xffffffff;
	return p;
}

#define M (app->style->m)

static bool in_card(const zui_node *n) { return n->parent && n->parent->kind == ZK_CARD; }
static float pad_of(const zui_app *app, const zui_node *n) {
	return in_card(n) && n->kind != ZK_ITEM && n->kind != ZK_SWITCH ? M.card_pad : 0;
}

static float page_top(zui_app *app, zui_node *page, float y, bool draw) {
	if (!page->text || !*page->text) return y + 8;
	if (draw) {
		struct zui_palette pal = cool_palette(app);
		struct zt_layout *L = zt_slot_get(&page->title_slot[0], page->text, ZT_STRONG, M.title,
			app->width - 2 * M.margin, 1);
		zt_draw(L, M.margin, y + 2, pal.text);
	}
	return y + M.large_title_h;
}

static void header(zui_app *app, zui_node *page, uint32_t now) {
	(void)now;
	struct zui_palette pal = cool_palette(app);
	float s = page->scroll;
	/* the bar fills in as the list goes under it */
	float a = (s - 4) / 18;
	a = a < 0 ? 0 : a > 1 ? 1 : a;
	if (a > 0) {
		zg_rect(0, 0, app->width, M.bar_h, 0, zc_alpha(pal.bg, 0.97f * a));
		zg_rect(0, M.bar_h - 0.5f, app->width, 0.5f, 0, zc_alpha(pal.divider, a * (pal.divider & 0xff) / 255.0f));
	}
	/* the small title takes over once the large one has gone */
	float t = (s - (M.large_title_h - 26)) / 16;
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	if (t > 0 && page->text) {
		struct zt_layout *L = zt_slot_get(&page->title_slot[1], page->text, ZT_STRONG, M.bar_title,
			app->width - 2 * 64, 1);
		float al = zg_get_alpha();
		zg_set_alpha(al * t);
		zt_draw(L, (app->width - L->w) / 2, (M.bar_h - L->line_h) / 2 + (1 - t) * 6, pal.text);
		zg_set_alpha(al);
	}
	if (page->depth > 0) {
		float d = 40, x = 10, y = (M.bar_h - d) / 2;
		zg_circle(x + d / 2, y + d / 2, d / 2, pal.surface2);
		zui_icon("back", x + 9, y + 9, 22, 2.2f, pal.text);
	}
}

static void card(zui_app *app, zui_node *n) {
	struct zui_palette pal = cool_palette(app);
	if (app->light) zg_shadow(n->x, n->y + 2, n->w, n->h, M.card_r, 10, pal.shadow);
	zg_rect(n->x, n->y, n->w, n->h, M.card_r, pal.surface);
}

static void divider(zui_app *app, zui_node *c, zui_node *below) {
	struct zui_palette pal = cool_palette(app);
	float inset = M.card_pad;
	if (below->icon && (below->kind == ZK_ITEM || below->kind == ZK_SWITCH)) inset += M.icon_tile + 14;
	float px = 1.0f / zui_gfx_scale();
	float h = px > 0.5f ? px : 0.5f;
	zg_rect(c->x + inset, below->y - h / 2, c->w - inset, h, 0, pal.divider);
}

static void section_label(zui_app *app, zui_node *n) {
	struct zui_palette pal = cool_palette(app);
	struct zt_layout *L = zt_slot_get(&n->t[0], n->text, ZT_MEDIUM, M.label, n->w - 12, 1);
	zt_draw(L, n->x + 6, n->y + M.section_gap, pal.text2);
}

static void button(zui_app *app, zui_node *n, float press) {
	struct zui_palette pal = cool_palette(app);
	float pad = pad_of(app, n);
	float x = n->x + pad, w = n->w - 2 * pad, y = n->y, h = M.button_h;
	uint32_t bg = 0, fg = pal.accent_text;
	switch (n->variant) {
	case ZUI_PRIMARY: bg = zc_rgb(app->accent); fg = pal.on_accent; break;
	case ZUI_TONAL: bg = zc_alpha(zc_rgb(app->accent), app->light ? 0.13f : 0.22f); break;
	case ZUI_DANGER: bg = pal.danger; fg = 0xffffffff; break;
	default: break;
	}
	if (bg) zg_rect(x, y, w, h, h / 2, bg);
	if (press > 0) {
		uint32_t over = (bg && zc_luma(bg) < 0.5f) || !app->light ? 0xffffffff : 0x000000ff;
		zg_rect(x, y, w, h, h / 2, zc_alpha(over, 0.12f * press));
	}
	struct zt_layout *L = zt_slot_get(&n->t[0], n->text, ZT_MEDIUM, M.button_text, w - 24, 1);
	zt_draw(L, x + (w - L->w) / 2, y + (h - L->line_h) / 2, fg);
}

static void toggle(zui_app *app, zui_node *n, float x, float y, float k) {
	(void)n;
	struct zui_palette pal = cool_palette(app);
	const float w = 52, h = 32, r = 13;
	zg_rect(x, y, w, h, h / 2, zc_mix(pal.track, zc_rgb(app->accent), k));
	float cx = x + 3 + r + k * (w - 6 - 2 * r), cy = y + h / 2;
	zg_shadow(cx - r, cy - r + 1, 2 * r, 2 * r, r, 2.5f, 0x00000040);
	zg_circle(cx, cy, r, pal.knob);
}

static void item(zui_app *app, zui_node *n, float press) {
	struct zui_palette pal = cool_palette(app);
	float x = n->x, y = n->y, w = n->w, h = n->h;
	bool carded = in_card(n);
	float inset = carded ? M.card_pad : 2;
	if (press > 0) zg_rect(x + 4, y + 3, w - 8, h - 6, 14, zc_alpha(pal.text, (app->light ? 0.06f : 0.08f) * press));
	float cx = x + inset, right = x + w - inset;
	if (n->icon) {
		float t = M.icon_tile, ty = y + (h - t) / 2;
		uint32_t tint = n->tint ? zc_rgb(n->tint & 0xffffff) : zc_rgb(app->accent);
		zg_rect(cx, ty, t, t, 9, tint);
		zui_icon(n->icon, cx + 6, ty + 6, t - 12, 2.0f, zc_luma(tint) > 0.62f ? 0x111111ff : 0xffffffff);
		cx += t + 14;
	}
	float now_k = 0;
	if (n->kind == ZK_SWITCH) {
		now_k = zanim_value(&n->knob, zui_now(), NULL);
		toggle(app, n, right - 52, y + (h - 32) / 2, now_k);
		right -= 52 + 12;
	} else if (n->cb) {
		zui_icon("chevron", right - 16, y + (h - 16) / 2, 16, 2.0f, pal.text3);
		right -= 16 + 8;
	}
	if (n->kind == ZK_ITEM && n->detail && *n->detail) {
		struct zt_layout *D = zt_slot_get(&n->t[2], n->detail, ZT_REGULAR, 15, (right - cx) * 0.5f, 1);
		zt_draw(D, right - D->w, y + (h - D->line_h) / 2, pal.text2);
		right -= D->w + 12;
	}
	const char *sub = n->subtitle && *n->subtitle ? n->subtitle :
		(n->kind == ZK_SWITCH && n->detail ? n->detail : NULL);
	struct zt_layout *T = zt_slot_get(&n->t[0], n->text, ZT_REGULAR, M.item_title, right - cx, 1);
	struct zt_layout *S = sub ? zt_slot_get(&n->t[1], sub, ZT_REGULAR, M.item_sub, right - cx, 1) : NULL;
	float block = T->line_h + (S ? S->line_h - 1 : 0);
	float ty = y + (h - block) / 2;
	zt_draw(T, cx, ty, pal.text);
	if (S) zt_draw(S, cx, ty + T->line_h - 1, pal.text2);
}

static void slider(zui_app *app, zui_node *n) {
	struct zui_palette pal = cool_palette(app);
	float pad = pad_of(app, n);
	float x = n->x + pad, w = n->w - 2 * pad, y = n->y;
	char pct[16];
	snprintf(pct, sizeof(pct), "%d%%", (int)lround(n->value * 100));
	struct zt_layout *P = zt_slot_get(&n->t[2], pct, ZT_MEDIUM, 15, 0, 0);
	struct zt_layout *T = zt_slot_get(&n->t[0], n->text, ZT_REGULAR, 16, w - P->w - 12, 1);
	zt_draw(T, x, y + 10, pal.text);
	zt_draw(P, x + w - P->w, y + 10, pal.text2);
	float ty = y + 50, th = 6, v = (float)n->value;
	zg_rect(x, ty, w, th, th / 2, pal.track);
	if (v > 0) zg_rect(x, ty, w * v < th ? th : w * v, th, th / 2, zc_rgb(app->accent));
	float kx = x + w * v, ky = ty + th / 2, r = 12;
	zg_shadow(kx - r, ky - r + 1, 2 * r, 2 * r, r, 3, 0x00000045);
	zg_circle(kx, ky, r, pal.knob);
}

static void progress(zui_app *app, zui_node *n, uint32_t now) {
	struct zui_palette pal = cool_palette(app);
	float pad = pad_of(app, n);
	float x = n->x + pad, w = n->w - 2 * pad, th = 6, y = n->y + (n->h - th) / 2;
	zg_rect(x, y, w, th, th / 2, pal.track);
	uint32_t acc = zc_rgb(app->accent);
	if (n->value >= 0) {
		float v = (float)n->value;
		if (v > 0) zg_rect(x, y, w * v < th ? th : w * v, th, th / 2, acc);
		return;
	}
	/* busy, no idea how long: a piece sweeps across */
	float p = (now % 1400) / 1400.0f;
	float seg = w * 0.35f, at = x - seg + (w + seg) * zui_ease_out(p);
	float a = at < x ? x : at, b = at + seg > x + w ? x + w : at + seg;
	if (b - a > 1) zg_rect(a, y, b - a, th, th / 2, acc);
}

static void field(zui_app *app, zui_node *n, uint32_t now) {
	struct zui_palette pal = cool_palette(app);
	float pad = pad_of(app, n);
	float x = n->x + pad, w = n->w - 2 * pad, y = n->y, h = M.field_h;
	float f = zanim_value(&n->focus_anim, now, NULL);
	zg_rect(x, y, w, h, M.field_r, in_card(n) ? pal.surface2 : pal.surface);
	if (f > 0) zg_border(x, y, w, h, M.field_r, 2, zc_alpha(zc_rgb(app->accent), f));
	float tx = x + 16, avail = w - 32;
	bool empty = !n->text || !*n->text;
	if (empty) {
		struct zt_layout *P = zt_slot_get(&n->t[1], n->subtitle ? n->subtitle : "", ZT_REGULAR, 16, avail, 1);
		zt_draw(P, tx, y + (h - P->line_h) / 2, pal.text3);
	}
	float end = 0;
	if (!empty) {
		struct zt_layout *T = zt_slot_get(&n->t[0], n->text, ZT_REGULAR, 16, n->focused ? 0 : avail, n->focused ? 0 : 1);
		float shift = n->focused && T->w > avail - 4 ? T->w - (avail - 4) : 0;
		zt_draw_clipped(T, tx - shift, y + (h - T->line_h) / 2, pal.text, shift, shift + avail);
		end = T->w - shift;
	}
	if (n->focused) zg_rect(tx + end + 1, y + (h - 22) / 2, 2, 22, 1, zc_rgb(app->accent));
}

static void busy(zui_app *app, uint32_t now) {
	float p = ((now - app->busy_since) % 1300) / 1300.0f;
	float w = app->width, seg = w * 0.3f, at = -seg + (w + seg) * zui_ease_emph(p);
	zg_rect(at, M.bar_h - 3, seg, 3, 1.5f, zc_rgb(app->accent));
}

static void toast(zui_app *app, const char *text, float shown) {
	struct zui_palette pal = cool_palette(app);
	struct zt_layout *L = zt_layout_make(text, ZT_MEDIUM, 15, app->width - 2 * M.margin - 40, 1);
	float w = L->w + 40, h = 44, x = (app->width - w) / 2, y = app->height - 28 - h + (1 - shown) * 16;
	float a = zg_get_alpha();
	zg_set_alpha(a * shown);
	zg_shadow(x, y + 3, w, h, h / 2, 12, 0x00000066);
	zg_rect(x, y, w, h, h / 2, app->light ? 0x1f2228ff : 0x2c3038ff);
	zt_draw(L, x + 20, y + (h - L->line_h) / 2, 0xf2f3f5ff);
	zg_set_alpha(a);
	zt_layout_free(L);
	(void)pal;
}

static void press_xf(zui_app *app, zui_node *n, float amount, float fx, float fy, struct zg_xf *xf) {
	(void)app; (void)fx; (void)fy;
	if (n->kind != ZK_BUTTON) return;
	zg_xf_scale_about(xf, 1 - 0.035f * amount, n->x + n->w / 2, n->y + n->h / 2);
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* Fade through: the old page is gone in the first third while it drifts
 * back a little; the new one arrives from a short way to the right (forward)
 * or left (back) and fades in after, its rows 14 ms apart. Never two pages
 * fully on screen at once. */
static void page_xf(zui_app *app, float t, int dir, bool incoming, int row, int rows,
		struct zg_xf *xf, float *alpha) {
	(void)rows;
	float W = app->width;
	if (incoming) {
		int r = row < 0 ? 0 : row > 8 ? 8 : row;
		float total = (float)app->style->transition_ms, delay = r * 14.0f;
		t = clamp01((t * total - delay) / (total - delay));
		float e = zui_ease_emph(t);
		zg_xf_translate(xf, (dir > 0 ? 1 : -1) * W * (dir > 0 ? 0.16f : 0.06f) * (1 - e), 0, 0);
		*alpha = clamp01((t - 0.18f) / 0.42f);
	} else {
		float e = zui_ease_out(clamp01(t / 0.35f));
		zg_xf_translate(xf, (dir > 0 ? -W * 0.04f : W * 0.10f) * e, 0, 0);
		*alpha = 1 - e;
	}
}

const struct zui_style zui_style_cool = {
	.name = "cool",
	.fonts = {
		[ZT_REGULAR] = "Inter-Regular.ttf;Inter:weight=80",
		[ZT_MEDIUM] = "Inter-Medium.ttf;Inter:weight=100",
		[ZT_STRONG] = "Inter-SemiBold.ttf;Inter:weight=180",
		[ZT_LIGHT] = "Inter-Regular.ttf;Inter:weight=80",
	},
	.line_spacing = 1.36f,
	.transition_ms = 380,
	.palette = cool_palette,
	.m = {
		.margin = 18, .gap = 16, .card_r = 20, .card_pad = 16, .section_gap = 6,
		.text = 16, .heading = 21, .caption = 13.5f, .label = 13.5f, .title = 32, .bar_title = 17,
		.item_h = 56, .item_h2 = 68, .item_title = 16, .item_sub = 13.5f, .icon_tile = 32,
		.button_h = 52, .button_r = 26, .button_text = 16,
		.field_h = 52, .field_r = 14,
		.bar_h = 56, .large_title_h = 54,
	},
	.header = header,
	.page_top = page_top,
	.card = card,
	.divider = divider,
	.section_label = section_label,
	.button = button,
	.item = item,
	.toggle = toggle,
	.slider = slider,
	.progress = progress,
	.field = field,
	.busy = busy,
	.toast = toast,
	.press_xf = press_xf,
	.page_xf = page_xf,
	.rows_highlight = true,
	.flat_cards = false,
};
