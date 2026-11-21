/**
 * zrnui - Frame and Page: the navigation of a Windows Phone app.
 */

#include "zrnui/internal/controls/frame.h"
#include "zrnui/internal/util.h"
#include "zrnui/internal/class.h"
#include "zrnui/internal/element.h"
#include "zrnui/internal/anim.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/controls/control.h"
#include "zrnui/internal/controls/appbar.h"
#include "zrnui/internal/controls/panels.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * The safe area: what the status bar of the shell covers at the top of the window, in logical
 * pixels, where the first element of a page starts. The ztOS shell (ztmetro) already places an app's
 * window under its own status bar, so a page starts at the window's top: 0. A host whose window
 * reaches under a status bar sets ZRNUI_SAFE_AREA_TOP (for example 32, the bar of Windows Phone).
 */
static float zrn_safe_area_top(void) {
	static float top = -1.0f;
	if (top < 0) {
		const char* const s = getenv("ZRNUI_SAFE_AREA_TOP");
		top = s && *s ? (float)atof(s) : 0.0f;
		if (!(top >= 0 && top < 200)) top = 0;
	}
	return top;
}

/** A page a Frame shows (frame.c): the navigation parameter it was created for. */
typedef struct zrn_page {
	zrn_content_control_t base;
	void* parameter;
	/** How the frame reached the page now: what OnNavigatedTo and OnNavigatedFrom are told. */
	ZNavigationMode navigation_mode;
	ZNavigationCacheMode cache_mode;
	/** Page.TopAppBar and BottomAppBar: children of the page after its content. */
	zrn_element_t* top_bar;
	zrn_element_t* bottom_bar;
	/** The index in the history of the entry the page was built for. */
	int entry_index;
	/** Zpage_set_continuum_element: what flies in when the page is reached by a continuum transition. */
	Zid continuum;
} zrn_page_t;

typedef struct zrn_frame_entry {
	ZPageFactory factory;
	void* parameter;
	/** The page of the entry, alive and out of the frame, when its NavigationCacheMode keeps it. */
	Zid cached;
	/** The transition that brought the entry: going back from it runs the same kind backwards. */
	ZPageTransition transition;
} zrn_frame_entry_t;

/** A Frame: the navigation history, `current` is the entry shown. */
typedef struct zrn_frame {
	zrn_content_control_t base;
	zrn_frame_entry_t* entries;
	int count, capacity, current;
	/** true while the callbacks of a navigation run; another Navigate from them is refused. */
	bool b_navigating;
	/** The transition (frame.c): 1 the old page turns away, 2 the new one swings in, 0 none. */
	ZPageTransition transition;
	/** The first page went in before it was known whether animations run (the platform makes its surface later): Zroot_start_intro starts its swing-in. */
	bool b_intro_pending;
	int phase, direction;
	/** The mode of the navigation in progress (the page that leaves is told it too). */
	ZNavigationMode mode;
	float elapsed;
	/** The new page, built but not yet in the frame, during phase 1. */
	Zid pending;
	void* pending_parameter;
	/** The rows being turned, the opacity each had, the top of each in the root (collect) and then its place in the stagger. */
	Zid* rows;
	float* row_opacity;
	float* row_top;
	int* row_order;
	int row_count, row_cap;
	/** The transition of the navigation that runs (the frame's own, or the one of Zframe_navigate_with_transition). */
	ZPageTransition active;
	/** Zframe_set_continuum_element, for the next continuum navigation. */
	Zid continuum_from;
	/** The element that flies (out in phase 1, in in phase 2) and its opacity before; the page that slides, and its opacity before. */
	Zid fly;
	float fly_opacity;
	Zid slide;
	float slide_opacity;
} zrn_frame_t;

#define PAGE(e) ZRN_AS(zrn_page_t, e)
#define FRAME(e) ZRN_AS(zrn_frame_t, e)


/* ============================================================================
 *  Page
 * ============================================================================ */

static void page_init(zrn_element_t* const e) {
	e->b_focusable = false;
	ZRN_AS(zrn_control_t, e)->h_content = Z_HALIGN_STRETCH;
	ZRN_AS(zrn_control_t, e)->v_content = Z_VALIGN_STRETCH;
	PAGE(e)->navigation_mode = Z_NAVIGATION_NEW;
	PAGE(e)->cache_mode = Z_NAVIGATION_CACHE_DISABLED;
	PAGE(e)->entry_index = -1;
}

/* The page makes room for its app bars: what is left is the content's, and an open bar grows over the content. */
static void page_bar_room(const zrn_page_t* const p, float* const top, float* const bottom) {
	/* a bar with a backdrop effect (zrn-frosted-diffusion) takes no room: the content goes on under it */
	*top = p->top_bar && p->top_bar->visibility == Z_VISIBLE && !zrn_has_backdrop(p->top_bar) ? zrn_app_bar_closed_height(p->top_bar) : 0;
	*bottom = p->bottom_bar && p->bottom_bar->visibility == Z_VISIBLE && !zrn_has_backdrop(p->bottom_bar) ? zrn_app_bar_closed_height(p->bottom_bar) : 0;
}

