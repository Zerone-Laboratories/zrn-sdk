// SPDX-License-Identifier: GPL-2.0-or-later
/* libzrnui without a display: layout, touch, callbacks, typing, styles, and
 * one offscreen frame checked pixel by pixel. */
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zrnui.h>

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

static int clicks, changes, submits;
static void on_any(zui_node *n, void *data) {
	switch (zui_event(n)) {
	case ZUI_EVENT_CLICK: clicks++; break;
	case ZUI_EVENT_CHANGE: changes++; break;
	case ZUI_EVENT_SUBMIT: submits++; break;
	}
	if (data) *(int *)data += 1;
}

static void centre(zui_node *n, double *x, double *y) {
	double nx, ny, w, h;
	zui_node_frame(n, &nx, &ny, &w, &h);
	*x = nx + w / 2;
	*y = ny + h / 2;
}

static void tap(zui_app *app, zui_node *n) {
	double x, y;
	centre(n, &x, &y);
	zui_inject_touch(app, 0, x, y);
	zui_inject_touch(app, 2, x, y);
}

static int freed;
static void count_free(void *d) { freed++; }

static void test_layout_and_taps(const char *style) {
	setenv("ZUI_STYLE", style, 1);
	zui_app *app = zui_app_new("zrn-test", "Test");
	unsetenv("ZUI_STYLE");
	CHECK(strcmp(zui_app_style(app), style) == 0, "style %s", style);
	zui_node *page = zui_page(app, "Test");
	zui_node *text = zui_text(page, "A long line of text that has to wrap at least once on a phone screen, "
		"because it is much wider than three hundred and sixty points.");
	zui_node *card = zui_section(page, "Group");
	int item_hits = 0;
	zui_node *item = zui_item(card, "Wi-Fi", "Connected", on_any, &item_hits);
	zui_node *still = zui_item(card, "Model", NULL, NULL, NULL);
	zui_node *sw = zui_switch(card, "Bluetooth", false, on_any, NULL);
	zui_node *button = zui_button(page, "Go", on_any, NULL);
	zui_layout(app, page, 360, 740);
	double x, y, w, h, ty, th;
	zui_node_frame(text, NULL, &ty, &w, &th);
	CHECK(th > 30, "%s: text wrapped to more than one line (h %.1f)", style, th);
	CHECK(w <= 360, "%s: text fits the width", style);
	zui_node_frame(item, &x, &y, &w, &h);
	CHECK(y > ty + th, "%s: things go down the page in order", style);
	CHECK(h >= 44, "%s: rows are tall enough for a finger (%.1f)", style, h);
	clicks = changes = submits = 0;
	tap(app, item);
	CHECK(item_hits == 1 && clicks == 1, "%s: a tap on an item calls it once", style);
	tap(app, still);
	CHECK(clicks == 1, "%s: an item with no callback is not a button", style);
	tap(app, sw);
	CHECK(zui_get_on(sw) && changes == 1, "%s: a tap turns a switch on and reports it", style);
	tap(app, button);
	CHECK(clicks == 2, "%s: buttons", style);
	/* a drag is a scroll, not a tap */
	double bx, by;
	centre(button, &bx, &by);
	zui_inject_touch(app, 0, bx, by);
	zui_inject_touch(app, 1, bx, by - 40);
	zui_inject_touch(app, 2, bx, by - 40);
	CHECK(clicks == 2, "%s: a finger that moved does not tap", style);
	/* freeing data with the node */
	freed = 0;
	zui_set_handler(button, on_any, NULL, count_free);
	zui_destroy(button);
	CHECK(freed == 1, "%s: a node's data is freed with it", style);
	zui_app_free(app);
}

