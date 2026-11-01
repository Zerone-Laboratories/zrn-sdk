// SPDX-License-Identifier: LGPL-2.1-or-later
/* A small set of line icons, drawn as strokes on a 24-unit grid. */
#include <string.h>
#include "zui-internal.h"

bool zui_icons_as_dots;

struct pen { float x, y, k, w; uint32_t c; };

static void L(const struct pen *p, float x0, float y0, float x1, float y1) {
	zg_line(p->x + x0 * p->k, p->y + y0 * p->k, p->x + x1 * p->k, p->y + y1 * p->k, p->w, p->c);
}

static void poly(const struct pen *p, const float *xy, int n) {
	for (int i = 0; i + 1 < n; i++) L(p, xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3]);
}

static void A(const struct pen *p, float cx, float cy, float r, float a0, float a1) {
	zg_arc(p->x + cx * p->k, p->y + cy * p->k, r * p->k, a0, a1, p->w, p->c);
}

static void ring(const struct pen *p, float cx, float cy, float r) {
	zg_ring(p->x + cx * p->k, p->y + cy * p->k, r * p->k + p->w / 2, p->w, p->c);
}

static void dot(const struct pen *p, float cx, float cy, float r) {
	zg_circle(p->x + cx * p->k, p->y + cy * p->k, r * p->k, p->c);
}

/* Segoe MDL2 Assets, the system's, by the names apps use. DotMatrix and a system
 * without the font draw the strokes below: they fall on the lattice as dots. */
static const struct { const char *name; uint32_t code; } MDL2[] = {
	{ "send", 0xE724 }, { "settings", 0xE713 }, { "smile", 0xE76E }, { "chevron", 0xE76C }, { "back", 0xE72B }, { "check", 0xE73E }, { "plus", 0xE710 },
	{ "close", 0xE711 }, { "more", 0xE712 }, { "search", 0xE721 }, { "wifi", 0xE701 }, { "bluetooth", 0xE702 },
	{ "bell", 0xEA8F }, { "moon", 0xE708 }, { "sun", 0xE706 }, { "lock", 0xE72E }, { "volume", 0xE767 },
	{ "battery", 0xE83F }, { "info", 0xE946 }, { "music", 0xE8D6 }, { "person", 0xE77B }, { "gear", 0xE713 },
	{ "phone", 0xE717 },
};

