// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The node tree: making and changing things, laying pages out, the finger
 * (press, tap, scroll, fling, drag a slider), moving between pages, and
 * drawing a frame through the style in use.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

#include "zui-internal.h"

#define SLOP 8.0f

ZUI_EXPORT const char *zui_version(void) { return "0.1.0"; }

uint32_t zui_now(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* ---- easing and motion ---- */
float zui_ease_out(float t) {
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	float u = 1 - t;
	return 1 - u * u * u;
}

float zui_ease_in(float t) {
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	return t * t * t;
}

/* quick away, then a long soft settle: (0.2, 0.8, 0.2, 1) as a cubic Bezier */
float zui_ease_emph(float t) {
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	const float x1 = 0.2f, y1 = 0.8f, x2 = 0.2f, y2 = 1.0f;
	float s = t;
	for (int i = 0; i < 8; i++) {
		float u = 1 - s;
		float x = 3 * u * u * s * x1 + 3 * u * s * s * x2 + s * s * s - t;
		float dx = 3 * u * u * x1 + 6 * u * s * (x2 - x1) + 3 * s * s * (1 - x2);
		if (fabsf(dx) < 1e-5f) break;
		s -= x / dx;
		s = s < 0 ? 0 : s > 1 ? 1 : s;
	}
	float u = 1 - s;
	return 3 * u * u * s * y1 + 3 * u * s * s * y2 + s * s * s;
}

static uint32_t anim_until;	/* something is moving until then */

float zanim_value(const struct zanim *a, uint32_t now, bool *running) {
	if (!a->ms || (int32_t)(now - a->start) >= (int32_t)a->ms) {
		return a->to;
	}
	if (running) *running = true;
	float t = (float)(int32_t)(now - a->start) / a->ms;
	return a->from + (a->to - a->from) * zui_ease_out(t < 0 ? 0 : t);
}

void zanim_go(struct zanim *a, float to, uint32_t ms, uint32_t now) {
	a->from = zanim_value(a, now, NULL);
	a->to = to;
	a->start = now;
	a->ms = ms;
	if ((int32_t)(now + ms - anim_until) > 0) anim_until = now + ms;
}

void zanim_set(struct zanim *a, float v) {
	a->from = a->to = v;
	a->ms = 0;
}

/* ---- where libzrnui's own files are (fonts) ---- */
const char *zui_data_dir(void) {
	static char dir[1024];
	if (dir[0]) return dir;
	const char *env = getenv("ZUI_DATA_DIR");
	if (env) {
		snprintf(dir, sizeof(dir), "%s", env);
		return dir;
	}
	/* next to the library: the build tree (libzrnui/build/libzrnui.so -> libzrnui/)
	 * or an installed one (PREFIX/lib/libzrnui.so -> PREFIX/share/zrnui) */
	Dl_info info;
	if (dladdr((void *)zui_data_dir, &info) && info.dli_fname) {
		char lib[1024];
		snprintf(lib, sizeof(lib), "%s", info.dli_fname);
		char *slash = strrchr(lib, '/');
		if (slash) {
			*slash = 0;
			const char *tries[] = { "%s/../fonts/..", "%s/../share/zrnui", "%s/../../share/zrnui" };
			for (size_t i = 0; i < sizeof(tries) / sizeof(tries[0]); i++) {
				char probe[1100], fonts[1200];
				snprintf(probe, sizeof(probe), tries[i], lib);
				snprintf(fonts, sizeof(fonts), "%s/fonts", probe);
				if (access(fonts, R_OK) == 0) {
					char *real = realpath(probe, NULL);
					snprintf(dir, sizeof(dir), "%.1000s", real ? real : probe);
					free(real);
					return dir;
				}
			}
		}
	}
#ifdef ZUI_DATADIR
	snprintf(dir, sizeof(dir), "%s", ZUI_DATADIR);
#else
	snprintf(dir, sizeof(dir), "/usr/share/zrnui");
#endif
	return dir;
}

/* ---- the system's settings (ztmetro's config file) ---- */
static bool config_value(const char *key, char *out, size_t size) {
	char path[512], line[256];
	const char *home = getenv("HOME");
	snprintf(path, sizeof(path), "%s/.config/ztmetro/config", home ? home : "");
	FILE *f = fopen(path, "r");
	bool found = false;
	while (f && fgets(line, sizeof(line), f)) {
		char *p = line;
		while (isspace((unsigned char)*p)) p++;
		size_t k = strlen(key);
		if (strncmp(p, key, k) != 0) continue;
		p += k;
		while (isspace((unsigned char)*p)) p++;
		if (*p != '=') continue;
		p++;
		while (isspace((unsigned char)*p)) p++;
		size_t n = strcspn(p, " \t\r\n#");
		snprintf(out, size, "%.*s", (int)n, p);
		found = true;	/* the last one counts, as in ztmetro */
	}
	if (f) fclose(f);
	return found;
}

static uint32_t system_accent(void) {
	static const struct { const char *name; uint32_t color; } accents[] = {
		{ "lime", 0xa4c400 }, { "green", 0x60a917 }, { "emerald", 0x008a00 },
		{ "teal", 0x00aba9 }, { "cyan", 0x1ba1e2 }, { "cobalt", 0x0050ef },
		{ "indigo", 0x6a00ff }, { "violet", 0xaa00ff }, { "pink", 0xf472d0 },
		{ "magenta", 0xd80073 }, { "crimson", 0xa20025 }, { "red", 0xe51400 },
		{ "orange", 0xfa6800 }, { "amber", 0xf0a30a }, { "yellow", 0xe3c800 },
		{ "brown", 0x825a2c }, { "olive", 0x6d8764 }, { "steel", 0x647687 },
		{ "mauve", 0x76608a }, { "taupe", 0x87794e },
	};
	char v[64];
	const char *env = getenv("ZUI_ACCENT");
	if (env) snprintf(v, sizeof(v), "%s", env);
	else if (!config_value("accent", v, sizeof(v))) return 0x0050ef;
	if (v[0] == '#') return (uint32_t)strtoul(v + 1, NULL, 16) & 0xffffff;
	for (size_t i = 0; i < sizeof(accents) / sizeof(accents[0]); i++)
		if (strcmp(accents[i].name, v) == 0) return accents[i].color;
	return 0x0050ef;
}

const struct zui_style *zui_style_find(const char *name) {
	if (!name) return NULL;
	if (strcmp(name, "cool") == 0 || strcmp(name, "zrnCoolStyle") == 0) return &zui_style_cool;
	if (strcmp(name, "metro") == 0 || strcmp(name, "Metro") == 0) return &zui_style_metro;
	return NULL;
}

/* The style an app is drawn in: what the user asked for (ZUI_STYLE, or
 * zrnui_style in the config), else what the app asked for, else zrnCoolStyle.
 * ztRetrofit's DotMatrix session never mixes styles: until DotMatrix exists
 * here its apps are drawn in Metro, the nearest. */
static const struct zui_style *pick_style(const char *app_choice) {
	const struct zui_style *s = zui_style_find(getenv("ZUI_STYLE"));
	if (s) return s;
	const char *session = getenv("ZTRF_SHELL_STYLE");
	if (session && strcmp(session, "dotmatrix") == 0) return &zui_style_metro;
	char v[32];
	if (config_value("zrnui_style", v, sizeof(v))) {
		if (strcmp(v, "follow") == 0) return &zui_style_metro;
		if ((s = zui_style_find(v))) return s;
	}
	if ((s = zui_style_find(app_choice))) return s;
	return &zui_style_cool;
}

static void apply_style(zui_app *app) {
	zt_set_fonts(app->style->fonts, app->style->line_spacing);
	app->style_gen++;
	app->laid_out = false;
	app->dirty = true;
}

struct zui_post { zui_fn fn; void *data; struct zui_post *next; };

ZUI_EXPORT zui_app *zui_app_new(const char *app_id, const char *title) {
	zui_app *app = calloc(1, sizeof(*app));
	app->app_id = strdup(app_id ? app_id : "zrn-app");
	app->title = strdup(title ? title : app->app_id);
	app->accent = system_accent();
	app->style = pick_style(NULL);
	char v[32];
	const char *theme = getenv("ZUI_THEME");
	if (!theme && config_value("zrnui_theme", v, sizeof(v))) theme = v;
	app->light = theme && strcmp(theme, "light") == 0;
	app->width = 360;
	app->height = 740;
	app->scale = 1;
	app->running = true;
	app->next_timer_id = 1;
	app->post_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	pthread_mutex_t *lock = malloc(sizeof(*lock));
	pthread_mutex_init(lock, NULL);
	app->post_lock = lock;
	apply_style(app);
	return app;
}

ZUI_EXPORT void zui_app_set_style(zui_app *app, const char *style) {
	app->style = pick_style(style);
	apply_style(app);
}

ZUI_EXPORT const char *zui_app_style(zui_app *app) { return app->style->name; }

ZUI_EXPORT void zui_app_set_theme(zui_app *app, const char *theme) {
	if (getenv("ZUI_THEME")) return;	/* the user's choice wins */
	app->light = theme && strcmp(theme, "light") == 0;
	app->dirty = true;
}

ZUI_EXPORT void zui_app_set_accent(zui_app *app, uint32_t rgb) {
	app->accent = rgb & 0xffffff;
	app->dirty = true;
}

ZUI_EXPORT uint32_t zui_app_accent(zui_app *app) { return app->accent; }

struct zui_palette zui_pal(const zui_app *app) { return app->style->palette(app); }

/* ---- nodes ---- */
static zui_node *node_new(zui_app *app, zui_node *parent, int kind) {
	zui_node *n = calloc(1, sizeof(*n));
	n->app = app;
	n->kind = kind;
	n->enabled = n->visible = true;
	n->parent = parent;
	if (parent) {
		n->prev = parent->last;
		if (parent->last) parent->last->next = n;
		else parent->first = n;
		parent->last = n;
	}
	app->laid_out = false;
	app->dirty = true;
	return n;
}

static void node_free(zui_node *n) {
	zui_node *c = n->first;
	while (c) {
		zui_node *next = c->next;
		node_free(c);
		c = next;
	}
	zui_app *app = n->app;
	if (app->pressed == n) app->pressed = NULL;
	if (app->drag_slider == n) app->drag_slider = NULL;
	if (app->focus == n) {
		app->focus = NULL;
		zui_osk(app, false);
	}
	if (n->free_data) n->free_data(n->data);
	for (int i = 0; i < 3; i++) zt_slot_free(&n->t[i]);
	for (int i = 0; i < 2; i++) zt_slot_free(&n->title_slot[i]);
	free(n->text);
	free(n->subtitle);
	free(n->detail);
	free(n->icon);
	free(n);
}

static void unlink_node(zui_node *n) {
	zui_node *p = n->parent;
	if (!p) return;
	if (n->prev) n->prev->next = n->next;
	else p->first = n->next;
	if (n->next) n->next->prev = n->prev;
	else p->last = n->prev;
	n->parent = n->prev = n->next = NULL;
}

static void set_str(char **slot, const char *s) {
	if (*slot && s && strcmp(*slot, s) == 0) return;
	free(*slot);
	*slot = s ? strdup(s) : NULL;
}

ZUI_EXPORT zui_node *zui_page(zui_app *app, const char *title) {
	zui_node *n = node_new(app, NULL, ZK_PAGE);
	set_str(&n->text, title);
	if (app->n_pages == app->cap_pages) {
		app->cap_pages = app->cap_pages ? app->cap_pages * 2 : 8;
		app->pages = realloc(app->pages, sizeof(*app->pages) * app->cap_pages);
	}
	app->pages[app->n_pages++] = n;
	return n;
}

ZUI_EXPORT zui_node *zui_column(zui_node *parent) { return node_new(parent->app, parent, ZK_COLUMN); }
ZUI_EXPORT zui_node *zui_row(zui_node *parent) { return node_new(parent->app, parent, ZK_ROW); }
ZUI_EXPORT zui_node *zui_card(zui_node *parent) { return node_new(parent->app, parent, ZK_CARD); }

ZUI_EXPORT zui_node *zui_section(zui_node *parent, const char *title) {
	if (title && *title) {
		zui_node *label = node_new(parent->app, parent, ZK_SECTION_LABEL);
		set_str(&label->text, title);
	}
	return zui_card(parent);
}

ZUI_EXPORT zui_node *zui_space(zui_node *parent, double points) {
	zui_node *n = node_new(parent->app, parent, ZK_SPACE);
	n->space = points;
	return n;
}

static zui_node *text_node(zui_node *parent, int kind, const char *text) {
	zui_node *n = node_new(parent->app, parent, kind);
	set_str(&n->text, text);
	return n;
}

ZUI_EXPORT zui_node *zui_text(zui_node *p, const char *t) { return text_node(p, ZK_TEXT, t); }
ZUI_EXPORT zui_node *zui_heading(zui_node *p, const char *t) { return text_node(p, ZK_HEADING, t); }
ZUI_EXPORT zui_node *zui_caption(zui_node *p, const char *t) { return text_node(p, ZK_CAPTION, t); }

ZUI_EXPORT zui_node *zui_button(zui_node *parent, const char *label, zui_cb cb, void *data) {
	zui_node *n = text_node(parent, ZK_BUTTON, label);
	n->cb = cb;
	n->data = data;
	return n;
}

ZUI_EXPORT zui_node *zui_item(zui_node *parent, const char *title, const char *subtitle, zui_cb cb, void *data) {
	zui_node *n = text_node(parent, ZK_ITEM, title);
	set_str(&n->subtitle, subtitle);
	n->cb = cb;
	n->data = data;
	return n;
}

ZUI_EXPORT zui_node *zui_switch(zui_node *parent, const char *title, bool on, zui_cb cb, void *data) {
	zui_node *n = text_node(parent, ZK_SWITCH, title);
	n->on = on;
	zanim_set(&n->knob, on ? 1 : 0);
	n->cb = cb;
	n->data = data;
	return n;
}

ZUI_EXPORT zui_node *zui_slider(zui_node *parent, const char *title, double value, zui_cb cb, void *data) {
	zui_node *n = text_node(parent, ZK_SLIDER, title);
	n->value = value < 0 ? 0 : value > 1 ? 1 : value;
	n->cb = cb;
	n->data = data;
	return n;
}

ZUI_EXPORT zui_node *zui_progress(zui_node *parent, double value) {
	zui_node *n = node_new(parent->app, parent, ZK_PROGRESS);
	n->value = value > 1 ? 1 : value;
	return n;
}

ZUI_EXPORT zui_node *zui_field(zui_node *parent, const char *placeholder, zui_cb cb, void *data) {
	zui_node *n = node_new(parent->app, parent, ZK_FIELD);
	set_str(&n->subtitle, placeholder);
	set_str(&n->text, "");
	n->cb = cb;
	n->data = data;
	return n;
}

static void changed(zui_node *n, bool layout) {
	if (layout) n->app->laid_out = false;
	n->app->dirty = true;
}

ZUI_EXPORT void zui_set_text(zui_node *n, const char *t) { set_str(&n->text, t ? t : ""); changed(n, true); }
ZUI_EXPORT const char *zui_get_text(zui_node *n) { return n->text ? n->text : ""; }
ZUI_EXPORT void zui_set_detail(zui_node *n, const char *t) { set_str(&n->detail, t); changed(n, true); }
ZUI_EXPORT void zui_set_subtitle(zui_node *n, const char *t) { set_str(&n->subtitle, t); changed(n, true); }

ZUI_EXPORT void zui_set_value(zui_node *n, double v) {
	if (n->kind == ZK_SLIDER) v = v < 0 ? 0 : v > 1 ? 1 : v;
	if (n->kind == ZK_PROGRESS && v > 1) v = 1;
	n->value = v;
	changed(n, false);
}

ZUI_EXPORT double zui_get_value(zui_node *n) { return n->kind == ZK_SWITCH ? (n->on ? 1 : 0) : n->value; }

ZUI_EXPORT void zui_set_on(zui_node *n, bool on) {
	if (n->on == on) return;
	n->on = on;
	zanim_go(&n->knob, on ? 1 : 0, 180, zui_now());
	changed(n, false);
}

ZUI_EXPORT bool zui_get_on(zui_node *n) { return n->on; }
ZUI_EXPORT void zui_set_enabled(zui_node *n, bool e) { n->enabled = e; changed(n, false); }
ZUI_EXPORT void zui_set_visible(zui_node *n, bool v) { n->visible = v; changed(n, true); }
ZUI_EXPORT void zui_set_variant(zui_node *n, int v) { n->variant = v; changed(n, false); }
ZUI_EXPORT void zui_set_flex(zui_node *n, double f) { n->flex = f; changed(n, true); }
ZUI_EXPORT void zui_set_icon(zui_node *n, const char *icon) { set_str(&n->icon, icon); changed(n, true); }
ZUI_EXPORT void zui_set_tint(zui_node *n, uint32_t rgb) { n->tint = rgb ? (rgb & 0xffffff) | 0x1000000 : 0; changed(n, false); }

ZUI_EXPORT void zui_set_handler(zui_node *n, zui_cb cb, void *data, zui_free_fn free_data) {
	if (n->free_data) n->free_data(n->data);
	n->cb = cb;
	n->data = data;
	n->free_data = free_data;
	changed(n, false);
}

ZUI_EXPORT int zui_event(zui_node *n) { return n->event; }
ZUI_EXPORT zui_app *zui_node_app(zui_node *n) { return n->app; }

ZUI_EXPORT void zui_node_frame(zui_node *n, double *x, double *y, double *w, double *h) {
	if (x) *x = n->x;
	if (y) *y = n->y;
	if (w) *w = n->w;
	if (h) *h = n->h;
}

ZUI_EXPORT void zui_clear(zui_node *n) {
	while (n->first) {
		zui_node *c = n->first;
		unlink_node(c);
		node_free(c);
	}
	changed(n, true);
}

ZUI_EXPORT void zui_destroy(zui_node *n) {
	zui_app *app = n->app;
	if (n->kind == ZK_PAGE) {
		if (app->page == n || app->trans_from == n || app->trans_to == n) return;	/* in use */
		for (int i = 0; i < app->n_pages; i++) {
			if (app->pages[i]->back_to == n) app->pages[i]->back_to = NULL;
			if (app->pages[i] == n) {
				app->pages[i] = app->pages[--app->n_pages];
				i--;
			}
		}
		node_free(n);
		return;
	}
	unlink_node(n);
	node_free(n);
	app->laid_out = false;
	app->dirty = true;
}

void zui_fire(zui_node *n, int event) {
	n->event = event;
	if (n->cb) n->cb(n, n->data);
	n->event = ZUI_EVENT_NONE;
	n->app->dirty = true;
}

ZUI_EXPORT void zui_busy(zui_app *app, bool busy) {
	if (busy && !app->busy) app->busy_since = zui_now();
	app->busy = busy;
	app->dirty = true;
}

ZUI_EXPORT void zui_toast(zui_app *app, const char *text) {
	set_str(&app->toast, text);
	app->toast_at = zui_now();
	app->dirty = true;
}

/* ---- pages ---- */
ZUI_EXPORT zui_node *zui_current_page(zui_app *app) { return app->page; }

static void transition_finish(zui_app *app) {
	if (!app->trans_to) return;
	app->trans_from = app->trans_to = NULL;
	if (app->quit_after_trans) app->running = false;
	app->dirty = true;
}

ZUI_EXPORT void zui_push(zui_node *page) {
	zui_app *app = page->app;
	if (page->kind != ZK_PAGE || page == app->page) return;
	transition_finish(app);
	if (app->focus) {
		app->focus->focused = false;
		app->focus = NULL;
		zui_osk(app, false);
	}
	page->back_to = app->page;
	page->depth = app->page ? app->page->depth + 1 : 0;
	page->scroll = 0;
	page->vel = 0;
	app->fling = false;
	if (app->page && app->wl) {
		app->trans_from = app->page;
		app->trans_to = page;
		app->trans_dir = 1;
		app->trans_start = zui_now();
	}
	app->page = page;
	app->pressed = NULL;
	app->laid_out = false;
	app->dirty = true;
}

ZUI_EXPORT void zui_back(zui_app *app) {
	transition_finish(app);
	zui_node *page = app->page;
	if (!page) return;
	if (app->focus) {
		app->focus->focused = false;
		app->focus = NULL;
		zui_osk(app, false);
	}
	if (!page->back_to) {
		zui_quit(app);
		return;
	}
	if (app->wl) {
		app->trans_from = page;
		app->trans_to = page->back_to;
		app->trans_dir = -1;
		app->trans_start = zui_now();
	}
	app->page = page->back_to;
	app->pressed = NULL;
	app->fling = false;
	app->laid_out = false;
	app->dirty = true;
}

ZUI_EXPORT void zui_quit(zui_app *app) {
	app->running = false;
	zui_wl_wake(app);
}

ZUI_EXPORT void zui_app_free(zui_app *app) {
	if (!app) return;
	for (int i = 0; i < app->n_pages; i++) node_free(app->pages[i]);
	free(app->pages);
	free(app->app_id);
	free(app->title);
	free(app->toast);
	struct zui_post *p = app->posts;
	while (p) {
		struct zui_post *next = p->next;
		free(p);
		p = next;
	}
	if (app->post_fd >= 0) close(app->post_fd);
	pthread_mutex_destroy(app->post_lock);
	free(app->post_lock);
	free(app);
}

/* ---- layout ---- */
static bool in_card(const zui_node *n) {
	return n->parent && n->parent->kind == ZK_CARD;
}

/* a row (item, switch) fills a card edge to edge; anything else sits in its padding */
static bool card_row_kind(int kind) {
	return kind == ZK_ITEM || kind == ZK_SWITCH;
}

static float text_size(const zui_app *app, int kind) {
	const struct zui_metrics *m = &app->style->m;
	return kind == ZK_HEADING ? m->heading : kind == ZK_CAPTION ? m->caption : m->text;
}

static int text_font(int kind) {
	return kind == ZK_HEADING ? ZT_STRONG : ZT_REGULAR;
}

static float intrinsic_w(zui_node *n) {
	const struct zui_metrics *m = &n->app->style->m;
	switch (n->kind) {
	case ZK_BUTTON: {
		float w = zt_width(n->text, ZT_MEDIUM, m->button_text) + m->button_h * 0.9f;
		return w < m->button_h * 1.6f ? m->button_h * 1.6f : w;
	}
	case ZK_TEXT: case ZK_HEADING: case ZK_CAPTION:
		return zt_width(n->text, text_font(n->kind), text_size(n->app, n->kind));
	case ZK_SPACE:
		return (float)n->space;
	default:
		return -1;	/* takes a share of the room */
	}
}

static float layout_node(zui_node *n, float x, float y, float w);

/* things in a card that read as a list: dividers go next to them */
static bool listy(const zui_node *n) {
	return n->kind == ZK_ITEM || n->kind == ZK_SWITCH || n->kind == ZK_SLIDER;
}

static float layout_card(zui_node *n, float x, float y, float w, float gap) {
	float at = y;
	zui_node *prev = NULL;
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		/* two things that are not list rows get room between them */
		if (prev && !listy(prev) && !listy(c)) at += gap;
		else if (prev && listy(prev) != listy(c)) at += gap * 0.5f;
		at += layout_node(c, x, at, w);
		prev = c;
	}
	return at - y;
}