static void test_slider_field_pages(void) {
	zui_app *app = zui_app_new("zrn-test", "Test");
	zui_node *page = zui_page(app, "One");
	zui_node *slider = zui_slider(zui_card(page), "Volume", 0.2, on_any, NULL);
	zui_node *field = zui_field(page, "Name", on_any, NULL);
	zui_node *two = zui_page(app, "Two");
	zui_layout(app, page, 360, 740);
	double x, y, w, h;
	zui_node_frame(slider, &x, &y, &w, &h);
	changes = submits = 0;
	zui_inject_touch(app, 0, x + w * 0.75, y + h / 2);
	zui_inject_touch(app, 1, x + w * 0.9, y + h / 2);
	zui_inject_touch(app, 2, x + w * 0.9, y + h / 2);
	CHECK(zui_get_value(slider) > 0.8, "a slider follows the finger (%.2f)", zui_get_value(slider));
	CHECK(changes >= 2 && submits == 1, "a slider reports while moving and once on lifting (%d, %d)", changes, submits);
	tap(app, field);
	changes = submits = 0;
	zui_inject_text(app, "Hé");
	zui_inject_text(app, "\b");
	CHECK(strcmp(zui_get_text(field), "H") == 0, "backspace removes a whole UTF-8 character (%s)", zui_get_text(field));
	zui_inject_text(app, "ey");
	CHECK(strcmp(zui_get_text(field), "Hey") == 0, "typing (%s)", zui_get_text(field));
	zui_inject_text(app, "\n");
	CHECK(changes == 3 && submits == 1, "a field reports edits and Enter (%d, %d)", changes, submits);
	zui_push(two);
	CHECK(zui_current_page(app) == two, "push goes to the page");
	zui_back(app);
	CHECK(zui_current_page(app) == page, "back returns");
	zui_app_free(app);
}

static int read_png(const char *path, int *w, int *h, unsigned char **rgba) {
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	if (setjmp(png_jmpbuf(png))) { fclose(f); return -1; }
	png_init_io(png, f);
	png_read_info(png, info);
	*w = png_get_image_width(png, info);
	*h = png_get_image_height(png, info);
	png_set_expand(png);
	png_read_update_info(png, info);
	*rgba = malloc((size_t)*w * *h * 4);
	for (int y = 0; y < *h; y++) png_read_row(png, *rgba + (size_t)y * *w * 4, NULL);
	png_destroy_read_struct(&png, &info, NULL);
	fclose(f);
	return 0;
}

static void test_offscreen(const char *dir) {
	zui_app *app = zui_app_new("zrn-test", "Test");
	zui_app_set_accent(app, 0x0050ef);
	zui_node *page = zui_page(app, "Pixels");
	zui_node *button = zui_button(page, "Primary", NULL, NULL);
	char path[512];
	snprintf(path, sizeof(path), "%s/test-offscreen.png", dir);
	int rc = zui_render_png(app, page, path, 360, 740, 2);
	CHECK(rc == 0, "offscreen render (needs Mesa EGL)");
	int w, h;
	unsigned char *px = NULL;
	if (rc == 0 && read_png(path, &w, &h, &px) == 0) {
		CHECK(w == 720 && h == 1480, "size %dx%d", w, h);
		double x, y, bw, bh;
		zui_node_frame(button, &x, &y, &bw, &bh);
		/* left part of the button, clear of the label: the accent */
		unsigned char *p = px + ((size_t)(int)((y + bh / 2) * 2) * w + (int)((x + 20) * 2)) * 4;
		CHECK(abs(p[0] - 0x00) < 6 && abs(p[1] - 0x50) < 6 && abs(p[2] - 0xef) < 6,
			"the button is the accent (%02x%02x%02x)", p[0], p[1], p[2]);
		unsigned char *bg = px + ((size_t)(h - 10) * w + 10) * 4;
		CHECK(bg[0] < 3 && bg[1] < 3 && bg[2] < 3,
			"the page is black (%02x%02x%02x)", bg[0], bg[1], bg[2]);
		/* the title is light text somewhere in its row */
		int bright = 0;
		for (int yy = 40; yy < 190; yy++)
			for (int xx = 36; xx < 300; xx++) if (px[((size_t)yy * w + xx) * 4] > 200) bright++;
		CHECK(bright > 400, "the title is drawn (%d light pixels)", bright);
		free(px);
	}
	zui_app_free(app);
}

static int binds;
static double row_guess(int row, void *d) { return 50; }
static void row_bind(int row, zui_node *n, void *d) {
	char t[32];
	snprintf(t, sizeof(t), "row %d", row);
	binds++;
	zui_node *b = zui_bubble(n, row % 2);
	zui_text(b, t);
}