static ZSize page_measure(zrn_element_t* const e, const ZSize available) {
	const auto p = PAGE(e);
	const zrn_content_control_t* const cc = &p->base;
	float top, bottom;
	ZSize content = { .width = 0, .height = 0 };
	page_bar_room(p, &top, &bottom);
	if (cc->content) {
		zrn_measure(cc->content, zrn_size(available.width, zrn_maxf(0, available.height - top - bottom)));
		content = cc->content->desired;
	}
	if (p->top_bar) zrn_measure(p->top_bar, zrn_size(available.width, available.height));
	if (p->bottom_bar) zrn_measure(p->bottom_bar, zrn_size(available.width, available.height));
	return zrn_size(content.width, content.height + top + bottom);
}

static ZSize page_arrange(zrn_element_t* const e, const ZSize final) {
	const auto p = PAGE(e);
	float top, bottom;
	page_bar_room(p, &top, &bottom);
	if (p->base.content) zrn_arrange(p->base.content, zrn_rect(0, top, final.width, zrn_maxf(0, final.height - top - bottom)));
	if (p->top_bar) zrn_arrange(p->top_bar, zrn_rect(0, 0, final.width, p->top_bar->desired.height));
	if (p->bottom_bar) zrn_arrange(p->bottom_bar, zrn_rect(0, final.height - p->bottom_bar->desired.height, final.width, p->bottom_bar->desired.height));
	return final;
}

static void page_child_removed(zrn_element_t* const e, zrn_element_t* const child) {
	const auto p = PAGE(e);
	if (p->top_bar == child) p->top_bar = nullptr;
	else if (p->bottom_bar == child) p->bottom_bar = nullptr;
	else zrn_cc_child_removed(e, child);
}

zrn_class_t zrn_class_page = {
	.base = &zrn_class_user_control, .name = "Page", .id = Z_CLASS_PAGE, .size = sizeof(zrn_page_t),
	.init = page_init, .measure = page_measure, .arrange = page_arrange, .child_removed = page_child_removed
};

Zid Zpage_create(void) {
	const zrn_element_t* const e = zrn_create(Z_CLASS_PAGE);
	return e ? e->id : Z_INVALID_ID;
}

void Zpage_set_content(Zid id, const Zid child) { Zuser_control_set_content(id, child); }

Zid Zpage_get_content(Zid id) { return Zuser_control_get_content(id); }

void* Zpage_get_parameter(const Zid page) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	return e ? PAGE(e)->parameter : nullptr;
}

ZNavigationMode Zpage_get_navigation_mode(const Zid page) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	return e ? PAGE(e)->navigation_mode : Z_NAVIGATION_NEW;
}

void Zpage_set_navigation_cache_mode(const Zid page, const ZNavigationCacheMode mode) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	if (e) PAGE(e)->cache_mode = mode;
}

ZNavigationCacheMode Zpage_get_navigation_cache_mode(const Zid page) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	return e ? PAGE(e)->cache_mode : Z_NAVIGATION_CACHE_DISABLED;
}

/* The bars are children of the page after its content: z-order puts them over it, and an open one grows over the content. */
static void page_set_bar(const Zid page, const Zid bar, const int top) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	zrn_element_t* const b = bar != Z_INVALID_ID ? zrn_get(bar, Z_CLASS_APP_BAR) : nullptr;
	if (!e || (bar != Z_INVALID_ID && !b)) return;
	zrn_element_t** slot = top ? &PAGE(e)->top_bar : &PAGE(e)->bottom_bar;
	zrn_set_only_child(e, slot, b);
	if (b && *slot == b) {
		b->zindex = 1;
		zrn_app_bar_set_on_top(b, top);
	}
}

void Zpage_set_top_app_bar(const Zid page, const Zid app_bar) { page_set_bar(page, app_bar, 1); }
void Zpage_set_bottom_app_bar(const Zid page, const Zid app_bar) { page_set_bar(page, app_bar, 0); }

Zid Zpage_get_top_app_bar(const Zid page) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	return e && PAGE(e)->top_bar ? PAGE(e)->top_bar->id : Z_INVALID_ID;
}

Zid Zpage_get_bottom_app_bar(const Zid page) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	return e && PAGE(e)->bottom_bar ? PAGE(e)->bottom_bar->id : Z_INVALID_ID;
}

Zid Zpage_get_frame(const Zid page) {
	const zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	return e && e->parent && zrn_is_a(e->parent, Z_CLASS_FRAME) ? e->parent->id : Z_INVALID_ID;
}

void Zpage_on_navigated_to(const Zid page, const ZRoutedCallback callback, void* const user_data) {
	if (zrn_get(page, Z_CLASS_PAGE)) Zelement_on(page, Z_EVENT_NAVIGATED_TO, callback, user_data);
}

void Zpage_on_navigated_from(const Zid page, const ZRoutedCallback callback, void* const user_data) {
	if (zrn_get(page, Z_CLASS_PAGE)) Zelement_on(page, Z_EVENT_NAVIGATED_FROM, callback, user_data);
}


/* ============================================================================
 *  Frame
 * ============================================================================ */

/* ---- the turnstile, as ZrnMetroCore times it from the Lumia: the rows of the page that leaves turn away
 * round the left edge one after another (128 ms each, 28 ms apart), a moment of nothing, then the rows
 * of the new page swing in from behind the screen (180 ms each). Going back the order is reversed. */
