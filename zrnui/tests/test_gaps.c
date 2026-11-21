/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * The additions for apps such as a messenger: the theme callback, rows painted once into a texture (CacheMode), colour
 * emoji, Holding with a context flyout, the input pane, the continuum and slide-up transitions, scroll anchoring.
 */

#include "test.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/font.h"

#include <stdlib.h>

/* ---- G1: Zroot_on_theme_changed ---- */

static int g_theme_calls;
static Zid g_theme_sender;

static void on_theme(const Zid sender, void* const user_data) {
	g_theme_calls++;
	g_theme_sender = sender;
	(void)user_data;
}

static void test_theme_changed(void) {
	const Zid root = zrn_test_root(200, 200);
	g_theme_calls = 0;
	Zroot_on_theme_changed(root, on_theme, nullptr);
	Zroot_set_theme(root, Z_THEME_LIGHT);
	CHECK(g_theme_calls == 1);
	CHECK(g_theme_sender == root);
	Zroot_set_theme(root, Z_THEME_LIGHT);   /* the same theme again: nothing changed */
	CHECK(g_theme_calls == 1);
	Zroot_set_accent(root, Z_ACCENT_LIME);
	CHECK(g_theme_calls == 2);
	Zroot_set_accent(root, Z_ACCENT_LIME);
	CHECK(g_theme_calls == 2);
	Zroot_on_theme_changed(root, nullptr, nullptr);
	Zroot_set_theme(root, Z_THEME_DARK);
	CHECK(g_theme_calls == 2);
	Zroot_destroy(root);
}

/* ---- G6: CacheMode ---- */

static Zid g_rows[12];
static Zid g_texts[12];

static Zid build_list(const Zid root, const bool b_cached) {
	const Zid stack = Zstack_panel_create(Z_VERTICAL);
	Zroot_set_visual(root, stack);
	for (int i = 0; i < 12; i++) {
		char text[64];
		snprintf(text, sizeof text, "row %d: a message in a bubble", i);
		const Zid row = Zpanel_add(stack, Zborder_create());
		Zelement_set_height(row, 56);
		Zelement_set_margin(row, Zthickness(12, 4, 12, 4));
		Zborder_set_background(row, Zcolor_rgba(0.1f + 0.05f * (float)i, 0.4f, 0.6f, 0.8f));
		Zborder_set_corner_radius(row, (ZCornerRadius){ 6, 6, 6, 6 });
		g_texts[i] = Ztext_block_create(text);
		Zborder_set_child(row, g_texts[i]);
		if (b_cached) Zelement_set_cache_mode(row, Z_CACHE_BITMAP);
		g_rows[i] = row;
	}
	return stack;
}

static double mean_difference(const ZFrame a, const ZFrame b) {
	double sum = 0;
	for (int y = 0; y < a.height; y++) {
		for (int x = 0; x < a.width; x++) {
			const uint8_t* const p = a.pixels + (size_t)y * (size_t)a.stride + (size_t)x * 4;
			const uint8_t* const q = b.pixels + (size_t)y * (size_t)b.stride + (size_t)x * 4;
			sum += (abs(p[0] - q[0]) + abs(p[1] - q[1]) + abs(p[2] - q[2])) / 3.0;
		}
	}
	return sum / (double)(a.width * a.height);
}

/* a few new bitmaps are made per frame (host.c): frames until none is left to make */
static bool render_settled(const Zid root, ZFrame* const f) {
	const bool b_ok = Zroot_render(root, f);
	for (int i = 0; i < 16 && b_ok; i++) {
		const uint32_t before = zrn_cache_paints;
		if (!Zroot_render(root, f) || zrn_cache_paints == before) break;
	}
	return b_ok;
}

