// SPDX-License-Identifier: GPL-2.0-or-later
/* libzrnui without a display: layout, touch, callbacks, typing, styles, and
 * one offscreen frame checked pixel by pixel. */
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	zui_app *app = zui_app_new("zrn-test", "Test");
	zui_app_set_style(app, style);
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
	zui_app_set_style(app, "cool");
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
	zui_app_set_style(app, "cool");
	zui_app_set_theme(app, "dark");
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
		CHECK(abs(bg[0] - 0x0e) < 3 && abs(bg[1] - 0x0f) < 3 && abs(bg[2] - 0x12) < 3,
			"the page is zrnCoolStyle's dark background (%02x%02x%02x)", bg[0], bg[1], bg[2]);
		/* the title is light text somewhere in its row */
		int bright = 0;
		for (int yy = 112; yy < 190; yy++)
			for (int xx = 36; xx < 300; xx++) if (px[((size_t)yy * w + xx) * 4] > 200) bright++;
		CHECK(bright > 400, "the large title is drawn (%d light pixels)", bright);
		free(px);
	}
	zui_app_free(app);
}

int main(int argc, char **argv) {
	const char *dir = argc > 1 ? argv[1] : "/tmp";
	test_layout_and_taps("cool");
	test_layout_and_taps("metro");
	test_slider_field_pages();
	if (!getenv("ZUI_TEST_NO_GPU")) test_offscreen(dir);
	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
