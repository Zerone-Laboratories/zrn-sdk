// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Metro (ztRetrofit): the same widgets as ztMETRO's own apps draw them with
 * ZrnMetroCore. Black, white type in Segoe's open twin (Selawik), the app's
 * name in capitals over a large lowercase title, rectangular toggles and
 * outlined buttons, the accent for what is on. A press tilts what is under
 * the finger; pages turn on their left edge, row after row (the turnstile).
 * Numbers are ZrnMetroCore's (ztmetro/core/zrnmetrocore.c).
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zui-internal.h"

static struct zui_palette metro_palette(const zui_app *app) {
	struct zui_palette p = {
		.bg = 0x000000ff, .surface = 0x000000ff, .surface2 = 0x1f1f1fff,
		.text = 0xffffffff, .text2 = 0x999999ff, .text3 = 0x666666ff,
		.divider = 0x00000000, .track = 0x333333ff, .knob = 0xffffffff,
		.shadow = 0x00000000, .danger = 0xe51400ff, .scrim = 0x000000b3,
		.on_accent = 0xffffffff,
	};
	p.accent_text = zc_rgb(app->accent);
	return p;
}

#define M (app->style->m)
#define TILT_DEG 14.0f
#define TILT_LIFT 40.0f
#define PRESS_SINK 0.045f


static char *lower(const char *s) {
	char *o = strdup(s ? s : "");
	for (char *p = o; *p; p++) if ((unsigned char)*p < 0x80) *p = (char)tolower((unsigned char)*p);
	return o;
}

static char *upper(const char *s) {
	char *o = strdup(s ? s : "");
	for (char *p = o; *p; p++) if ((unsigned char)*p < 0x80) *p = (char)toupper((unsigned char)*p);
	return o;
}

static float page_top(zui_app *app, zui_node *page, float y, bool draw) {
	(void)app; (void)page; (void)draw;
	return y;
}

static void header(zui_app *app, zui_node *page, uint32_t now) {
	(void)now;
	struct zui_palette pal = metro_palette(app);
	zg_rect(0, 0, app->width, M.bar_h, 0, pal.bg);
	char *caps = upper(app->title), *title = lower(page->text);
	struct zt_layout *C = zt_slot_get(&page->title_slot[1], caps, ZT_STRONG, 11.5f, app->width - 2 * M.margin, 1);
	zt_draw(C, M.margin, 12, pal.text);
	struct zt_layout *T = zt_slot_get(&page->title_slot[0], title, ZT_LIGHT, M.title, 0, 0);
	zt_draw(T, M.margin - 3, 20, pal.text);
	free(caps);
	free(title);
}

static void card(zui_app *app, zui_node *n) { (void)app; (void)n; }
static void divider(zui_app *app, zui_node *c, zui_node *b) { (void)app; (void)c; (void)b; }

static void section_label(zui_app *app, zui_node *n) {
	struct zui_palette pal = metro_palette(app);
	struct zt_layout *L = zt_slot_get(&n->t[0], n->text, ZT_REGULAR, M.label, n->w, 1);
	zt_draw(L, n->x, n->y + M.section_gap, pal.text2);
}

static void button(zui_app *app, zui_node *n, float press) {
	struct zui_palette pal = metro_palette(app);
	float x = n->x, w = n->w, y = n->y, h = M.button_h;
	bool down = press > 0.5f;
	uint32_t edge = n->variant == ZUI_DANGER ? pal.danger : pal.text;
	if (down) zg_rect(x, y, w, h, 0, edge);
	else if (n->variant == ZUI_PRIMARY) {
		zg_rect(x, y, w, h, 0, zc_rgb(app->accent));
		zg_border(x, y, w, h, 0, 2, edge);
	} else {
		zg_border(x, y, w, h, 0, 2, edge);
	}
	struct zt_layout *L = zt_slot_get(&n->t[0], n->text, ZT_STRONG, M.button_text, w - 16, 1);
	zt_draw(L, x + (w - L->w) / 2, y + (h - L->line_h) / 2, down ? 0x000000ff : pal.text);
}

/* ZrnMetroCore's zmc_toggle: a white frame, the accent (or dark grey) in it,
 * and a white block that sits right when on */
static void toggle(zui_app *app, zui_node *n, float x, float y, float k) {
	(void)n;
	struct zui_palette pal = metro_palette(app);
	zg_border(x, y, 54, 24, 0, 2, pal.text);
	zg_rect(x + 4, y + 4, 46, 16, 0, zc_mix(pal.track, zc_rgb(app->accent), k));
	zg_rect(x + 1 + k * 40, y - 1, 12, 26, 0, pal.text);
}