static void test_cache_mode(void) {
	const Zid plain = zrn_test_root(360, 800);
	const Zid cached = zrn_test_root(360, 800);
	Zroot_set_renderer(plain, Z_RENDERER_GPU);
	Zroot_set_renderer(cached, Z_RENDERER_GPU);
	build_list(plain, false);
	build_list(cached, true);
	CHECK(Zelement_get_cache_mode(g_rows[0]) == Z_CACHE_BITMAP);
	ZFrame fp, fc;
	if (!Zroot_render(plain, &fp)) {
		printf("  no GPU renderer: the cache check is skipped\n");
		Zroot_destroy(plain);
		Zroot_destroy(cached);
		return;
	}
	/* the frame of `plain` lives in its root: copy it before the other root renders */
	const size_t size = (size_t)fp.stride * (size_t)fp.height;
	uint8_t* const copy = malloc(size);
	memcpy(copy, fp.pixels, size);
	fp.pixels = copy;
	const uint32_t before = zrn_cache_paints;
	CHECK(render_settled(cached, &fc));
	CHECK(zrn_cache_paints - before == 12);
	const double mean = mean_difference(fp, fc);
	printf("  cached against painted: mean difference %.3f / 255\n", mean);
	CHECK(mean < 1.0);
	char path[512];
	snprintf(path, sizeof path, "%s/gaps_cached_rows.png", ZRNUI_TEST_OUTPUT_DIR);
	CHECK(test_save_png(cached, path));

	/* moved, faded: the same textures, nothing painted again */
	Zelement_set_translate(g_rows[3], 40, 0);
	Zelement_set_opacity(g_rows[4], 0.5f);
	CHECK(render_settled(cached, &fc));
	CHECK(zrn_cache_paints - before == 12);

	/* the text of one row changed: that row alone is painted again */
	Ztext_block_set_text(g_texts[5], "row 5: edited");
	CHECK(render_settled(cached, &fc));
	CHECK(zrn_cache_paints - before == 13);

	/* a new accent: every row */
	Zroot_set_accent(cached, Z_ACCENT_RED);
	CHECK(render_settled(cached, &fc));
	CHECK(zrn_cache_paints - before == 25);

	/* off again: the texture goes, the row is painted directly */
	Zelement_set_cache_mode(g_rows[0], Z_CACHE_NONE);
	CHECK(render_settled(cached, &fc));
	CHECK(zrn_cache_paints - before == 25);
	free(copy);
	Zroot_destroy(plain);
	Zroot_destroy(cached);
}

/* ---- G7: colour emoji ---- */

static int colored_pixels(const ZFrame f) {
	int n = 0;
	for (int y = 0; y < f.height; y++) {
		for (int x = 0; x < f.width; x++) {
			const uint8_t* const px = f.pixels + (size_t)y * (size_t)f.stride + (size_t)x * 4;
			const int hi = px[0] > px[1] ? (px[0] > px[2] ? px[0] : px[2]) : (px[1] > px[2] ? px[1] : px[2]);
			const int lo = px[0] < px[1] ? (px[0] < px[2] ? px[0] : px[2]) : (px[1] < px[2] ? px[1] : px[2]);
			if (hi - lo > 80) n++;
		}
	}
	return n;
}

static void test_color_emoji(const ZRenderer renderer) {
	const Zid root = zrn_test_root(240, 60);
	Zroot_set_renderer(root, renderer);
	const Zid text = Ztext_block_create("hi \xF0\x9F\x98\x80");   /* U+1F600 */
	Zroot_set_visual(root, text);
	ZFrame f;
	if (!Zroot_render(root, &f)) {
		printf("  no renderer: skipped\n");
		Zroot_destroy(root);
		return;
	}
	const int with_emoji = colored_pixels(f);
	printf("  %d coloured pixels\n", with_emoji);
	if (!zrn_emoji_face()) {
		printf("  no colour emoji font on this system: only the width is checked\n");
	} else {
		CHECK(with_emoji > 50);
		char path[512];
		snprintf(path, sizeof path, "%s/gaps_emoji_%s.png", ZRNUI_TEST_OUTPUT_DIR, renderer == Z_RENDERER_GPU ? "gpu" : "software");
		CHECK(test_save_png(root, path));
	}
	/* a skin tone and a variation selector take no room: without shaping, the thumb is drawn alone */
	Ztext_block_set_text(text, "\xF0\x9F\x91\x8D");
	Zroot_update_layout(root);
	const float plain = Zelement_get_desired_size(text).width;
	Ztext_block_set_text(text, "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD\xEF\xB8\x8F");
	Zroot_update_layout(root);
	CHECK_NEAR(Zelement_get_desired_size(text).width, plain);
	CHECK(plain > 10);
	Zroot_destroy(root);
}

static void test_color_emoji_software(void) { test_color_emoji(Z_RENDERER_SOFTWARE); }
static void test_color_emoji_gpu(void) { test_color_emoji(Z_RENDERER_GPU); }

/* ---- G2: Holding and the context flyout ---- */

static int g_clicks, g_holds;
static void on_click(const Zid sender, void* const user_data) { g_clicks++; (void)sender; (void)user_data; }
static void on_hold(const Zid sender, void* const user_data) { g_holds++; (void)sender; (void)user_data; }