static float layout_children_column(zui_node *n, float x, float y, float w, float gap) {
	float at = y;
	bool first = true;
	bool after_label = false;
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		/* a section's label sits close over its card */
		if (!first) at += after_label ? gap * 0.35f : gap;
		at += layout_node(c, x, at, w);
		first = false;
		after_label = c->kind == ZK_SECTION_LABEL;
	}
	return at - y;
}

static float layout_node(zui_node *n, float x, float y, float w) {
	zui_app *app = n->app;
	const struct zui_metrics *m = &app->style->m;
	float pad = in_card(n) && !card_row_kind(n->kind) ? m->card_pad : 0;
	float h = 0;
	n->x = x;
	n->y = y;
	n->w = w;
	switch (n->kind) {
	case ZK_COLUMN:
		h = layout_children_column(n, x + pad, y, w - 2 * pad, m->gap);
		break;
	case ZK_CARD:
		h = layout_card(n, x, y, w, m->gap * 0.75f);
		if (n->first && m->card_r > 0) {
			/* a card's own breathing room above its first and under its last
			 * thing, unless those are rows that bring their own */
			zui_node *f = n->first, *l = n->last;
			while (f && !f->visible) f = f->next;
			while (l && !l->visible) l = l->prev;
			float top = f && !card_row_kind(f->kind) ? m->card_pad * 0.75f : 0;
			float bottom = l && !card_row_kind(l->kind) ? m->card_pad * 0.75f : 0;
			if (top > 0) {
				for (zui_node *c = n->first; c; c = c->next)
					if (c->visible) layout_node(c, c->x, c->y + top, c->w);
			}
			h += top + bottom;
		}
		break;
	case ZK_ROW: {
		float gap = m->gap * 0.75f, fixed = 0, flex = 0;
		int count = 0;
		for (zui_node *c = n->first; c; c = c->next) {
			if (!c->visible) continue;
			count++;
			float iw = c->flex > 0 ? -1 : intrinsic_w(c);
			if (iw < 0) flex += c->flex > 0 ? (float)c->flex : 1;
			else fixed += iw;
		}
		float room = w - 2 * pad - fixed - gap * (count > 1 ? count - 1 : 0);
		if (room < 0) room = 0;
		float at = x + pad;
		for (zui_node *c = n->first; c; c = c->next) {
			if (!c->visible) continue;
			float iw = c->flex > 0 ? -1 : intrinsic_w(c);
			float cw = iw < 0 ? room * (c->flex > 0 ? (float)c->flex : 1) / (flex > 0 ? flex : 1) : iw;
			float ch = layout_node(c, at, y, cw);
			if (ch > h) h = ch;
			at += cw + gap;
		}
		/* centred across the row's height */
		for (zui_node *c = n->first; c; c = c->next) {
			if (!c->visible || c->h >= h) continue;
			layout_node(c, c->x, y + (h - c->h) / 2, c->w);
		}
		break;
	}
	case ZK_SECTION_LABEL:
		h = zt_line_height(m->label) + m->section_gap;
		break;
	case ZK_SPACE:
		h = (float)n->space;
		break;
	case ZK_TEXT: case ZK_HEADING: case ZK_CAPTION: {
		struct zt_layout *L = zt_slot_get(&n->t[0], n->text, text_font(n->kind),
			text_size(app, n->kind), w - 2 * pad, 0);
		h = L->h + (pad > 0 ? 0 : 0);
		break;
	}
	case ZK_BUTTON:
		h = m->button_h;
		break;
	case ZK_ITEM:
		h = (n->subtitle && *n->subtitle) || (app->style == &zui_style_metro && n->detail) ? m->item_h2 : m->item_h;
		break;
	case ZK_SWITCH:
		h = (n->subtitle && *n->subtitle) || app->style == &zui_style_metro ? m->item_h2 : m->item_h;
		break;
	case ZK_SLIDER:
		h = app->style == &zui_style_metro ? 56 : 76;
		break;
	case ZK_PROGRESS:
		h = app->style == &zui_style_metro ? 12 : 18;
		break;
	case ZK_FIELD:
		h = m->field_h + (app->style == &zui_style_metro ? 22 : 0);
		break;
	default:
		break;
	}
	n->h = h;
	return h;
}