static void item(zui_app *app, zui_node *n, float press) {
	(void)press;
	struct zui_palette pal = metro_palette(app);
	float x = n->x, y = n->y, w = n->w, h = n->h, right = x + w;
	if (n->kind == ZK_SWITCH) {
		float k = zanim_value(&n->knob, zui_now(), NULL);
		toggle(app, n, right - 54, y + 10, k);
		right -= 54 + 12;
	}
	const char *line2 = NULL;
	uint32_t c2 = pal.text2;
	float s2 = M.item_sub;
	if (n->kind == ZK_ITEM && n->detail && *n->detail) {
		line2 = n->detail;	/* a choice: its value in the accent, as zmc_choice */
		c2 = pal.accent_text;
		s2 = 15;
	} else if (n->subtitle && *n->subtitle) {
		line2 = n->subtitle;
	} else if (n->kind == ZK_SWITCH) {
		line2 = n->detail && *n->detail ? n->detail : n->on ? "On" : "Off";
	}
	struct zt_layout *T = zt_slot_get(&n->t[0], n->text, ZT_LIGHT, M.item_title, right - x, 1);
	float ty = line2 ? y + 6 : y + (h - T->line_h) / 2;
	zt_draw(T, x, ty, pal.text);
	if (line2) {
		struct zt_layout *S = zt_slot_get(&n->t[1], line2, ZT_REGULAR, s2, right - x, 1);
		zt_draw(S, x, ty + 30, c2);
	}
}

static void slider(zui_app *app, zui_node *n) {
	struct zui_palette pal = metro_palette(app);
	float x = n->x, w = n->w, y = n->y;
	char pct[16];
	snprintf(pct, sizeof(pct), "%d%%", (int)lround(n->value * 100));
	struct zt_layout *P = zt_slot_get(&n->t[2], pct, ZT_STRONG, 14, 0, 0);
	struct zt_layout *T = zt_slot_get(&n->t[0], n->text, ZT_REGULAR, 13, w - 50, 1);
	zt_draw(T, x, y, pal.text2);
	zt_draw(P, x + w - P->w, y, pal.text);
	float ty = y + 30, fill = w * (float)n->value;
	zg_rect(x, ty, w, 4, 0, 0x383838ff);
	zg_rect(x, ty, fill, 4, 0, zc_rgb(app->accent));
	zg_rect(x + fill - 5, ty - 7, 10, 18, 0, pal.text);
}

/* five dots cross, quick at the edges and slow through the middle */
static void dots(zui_app *app, float y, float x0, float w, uint32_t since, uint32_t now) {
	uint32_t acc = zc_rgb(app->accent);
	for (int i = 0; i < 5; i++) {
		float p = (float)((now - since) % 2600) / 2000.0f - i * 0.075f;
		if (p < 0 || p > 1) continue;
		float c = p - 0.5f;
		float at = x0 + w * (0.5f + 4 * c * c * c);
		zg_rect(at - 2, y, 4, 4, 0, acc);
	}
}

static void progress(zui_app *app, zui_node *n, uint32_t now) {
	float x = n->x, w = n->w, y = n->y + (n->h - 4) / 2;
	if (n->value < 0) {
		dots(app, y, x, w, 0, now);
		return;
	}
	zg_rect(x, y, w, 4, 0, 0x383838ff);
	zg_rect(x, y, w * (float)n->value, 4, 0, zc_rgb(app->accent));
}

/* ZrnMetroCore's zmc_textbox: a caption, then a box, white while typed into */
static void field(zui_app *app, zui_node *n, uint32_t now) {
	(void)now;
	struct zui_palette pal = metro_palette(app);
	float x = n->x, w = n->w, y = n->y;
	struct zt_layout *C = zt_slot_get(&n->t[1], n->subtitle ? n->subtitle : "", ZT_REGULAR, 13, w, 1);
	zt_draw(C, x, y, pal.text2);
	float by = y + 22, h = M.field_h;
	zg_rect(x, by, w, h, 0, n->focused ? 0xffffffff : 0xc7c7c7ff);
	float avail = w - 16, end = 0;
	if (n->text && *n->text) {
		struct zt_layout *T = zt_slot_get(&n->t[0], n->text, ZT_REGULAR, 17, n->focused ? 0 : avail, n->focused ? 0 : 1);
		float shift = n->focused && T->w > avail - 4 ? T->w - (avail - 4) : 0;
		zt_draw_clipped(T, x + 8 - shift, by + (h - T->line_h) / 2, 0x000000ff, shift, shift + avail);
		end = T->w - shift;
	}
	if (n->focused) zg_rect(x + 8 + end + 1, by + 8, 1.5f, h - 16, 0, 0x000000ff);
}