void zui_icon(const char *name, float x, float y, float size, float stroke, uint32_t c) {
	if (!name) return;
	if (!zui_icons_as_dots)
		for (size_t i = 0; i < sizeof(MDL2) / sizeof(MDL2[0]); i++)
			if (!strcmp(MDL2[i].name, name) && zt_icon(MDL2[i].code, x, y, size, c)) return;
	struct pen p = { x, y, size / 24.0f, stroke, c };
	const struct pen *P = &p;
	if (!strcmp(name, "send")) {
		const float v[] = { 4, 5, 20, 12, 4, 19, 7, 12, 4, 5 };
		poly(P, v, 5);
	} else if (!strcmp(name, "chevron")) {
		const float v[] = { 9, 5.5f, 15.5f, 12, 9, 18.5f };
		poly(P, v, 3);
	} else if (!strcmp(name, "back")) {
		const float v[] = { 15, 5.5f, 8.5f, 12, 15, 18.5f };
		poly(P, v, 3);
	} else if (!strcmp(name, "check")) {
		const float v[] = { 5, 12.5f, 10, 17.5f, 19, 7 };
		poly(P, v, 3);
	} else if (!strcmp(name, "plus")) {
		L(P, 12, 5, 12, 19);
		L(P, 5, 12, 19, 12);
	} else if (!strcmp(name, "close")) {
		L(P, 6, 6, 18, 18);
		L(P, 18, 6, 6, 18);
	} else if (!strcmp(name, "more")) {
		dot(P, 5.5f, 12, 1.8f);
		dot(P, 12, 12, 1.8f);
		dot(P, 18.5f, 12, 1.8f);
	} else if (!strcmp(name, "search")) {
		ring(P, 10.5f, 10.5f, 6);
		L(P, 15.2f, 15.2f, 20, 20);
	} else if (!strcmp(name, "wifi")) {
		A(P, 12, 19, 4.5f, 225, 315);
		A(P, 12, 19, 9, 225, 315);
		A(P, 12, 19, 13.5f, 228, 312);
		dot(P, 12, 19, 1.6f);
	} else if (!strcmp(name, "bluetooth")) {
		const float v[] = { 6.5f, 7.5f, 17, 16.5f, 12, 21, 12, 3, 17, 7.5f, 6.5f, 16.5f };
		poly(P, v, 6);
	} else if (!strcmp(name, "bell")) {
		A(P, 12, 11, 5, 180, 360);
		L(P, 7, 11, 7, 16.5f);
		L(P, 17, 11, 17, 16.5f);
		L(P, 5, 17, 19, 17);
		L(P, 10.5f, 20.5f, 13.5f, 20.5f);
	} else if (!strcmp(name, "moon")) {
		A(P, 12, 12, 8, 60, 300);
		A(P, 20, 12, 8, 120, 240);
	} else if (!strcmp(name, "sun")) {
		ring(P, 12, 12, 3.6f);
		for (int i = 0; i < 8; i++) {
			static const float d[8][2] = { { 1, 0 }, { .7071f, .7071f }, { 0, 1 }, { -.7071f, .7071f },
				{ -1, 0 }, { -.7071f, -.7071f }, { 0, -1 }, { .7071f, -.7071f } };
			L(P, 12 + d[i][0] * 7, 12 + d[i][1] * 7, 12 + d[i][0] * 9.2f, 12 + d[i][1] * 9.2f);
		}
	} else if (!strcmp(name, "lock")) {
		zg_border(x + 5 * p.k, y + 11 * p.k, 14 * p.k, 10 * p.k, 2.2f * p.k, stroke, c);
		A(P, 12, 8, 4, 180, 360);
		L(P, 8, 8, 8, 11);
		L(P, 16, 8, 16, 11);
	} else if (!strcmp(name, "volume")) {
		const float v[] = { 4, 9.5f, 8, 9.5f, 12.5f, 5.5f, 12.5f, 18.5f, 8, 14.5f, 4, 14.5f, 4, 9.5f };
		poly(P, v, 7);
		A(P, 13, 12, 4, -45, 45);
		A(P, 13, 12, 7.5f, -50, 50);
	} else if (!strcmp(name, "battery")) {
		zg_border(x + 2.5f * p.k, y + 7 * p.k, 17 * p.k, 10 * p.k, 2.4f * p.k, stroke, c);
		zg_rect(x + 20.5f * p.k, y + 10 * p.k, 1.8f * p.k, 4 * p.k, 0.8f * p.k, c);
		zg_rect(x + 5 * p.k, y + 9.5f * p.k, 8 * p.k, 5 * p.k, 1 * p.k, c);
	} else if (!strcmp(name, "info")) {
		ring(P, 12, 12, 8.6f);
		L(P, 12, 11, 12, 16.5f);
		dot(P, 12, 7.8f, 1.3f);
	} else if (!strcmp(name, "music")) {
		L(P, 9, 17, 9, 5.5f);
		L(P, 9, 5.5f, 19, 3.5f);
		L(P, 19, 3.5f, 19, 15);
		dot(P, 6.8f, 17, 2.6f);
		dot(P, 16.8f, 15, 2.6f);
	} else if (!strcmp(name, "person")) {
		ring(P, 12, 8, 3.8f);
		A(P, 12, 21.5f, 7.5f, 200, 340);
	} else if (!strcmp(name, "gear")) {
		ring(P, 12, 12, 3);
		ring(P, 12, 12, 6.5f);
		for (int i = 0; i < 8; i++) {
			static const float d[8][2] = { { 1, 0 }, { .7071f, .7071f }, { 0, 1 }, { -.7071f, .7071f },
				{ -1, 0 }, { -.7071f, -.7071f }, { 0, -1 }, { .7071f, -.7071f } };
			L(P, 12 + d[i][0] * 7.5f, 12 + d[i][1] * 7.5f, 12 + d[i][0] * 9.5f, 12 + d[i][1] * 9.5f);
		}
	} else if (!strcmp(name, "phone")) {
		zg_border(x + 7 * p.k, y + 2.5f * p.k, 10 * p.k, 19 * p.k, 2.4f * p.k, stroke, c);
		L(P, 10.5f, 18.5f, 13.5f, 18.5f);
	}
}