void zui_do_layout(zui_app *app, zui_node *page) {
	const struct zui_metrics *m = &app->style->m;
	page->x = 0;
	page->y = 0;
	page->w = app->width;
	page->header_h = m->bar_h;
	float y = page->header_h;
	y = app->style->page_top(app, page, y, false);
	y += layout_children_column(page, m->margin, y, app->width - 2 * m->margin, m->gap);
	page->content_h = y + m->margin * 2;
	page->h = page->content_h;
}

ZUI_EXPORT void zui_layout(zui_app *app, zui_node *page, int width, int height) {
	app->width = (float)width;
	app->height = (float)height;
	if (!app->page) app->page = page;
	zui_do_layout(app, page);
}

static float scroll_max(zui_app *app, zui_node *page) {
	float max = page->content_h - app->height;
	return max > 0 ? max : 0;
}

/* ---- the finger ---- */
static bool interactive(const zui_node *n) {
	if (!n->enabled) return false;
	switch (n->kind) {
	case ZK_BUTTON: case ZK_SWITCH: case ZK_SLIDER: case ZK_FIELD: return true;
	case ZK_ITEM: return n->cb != NULL;
	default: return false;
	}
}

static zui_node *hit_in(zui_node *n, float x, float y) {
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		if (x < c->x || x >= c->x + c->w || y < c->y || y >= c->y + c->h) continue;
		zui_node *deeper = hit_in(c, x, y);
		if (deeper) return deeper;
		if (interactive(c)) return c;
	}
	return NULL;
}

