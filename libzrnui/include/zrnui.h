// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libzrnui: the ztOS app toolkit. One C library underneath C, C++, Rust and
 * Python apps; widgets are drawn straight on the GPU (GLES2), in a style
 * the system chooses (Metro or DotMatrix); an app follows it, so they never mix.
 *
 * An app is a set of pages. A page is a column: put things in it and the
 * library lays them out, scrolls them, presses them and moves between pages.
 *
 *	static zui_node *label;
 *	static void tapped(zui_node *button, void *data) {
 *		zui_set_text(label, "Hello again");
 *	}
 *	int main(void) {
 *		zui_app *app = zui_app_new("zrn-hello", "Hello");
 *		zui_node *page = zui_page(app, "Hello");
 *		label = zui_text(page, "Hello from ztOS");
 *		zui_button(page, "Tap me", tapped, NULL);
 *		return zui_run(app, page);
 *	}
 *
 * Everything here is called from one thread, the one that calls zui_run;
 * zui_post is the one way in from another thread.
 */
#ifndef ZRNUI_H
#define ZRNUI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZUI_VERSION_MAJOR 0
#define ZUI_VERSION_MINOR 1
#define ZUI_VERSION_PATCH 0
/* The ABI an app was built against; a .zexec manifest names it ("zrnui": "0.1"). */
#define ZUI_ABI "0.1"

typedef struct zui_app zui_app;
typedef struct zui_node zui_node;

/* One callback shape for everything a node reports: a tap, a switch turned, a
 * slider moved, text typed. zui_event() says which. */
typedef void (*zui_cb)(zui_node *node, void *data);
typedef void (*zui_free_fn)(void *data);
typedef void (*zui_fn)(void *data);

enum zui_event {
	ZUI_EVENT_NONE = 0,
	ZUI_EVENT_CLICK,	/* a button or an item was tapped */
	ZUI_EVENT_CHANGE,	/* a switch, slider or field changed (a slider: while dragged) */
	ZUI_EVENT_SUBMIT,	/* a field's Enter, or a slider let go */
};

enum zui_variant {
	ZUI_PRIMARY = 0,	/* a button filled with the accent colour */
	ZUI_TONAL,		/* a quieter button: the accent, faint */
	ZUI_PLAIN,		/* text only */
	ZUI_DANGER,		/* a button that deletes or resets */
};

/* ---- the app ---- */
const char *zui_version(void);
/* app_id: the manifest's id, also the Wayland app_id the shell matches */
zui_app *zui_app_new(const char *app_id, const char *title);
/* Show `first_page` and run until the app quits or the window is closed.
 * Returns the exit status (0). */
int zui_run(zui_app *app, zui_node *first_page);
void zui_quit(zui_app *app);
void zui_app_free(zui_app *app);
/* The style is the system's: ZTRF_SHELL_STYLE, else `style` in ztmetro's config,
 * else Metro. ZUI_STYLE overrides it for looking at renders. An app cannot pick
 * one: this only exists for tests, and only while nothing else has decided. */
void zui_app_set_style(zui_app *app, const char *style);
const char *zui_app_style(zui_app *app);
/* Does nothing: both styles are black. Kept for the ABI. */
void zui_app_set_theme(zui_app *app, const char *theme);
/* 0xRRGGBB; by default the system's accent (~/.config/ztmetro/config).
 * DotMatrix keeps its palette whatever is set here. */
void zui_app_set_accent(zui_app *app, uint32_t rgb);
uint32_t zui_app_accent(zui_app *app);
/* The loading dots across the top, while something loads. */
void zui_busy(zui_app *app, bool busy);
/* A short message at the bottom for a couple of seconds ("Saved"). */
void zui_toast(zui_app *app, const char *text);

/* ---- pages ---- */
zui_node *zui_page(zui_app *app, const char *title);
/* Go to a page: it slides (or turns) in over the one in front. */
void zui_push(zui_node *page);
/* Up a page; on the first page this closes the app. */
void zui_back(zui_app *app);
zui_node *zui_current_page(zui_app *app);

/* ---- layout: things go in the order they are added ---- */
zui_node *zui_column(zui_node *parent);
zui_node *zui_row(zui_node *parent);	/* side by side; zui_set_flex shares the width */
zui_node *zui_card(zui_node *parent);	/* a group: items in it get dividers */
/* a heading with a card under it; returns the card */
zui_node *zui_section(zui_node *parent, const char *title);
zui_node *zui_space(zui_node *parent, double points);

