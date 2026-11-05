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
	ZUI_EVENT_HOLD,		/* zui_set_hold: the finger stayed down (half a second) */
};

enum zui_variant {
	ZUI_PRIMARY = 0,	/* a button filled with the accent colour */
	ZUI_TONAL,		/* a quieter button: the accent, faint */
	ZUI_PLAIN,		/* text only */
	ZUI_DANGER,		/* a button that deletes or resets */
	ZUI_LINK,		/* a button as a hyperlink: the accent colour, no frame */
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
/* The page in front becomes the first one: back closes the app (a splash that was replaced). */
void zui_page_set_root(zui_node *page);
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
/* An item as the letter that opens a group in the app list: an outline of the accent
 * with the letter, lower case, in its corner (an alphabetical list's divider). */
void zui_set_group_letter(zui_node *node, const char *letter);
/* An item with a picture or monogram drawn as a row of the app list: a 45 pt square,
 * the title 22 pt beside it, 55 pt from one row to the next. */
void zui_set_applist_row(zui_node *node, bool on);
/* the space between the things in a row or column, in points (default: the style's) */
void zui_set_gap(zui_node *row, double gap);
/* Text in the system's quiet colour (the grey of the titles that are not in front) and a
 * light weight, whatever its size: a timeline's day headings. */
void zui_set_quiet(zui_node *text, bool quiet);
/* A page without its title and tab titles, its content over the whole window (above the
 * footer): a photo viewer. The page does not scroll. */
void zui_page_set_fullscreen(zui_node *page, bool on);
/* An image that fills the room there is, whole and centred (the larger side touches the
 * edges): on a fullscreen page the window above the footer. */
void zui_set_image_fit(zui_node *image, bool fit);
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
/* an application bar without WP's three dots (and so without the words under its buttons) */
void zui_set_bar_dots(zui_node *row, bool dots);
/* A speech bubble in a list: as wide as what is in it, on the end if `mine`. */
zui_node *zui_bubble(zui_node *parent, bool mine);
/* A page's title as given and smaller, with a line under it (a person's name) */
void zui_page_set_compact(zui_node *page, bool compact);
/* Windows Phone's Swivel: in (from -45 degrees, 300 ms) or out (to 60, 200 ms), after delay_ms */
void zui_swivel(zui_node *node, bool in, int delay_ms);
/* Rows of lists are bound on a thread of their own: hold this while changing what a
 * bind function reads. It may be held again by the same thread. */
void zui_list_lock(zui_app *app, bool lock);
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

/* ---- more of what Windows Phone 8.1 had ---- */
/* Each of these looks as ZrnMetroCore draws it in Metro, and the same in
 * DotMatrix. None of them is meant to be put inside a zui_list row: rows are
 * painted once, so what moves in one stays still until zui_list_refresh. */

/* CheckBox and RadioButton. zui_get_on / zui_set_on; the handler gets
 * ZUI_EVENT_CHANGE. The radios that share a parent are one group: turning one
 * on turns the others off (and each of them is told). */
zui_node *zui_check(zui_node *parent, const char *title, bool on, zui_cb on_change, void *data);
zui_node *zui_radio(zui_node *parent, const char *title, bool on, zui_cb on_change, void *data);

/* PasswordBox: a field that shows dots. zui_get_text still gives the text. */
void zui_field_set_password(zui_node *field, bool password);
/* AutoSuggestBox: while the field has the keyboard and something is typed,
 * the items that contain it (any case) hang under it, four at most; a tap
 * puts one in the field and gives ZUI_EVENT_SUBMIT. */
void zui_field_set_suggestions(zui_node *field, const char *const *items, int count);

/* ProgressRing: five dots going round, for something of no known length.
 * zui_set_visible(ring, false) stops it. */
zui_node *zui_ring(zui_node *parent);

/* ListPicker / ComboBox, in place: a caption over a box with the chosen
 * option; tapped, the box opens to show every option, the chosen one in the
 * accent. The handler gets ZUI_EVENT_CHANGE. */
zui_node *zui_picker(zui_node *parent, const char *caption, const char *const *options, int count,
		int selected, zui_cb on_change, void *data);
int zui_picker_selected(zui_node *picker);
void zui_picker_select(zui_node *picker, int index);

/* DatePicker and TimePicker: a row with the value in the accent; tapped, the
 * whole screen goes to three looping columns (day month year; hour minute),
 * and a tick and a cross under them. month 1..12, day 1..31, hour 0..23.
 * The handler gets ZUI_EVENT_CHANGE when the tick is tapped. */
zui_node *zui_date_picker(zui_node *parent, const char *title, int year, int month, int day,
		zui_cb on_change, void *data);
void zui_date_picker_get(zui_node *picker, int *year, int *month, int *day);
void zui_date_picker_set(zui_node *picker, int year, int month, int day);
zui_node *zui_time_picker(zui_node *parent, const char *title, int hour, int minute,
		zui_cb on_change, void *data);
void zui_time_picker_get(zui_node *picker, int *hour, int *minute);
void zui_time_picker_set(zui_node *picker, int hour, int minute);

/* Tiles: a grid of squares (two to a row, a wide one a row of its own) in the
 * accent colour or the tile's tint, an icon in the middle and the title under
 * it; they lean when pressed, as everything does. zui_set_badge puts a count
 * in a corner. */
zui_node *zui_tiles(zui_node *parent);
zui_node *zui_tile(zui_node *tiles, const char *title, const char *icon, bool wide, zui_cb on_tap, void *data);
/* A live tile (HubTile): every period_ms it turns over to show `back` and
 * turns back again. period_ms 0 stops it. */
void zui_tile_set_live(zui_node *tile, const char *back, int period_ms);

/* Hub (Panorama): sections side by side, wider than the screen, the next one
 * showing at the edge. A swipe slides to the next; the page's title moves more
 * slowly than the sections and a picture behind them slower still. A page
 * has either tabs or a hub, not both. A section is a column: put anything in it. */
zui_node *zui_hub(zui_node *page);
zui_node *zui_hub_section(zui_node *hub, const char *title);
void zui_hub_set_background(zui_node *hub, zui_pixmap *pix);
void zui_hub_show(zui_node *hub, int section);
int zui_hub_current(zui_node *hub);

/* Press and hold: the handler of zui_set_hold gets ZUI_EVENT_HOLD when the
 * finger has stayed on the node for half a second without moving; the tap
 * that would have followed is not given. */
void zui_set_hold(zui_node *node, zui_cb on_hold, void *data, zui_free_fn free_data);
/* Scroll a page so `y` (page points, as zui_node_frame gives them) is at the top of its content. */
void zui_page_scroll_to(zui_node *page, double y, bool glide);

/* ---- over the page ---- */
/* One at a time, over the page, header and bar and all; Back closes it, and
 * while it is open it takes every touch. index is what was chosen. */
typedef void (*zui_choice_fn)(zui_app *app, int index, void *data);
/* A menu (ContextMenu, MenuFlyout): a panel across the screen under `anchor`
 * (or at the bottom if NULL) with the items in it; a tap outside closes it. */
void zui_menu(zui_app *app, zui_node *anchor, const char *const *items, int count,
		zui_choice_fn on_choose, void *data);
/* A MessageDialog: a panel with a title, some words and up to three buttons
 * in a row; on_choose gets the button's index. */
void zui_dialog(zui_app *app, const char *title, const char *message, const char *const *buttons,
		int count, zui_choice_fn on_choose, void *data);
/* A jump list (SemanticZoom's zoomed-out view): a grid of tiles that turn in
 * one after another, four to a row; live[i] false greys one out (NULL: all
 * live). A tap on a live tile gives its index. */
void zui_jump_list(zui_app *app, const char *const *labels, const bool *live, int count,
		zui_choice_fn on_choose, void *data);
bool zui_overlay_open(zui_app *app);
void zui_overlay_close(zui_app *app);

#ifdef __cplusplus
}
#endif
#endif