static void focus(zui_app *app, zui_node *field) {
	if (app->focus == field) return;
	uint32_t now = zui_now();
	if (app->focus) {
		app->focus->focused = false;
		zanim_go(&app->focus->focus_anim, 0, 150, now);
	}
	app->focus = field;
	if (field) {
		field->focused = true;
		zanim_go(&field->focus_anim, 1, 150, now);
	}
	zui_osk(app, field != NULL);
	app->dirty = true;
}

static void slider_to(zui_app *app, zui_node *s, float x) {
	float pad = in_card(s) ? app->style->m.card_pad : 0;
	float x0 = s->x + pad, w = s->w - 2 * pad;
	double v = w > 0 ? (x - x0) / w : 0;
	v = v < 0 ? 0 : v > 1 ? 1 : v;
	if (fabs(v - s->value) < 1e-4) return;
	s->value = v;
	zui_fire(s, ZUI_EVENT_CHANGE);
}

static void activate(zui_app *app, zui_node *n) {
	switch (n->kind) {
	case ZK_SWITCH:
		zui_set_on(n, !n->on);
		zui_haptic("click");
		zui_fire(n, ZUI_EVENT_CHANGE);
		break;
	case ZK_FIELD:
		focus(app, n);
		break;
	default:
		zui_haptic("tap");
		zui_fire(n, ZUI_EVENT_CLICK);
		break;
	}
}