static constexpr float OUT_MS = 128.0f;
static constexpr float IN_MS = 180.0f;
static constexpr float STAGGER = 28.0f;
static constexpr float GAP_AT = 330.0f;
static constexpr int MAX_ROWS = 160;
static constexpr int MAX_STAGGER_ROWS = 7;

static void frame_init(zrn_element_t* const e) {
	e->b_focusable = false;
	FRAME(e)->current = -1;
	FRAME(e)->transition = Z_TRANSITION_TURNSTILE;
	FRAME(e)->active = Z_TRANSITION_TURNSTILE;
	FRAME(e)->mode = Z_NAVIGATION_NEW;
	ZRN_AS(zrn_control_t, e)->h_content = Z_HALIGN_STRETCH;
	ZRN_AS(zrn_control_t, e)->v_content = Z_VALIGN_STRETCH;
}

/* The pages kept alive for the entries from `first` on go with them. */
static void drop_cached(const zrn_frame_t* const f, const int first) {
	for (int i = first; i < f->count; i++) {
		zrn_element_t* const page = f->entries[i].cached != Z_INVALID_ID ? zrn_lookup(f->entries[i].cached) : nullptr;
		f->entries[i].cached = Z_INVALID_ID;
		if (page && !page->parent) zrn_destroy(page);
	}
}

static void frame_destroy(zrn_element_t* const e) {
	const auto f = FRAME(e);
	zrn_element_t* const pending = f->pending != Z_INVALID_ID ? zrn_lookup(f->pending) : nullptr;
	if (pending && !pending->parent) zrn_destroy(pending);
	drop_cached(f, 0);
	free(f->entries);
	free(f->rows);
	free(f->row_opacity);
	free(f->row_top);
	free(f->row_order);
	f->entries = nullptr;
	f->rows = nullptr;
	f->row_opacity = nullptr;
	f->row_top = nullptr;
	f->row_order = nullptr;
}

static void frame_tick(zrn_element_t* e, float dt);

/* The page starts under the status bar: the safe area is room the frame keeps, which is also where a turnstile turns. */
static ZThickness frame_chrome(zrn_element_t* const e) {
	ZRN_UNUSED(e);
	return Zthickness(0, zrn_safe_area_top(), 0, 0);
}

zrn_class_t zrn_class_frame = {
	.base = &zrn_class_content_control, .name = "Frame", .id = Z_CLASS_FRAME, .size = sizeof(zrn_frame_t),
	.init = frame_init, .destroy = frame_destroy, .tick = frame_tick, .chrome = frame_chrome
};

Zid Zframe_create(void) {
	const zrn_element_t* const e = zrn_create(Z_CLASS_FRAME);
	return e ? e->id : Z_INVALID_ID;
}

static zrn_frame_t* get_frame(const Zid id) { return FRAME(zrn_get(id, Z_CLASS_FRAME)); }

/* A row is what turns: a leaf of the tree, or a control that is not a container. */
static bool is_container(const zrn_element_t* const e) {
	return zrn_is_a(e, Z_CLASS_PANEL) || zrn_is_a(e, Z_CLASS_BORDER) || zrn_is_a(e, Z_CLASS_VIEWBOX) ||
		zrn_is_a(e, Z_CLASS_SCROLL_VIEWER) || zrn_is_a(e, Z_CLASS_USER_CONTROL) || zrn_is_a(e, Z_CLASS_ITEMS_CONTROL);
}

/* A container that turns whole, as a row: a bitmap cache (it is drawn as one picture anyway), or a box that paints a
 * background of its own (a bubble, a tile) and is no taller than a third of the frame. Its text turning without the box
 * behind it was the glitch of a turnstile over such rows. */
static bool is_unit(const zrn_frame_t* const f, const zrn_element_t* const e) {
	if (e->cache_mode == Z_CACHE_BITMAP) return true;
	bool b_paints = false;
	if (zrn_is_a(e, Z_CLASS_BORDER)) {
		const zrn_border_t* const b = ZRN_AS(zrn_border_t, e);
		b_paints = (b->b_has_background && b->background.a > 0) || b->b_has_border_brush;
	} else if (zrn_is_a(e, Z_CLASS_PANEL)) {
		const zrn_panel_t* const pn = ZRN_AS(zrn_panel_t, e);
		b_paints = pn->b_has_background && pn->background.a > 0;
	}
	return b_paints && e->rect.height * 3.0f <= f->base.base.base.rect.height;
}

static bool rows_room(zrn_frame_t* const f) {
	if (f->row_count < f->row_cap) return true;
	const int cap = f->row_cap ? f->row_cap * 2 : 16;
	Zid* const rows = realloc(f->rows, (size_t)cap * sizeof *rows);
	if (!rows) return false;
	f->rows = rows;
	float* const ops = realloc(f->row_opacity, (size_t)cap * sizeof *ops);
	if (!ops) return false;
	f->row_opacity = ops;
	float* const tops = realloc(f->row_top, (size_t)cap * sizeof *tops);
	if (!tops) return false;
	f->row_top = tops;
	int* const orders = realloc(f->row_order, (size_t)cap * sizeof *orders);
	if (!orders) return false;
	f->row_order = orders;
	f->row_cap = cap;
	return true;
}

/* The rows of a page that are in view: what is outside the frame, or clipped away by a scroll viewer (the rest of a long
 * list, the pivot items aside), does not turn and does not take a place in the stagger. `x` `y` is the origin of `e`'s
 * parent in the root, `view` what is visible there (root coordinates). */