/* ---- things on a page ---- */
zui_node *zui_text(zui_node *parent, const char *text);
zui_node *zui_heading(zui_node *parent, const char *text);
zui_node *zui_caption(zui_node *parent, const char *text);	/* small, quiet */
zui_node *zui_button(zui_node *parent, const char *label, zui_cb on_click, void *data);
/* A row in a list: a title, a line under it (NULL for none), an arrow if it
 * goes somewhere (on_click not NULL). zui_set_detail puts a value on the right. */
zui_node *zui_item(zui_node *parent, const char *title, const char *subtitle,
		zui_cb on_click, void *data);
zui_node *zui_switch(zui_node *parent, const char *title, bool on, zui_cb on_change, void *data);
zui_node *zui_slider(zui_node *parent, const char *title, double value, zui_cb on_change, void *data);
/* 0..1, or below 0 for "busy, no idea how long" */
zui_node *zui_progress(zui_node *parent, double value);
/* A box to type into; on_change on every edit, ZUI_EVENT_SUBMIT on Enter. */
zui_node *zui_field(zui_node *parent, const char *placeholder, zui_cb on_change, void *data);

/* ---- pictures ---- */
typedef struct zui_pixmap zui_pixmap;
/* Decoded on the CPU (JPEG, PNG), from any thread: decode big ones off the UI
 * thread and zui_post the result. max_side > 0 shrinks to about that. NULL if it
 * is not a picture. zui_pixmap_free only on the UI thread once it has been used. */
zui_pixmap *zui_pixmap_decode(const void *bytes, size_t size, int max_side);
zui_pixmap *zui_pixmap_load(const char *path, int max_side);
zui_pixmap *zui_pixmap_rgba(int w, int h, const void *rgba);
void zui_pixmap_free(zui_pixmap *pix);	/* nodes keep their own reference */
void zui_pixmap_size(const zui_pixmap *pix, int *w, int *h);
/* w 0: the width there is; h 0: from the picture's shape */
zui_node *zui_image(zui_node *parent, zui_pixmap *pix, double w, double h);
/* on an image, or on an item as its avatar */
void zui_set_image(zui_node *node, zui_pixmap *pix);
/* no smoothing, on whole pixels: a QR code */
void zui_set_image_sharp(zui_node *node, bool sharp);
void zui_set_radius(zui_node *node, double radius);
/* an item without a picture: this letter in a tinted avatar */
void zui_set_monogram(zui_node *node, const char *text);
/* a small count at an item's end */
void zui_set_badge(zui_node *node, const char *text);
/* an item's title stands out (unread) */
void zui_set_emphasis(zui_node *node, bool on);
/* text: 0 start, 1 end, 2 centre */
void zui_set_align(zui_node *node, int align);

/* ---- long lists ---- */
/* Thousands of rows of differing heights. bind fills `row_node` (a column) when
 * the row comes near the screen; height is only a guess, corrected once the row
 * is made. Must be the last thing in its page or tab. The handler gets
 * ZUI_EVENT_CHANGE near the start: a chance to zui_list_prepend older rows. */
typedef double (*zui_list_height)(int row, void *data);
typedef void (*zui_list_bind)(int row, zui_node *row_node, void *data);
zui_node *zui_list(zui_node *parent, int count, zui_list_height height, zui_list_bind bind, void *data);
void zui_list_set_row_height(zui_node *list, double height);	/* when height is NULL */
void zui_list_set_count(zui_node *list, int count);	/* everything anew */
void zui_list_append(zui_node *list, int more);
void zui_list_prepend(zui_node *list, int more);	/* what is on the screen stays there */
void zui_list_refresh(zui_node *list, int row);
void zui_list_allow_more(zui_node *list);	/* ask again near the start */
/* show the end of a page and stay there as it grows, until the finger moves it */
void zui_page_scroll_end(zui_node *page, bool glide);
bool zui_page_at_end(zui_node *page, double within);

/* ---- tabs and the bottom bar ---- */
/* Tabs under the page's title; they swipe. A tab is a column with its own scroll
 * place. The handler of the tabs gets ZUI_EVENT_CHANGE when the tab changes. */