/* phase 0 down, 1 move, 2 up, 3 cancel; x, y in window points */
void zui_touch(zui_app *app, int phase, float x, float y, uint32_t time) {
	zui_node *page = app->page;
	if (!page) return;
	uint32_t now = zui_now();
	if (!app->laid_out) {
		zui_do_layout(app, page);
		app->laid_out = true;
	}
	if (phase == 0) {
		if (app->trans_to) transition_finish(app);
		app->touching = true;
		app->dragging = false;
		app->down_x = app->last_x = x;
		app->down_y = app->last_y = y;
		app->last_t = time;
		app->down_scroll = page->scroll;
		app->fling = false;
		page->vel = 0;
		app->pressed = NULL;
		app->drag_slider = NULL;
		if (y < page->header_h) {
			return;	/* the header: a tap there is handled on the lift */
		}
		zui_node *hit = hit_in(page, x, y + page->scroll);
		if (hit && hit->kind == ZK_SLIDER) {
			app->drag_slider = hit;
			slider_to(app, hit, x);
			zui_haptic("tick");
			return;
		}
		if (hit) {
			app->pressed = hit;
			app->press_fx = hit->w > 0 ? (x - hit->x) / hit->w * 2 - 1 : 0;
			app->press_fy = hit->h > 0 ? (y + page->scroll - hit->y) / hit->h * 2 - 1 : 0;
			zanim_go(&app->press, 1, 90, now);
		}
		app->dirty = true;
		return;
	}
	if (!app->touching) return;
	if (phase == 1) {
		if (app->drag_slider) {
			slider_to(app, app->drag_slider, x);
			return;
		}
		float dy = y - app->down_y;
		if (!app->dragging && fabsf(dy) > SLOP) {
			app->dragging = true;
			if (app->pressed) zanim_go(&app->press, 0, 160, now);
		}
		if (app->dragging) {
			float raw = app->down_scroll - dy, max = scroll_max(app, page);
			/* past either end the page follows the finger less and less */
			if (raw < 0) raw = -(-raw) * 0.45f / (1 + (-raw) / (app->height * 0.6f));
			else if (raw > max) raw = max + (raw - max) * 0.45f / (1 + (raw - max) / (app->height * 0.6f));
			float dt = (float)(time - app->last_t);
			if (dt > 0) {
				float v = -(y - app->last_y) / dt;	/* points per ms */
				page->vel = page->vel * 0.3f + v * 0.7f;
			}
			page->scroll = raw;
			app->dirty = true;
		}
		app->last_x = x;
		app->last_y = y;
		app->last_t = time;
		return;
	}
	/* up or cancel */
	app->touching = false;
	if (app->drag_slider) {
		zui_node *s = app->drag_slider;
		app->drag_slider = NULL;
		if (phase == 2) zui_fire(s, ZUI_EVENT_SUBMIT);
		return;
	}
	if (app->dragging) {
		app->dragging = false;
		if (phase == 2 && (int32_t)(time - app->last_t) < 80) {
			app->fling = true;
			app->fling_t = now;
		} else {
			page->vel = 0;
		}
		app->dirty = true;
		return;
	}
	if (phase != 2) {
		if (app->pressed) zanim_go(&app->press, 0, 160, now);
		return;
	}
	/* a tap */
	if (app->down_y < page->header_h && y < page->header_h) {
		if (page->depth > 0 && (app->style == &zui_style_metro || x < 64)) {
			zui_haptic("tap");
			zui_back(app);
		}
		return;
	}
	zui_node *hit = app->pressed;
	if (hit) {
		zanim_go(&app->press, 0, app->style == &zui_style_metro ? 200 : 220, now);
		/* the press shows for at least a moment even on a quick tap */
		if (app->press.from < 0.6f) {
			app->press.from = 1;
		}
		activate(app, hit);
	} else if (app->focus) {
		focus(app, NULL);
	}
	app->dirty = true;
}