static void collect_rows(zrn_frame_t* const f, const zrn_element_t* const e, const float x, const float y, ZRect view) {
	if (e->visibility != Z_VISIBLE || f->row_count >= MAX_ROWS || zrn_is_a(e, Z_CLASS_APP_BAR)) return;
	/* what flies on its own does not turn */
	if (f->fly != Z_INVALID_ID && e->id == f->fly) return;
	const float ex = x + e->rect.x, ey = y + e->rect.y;
	const bool b_overflows = e->child_count > 0 && !e->b_clip_to_bounds;
	/* a zero-sized holder (a Canvas) may have children that overflow it: only a clipped or childless element is judged by its own box */
	if (!b_overflows && (ex >= view.x + view.width || ey >= view.y + view.height || ex + e->rect.width <= view.x || ey + e->rect.height <= view.y)) return;
	if (is_container(e) && e->child_count > 0 && !is_unit(f, e)) {
		if (e->b_clip_to_bounds) {
			const float x0 = zrn_maxf(view.x, ex), y0 = zrn_maxf(view.y, ey);
			const float x1 = zrn_minf(view.x + view.width, ex + e->rect.width), y1 = zrn_minf(view.y + view.height, ey + e->rect.height);
			if (x1 <= x0 || y1 <= y0) return;
			view = zrn_rect(x0, y0, x1 - x0, y1 - y0);
		}
		for (int i = 0; i < e->child_count; i++) collect_rows(f, e->children[i], ex, ey, view);
		return;
	}
	if (!rows_room(f)) return;
	f->rows[f->row_count] = e->id;
	f->row_opacity[f->row_count] = e->opacity;
	f->row_top[f->row_count] = ey;
	f->row_count++;
}

/* The place of each row in the stagger: by its top on the screen, the rows that start on the same line together, as the
 * Lumia turns a page from the top down whatever order the tree has them in. */
static void rows_order(const zrn_frame_t* const f) {
	for (int i = 0; i < f->row_count; i++) {
		int above = 0;
		for (int j = 0; j < f->row_count; j++) {
			/* the distinct lines above this one: a row counts once, with the first of the rows of its line */
			if (f->row_top[j] >= f->row_top[i] - 4.0f) continue;
			bool b_first = true;
			for (int k = 0; k < j && b_first; k++) {
				if (fabsf(f->row_top[k] - f->row_top[j]) < 4.0f) b_first = false;
			}
			if (b_first) above++;
		}
		f->row_order[i] = above;
	}
}

static bool frame_log(void) {
	static int b_log = -1;
	if (b_log < 0) b_log = getenv("ZRNUI_FRAME_LOG") != nullptr;
	return b_log;
}

static void rows_of(zrn_frame_t* const f, zrn_element_t* const page) {
	f->row_count = 0;
	if (!page) return;
	const zrn_element_t* const frame = &f->base.base.base;
	const ZPoint origin = zrn_to_root(frame, (ZPoint){ .x = 0, .y = 0 });
	const ZPoint parent = page->parent ? zrn_to_root(page->parent, (ZPoint){ .x = 0, .y = 0 }) : origin;
	collect_rows(f, page, parent.x, parent.y, zrn_rect(origin.x, origin.y, frame->rect.width, frame->rect.height));
	rows_order(f);
	if (frame_log()) {
		int lines = 0;
		for (int i = 0; i < f->row_count; i++) if (f->row_order[i] + 1 > lines) lines = f->row_order[i] + 1;
		fprintf(stderr, "zrnui-turnstile rows=%d lines=%d\n", f->row_count, lines);
	}
}

/* The rows back as they were. */
static void rows_reset(zrn_frame_t* const f) {
	for (int i = 0; i < f->row_count; i++) {
		zrn_element_t* const row = zrn_lookup(f->rows[i]);
		if (!row) continue;
		row->rot_x = row->rot_y = 0;
		row->proj_cx = row->proj_cy = 0.5f;
		row->opacity = f->row_opacity[i];
		zrn_update_transformed(row);
		/* a render property: the row's own bitmap cache stays as it is, only what holds it is drawn again */
		zrn_changed_render(row);
	}
	f->row_count = 0;
}