static void test_list_tabs_image(const char *dir) {
	zui_app *app = zui_app_new("zrn-test-list", "List");
	zui_node *page = zui_page(app, "Chats");
	zui_node *tabs = zui_tabs(page);
	zui_node *one = zui_tab(tabs, "One");
	zui_tab(tabs, "Two");
	zui_node *list = zui_list(one, 5000, row_guess, row_bind, NULL);
	zui_node *foot = zui_footer(page);
	zui_field(foot, "type", NULL, NULL);
	static const uint8_t px[16] = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255 };
	zui_pixmap *pix = zui_pixmap_rgba(2, 2, px);
	zui_image(zui_tab(tabs, "Three"), pix, 100, 100);
	CHECK(zui_tabs_current(tabs) == 0, "first tab");
	zui_layout(app, page, 360, 740);
	if (!getenv("ZUI_TEST_NO_GPU")) {
		char path[512];
		snprintf(path, sizeof(path), "%s/list.png", dir);
		CHECK(zui_render_png(app, page, path, 360, 740, 2) == 0, "list renders");
		CHECK(binds > 5 && binds < 60, "only the rows near the screen are made (%d)", binds);
	}
	int before = binds;
	zui_list_prepend(list, 100);
	CHECK(binds == before, "a prepend makes nothing again");
	zui_tabs_select(tabs, 1);
	CHECK(zui_tabs_current(tabs) == 1, "tab selected");
	zui_pixmap_free(pix);
	zui_app_free(app);
}


static int chosen = -9, held;
static void on_choose(zui_app *app, int index, void *data) { chosen = index; }
static void on_hold(zui_node *n, void *data) { if (zui_event(n) == ZUI_EVENT_HOLD) held++; }
static void tap_at(zui_app *app, double x, double y) {
	usleep(300000);	/* a closing overlay lets go of the touch after its 260 ms */
	zui_inject_touch(app, 0, x, y);
	zui_inject_touch(app, 2, x, y);
}