ZUI_EXPORT void zui_inject_touch(zui_app *app, int phase, double x, double y) {
	static uint32_t t;
	t += 16;
	zui_touch(app, phase, (float)x, (float)y, t);
}

ZUI_EXPORT void zui_inject_text(zui_app *app, const char *utf8) {
	zui_key_text(app, utf8);
}

void zui_scroll_by(zui_app *app, float dy) {
	zui_node *page = app->page;
	if (!page) return;
	float s = page->scroll + dy, max = scroll_max(app, page);
	page->scroll = s < 0 ? 0 : s > max ? max : s;
	app->dirty = true;
}

void zui_key_text(zui_app *app, const char *utf8) {
	zui_node *f = app->focus;
	if (!f) return;
	if (strcmp(utf8, "\n") == 0) {
		zui_fire(f, ZUI_EVENT_SUBMIT);
		return;
	}
	size_t len = f->text ? strlen(f->text) : 0;
	if (strcmp(utf8, "\b") == 0) {
		if (!len) return;
		size_t cut = len - 1;
		while (cut > 0 && ((unsigned char)f->text[cut] & 0xc0) == 0x80) cut--;
		f->text[cut] = 0;
	} else {
		char *t = malloc(len + strlen(utf8) + 1);
		memcpy(t, f->text ? f->text : "", len);
		strcpy(t + len, utf8);
		free(f->text);
		f->text = t;
	}
	app->dirty = true;
	zui_fire(f, ZUI_EVENT_CHANGE);
}

/* the keyboard is the shell's: asked for down its control pipe, with where
 * the field ends so the shell can lift the app over the keyboard */
void zui_osk(zui_app *app, bool show) {
	const char *runtime = getenv("XDG_RUNTIME_DIR");
	if (!runtime || !app->wl) return;
	char path[300], line[64];
	snprintf(path, sizeof(path), "%s/ztmetro.ctl", runtime);
	if (show && app->focus && app->page) {
		int bottom = (int)lroundf(app->focus->y + app->focus->h - app->page->scroll);
		snprintf(line, sizeof(line), "osk show %d\n", bottom);
	} else {
		snprintf(line, sizeof(line), show ? "osk show\n" : "osk hide\n");
	}
	int fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd >= 0) {
		if (write(fd, line, strlen(line)) < 0) {}
		close(fd);
	}
}