static float clamp01(const float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* One frame of the turnstile: row `i` of `n`, `t` ms into this half. */
static void rows_turn(const zrn_frame_t* const f, const zrn_element_t* const frame, const float t, const bool b_incoming) {
	int i, n = f->row_count, lines = 0;
	const float left = zrn_to_root(frame, (ZPoint){ .x = 0, .y = 0 }).x;
	for (i = 0; i < n; i++) if (f->row_order[i] + 1 > lines) lines = f->row_order[i] + 1;
	for (i = 0; i < n; i++) {
		zrn_element_t* const row = zrn_lookup(f->rows[i]);
		int order = f->direction >= 0 ? f->row_order[i] : lines - 1 - f->row_order[i];
		float u, angle, alpha;
		ZPoint origin;
		if (!row) continue;
		if (order > MAX_STAGGER_ROWS) order = MAX_STAGGER_ROWS;
		/* a row not laid out yet has no width to turn about: it is only kept clear until the first tick lays it out */
		if (row->rect.width > 0) {
			origin = zrn_to_root(row, (ZPoint){ .x = 0, .y = 0 });
			row->proj_cx = (left - origin.x) / row->rect.width;
			row->proj_cy = 0.5f;
		}
		if (!b_incoming) {
			u = clamp01((t - (float)order * STAGGER) / OUT_MS);
			angle = (f->direction >= 0 ? -50.0f : 50.0f) * zrn_ease(Z_EASE_IN, u);
			alpha = u >= 1 ? 0 : 1 - u * u;
		} else {
			u = clamp01((t - (float)order * STAGGER) / IN_MS);
			angle = 80.0f * (1 - zrn_ease(Z_EASE_OUT, u));
			alpha = u <= 0 ? 0 : clamp01(u * 3);
		}
		row->rot_y = angle;
		row->opacity = f->row_opacity[i] * alpha;
		zrn_update_transformed(row);
		zrn_changed_render(row);
	}
}

/* ---- the continuum and the slide ---- */

static constexpr float FLY_OUT_MS = 150.0f;
static constexpr float FLY_IN_MS = 250.0f;
static constexpr float SLIDE_FADE_MS = 100.0f;
static constexpr float SLIDE_IN_MS = 350.0f;
static constexpr float SLIDE_BACK_OUT_MS = 250.0f;
static constexpr float SLIDE_BACK_IN_MS = 150.0f;

static void fly_set(zrn_frame_t* const f, const Zid id) {
	const zrn_element_t* const e = id != Z_INVALID_ID ? zrn_lookup(id) : nullptr;
	f->fly = e ? id : Z_INVALID_ID;
	f->fly_opacity = e ? e->opacity : 1.0f;
}

static void fly_reset(zrn_frame_t* const f) {
	zrn_element_t* const e = f->fly != Z_INVALID_ID ? zrn_lookup(f->fly) : nullptr;
	f->fly = Z_INVALID_ID;
	if (!e) return;
	e->translate_x = e->translate_y = 0;
	e->opacity = f->fly_opacity;
	zrn_update_transformed(e);
	zrn_changed_render(e);
}

/* The continuum: out, 0 to (100, -50) with an ease in and a fade; in, from y -50 with an ease out and a fade in. */
static void fly_step(const zrn_frame_t* const f, const float t, const bool b_incoming) {
	zrn_element_t* const e = f->fly != Z_INVALID_ID ? zrn_lookup(f->fly) : nullptr;
	if (!e) return;
	if (!b_incoming) {
		const float u = zrn_ease(Z_EASE_IN, clamp01(t / FLY_OUT_MS));
		e->translate_x = 100.0f * u;
		e->translate_y = -50.0f * u;
		e->opacity = f->fly_opacity * (1 - u);
	} else {
		const float u = zrn_ease(Z_EASE_OUT, clamp01(t / FLY_IN_MS));
		e->translate_x = 0;
		e->translate_y = -50.0f * (1 - u);
		e->opacity = f->fly_opacity * clamp01(t / FLY_IN_MS * 2);
	}
	zrn_update_transformed(e);
	zrn_changed_render(e);
}

static void slide_set(zrn_frame_t* const f, zrn_element_t* const page) {
	f->slide = page ? page->id : Z_INVALID_ID;
	f->slide_opacity = page ? page->opacity : 1.0f;
}

static void slide_reset(zrn_frame_t* const f) {
	zrn_element_t* const e = f->slide != Z_INVALID_ID ? zrn_lookup(f->slide) : nullptr;
	f->slide = Z_INVALID_ID;
	if (!e) return;
	e->translate_y = 0;
	e->opacity = f->slide_opacity;
	zrn_update_transformed(e);
	zrn_changed_render(e);
}

/* The slide of a whole page: forward the old one fades and the new one rises; back the old one sinks and fades, the new one fades in. */
static void slide_step(const zrn_frame_t* const f, const float t, const bool b_incoming) {
	zrn_element_t* const e = f->slide != Z_INVALID_ID ? zrn_lookup(f->slide) : nullptr;
	if (!e) return;
	float y = 0, alpha = 1;
	if (f->direction >= 0 && !b_incoming) {
		alpha = 1 - clamp01(t / SLIDE_FADE_MS);
	} else if (f->direction >= 0) {
		const float u = zrn_ease(Z_EASE_EMPHASIZED, clamp01(t / SLIDE_IN_MS));
		y = 150.0f * (1 - u);
		alpha = clamp01(t / SLIDE_IN_MS * 2);
	} else if (!b_incoming) {
		const float u = zrn_ease(Z_EASE_IN, clamp01(t / SLIDE_BACK_OUT_MS));
		y = 150.0f * u;
		alpha = 1 - u;
	} else {
		alpha = clamp01(t / SLIDE_BACK_IN_MS);
	}
	e->translate_y = y;
	e->opacity = f->slide_opacity * alpha;
	zrn_update_transformed(e);
	zrn_changed_render(e);
}

/* When the old page is gone and how long the new one takes to come in, for the transition that runs. */
static float gap_at(const zrn_frame_t* const f) {
	if (f->active != Z_TRANSITION_SLIDE_UP) return GAP_AT;
	return f->direction >= 0 ? SLIDE_FADE_MS : SLIDE_BACK_OUT_MS;
}

static float in_ms(const zrn_frame_t* const f) {
	if (f->active == Z_TRANSITION_SLIDE_UP) return f->direction >= 0 ? SLIDE_IN_MS : SLIDE_BACK_IN_MS;
	const float rows = IN_MS + (float)MAX_STAGGER_ROWS * STAGGER;
	return f->active == Z_TRANSITION_CONTINUUM && rows < FLY_IN_MS ? FLY_IN_MS : rows;
}

/* One frame of the transition that runs. */
static void step(zrn_frame_t* const f, zrn_element_t* const frame, const float t, const bool b_incoming) {
	if (f->active == Z_TRANSITION_SLIDE_UP) {
		slide_step(f, t, b_incoming);
		return;
	}
	rows_turn(f, frame, t, b_incoming);
	if (f->active == Z_TRANSITION_CONTINUUM) fly_step(f, t, b_incoming);
}

/* Everything that moved back as it was. */
static void moved_reset(zrn_frame_t* const f) {
	rows_reset(f);
	fly_reset(f);
	slide_reset(f);
}

/* What moves when the new page comes in: its rows (and what flies in), or the whole page. */
static void incoming_of(zrn_frame_t* const f, zrn_element_t* const page) {
	f->row_count = 0;
	if (f->active == Z_TRANSITION_SLIDE_UP) {
		slide_set(f, page);
		return;
	}
	if (f->active == Z_TRANSITION_CONTINUUM && f->direction >= 0 && page && zrn_is_a(page, Z_CLASS_PAGE)) fly_set(f, PAGE(page)->continuum);
	rows_of(f, page);
}

/* The page goes into the frame: its parameter, the content, NavigatedTo, Navigated. */
static void attach(zrn_frame_t* const f, const Zid frame_id, const Zid page_id, void* const parameter) {
	zrn_element_t* const page = zrn_lookup(page_id);
	f->b_navigating = true;
	if (!zrn_lookup(frame_id)) {
		if (page && !page->parent) zrn_destroy(page);
		return;
	}
	if (!page) { f->b_navigating = false; return; }
	PAGE(page)->parameter = parameter;
	PAGE(page)->navigation_mode = f->mode;
	PAGE(page)->entry_index = f->current;
	page->b_hit_visible = true;
	/* a page that keeps its NavigationCacheMode stays alive, out of the frame, for the entry it was built for */
	{
		zrn_element_t* const old = f->base.content;
		if (old && old != page && zrn_is_a(old, Z_CLASS_PAGE) && PAGE(old)->cache_mode != Z_NAVIGATION_CACHE_DISABLED) {
			const int index = PAGE(old)->entry_index;
			if (index >= 0 && index < f->count && f->entries[index].cached == Z_INVALID_ID) {
				zrn_remove_child(&f->base.base.base, old);
				f->entries[index].cached = old->id;
			}
		}
	}
	zrn_content_set(&f->base, page);
	if (zrn_lookup(page_id)) zrn_raise(zrn_lookup(page_id), Z_EVENT_NAVIGATED_TO);
	if (zrn_lookup(frame_id)) zrn_raise(zrn_lookup(frame_id), Z_EVENT_NAVIGATED);
	if (zrn_lookup(frame_id)) f->b_navigating = false;
}

/* Ends a transition at once: the new page is in, nothing turns. */
static void finish(zrn_frame_t* const f, const Zid frame_id) {
	if (f->phase == 0) return;
	if (f->phase == 1) {
		const Zid page = f->pending;
		moved_reset(f);
		f->pending = Z_INVALID_ID;
		f->phase = 0;
		attach(f, frame_id, page, f->pending_parameter);
		if (!zrn_lookup(frame_id)) return;
	} else {
		moved_reset(f);
	}
	f->phase = 0;
	f->elapsed = 0;
}

static void frame_tick(zrn_element_t* e, const float dt) {
	auto f = FRAME(e);
	const Zid id = e->id;
	if (f->phase == 0) { e->b_wants_tick = false; return; }
	f->elapsed += dt;
	if (e->root && e->root->b_layout_dirty) Zroot_update_layout(e->root->id);
	if (f->phase == 1) {
		if (f->elapsed < gap_at(f)) {
			step(f, e, f->elapsed, 0);
			return;
		}
		/* the old page is gone, the new one comes in */
		{
			const Zid page = f->pending;
			const float carry = f->elapsed - gap_at(f);
			moved_reset(f);
			f->pending = Z_INVALID_ID;
			f->phase = 2;
			attach(f, id, page, f->pending_parameter);
			if (!zrn_lookup(id)) return;
			f = FRAME(zrn_lookup(id));
			e = &f->base.base.base;
			if (e->root && e->root->b_layout_dirty) Zroot_update_layout(e->root->id);
			incoming_of(f, f->base.content);
			f->elapsed = carry;
			step(f, e, f->elapsed, 1);
		}
		return;
	}
	if (f->elapsed >= in_ms(f)) {
		moved_reset(f);
		f->phase = 0;
		e->b_wants_tick = false;
		return;
	}
	step(f, e, f->elapsed, 1);
}

/*
 * Shows `page` (just built from the entry at `index`) in place of the current one. The history is
 * already settled, so a callback may ask the frame anything. Every callback can destroy what it
 * is called on, so the elements are found again by Zid. With animations the old page stays until
 * it has turned away, and the new one is held back till then.
 */
static void start_intro(zrn_frame_t* const f, const int direction) {
	zrn_element_t* const fe = &f->base.base.base;
	f->b_intro_pending = false;
	if (!f->base.content) return;
	f->direction = direction;
	/* the rows are the ones in view: the page is laid out first */
	if (fe->root) Zroot_update_layout(fe->root->id);
	incoming_of(f, f->base.content);
	f->phase = 2;
	f->elapsed = 0;
	zrn_want_tick(fe);
	step(f, fe, 0, 1);
}

void zrn_root_start_intro(zrn_root_t* const r) {
	if (!r || !r->visual || !zrn_is_a(r->visual, Z_CLASS_FRAME)) return;
	zrn_frame_t* f = FRAME(r->visual);
	if (f->b_intro_pending && f->phase == 0 && zrn_animations_enabled(r)) {
		f->active = f->transition == Z_TRANSITION_NONE ? Z_TRANSITION_TURNSTILE : f->transition;
		start_intro(f, 1);
	}
}

static void show(zrn_frame_t* f, const Zid frame_id, const Zid page_id, void* parameter, const int direction, const ZNavigationMode mode, const ZPageTransition transition) {
	f->b_intro_pending = false;
	f->mode = mode;
	f->active = transition;
	f->direction = direction;
	zrn_element_t* old = f->base.content;
	zrn_element_t* const frame = &f->base.base.base;
	const bool b_enabled = transition != Z_TRANSITION_NONE && frame->root && zrn_animations_enabled(frame->root);
	/* the element that flies out is taken by this navigation, whatever it runs */
	const Zid from = f->continuum_from;
	f->continuum_from = Z_INVALID_ID;
	const bool b_animate = b_enabled && old && zrn_is_a(old, Z_CLASS_PAGE);
	if (frame_log()) fprintf(stderr, "zrnui-navigate transition=%d direction=%d animate=%d\n", (int)transition, direction, (int)b_animate);
	f->b_navigating = true;
	if (old && zrn_is_a(old, Z_CLASS_PAGE)) {
		/* the page that leaves is told how the next one is reached */
		PAGE(old)->navigation_mode = mode;
		zrn_raise(old, Z_EVENT_NAVIGATED_FROM);
	}
	if (!zrn_lookup(frame_id)) {
		zrn_element_t* const page = zrn_lookup(page_id);
		if (page && !page->parent) zrn_destroy(page);
		return;
	}
	if (!b_animate || !f->base.content) {
		const bool b_first = old == nullptr;
		attach(f, frame_id, page_id, parameter);
		if (!zrn_lookup(frame_id)) return;
		/* the first page has nothing to turn away from, but it swings in: the app opens as the second half of a turn.
		 * Before the platform has a surface nobody knows yet whether animations run: Zroot_start_intro starts it then. */
		f = FRAME(zrn_lookup(frame_id));
		f->b_intro_pending = b_first && transition != Z_TRANSITION_NONE;
		if (b_enabled) start_intro(f, direction);
		return;
	}
	old = f->base.content;
	f->row_count = 0;
	if (transition == Z_TRANSITION_SLIDE_UP) {
		slide_set(f, old);
	} else {
		/* the element that flies must be on the page that leaves */
		if (transition == Z_TRANSITION_CONTINUUM && direction >= 0 && from != Z_INVALID_ID && zrn_lookup(from)) {
			const zrn_element_t* a = zrn_lookup(from);
			while (a && a != old) a = a->parent;
			if (a) fly_set(f, from);
		}
		rows_of(f, old);
	}
	old->b_hit_visible = false;
	f->pending = page_id;
	f->pending_parameter = parameter;
	f->direction = direction;
	f->phase = 1;
	f->elapsed = 0;
	f->b_navigating = false;
	zrn_want_tick(frame);
	step(f, frame, 0, 0);
}

/* Builds the page of an entry; Z_INVALID_ID, and the page destroyed, when it is not a Page. */
static Zid build(const Zid frame_id, const zrn_frame_entry_t entry) {
	const Zid page = entry.factory(frame_id, entry.parameter);
	if (page == Z_INVALID_ID) return Z_INVALID_ID;
	zrn_element_t* e = zrn_lookup(page);
	if (!e) return Z_INVALID_ID;
	if (!zrn_is_a(e, Z_CLASS_PAGE) || e->parent) {
		zrn_error(Z_ERROR_WRONG_CLASS, "a page factory must return a new Page");
		if (!e->parent) zrn_destroy(e);
		return Z_INVALID_ID;
	}
	return page;
}

static bool navigate(const Zid frame, const ZPageFactory factory, void* const parameter, const bool b_own, const ZPageTransition own) {
	zrn_frame_t* f = get_frame(frame);
	zrn_frame_entry_t entry = { .factory = factory, .parameter = parameter, .cached = Z_INVALID_ID };
	if (!f) return false;
	entry.transition = b_own ? own : f->transition;
	Zid page;
	if (!f) return false;
	if (!factory) {
		zrn_error(Z_ERROR_INVALID_ARGUMENT, "Navigate needs a page factory");
		return false;
	}
	if (f->b_navigating) return false;
	finish(f, frame);
	f = get_frame(frame);
	if (!f) return false;
	f->b_navigating = true;
	page = build(frame, entry);
	f = get_frame(frame);
	if (!f) return false;
	f->b_navigating = false;
	if (page == Z_INVALID_ID) return false;
	if (f->current + 1 >= f->capacity) {
		const int capacity = f->capacity ? f->capacity * 2 : 8;
		zrn_frame_entry_t* entries = realloc(f->entries, (size_t)capacity * sizeof *entries);
		if (!entries) {
			zrn_error(Z_ERROR_OUT_OF_MEMORY, "out of memory");
			zrn_destroy(zrn_lookup(page));
			return false;
		}
		f->entries = entries;
		f->capacity = capacity;
	}
	/* the entries ahead are dropped, and the pages kept for them */
	drop_cached(f, f->current + 1);
	f->current++;
	entry.cached = Z_INVALID_ID;
	f->entries[f->current] = entry;
	f->count = f->current + 1;
	show(f, frame, page, parameter, 1, Z_NAVIGATION_NEW, entry.transition);
	return true;
}

bool Zframe_navigate(const Zid frame, const ZPageFactory factory, void* const parameter) { return navigate(frame, factory, parameter, false, Z_TRANSITION_NONE); }

bool Zframe_navigate_with_transition(const Zid frame, const ZPageFactory factory, void* const parameter, const ZPageTransition transition) {
	if (transition < Z_TRANSITION_NONE || transition > Z_TRANSITION_SLIDE_UP) {
		zrn_error(Z_ERROR_INVALID_ARGUMENT, "unknown page transition %d", (int)transition);
		return false;
	}
	return navigate(frame, factory, parameter, true, transition);
}

void Zframe_set_continuum_element(const Zid frame, const Zid from_element) {
	zrn_frame_t* const f = get_frame(frame);
	if (!f) return;
	if (from_element != Z_INVALID_ID && !zrn_get(from_element, Z_CLASS_ELEMENT)) return;
	f->continuum_from = from_element;
}

void Zpage_set_continuum_element(const Zid page, const Zid element) {
	zrn_element_t* const e = zrn_get(page, Z_CLASS_PAGE);
	if (!e) return;
	if (element != Z_INVALID_ID && !zrn_get(element, Z_CLASS_ELEMENT)) return;
	PAGE(e)->continuum = element;
}

static bool move(const Zid id, const int step) {
	zrn_frame_t* f = get_frame(id);
	Zid page;
	if (!f || f->b_navigating) return false;
	finish(f, id);
	f = get_frame(id);
	if (!f) return false;
	int target = f->current + step;
	if (target < 0 || target >= f->count) return false;
	f->b_navigating = true;
	if (f->entries[target].cached != Z_INVALID_ID && zrn_lookup(f->entries[target].cached)) {
		page = f->entries[target].cached;
		f->entries[target].cached = Z_INVALID_ID;
	} else {
		page = build(id, f->entries[target]);
		f = get_frame(id);
		if (!f) return false;
	}
	f->b_navigating = false;
	if (page == Z_INVALID_ID) return false;
	/* back runs the transition that brought the page that leaves, forward the one of the entry it goes to; a continuum goes back as a turnstile */
	ZPageTransition transition = step < 0 ? f->entries[f->current].transition : f->entries[target].transition;
	if (transition == Z_TRANSITION_CONTINUUM && step < 0) transition = Z_TRANSITION_TURNSTILE;
	f->current = target;
	show(f, id, page, f->entries[target].parameter, step, step < 0 ? Z_NAVIGATION_BACK : Z_NAVIGATION_FORWARD, transition);
	return true;
}

bool Zframe_go_back(Zid id) { return move(id, -1); }
bool Zframe_go_forward(Zid frame) { return move(frame, 1); }

bool Zframe_can_go_back(const Zid frame) {
	const zrn_frame_t* const f = get_frame(frame);
	return f && f->current > 0;
}

bool Zframe_can_go_forward(const Zid frame) {
	const zrn_frame_t* const f = get_frame(frame);
	return f && f->current + 1 < f->count;
}

int Zframe_get_back_stack_depth(const Zid frame) {
	const zrn_frame_t* const f = get_frame(frame);
	return f && f->current > 0 ? f->current : 0;
}

Zid Zframe_get_page(const Zid frame) {
	const zrn_frame_t* const f = get_frame(frame);
	if (f && f->phase == 1 && f->pending != Z_INVALID_ID) return f->pending;
	return f && f->base.content ? f->base.content->id : Z_INVALID_ID;
}

ZPageFactory Zframe_get_current_source_page_type(const Zid frame) {
	const zrn_frame_t* const f = get_frame(frame);
	return f && f->current >= 0 && f->current < f->count ? f->entries[f->current].factory : nullptr;
}

void Zframe_on_navigated(const Zid frame, const ZRoutedCallback callback, void* const user_data) {
	if (zrn_get(frame, Z_CLASS_FRAME)) Zelement_on(frame, Z_EVENT_NAVIGATED, callback, user_data);
}

void Zframe_set_transition(const Zid frame, const ZPageTransition transition) {
	zrn_frame_t* const f = get_frame(frame);
	if (f) f->transition = transition;
}

bool Zframe_is_transitioning(const Zid frame) {
	const zrn_frame_t* const f = get_frame(frame);
	return f && f->phase != 0;
}