zui_node *zui_tabs(zui_node *page);
zui_node *zui_tab(zui_node *tabs, const char *title);
void zui_tabs_select(zui_node *tabs, int index);
int zui_tabs_current(zui_node *tabs);
/* A row fixed at the bottom of a page, over its list; a field in it asks the
 * shell to lift the window with the keyboard. */
zui_node *zui_footer(zui_node *page);
/* a field that grows with its lines to four, then scrolls; Enter is a new line */
void zui_field_set_multiline(zui_node *field, bool multiline);
/* a row across the whole screen on the bar colour: an application bar */
void zui_set_bar(zui_node *row, bool bar);
/* A speech bubble in a list: as wide as what is in it, on the end if `mine`. */
zui_node *zui_bubble(zui_node *parent, bool mine);
/* A page's title as given and smaller, with a line under it (a person's name) */
void zui_page_set_compact(zui_node *page, bool compact);
/* the window is in front of the others */
bool zui_app_active(zui_app *app);

/* ---- changing things ---- */
void zui_set_text(zui_node *node, const char *text);	/* label, title, field text */
const char *zui_get_text(zui_node *node);
void zui_set_detail(zui_node *node, const char *text);	/* an item's value on the right; a switch's line */
void zui_set_subtitle(zui_node *node, const char *text);
void zui_set_value(zui_node *node, double value);	/* slider, progress: 0..1 */
double zui_get_value(zui_node *node);
void zui_set_on(zui_node *node, bool on);
bool zui_get_on(zui_node *node);
void zui_set_enabled(zui_node *node, bool enabled);
void zui_set_visible(zui_node *node, bool visible);
void zui_set_variant(zui_node *node, int variant);	/* enum zui_variant */
void zui_set_flex(zui_node *node, double flex);	/* share of a row's spare width */
/* A small picture beside an item's title: "wifi", "bluetooth", "bell",
 * "moon", "sun", "lock", "volume", "battery", "info", "search", "plus",
 * "check", "close", "more", "back", "chevron", "music", "person". */
void zui_set_icon(zui_node *node, const char *icon);
/* a tint for an item's tile, 0xRRGGBB; 0: the accent */
void zui_set_tint(zui_node *node, uint32_t rgb);
/* What the node calls; free_data(data) runs when the node goes away. */
void zui_set_handler(zui_node *node, zui_cb cb, void *data, zui_free_fn free_data);
/* Why the handler is running (enum zui_event). */
int zui_event(zui_node *node);
void zui_clear(zui_node *node);		/* remove everything in it */
void zui_destroy(zui_node *node);	/* remove it (and all in it) */
zui_app *zui_node_app(zui_node *node);
/* Where the last layout put it, in page points (for tests). */
void zui_node_frame(zui_node *node, double *x, double *y, double *w, double *h);

/* ---- the main loop ---- */
/* ready(fd, data) when fd is readable; ready NULL stops watching it */
void zui_watch_fd(zui_app *app, int fd, void (*ready)(int fd, void *data), void *data);
/* fn(data) after ms, and every ms after that if repeat; returns an id > 0 */
int zui_timer(zui_app *app, int ms, bool repeat, zui_fn fn, void *data);
void zui_timer_cancel(zui_app *app, int id);
/* From any thread: fn(data) on the app's thread, soon. */
void zui_post(zui_app *app, zui_fn fn, void *data);
/* A touch to be felt ("tick", "tap", "click", "heavy"); the shell plays it. */
void zui_haptic(const char *kind);

/* ---- without a display ---- */
/* Draw `page` once into a PNG, `width` x `height` points at `scale` pixels
 * per point, with no compositor (EGL surfaceless). 0 on success. */
int zui_render_png(zui_app *app, zui_node *page, const char *path, int width, int height, int scale);
/* Lay `page` out at `width` x `height` points (no GPU needed) and feed it a
 * touch, for tests: phase 0 down, 1 move, 2 up. */
void zui_layout(zui_app *app, zui_node *page, int width, int height);
void zui_inject_touch(zui_app *app, int phase, double x, double y);
/* typing into the field that has the keyboard: UTF-8, "\b" backspace, "\n" Enter */
void zui_inject_text(zui_app *app, const char *utf8);

#ifdef __cplusplus
}
#endif
#endif