ZUI_EXPORT void zui_haptic(const char *kind) {
	static int fd = -1;
	static uint32_t tried;
	if (fd < 0) {
		uint32_t now = zui_now();
		const char *runtime = getenv("XDG_RUNTIME_DIR");
		if (!runtime || (tried && now - tried < 2000)) return;
		tried = now ? now : 1;
		char path[300];
		snprintf(path, sizeof(path), "%s/ztmetro.ctl", runtime);
		fd = open(path, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0) return;
	}
	char line[48];
	int n = snprintf(line, sizeof(line), "haptic %s\n", kind);
	if (write(fd, line, (size_t)n) < 0 && errno != EAGAIN) {
		close(fd);
		fd = -1;
	}
}

/* ---- motion between frames ---- */
static uint32_t last_step;

void zui_step(zui_app *app, uint32_t now) {
	zui_node *page = app->page;
	float dt = last_step ? (float)(int32_t)(now - last_step) : 16;
	if (dt > 64) dt = 64;
	if (dt < 0) dt = 0;
	last_step = now;
	if (app->trans_to && (int32_t)(now - app->trans_start) >= app->style->transition_ms)
		transition_finish(app);
	if (!page || app->touching) return;
	float max = scroll_max(app, page);
	if (app->fling) {
		page->scroll += page->vel * dt;
		page->vel *= powf(0.9972f, dt);
		if (page->scroll < 0 || page->scroll > max) page->vel *= powf(0.80f, dt / 16);
		if (fabsf(page->vel) < 0.02f) {
			app->fling = false;
			page->vel = 0;
		}
		app->dirty = true;
	}
	if (!app->fling && (page->scroll < 0 || page->scroll > max)) {
		float target = page->scroll < 0 ? 0 : max;
		page->scroll += (target - page->scroll) * (1 - expf(-dt / 70));
		if (fabsf(target - page->scroll) < 0.3f) page->scroll = target;
		app->dirty = true;
	}
}

static bool has_indeterminate(zui_node *n) {
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		if (c->kind == ZK_PROGRESS && c->value < 0) return true;
		if (has_indeterminate(c)) return true;
	}
	return false;
}

bool zui_animating(zui_app *app) {
	uint32_t now = zui_now();
	zui_node *page = app->page;
	if (app->trans_to || app->fling || app->busy) return true;
	if ((int32_t)(anim_until - now) > 0) return true;
	if (app->toast && (int32_t)(now - app->toast_at) < 2600) return true;
	if (page && !app->touching && (page->scroll < 0 || page->scroll > scroll_max(app, page))) return true;
	if (page && has_indeterminate(page)) return true;
	return false;
}

/* ---- drawing ---- */
static void draw_node(zui_app *app, zui_node *n, const struct zg_xf *base, uint32_t now);

static void draw_children(zui_app *app, zui_node *n, const struct zg_xf *base, uint32_t now) {
	const struct zui_style *st = app->style;
	zui_node *prev = NULL;
	for (zui_node *c = n->first; c; c = c->next) {
		if (!c->visible) continue;
		if (n->kind == ZK_CARD && prev && (listy(prev) || listy(c))) {
			zg_set_xf(base);
			st->divider(app, n, c);
		}
		draw_node(app, c, base, now);
		prev = c;
	}
}

static void draw_node(zui_app *app, zui_node *n, const struct zg_xf *base, uint32_t now) {
	const struct zui_style *st = app->style;
	struct zui_palette pal = st->palette(app);
	float press = 0;
	struct zg_xf xf = *base;
	if (n == app->pressed) {
		bool running = false;
		press = zanim_value(&app->press, now, &running);
		if (press > 0.001f && st->press_xf) {
			struct zg_xf p;
			zg_xf_identity(&p);
			st->press_xf(app, n, press, app->press_fx, app->press_fy, &p);
			/* the press happens on the page, before the page's own movement */
			struct zg_xf combined;
			/* combined = base * p: apply p first, then base */
			float r[16];
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++) {
					float s = 0;
					for (int k = 0; k < 4; k++) s += base->m[i * 4 + k] * p.m[k * 4 + j];
					r[i * 4 + j] = s;
				}
			memcpy(combined.m, r, sizeof(r));
			xf = combined;
		}
		if (!running && press <= 0.001f) app->pressed = NULL;
	}
	zg_set_xf(&xf);
	float alpha = zg_get_alpha();
	if (!n->enabled) zg_set_alpha(alpha * 0.4f);
	float pad = in_card(n) && !card_row_kind(n->kind) ? st->m.card_pad : 0;
	switch (n->kind) {
	case ZK_COLUMN: case ZK_ROW:
		draw_children(app, n, &xf, now);
		break;
	case ZK_CARD:
		st->card(app, n);
		draw_children(app, n, &xf, now);
		break;
	case ZK_SECTION_LABEL:
		st->section_label(app, n);
		break;
	case ZK_TEXT: case ZK_HEADING: case ZK_CAPTION: {
		struct zt_layout *L = zt_slot_get(&n->t[0], n->text, text_font(n->kind),
			text_size(app, n->kind), n->w - 2 * pad, 0);
		zt_draw(L, n->x + pad, n->y, n->kind == ZK_CAPTION ? pal.text2 : pal.text);
		break;
	}
	case ZK_BUTTON:
		st->button(app, n, press);
		break;
	case ZK_ITEM: case ZK_SWITCH:
		st->item(app, n, press);
		break;
	case ZK_SLIDER:
		st->slider(app, n);
		break;
	case ZK_PROGRESS:
		st->progress(app, n, now);
		break;
	case ZK_FIELD:
		st->field(app, n, now);
		break;
	default:
		break;
	}
	zg_set_alpha(alpha);
}