static void test_holding(void) {
	const Zid root = zrn_test_root(360, 640);
	const Zid stack = Zstack_panel_create(Z_VERTICAL);
	Zroot_set_visual(root, stack);
	const Zid button = Zpanel_add(stack, Zbutton_create("hold me"));
	Zelement_set_width(button, 200);
	Zelement_set_height(button, 80);
	Zelement_on(button, Z_EVENT_CLICK, on_click, nullptr);
	Zelement_on_holding(button, on_hold, nullptr);
	Zroot_update_layout(root);
	g_clicks = g_holds = 0;

	/* a tap is a click, not a hold */
	Zroot_inject_pointer(root, Z_POINTER_MOVE, 50, 40);
	Zroot_inject_pointer(root, Z_POINTER_DOWN, 50, 40);
	Zroot_tick(root, 100);
	Zroot_inject_pointer(root, Z_POINTER_UP, 50, 40);
	CHECK(g_clicks == 1 && g_holds == 0);

	/* held 500 ms: Holding, and the lift clicks nothing */
	Zroot_inject_pointer(root, Z_POINTER_DOWN, 50, 40);
	for (int i = 0; i < 6; i++) Zroot_tick(root, 90);
	CHECK(g_holds == 1);
	Zroot_inject_pointer(root, Z_POINTER_UP, 50, 40);
	CHECK(g_clicks == 1);

	/* a finger that moves more than 9.5 px is no hold */
	Zroot_inject_pointer(root, Z_POINTER_DOWN, 50, 40);
	Zroot_inject_pointer(root, Z_POINTER_MOVE, 62, 40);
	for (int i = 0; i < 8; i++) Zroot_tick(root, 90);
	Zroot_inject_pointer(root, Z_POINTER_UP, 62, 40);
	CHECK(g_holds == 1);
	CHECK(g_clicks == 2);   /* still on the button: a tap */

	/* a context flyout opens at the element after its handler */
	const Zid menu = Zmenu_flyout_create();
	Zmenu_flyout_add_item(menu, Zmenu_flyout_item_create("delete"));
	Zelement_set_context_flyout(button, menu);
	CHECK(Zelement_get_context_flyout(button) == menu);
	Zroot_inject_pointer(root, Z_POINTER_DOWN, 50, 40);
	for (int i = 0; i < 6; i++) Zroot_tick(root, 90);
	CHECK(g_holds == 2);
	CHECK(Zflyout_base_get_is_open(menu));
	Zroot_inject_pointer(root, Z_POINTER_UP, 50, 40);
	CHECK(Zflyout_base_get_is_open(menu));
	CHECK(g_clicks == 2);
	Zflyout_base_hide(menu);
	Zroot_destroy(root);
	Zelement_destroy(menu);
}

/* ---- G5: the input pane ---- */

static void test_input_pane(void) {
	const Zid root = zrn_test_root(360, 640);
	const Zid grid = Zgrid_create();
	Zroot_set_visual(root, grid);
	Zroot_update_layout(root);
	CHECK_NEAR(Zelement_get_bounds(grid).height, 640);
	Zroot_set_input_pane_height(root, 227);
	CHECK_NEAR(Zroot_get_input_pane_height(root), 227);
	Zroot_update_layout(root);
	CHECK_NEAR(Zelement_get_bounds(grid).height, 640 - 227);
	Zroot_set_input_pane_height(root, -5);
	Zroot_update_layout(root);
	CHECK_NEAR(Zelement_get_bounds(grid).height, 640);
	Zroot_destroy(root);
}

/* ---- G3, G4: the continuum and the slide up ---- */

static Zid g_list_row, g_header;

static Zid list_page(const Zid frame, void* const parameter) {
	const Zid page = Zpage_create();
	const Zid stack = Zstack_panel_create(Z_VERTICAL);
	Zpage_set_content(page, stack);
	Zpanel_add(stack, Ztext_block_create("chats"));
	g_list_row = Zpanel_add(stack, Ztext_block_create("a chat"));
	Zpanel_add(stack, Ztext_block_create("another chat"));
	(void)frame; (void)parameter;
	return page;
}

static Zid chat_page(const Zid frame, void* const parameter) {
	const Zid page = Zpage_create();
	const Zid stack = Zstack_panel_create(Z_VERTICAL);
	Zpage_set_content(page, stack);
	g_header = Zpanel_add(stack, Ztext_block_create("A CHAT"));
	Zpanel_add(stack, Ztext_block_create("hello"));
	Zpage_set_continuum_element(page, g_header);
	(void)frame; (void)parameter;
	return page;
}