/* the controls Windows Phone 8.1 had: check and radio, pickers, overlays, tiles, hub, hold */
static void test_widgets(const char *style, const char *dir) {
	setenv("ZUI_STYLE", style, 1);
	zui_app *app = zui_app_new("zrn-test-widgets", "Widgets");
	unsetenv("ZUI_STYLE");
	zui_node *page = zui_page(app, "Widgets");
	changes = 0;
	held = 0;
	zui_node *chk = zui_check(page, "Remember", false, on_any, NULL);
	zui_node *r1 = zui_radio(page, "One", true, on_any, NULL);
	zui_node *r2 = zui_radio(page, "Two", false, on_any, NULL);
	static const char *const opts[] = { "English", "Deutsch", "Fran\xc3\xa7" "ais" };
	zui_node *pick = zui_picker(page, "Language", opts, 3, 0, on_any, NULL);
	zui_node *date = zui_date_picker(page, "Birthday", 1990, 11, 5, on_any, NULL);
	zui_node *tm = zui_time_picker(page, "Alarm", 7, 30, on_any, NULL);
	zui_node *pw = zui_field(page, "Password", NULL, NULL);
	zui_field_set_password(pw, true);
	zui_node *hold = zui_item(page, "Hold me", NULL, NULL, NULL);
	zui_set_hold(hold, on_hold, NULL, NULL);
	zui_layout(app, page, 360, 740);

	tap(app, chk);
	CHECK(zui_get_on(chk) && changes == 1, "%s: a check box turns on and says so", style);
	changes = 0;
	tap(app, r2);
	CHECK(zui_get_on(r2) && !zui_get_on(r1), "%s: one radio on, the other off", style);
	CHECK(changes == 2, "%s: both radios are told (%d)", style, changes);
	tap(app, r2);
	CHECK(zui_get_on(r2), "%s: a chosen radio stays chosen", style);

	/* the list picker opens in place and gives its choice */
	changes = 0;
	tap(app, pick);
	zui_layout(app, page, 360, 740);
	double x, y, w, h;
	zui_node_frame(pick, &x, &y, &w, &h);
	CHECK(h > 40 + 24 + 100, "%s: an open picker is taller (%.0f)", style, h);
	tap_at(app, 100, y + 24 + 4 + 40 * 2 + 20);
	CHECK(zui_picker_selected(pick) == 2 && changes == 1, "%s: the third option chosen (%d)", style, zui_picker_selected(pick));
	zui_layout(app, page, 360, 740);
	zui_node_frame(pick, &x, &y, &w, &h);
	CHECK(h < 100, "%s: and the picker shut (%.0f)", style, h);

	/* a date: three columns, a drag on the month, the tick */
	changes = 0;
	tap(app, date);
	CHECK(zui_overlay_open(app), "%s: the date picker is up", style);
	zui_inject_touch(app, 0, 80, 400);
	for (int i = 1; i <= 10; i++) zui_inject_touch(app, 1, 80, 400 - i * 12);
	zui_inject_touch(app, 2, 80, 280);
	tap_at(app, 90, 700);
	int yy, mm, dd;
	zui_date_picker_get(date, &yy, &mm, &dd);
	CHECK(yy == 1990 && mm == 1 && dd == 5 && changes == 1, "%s: month dragged to January: %d-%d-%d", style, yy, mm, dd);
	/* the cross changes nothing */
	changes = 0;
	tap(app, tm);
	tap_at(app, 270, 700);
	int hh, mi;
	zui_time_picker_get(tm, &hh, &mi);
	CHECK(hh == 7 && mi == 30 && changes == 0 && !zui_overlay_open(app), "%s: cancel leaves the time as it was", style);
	zui_time_picker_set(tm, 25, 61);
	zui_time_picker_get(tm, &hh, &mi);
	CHECK(hh == 1 && mi == 1, "%s: time wraps (%d:%d)", style, hh, mi);
	zui_date_picker_set(date, 2023, 2, 31);
	zui_date_picker_get(date, &yy, &mm, &dd);
	CHECK(dd == 28, "%s: 31 February is the 28th (%d)", style, dd);

	/* dialog and menu give the index; Back closes them without choosing */
	static const char *const two[] = { "delete", "keep" };
	chosen = -9;
	usleep(300000);
	zui_dialog(app, "Delete?", "It cannot be undone.", two, 2, on_choose, NULL);
	CHECK(zui_overlay_open(app), "%s: a dialog is up", style);
	zui_back(app);
	CHECK(!zui_overlay_open(app) && chosen == -9, "%s: Back closes a dialog and chooses nothing", style);
	usleep(300000);
	zui_dialog(app, "Delete?", "It cannot be undone.", two, 2, on_choose, NULL);
	tap_at(app, 90, 80);	/* outside the panel's buttons: nothing */
	CHECK(zui_overlay_open(app) && chosen == -9, "%s: a dialog is modal", style);
	static const char *const items[] = { "copy", "pin", "delete" };
	usleep(300000);
	zui_layout(app, page, 360, 740);
	zui_menu(app, NULL, items, 3, on_choose, NULL);	/* at the bottom: 166 points tall, 8 above the edge */
	tap_at(app, 100, 740 - 8 - 166 + 8 + 50 + 25);
	CHECK(chosen == 1, "%s: the second menu item (%d)", style, chosen);
	const char *letters[] = { "#", "a", "b", "c" };
	bool live[] = { true, true, false, true };
	usleep(300000);
	zui_jump_list(app, letters, live, 4, on_choose, NULL);
	chosen = -9;
	tap_at(app, 16 + 75 * 2 + 9 * 2 + 30, 108 + 9 + 30);	/* the third cell, not live */
	CHECK(chosen == -9 && zui_overlay_open(app), "%s: a letter with nothing under it does not answer", style);
	tap_at(app, 16 + 75 * 3 + 9 * 3 + 30, 108 + 9 + 30);
	CHECK(chosen == 3, "%s: the fourth cell (%d)", style, chosen);

	/* a hold of half a second is a hold, and the tap after it is not given */
	usleep(300000);
	zui_node_frame(hold, &x, &y, &w, &h);
	zui_inject_touch(app, 0, x + 40, y + 20);
	for (int i = 0; i < 40; i++) zui_inject_touch(app, 1, x + 40, y + 20);
	zui_inject_touch(app, 2, x + 40, y + 20);
	CHECK(held == 1, "%s: held once (%d)", style, held);

	/* the password box keeps its text and shows none of it */
	tap(app, pw);
	zui_inject_text(app, "abc");
	CHECK(strcmp(zui_get_text(pw), "abc") == 0, "%s: a password box still holds its text", style);

	if (!getenv("ZUI_TEST_NO_GPU")) {
		char path[512];
		snprintf(path, sizeof(path), "%s/widgets-%s.png", dir, style);
		CHECK(zui_render_png(app, page, path, 360, 740, 2) == 0, "%s: widgets render", style);
	}
	zui_app_free(app);
}