/* one page, moved by `trans` (NULL at rest); role: 0 at rest, 1 incoming, 2 outgoing */
static void draw_page(zui_app *app, zui_node *page, uint32_t now, int role, float t, int dir) {
	const struct zui_style *st = app->style;
	float scroll = page->scroll;
	int rows = 0;
	bool flat = st->flat_cards;
	for (zui_node *c = page->first; c; c = c->next) {
		if (!c->visible) continue;
		if (flat && c->kind == ZK_CARD && c->first) {
			for (zui_node *k = c->first; k; k = k->next) rows += k->visible;
		} else {
			rows++;
		}
	}
	zg_clip(0, page->header_h, app->width, app->height - page->header_h);
	struct zg_xf base;
	float alpha = 1;
	/* the large title, if the style has one, scrolls as the first row */
	zg_xf_identity(&base);
	zg_xf_translate(&base, 0, -scroll, 0);
	if (role) st->page_xf(app, t, dir, role == 1, 0, rows + 1, &base, &alpha);
	zg_set_xf(&base);
	zg_set_alpha(alpha);
	st->page_top(app, page, page->header_h, true);
	int row = 1;
	for (zui_node *c = page->first; c; c = c->next) {
		if (!c->visible) continue;
		bool split = flat && c->kind == ZK_CARD && c->first;
		zui_node *k = split ? c->first : c;
		/* one row, or each row of a card in turn */
		for (; k; k = split ? k->next : NULL) {
			if (!k->visible) continue;
			int r = row++;
			if (k->y - scroll > app->height || k->y + k->h - scroll < 0) continue;
			zg_xf_identity(&base);
			zg_xf_translate(&base, 0, -scroll, 0);
			alpha = 1;
			if (role) st->page_xf(app, t, dir, role == 1, r, rows + 1, &base, &alpha);
			if (alpha <= 0.002f) continue;
			zg_set_alpha(alpha);
			if (split && k == c->first) {
				zg_set_xf(&base);
				st->card(app, c);
			}
			draw_node(app, k, &base, now);
		}
	}
	zg_noclip();
	zg_xf_identity(&base);
	alpha = 1;
	if (role) st->page_xf(app, t, dir, role == 1, -1, rows + 1, &base, &alpha);
	zg_set_xf(&base);
	zg_set_alpha(alpha);
	st->header(app, page, now);
	zg_set_xf(NULL);
	zg_set_alpha(1);
}

void zui_draw(zui_app *app, uint32_t now) {
	const struct zui_style *st = app->style;
	zui_node *page = app->page;
	if (!page) return;
	if (!app->laid_out) {
		for (int i = 0; i < app->n_pages; i++) {
			zui_node *p = app->pages[i];
			if (p == page || p == app->trans_from || p == app->trans_to) zui_do_layout(app, p);
		}
		app->laid_out = true;
	}
	if (app->trans_to) {
		float t = (float)(int32_t)(now - app->trans_start) / st->transition_ms;
		t = t < 0 ? 0 : t > 1 ? 1 : t;
		/* the page on top is drawn last: going forward the new one, going back the old one */
		if (app->trans_dir > 0) {
			draw_page(app, app->trans_from, now, 2, t, 1);
			draw_page(app, app->trans_to, now, 1, t, 1);
		} else {
			draw_page(app, app->trans_to, now, 1, t, -1);
			draw_page(app, app->trans_from, now, 2, t, -1);
		}
	} else {
		draw_page(app, page, now, 0, 0, 0);
	}
	if (app->busy) st->busy(app, now);
	if (app->toast) {
		float age = (float)(int32_t)(now - app->toast_at);
		if (age < 2600) {
			float shown = age < 200 ? age / 200 : age > 2300 ? (2600 - age) / 300 : 1;
			st->toast(app, app->toast, zui_ease_out(shown));
		}
	}
}

ZUI_EXPORT void zui_post(zui_app *app, zui_fn fn, void *data) {
	struct zui_post *p = malloc(sizeof(*p));
	p->fn = fn;
	p->data = data;
	p->next = NULL;
	pthread_mutex_lock(app->post_lock);
	struct zui_post **tail = &app->posts;
	while (*tail) tail = &(*tail)->next;
	*tail = p;
	pthread_mutex_unlock(app->post_lock);
	uint64_t one = 1;
	if (write(app->post_fd, &one, sizeof(one)) < 0) {}
}

/* run what other threads posted (from the main loop) */
void zui_run_posts(zui_app *app);
void zui_run_posts(zui_app *app) {
	uint64_t count;
	if (read(app->post_fd, &count, sizeof(count)) < 0) {}
	pthread_mutex_lock(app->post_lock);
	struct zui_post *p = app->posts;
	app->posts = NULL;
	pthread_mutex_unlock(app->post_lock);
	while (p) {
		struct zui_post *next = p->next;
		p->fn(p->data);
		free(p);
		p = next;
	}
	app->dirty = true;
}

ZUI_EXPORT int zui_render_png(zui_app *app, zui_node *page, const char *path, int width, int height, int scale) {
	return zui_offscreen_png(app, page, path, width, height, scale);
}

ZUI_EXPORT int zui_run(zui_app *app, zui_node *first) {
	if (first && !app->page) {
		first->depth = 0;
		first->back_to = NULL;
		app->page = first;
	}
	const char *png = getenv("ZUI_RENDER_PNG");
	if (png && *png) {
		/* draw the first page into a PNG and stop: for evidence and tests */
		const char *size = getenv("ZUI_RENDER_SIZE");
		int w = 360, h = 740, s = 2;
		if (size) sscanf(size, "%dx%d@%d", &w, &h, &s);
		/* ZUI_RENDER_PAGE: another page than the first, by its title or its
		 * number in the order the app made its pages (0 is the first made) */
		zui_node *page = app->page;
		const char *which = getenv("ZUI_RENDER_PAGE");
		if (which && *which) {
			char *end;
			long k = strtol(which, &end, 10);
			zui_node *found = NULL;
			for (int i = 0; i < app->n_pages && !found; i++)
				if (app->pages[i]->text && strcmp(app->pages[i]->text, which) == 0) found = app->pages[i];
			if (!found && !*end && k >= 0 && k < app->n_pages) found = app->pages[k];
			if (!found) {
				fprintf(stderr, "zrnui: no page \"%s\" (the app has %d pages:", which, app->n_pages);
				for (int i = 0; i < app->n_pages; i++)
					fprintf(stderr, " %d=\"%s\"", i, app->pages[i]->text ? app->pages[i]->text : "");
				fprintf(stderr, ")\n");
				return 2;
			}
			if (found != page) {
				found->back_to = page;
				found->depth = 1;	/* drawn as if opened from the first page */
				page = found;
			}
		}
		return zui_offscreen_png(app, page, png, w, h, s) == 0 ? 0 : 1;
	}
	return zui_wl_run(app);
}