static void busy(zui_app *app, uint32_t now) {
	dots(app, 4, 0, app->width, app->busy_since, now);
}

static void toast(zui_app *app, const char *text, float shown) {
	struct zg_xf xf;
	zg_xf_identity(&xf);
	zg_xf_translate(&xf, 0, -(1 - shown) * 60, 0);
	zg_set_xf(&xf);
	zg_rect(0, 0, app->width, 60, 0, zc_rgb(app->accent));
	struct zt_layout *L = zt_layout_make(text, ZT_STRONG, 15, app->width - 2 * M.margin, 1);
	zt_draw(L, M.margin, 30 - L->line_h / 2 + 6, 0xffffffff);
	zt_layout_free(L);
	zg_set_xf(NULL);
}

/* Windows Phone's tilt: what is under the finger leans toward it */
static void press_xf(zui_app *app, zui_node *n, float amount, float fx, float fy, struct zg_xf *xf) {
	(void)app;
	if (n->kind == ZK_BUTTON || n->kind == ZK_SLIDER || n->kind == ZK_FIELD) return;
	float cx = n->x + n->w / 2, cy = n->y + n->h / 2;
	fx = fx < -1 ? -1 : fx > 1 ? 1 : fx;
	fy = fy < -1 ? -1 : fy > 1 ? 1 : fy;
	/* its far end never rises more than TILT_LIFT points */
	float cap_y = n->w > 0 ? asinf(fminf(1, TILT_LIFT / (n->w / 2))) * 180 / (float)M_PI : TILT_DEG;
	float cap_x = n->h > 0 ? asinf(fminf(1, TILT_LIFT / (n->h / 2))) * 180 / (float)M_PI : TILT_DEG;
	float dy = fx * fminf(TILT_DEG, cap_y) * amount, dx = fy * fminf(TILT_DEG, cap_x) * amount;
	zg_xf_rotate_y(xf, dy, cx);
	zg_xf_rotate_x(xf, dx, cy);
	zg_xf_scale_about(xf, 1 - PRESS_SINK * amount, cx, cy);
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* The turnstile, as ZrnMetroCore times it from the Lumia: the old page's
 * rows turn away round the left edge one after another (128 ms each, 33 ms
 * apart; toward the viewer going forward, away going back), a moment of
 * black, then the new rows swing in from behind the screen (180 ms each).
 * Going back both go from the bottom up. */
#define OUT_MS 128.0f
#define IN_MS 180.0f
#define STAGGER 28.0f
#define GAP_AT 330.0f
static void page_xf(zui_app *app, float t, int dir, bool incoming, int row, int rows,
		struct zg_xf *xf, float *alpha) {
	float ms = t * app->style->transition_ms;
	int r = row < 0 ? 0 : row + 1;	/* the header goes first */
	if (dir < 0) r = row < 0 ? rows + 1 : rows - row;
	if (r > 7) r = 7;
	if (!incoming) {
		float u = clamp01((ms - r * STAGGER) / OUT_MS);
		zg_xf_rotate_y(xf, (dir > 0 ? -50.0f : 50.0f) * zui_ease_in(u), 0);
		*alpha = u >= 1 ? 0 : 1 - u * u;
	} else {
		float u = clamp01((ms - GAP_AT - r * STAGGER) / IN_MS);
		zg_xf_rotate_y(xf, 80.0f * (1 - zui_ease_out(u)), 0);
		*alpha = u <= 0 ? 0 : clamp01(u * 3);
	}
}

const struct zui_style zui_style_metro = {
	.name = "metro",
	.fonts = {
		[ZT_REGULAR] = "Selawik-Regular.ttf;Segoe UI",
		[ZT_MEDIUM] = "Selawik-Semibold.ttf;Segoe UI Semibold",
		[ZT_STRONG] = "Selawik-Semibold.ttf;Segoe UI Semibold",
		[ZT_LIGHT] = "Selawik-Semilight.ttf;Segoe UI Semilight",
	},
	.line_spacing = 1.25f,
	.transition_ms = 720,
	.palette = metro_palette,
	.m = {
		.margin = 16, .gap = 12, .card_r = 0, .card_pad = 0, .section_gap = 10,
		.text = 16, .heading = 25.5f, .caption = 13, .label = 13, .title = 48, .bar_title = 0,
		.item_h = 52, .item_h2 = 66, .item_title = 21, .item_sub = 13, .icon_tile = 0,
		.button_h = 40, .button_r = 0, .button_text = 14.5f,
		.field_h = 38, .field_r = 0,
		.bar_h = 108, .large_title_h = 0,
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
	.rows_highlight = false,
	.flat_cards = true,
};