/* tiles lay out two to a row, a wide one alone; a hub slides by sections; scroll_to */
static void test_tiles_hub(const char *dir) {
	zui_app *app = zui_app_new("zrn-test-hub", "Hub");
	zui_node *tp = zui_page(app, "Tiles");
	zui_node *g = zui_tiles(tp);
	zui_node *a = zui_tile(g, "A", "info", false, on_any, NULL);
	zui_node *b = zui_tile(g, "B", "info", false, NULL, NULL);
	zui_node *c = zui_tile(g, "C", "info", true, NULL, NULL);
	zui_node *d = zui_tile(g, "D", "info", false, NULL, NULL);
	zui_tile_set_live(a, "back", 0);
	zui_layout(app, tp, 360, 740);
	double ax, ay, aw, ah, bx, by, bw, bh, cx, cy, cw, ch, dx, dy, dw, dh;
	zui_node_frame(a, &ax, &ay, &aw, &ah);
	zui_node_frame(b, &bx, &by, &bw, &bh);
	zui_node_frame(c, &cx, &cy, &cw, &ch);
	zui_node_frame(d, &dx, &dy, &dw, &dh);
	CHECK(fabs(ay - by) < 0.5 && bx > ax + aw - 0.5, "two squares share a row");
	CHECK(fabs(aw - ah) < 0.5, "a tile is square (%.1f x %.1f)", aw, ah);
	CHECK(cy > ay + ah && cw > aw * 1.9, "a wide tile has a row to itself (w %.0f)", cw);
	CHECK(dy > cy + ch - 0.5 && fabs(dx - ax) < 0.5, "the next square starts a new row");
	clicks = 0;
	tap(app, a);
	CHECK(clicks == 1, "a tile can be tapped");

	zui_node *hp = zui_page(app, "Hub");
	zui_node *hub = zui_hub(hp);
	zui_node *s1 = zui_hub_section(hub, "one");
	zui_hub_section(hub, "two");
	zui_node *s3 = zui_hub_section(hub, "three");
	zui_node *btn = zui_button(s3, "Deep", on_any, NULL);
	zui_text(s1, "hello");
	zui_layout(app, hp, 360, 740);
	CHECK(zui_hub_current(hub) == 0, "the hub starts at its first section");
	double bx0, by0, bw0, bh0;
	zui_node_frame(btn, &bx0, &by0, &bw0, &bh0);
	CHECK(bx0 > 360, "the third section is off to the right (%.0f)", bx0);
	zui_app_free(app);
	app = zui_app_new("zrn-test-hub2", "Hub");
	hp = zui_page(app, "Hub");
	hub = zui_hub(hp);
	zui_hub_section(hub, "one");
	zui_hub_section(hub, "two");
	s3 = zui_hub_section(hub, "three");
	btn = zui_button(s3, "Deep", on_any, NULL);
	zui_layout(app, hp, 360, 740);
	if (!getenv("ZUI_TEST_NO_GPU")) {
		char path[512];
		snprintf(path, sizeof(path), "%s/hub.png", dir);
		CHECK(zui_render_png(app, hp, path, 360, 740, 2) == 0, "a hub renders");
	}
	/* a swipe left past the 50 points goes to the next section */
	zui_inject_touch(app, 0, 300, 400);
	for (int i = 1; i <= 8; i++) zui_inject_touch(app, 1, 300 - i * 12, 400);
	zui_inject_touch(app, 2, 204, 400);
	usleep(600000);
	CHECK(zui_hub_current(hub) == 1, "a swipe slides the hub to the second section (%d)", zui_hub_current(hub));
	zui_hub_show(hub, 2);
	usleep(600000);
	clicks = 0;
	zui_node_frame(btn, &bx0, &by0, &bw0, &bh0);
	tap_at(app, bx0 + bw0 / 2 - 2 * (360 - 16 - 44 + 12), by0 + bh0 / 2);	/* where the finger finds it, slid */
	CHECK(clicks == 1, "a button in a section is tapped where it now is (x %.0f)", bx0);
	zui_app_free(app);

	app = zui_app_new("zrn-test-scroll", "Scroll");
	zui_node *sp = zui_page(app, "Scroll");
	zui_node *last = NULL;
	for (int i = 0; i < 40; i++) last = zui_item(sp, "row", NULL, NULL, NULL);
	zui_layout(app, sp, 360, 740);
	double lx, ly, lw, lh;
	zui_node_frame(last, &lx, &ly, &lw, &lh);
	zui_page_scroll_to(sp, ly, false);
	zui_node_frame(last, &lx, &ly, &lw, &lh);
	CHECK(ly > 740, "scroll_to is bounded by the page's end");
	zui_app_free(app);
}

int main(int argc, char **argv) {
	setenv("ZUI_NO_OPEN", "1", 1);	/* one frame at a time: not the opening turn */
	const char *dir = argc > 1 ? argv[1] : "/tmp";
	test_layout_and_taps("dotmatrix");
	test_layout_and_taps("metro");
	test_slider_field_pages();
	test_list_tabs_image(dir);
	test_widgets("metro", dir);
	test_widgets("dotmatrix", dir);
	test_tiles_hub(dir);
	if (!getenv("ZUI_TEST_NO_GPU")) test_offscreen(dir);
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