static void test_transitions(void) {
	const Zid root = zrn_test_root(360, 640);
	Zroot_set_animations(root, Z_ANIMATIONS_ON);
	const Zid frame = Zframe_create();
	Zroot_set_visual(root, frame);
	Zframe_set_transition(frame, Z_TRANSITION_NONE);
	CHECK(Zframe_navigate(frame, list_page, nullptr));
	Zroot_update_layout(root);
	CHECK(!Zframe_is_transitioning(frame));

	/* the continuum: the row flies out to the upper right, the header of the chat drops in */
	Zframe_set_continuum_element(frame, g_list_row);
	CHECK(Zframe_navigate_with_transition(frame, chat_page, nullptr, Z_TRANSITION_CONTINUUM));
	Zroot_tick(root, 75);
	CHECK(Zelement_get_property(g_list_row, Z_PROP_TRANSLATE_X) > 1);
	CHECK(Zelement_get_property(g_list_row, Z_PROP_TRANSLATE_Y) < -1);
	CHECK(Zelement_get_property(g_list_row, Z_PROP_ROTATION_Y) == 0);   /* it flies, it does not turn */
	Zroot_tick(root, 300);   /* 375: the chat is in */
	CHECK(Zelement_get_property(g_header, Z_PROP_TRANSLATE_Y) < -1);
	for (int i = 0; i < 20; i++) Zroot_tick(root, 50);
	CHECK(!Zframe_is_transitioning(frame));
	CHECK_NEAR(Zelement_get_property(g_header, Z_PROP_TRANSLATE_Y), 0);

	/* the slide up: the old page fades, the new one rises; back, it sinks */
	CHECK(Zframe_navigate_with_transition(frame, list_page, nullptr, Z_TRANSITION_SLIDE_UP));
	Zroot_tick(root, 120);
	const Zid page = Zframe_get_page(frame);
	CHECK(Zelement_get_property(page, Z_PROP_TRANSLATE_Y) > 50);
	for (int i = 0; i < 10; i++) Zroot_tick(root, 50);
	CHECK(!Zframe_is_transitioning(frame));
	CHECK_NEAR(Zelement_get_property(page, Z_PROP_TRANSLATE_Y), 0);
	CHECK(Zframe_go_back(frame));
	Zroot_tick(root, 120);
	CHECK(Zelement_get_property(page, Z_PROP_TRANSLATE_Y) > 5);   /* 150 x ease-in(120 / 250) */
	for (int i = 0; i < 10; i++) Zroot_tick(root, 50);
	CHECK(!Zframe_is_transitioning(frame));
	Zroot_destroy(root);
}

/* ---- G9: scroll anchoring ---- */

static Zid row_of(const char* const text) {
	const Zid t = Ztext_block_create(text);
	Zelement_set_height(t, 40);
	return t;
}

static void test_scroll_anchor(void) {
	const Zid root = zrn_test_root(360, 640);
	const Zid sv = Zscroll_viewer_create();
	const Zid stack = Zstack_panel_create(Z_VERTICAL);
	Zscroll_viewer_set_content(sv, stack);
	Zroot_set_visual(root, sv);
	Zid anchor = Z_INVALID_ID;
	for (int i = 0; i < 50; i++) {
		const Zid row = Zpanel_add(stack, row_of("message"));
		if (i == 20) anchor = row;
	}
	Zroot_update_layout(root);
	Zscroll_viewer_scroll_to_vertical_offset(sv, 600);
	Zroot_update_layout(root);
	CHECK_NEAR(Zscroll_viewer_get_vertical_offset(sv), 600);
	Zscroll_viewer_set_anchor(sv, anchor);
	CHECK(Zscroll_viewer_get_anchor(sv) == anchor);
	Zroot_update_layout(root);

	/* five older messages above: the anchor stays where it was on screen */
	for (int i = 0; i < 5; i++) Zpanel_insert(stack, 0, row_of("older"));
	Zroot_update_layout(root);
	CHECK_NEAR(Zscroll_viewer_get_vertical_offset(sv), 800);

	/* a message below it: nothing moves */
	Zpanel_add(stack, row_of("newer"));
	Zroot_update_layout(root);
	CHECK_NEAR(Zscroll_viewer_get_vertical_offset(sv), 800);

	/* the keyboard: the viewport is 227 shorter, the anchor keeps its distance to the bottom */
	Zroot_set_input_pane_height(root, 227);
	Zroot_update_layout(root);
	CHECK_NEAR(Zscroll_viewer_get_vertical_offset(sv), 1027);

	/* the user scrolls: that moves it */
	Zscroll_viewer_scroll_to_vertical_offset(sv, 100);
	Zroot_update_layout(root);
	CHECK_NEAR(Zscroll_viewer_get_vertical_offset(sv), 100);
	Zpanel_insert(stack, 0, row_of("oldest"));
	Zroot_update_layout(root);
	CHECK_NEAR(Zscroll_viewer_get_vertical_offset(sv), 140);
	Zroot_destroy(root);
}

int main(void) {
	if (!Zinit()) return 1;
	RUN(test_theme_changed);
	RUN(test_cache_mode);
	RUN(test_color_emoji_software);
	RUN(test_color_emoji_gpu);
	RUN(test_holding);
	RUN(test_input_pane);
	RUN(test_transitions);
	RUN(test_scroll_anchor);
	Zshutdown();
	return test_finish("test_gaps");
}
