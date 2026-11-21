/**
 * zrnui - host.c
 * The root: owns a tree, lays it out, paints it with its popups and
 * tooltip, routes input (hit testing, hover, capture, focus), and keeps
 * time.
 */

#include "zrnui/platform.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/util.h"
#include "zrnui/internal/input.h"
#include "zrnui/internal/element.h"
#include "zrnui/internal/theme.h"
#include "zrnui/internal/resources.h"
#include "zrnui/internal/text.h"
#include "zrnui/internal/xform.h"
#include "zrnui/internal/anim.h"
#include "zrnui/internal/swipe.h"
#include "zrnui/internal/render/painter.h"
#include "zrnui/internal/render/gles.h"
#include "zrnui/internal/render/backend.h"
#include "zrnui/internal/image.h"
#include "zrnui/internal/controls/popup.h"
#include "zrnui/internal/controls/appbar.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

zrn_root_t* zrn_root_lookup(const Zid id) { return zrn_handle_get(id, 1); }

static zrn_root_t* get_root(const Zid id) {
	zrn_root_t* const r = zrn_root_lookup(id);
	if (!r) zrn_error(Z_ERROR_INVALID_ID, "invalid root handle 0x%x", (unsigned)id);
	return r;
}


/* ============================================================================
 *  attaching trees
 * ============================================================================ */

void zrn_root_register_popup(zrn_root_t* const r, zrn_element_t* const popup) {
	for (int i = 0; i < r->popup_count; i++) if (r->popups[i] == popup) return;
	if (r->popup_count == r->popup_cap) {
		const int cap = r->popup_cap ? r->popup_cap * 2 : 4;
		zrn_element_t** list = realloc(r->popups, (size_t)cap * sizeof *list);
		if (!list) return;
		r->popups = list;
		r->popup_cap = cap;
	}
	r->popups[r->popup_count++] = popup;
	zrn_changed_layout(popup);
}

void zrn_root_unregister_popup(zrn_root_t* const r, const zrn_element_t* const popup) {
	for (int i = 0; i < r->popup_count; i++) {
		if (r->popups[i] == popup) {
			memmove(r->popups + i, r->popups + i + 1, (size_t)(r->popup_count - i - 1) * sizeof *r->popups);
			r->popup_count--;
			r->b_paint_dirty = true;
			return;
		}
	}
}

void zrn_root_attach(zrn_root_t* const r, zrn_element_t* const e) {
	e->root = r;
	zrn_bindings_apply(e);
	if (e->cls == &zrn_class_popup && ZRN_AS(zrn_popup_t, e)->b_is_open) zrn_root_register_popup(r, e);
	for (int i = 0; i < e->child_count; i++) zrn_root_attach(r, e->children[i]);
	zrn_raise(e, Z_EVENT_LOADED);
}

void zrn_element_forget(zrn_element_t* const e) {
	zrn_root_t* const r = e->root;
	if (!r) return;
	if (r->focus == e) r->focus = nullptr;
	if (r->capture == e) r->capture = nullptr;
	if (r->hover == e) r->hover = nullptr;
	if (r->tilt_element == e) {
		/* a pressed row that leaves the tree (its page goes, a cached page is put aside) does not stay leaning */
		e->rot_x = e->rot_y = 0;
		e->render_scale = 1;
		zrn_update_transformed(e);
		r->tilt_element = nullptr;
	}
	if (r->tooltip_target == e) r->tooltip_target = nullptr;
	if (r->visual == e) r->visual = nullptr;
	if (r->open_bar == e) r->open_bar = nullptr;
	if (e->cls == &zrn_class_popup) zrn_root_unregister_popup(r, e);
}

void zrn_root_detach(zrn_element_t* const e) {
	for (int i = 0; i < e->child_count; i++) zrn_root_detach(e->children[i]);
	if (e->root) {
		zrn_root_t* const r = e->root;
		zrn_element_forget(e);
		e->b_pointer_over = 0;
		r->b_paint_dirty = true;
		r->b_layout_dirty = true;
		e->root = nullptr;
	}
}


/* ============================================================================
 *  focus and capture
 * ============================================================================ */

void zrn_set_focus(zrn_root_t* r, zrn_element_t* e) {
	zrn_element_t* old = r->focus;
	zrn_input_t in;
	if (old == e) return;
	memset(&in, 0, sizeof in);
	/* the focus is drawn: the cached bitmaps around the old and the new one are stale */
	zrn_cache_stale(old);
	zrn_cache_stale(e);
	r->focus = e;
	r->b_paint_dirty = true;
	{
		const Zid new_id = e ? e->id : Z_INVALID_ID, old_id = old ? old->id : Z_INVALID_ID;
		if (old) {
			in.type = ZRN_IN_FOCUS_OUT;
			if (old->cls->input) old->cls->input(old, &in);
			old = zrn_lookup(old_id);
			if (old) zrn_raise(old, Z_EVENT_LOST_FOCUS);
		}
		/* the handlers above may have destroyed the new element or moved the focus elsewhere */
		e = new_id != Z_INVALID_ID ? zrn_lookup(new_id) : nullptr;
		if (e && r->focus == e) {
			in.type = ZRN_IN_FOCUS_IN;
			if (e->cls->input) e->cls->input(e, &in);
			e = zrn_lookup(new_id);
			if (e) zrn_raise(e, Z_EVENT_GOT_FOCUS);
		}
	}
}

void zrn_set_capture(zrn_root_t* const r, zrn_element_t* const e) {
	zrn_element_t* const old = r->capture;
	if (old == e) return;
	r->capture = e;
	if (old && old->cls->input) {
		zrn_input_t in = { .type = ZRN_IN_CAPTURE_LOST };
		old->cls->input(old, &in);
	}
}

void zrn_release_capture(zrn_root_t* const r, zrn_element_t* const e) {
	if (r->capture == e) r->capture = nullptr;
}

Zid Zroot_get_focus(const Zid root) {
	const zrn_root_t* const r = get_root(root);
	return r && r->focus ? r->focus->id : Z_INVALID_ID;
}

Zid Zroot_get_pointer_over(const Zid root) {
	const zrn_root_t* const r = get_root(root);
	return r && r->hover ? r->hover->id : Z_INVALID_ID;
}

static void collect_tab_stops(zrn_element_t* const e, zrn_element_t*** list, int* const count, int* const cap) {
	if (e->visibility != Z_VISIBLE || !e->b_enabled) return;
	/* what a popup holds is a tab stop only while it is open, and then the list of the root has it */
	if (e->cls == &zrn_class_popup) return;
	if (e->b_focusable && e->b_tab_stop) {
		if (*count == *cap) {
			const int ncap = *cap ? *cap * 2 : 16;
			zrn_element_t** more = realloc(*list, (size_t)ncap * sizeof *more);
			if (!more) return;
			*list = more;
			*cap = ncap;
		}
		(*list)[(*count)++] = e;
	}
	for (int i = 0; i < e->child_count; i++) collect_tab_stops(e->children[i], list, count, cap);
}

void Zroot_move_focus(const Zid root, const bool forward) {
	zrn_root_t* const r = get_root(root);
	zrn_element_t** list = nullptr;
	int count = 0, cap = 0, i, j, current = -1, first_popup;
	if (!r || !r->visual) return;
	/* a modal popup is all there is to reach while it is open */
	first_popup = 0;
	for (i = 0; i < r->popup_count; i++) if (ZRN_AS(zrn_popup_t, r->popups[i])->b_modal) first_popup = i + 1;
	if (first_popup == 0) collect_tab_stops(r->visual, &list, &count, &cap);
	for (i = first_popup > 0 ? first_popup - 1 : 0; i < r->popup_count; i++) if (r->popups[i]->child_count) collect_tab_stops(r->popups[i]->children[0], &list, &count, &cap);
	if (count == 0) { free(list); return; }
	/* stable sort by TabIndex */
	for (i = 1; i < count; i++) {
		zrn_element_t* const x = list[i];
		for (j = i - 1; j >= 0 && list[j]->tab_index > x->tab_index; j--) list[j + 1] = list[j];
		list[j + 1] = x;
	}
	for (i = 0; i < count; i++) if (list[i] == r->focus) current = i;
	if (current < 0) current = forward ? -1 : count;
	current += forward ? 1 : -1;
	current = (current + count) % count;
	zrn_set_focus(r, list[current]);
	free(list);
}


/* ============================================================================
 *  public: lifecycle and properties
 * ============================================================================ */

Zid Zroot_create(void) {
	zrn_root_t* r;
	if (!Zinit()) return Z_INVALID_ID;
	r = (zrn_root_t*)calloc(1, sizeof *r);
	if (!r) {
		zrn_error(Z_ERROR_OUT_OF_MEMORY, "out of memory");
		Zshutdown(); /* gives back the count of the Zinit above */
		return Z_INVALID_ID;
	}
	r->width = 0;
	r->height = 0;
	/* the scale divides into the UI scale (of the layout grid) and the density of the buffer (crispness) */
	r->ui = r->density = r->scale = 1.0f;
	r->b_first_frame = true;
	r->b_layout_dirty = r->b_paint_dirty = true;
	r->renderer = zrn_config()->renderer;
	r->animations = zrn_config()->animations;
	zrn_theme_init(&r->theme, Z_THEME_DARK, zrn_accent_color(Z_ACCENT_COBALT));
	r->theme_from = r->theme_target = r->theme;
	r->b_theme_transitioning = false;
	r->theme_t = 0;
	r->id = zrn_handle_alloc(r, 1);
	if (r->id == Z_INVALID_ID) {
		zrn_error(Z_ERROR_OUT_OF_MEMORY, "out of memory");
		free(r);
		Zshutdown();
		return Z_INVALID_ID;
	}
	r->overlay = zrn_create(Z_CLASS_CANVAS);
	if (r->overlay) zrn_root_attach(r, r->overlay);
	return r->id;
}

void Zroot_destroy(const Zid root) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	if (r->visual) zrn_destroy(r->visual);
	if (r->overlay) {
		/* what is shown at an element belongs to the program: it only stops being shown */
		while (r->overlay->child_count > 0) zrn_remove_child(r->overlay, r->overlay->children[r->overlay->child_count - 1]);
		zrn_destroy(r->overlay);
	}
	free(r->popups);
	zrn_root_resources_free(r);
	free(r->pixels);
	if (r->gpu) zrn_gl_destroy(r->gpu);
	zrn_handle_free(r->id);
	free(r);
	Zshutdown();
}

bool Zroot_is_valid(const Zid root) { return zrn_root_lookup(root) != nullptr; }

void Zroot_set_visual(const Zid root, const Zid visual) {
	zrn_root_t* const r = get_root(root);
	zrn_element_t* e = nullptr;
	if (!r) return;
	if (visual != Z_INVALID_ID) {
		e = zrn_get(visual, Z_CLASS_ELEMENT);
		if (!e) return;
		if (e->parent) zrn_remove_child(e->parent, e);
	}
	if (r->visual && r->visual != e) zrn_destroy(r->visual);
	r->visual = e;
	if (e) zrn_root_attach(r, e);
	r->b_layout_dirty = r->b_paint_dirty = true;
}

Zid Zroot_get_visual(const Zid root) {
	const zrn_root_t* const r = get_root(root);
	return r && r->visual ? r->visual->id : Z_INVALID_ID;
}

void Zroot_set_size(const Zid root, const int width, const int height) {
	zrn_root_t* const r = get_root(root);
	if (!r || width <= 0 || height <= 0) return;
	r->width = width;
	r->height = height;
	r->b_layout_dirty = r->b_paint_dirty = true;
	if (r->visual) zrn_changed_layout(r->visual);
}

/** The UI scale: the size of the layout grid relative to the window (the painter draws it
 * back up); at 720 x 1280 and a scale of 1.5 the page is laid out in the 480 x 800 grid.
 * The scale belongs to the shell, the platform layer passes it on; it is not a zoom. */
void Zroot_set_scale(const Zid root, const float scale) {
	zrn_root_t* const r = get_root(root);
	if (!r || scale <= 0) return;
	r->ui = scale;
	r->scale = scale * r->density;
	r->b_paint_dirty = true;
}

/** The density of the buffer against the logical size of the window: only crispness,
 * what the shell needs to show the content sharply; the size of the content is the job of the UI scale. */
void Zroot_set_density(const Zid root, const float density) {
	zrn_root_t* const r = get_root(root);
	if (!r || density <= 0) return;
	r->density = density;
	r->scale = density * r->ui;
	r->b_paint_dirty = true;
}

/* The theme or the accent changed: what is cached was painted in the old one, and the app may keep resources of its own. */
static void theme_changed(zrn_root_t* const r) {
	const Zid id = r->id;
	r->cache_epoch++;
	if (r->theme_changed) r->theme_changed(id, r->theme_changed_data);
}

void Zroot_on_theme_changed(const Zid root, const ZRoutedCallback callback, void* const user_data) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	r->theme_changed = callback;
	r->theme_changed_data = user_data;
}

void Zroot_set_input_pane_height(const Zid root, const float height) {
	zrn_root_t* const r = get_root(root);
	if (!r || height != height) return;
	const float h = height > 0 ? height : 0;
	if (h == r->input_pane) return;
	r->input_pane = h;
	r->b_layout_dirty = r->b_paint_dirty = true;
	if (r->visual) zrn_changed_layout(r->visual);
}

float Zroot_get_input_pane_height(const Zid root) {
	const zrn_root_t* const r = get_root(root);
	return r ? r->input_pane : 0;
}

void Zroot_set_theme(const Zid root, const ZTheme theme) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	const ZTheme going_to = r->b_theme_transitioning ? r->theme_target.theme : r->theme.theme;
	if (going_to == theme) return;
	ZColor bg = r->theme.background;
	bool b_has = r->theme.b_has_background;
	if (!zrn_animations_enabled(r)) {
		/* instant, as before: no cross-fade when animations are off */
		zrn_theme_init(&r->theme, theme, r->theme.accent);
		if (b_has) { r->theme.background = bg; r->theme.b_has_background = true; }
		r->theme_from = r->theme_target = r->theme;
		r->b_theme_transitioning = false;
		r->theme_t = 0;
		r->b_paint_dirty = true;
		zrn_root_resources_changed(r);
		theme_changed(r);
		return;
	}
	r->theme_from = r->theme;
	zrn_theme_init(&r->theme_target, theme, r->theme.accent);
	if (b_has) { r->theme_target.background = bg; r->theme_target.b_has_background = true; }
	r->b_theme_transitioning = true;
	r->theme_t = 0;
	r->b_paint_dirty = true;
	zrn_root_resources_changed(r);
	theme_changed(r);
}

ZTheme Zroot_get_theme(const Zid root) {
	zrn_root_t* const r = get_root(root);
	return r ? r->theme.theme : Z_THEME_DARK;
}

void Zroot_set_accent_color(const Zid root, const ZColor color) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	const ZColor was = r->theme.accent;
	if (was.r == color.r && was.g == color.g && was.b == color.b && was.a == color.a) return;
	r->theme.accent = color;
	r->theme.colors[ZRN_TC_ACCENT] = color;
	/* a cross-fade in progress ends in the new accent too */
	r->theme_target.accent = color;
	r->theme_target.colors[ZRN_TC_ACCENT] = color;
	r->b_paint_dirty = true;
	zrn_root_resources_changed(r);
	theme_changed(r);
}

void Zroot_set_accent(const Zid root, const ZAccent accent) { Zroot_set_accent_color(root, zrn_accent_color(accent)); }

ZColor Zroot_get_accent_color(const Zid root) {
	zrn_root_t* const r = get_root(root);
	return r ? r->theme.accent : Zcolor_rgba(0, 0, 0, 0);
}

void Zroot_set_background(const Zid root, const ZColor color) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	r->theme.background = color;
	r->theme.b_has_background = true;
	r->b_paint_dirty = true;
}


/* ============================================================================
 *  layout
 * ============================================================================ */

static void layout_popups(const zrn_root_t* const r) {
	const float lw = (float)r->width / r->ui, lh = (float)r->height / r->ui;
	for (int i = 0; i < r->popup_count; i++) {
		const zrn_popup_t* const popup = ZRN_AS(zrn_popup_t, r->popups[i]);
		zrn_element_t* const child = popup->base.child_count ? popup->base.children[0] : nullptr;
		ZPoint origin, pos;
		float width;
		if (!child) continue;
		zrn_measure(child, zrn_size(lw, lh));
		width = popup->b_full_width ? lw : child->desired.width;
		zrn_arrange(child, zrn_rect(0, 0, width, child->desired.height));
		origin = zrn_to_root(&popup->base, (ZPoint){ .x = 0, .y = 0 });
		pos.x = popup->h_offset;
		pos.y = popup->v_offset;
		if (popup->target != Z_INVALID_ID) {
			const zrn_element_t* const target = zrn_lookup(popup->target);
			if (target) {
				const ZPoint t = zrn_to_root(target, (ZPoint){ .x = 0, .y = 0 });
				switch (popup->placement) {
					case Z_PLACE_TOP: pos.x += t.x; pos.y += t.y - child->desired.height; break;
					case Z_PLACE_LEFT: pos.x += t.x - width; pos.y += t.y; break;
					case Z_PLACE_RIGHT: pos.x += t.x + target->rect.width; pos.y += t.y; break;
					default: pos.x += t.x; pos.y += t.y + target->rect.height; break;
				}
				/* flip to the other side when it would leave the screen */
				if (popup->placement == Z_PLACE_BOTTOM && pos.y + child->desired.height > lh && t.y - child->desired.height >= 0) pos.y = t.y - child->desired.height;
				if (popup->placement == Z_PLACE_TOP && pos.y < 0 && t.y + target->rect.height + child->desired.height <= lh) pos.y = t.y + target->rect.height;
			}
		}
		if (popup->b_full_width) pos.x = 0;
		pos.x = zrn_clampf(pos.x, 0, zrn_maxf(0, lw - width));
		pos.y = zrn_clampf(pos.y, 0, zrn_maxf(0, lh - child->desired.height));
		child->rect.x += pos.x - origin.x;
		child->rect.y += pos.y - origin.y;
	}
}

void Zroot_update_layout(const Zid root) {
	zrn_root_t* const r = get_root(root);
	int pass;
	float lw, lh;
	if (!r) return;
	/* the layout grid is the window divided by the scale: at 720 x 1280 and a scale of 1.5
	 * the page is laid out in the phone grid of 480 x 800, the painter draws it x 1.5 */
	lw = (float)r->width / r->ui;
	lh = zrn_maxf(0, ((float)r->height - r->input_pane) / r->ui);
	/* a pass can change what the next one needs (scroll bars appear): up to a few */
	for (pass = 0; pass < 4; pass++) {
		r->b_layout_dirty = false;
		if (r->visual) {
			zrn_measure(r->visual, zrn_size(lw, lh));
			zrn_arrange(r->visual, zrn_rect(0, 0, lw, lh));
		}
		layout_popups(r);
		if (!r->b_layout_dirty) break;
	}
}


/* ============================================================================
 *  painting
 * ============================================================================ */

/* What an element paints at the painter's origin: itself, its children, what goes over them, inside its clip. */
/* ============================================================================
 *  zrn-frosted-diffusion (Zelement_set_frosted_diffusion)
 * ============================================================================ */

/* the effect's state of one element: its strip and blur (GPU), what was under it when they were made (sig) and whether
 * anything but the background was, and where it was in the root the last frame (logical) */
struct zrn_backdrop {
	uint32_t sig;
	bool b_valid, b_hit;
	ZRect at;
	/** its region of the shared strip and blur (texture pixels, y down from the top of the strip) */
	int rx, ry, rw, rh;
};

/* the strips of all the frosted elements, one above the other with a gap, painted in one pass and blurred in one: two small
 * render passes a frame however many there are (three in a chat), not two each */
/* the device rectangles of this frame's opaque frosted elements (NORMAL, fully opaque, not turned): a scrolled row wholly
 * behind one is not painted in the frame (it is in the strip), so a list running under a header, a composer and an app bar
 * costs no more than one cut at them */
static float g_bd_cover[16][4];
static int g_bd_cover_count;
/* how many ScrollViewers the painting is inside: only their content is culled behind a frosted element */
static int g_in_scroll;
/* how many bitmap caches are being painted into: their coordinates are the bitmap's, not the screen's, so nothing in them is
 * culled against the frosted elements (text was left out of rows near the top of a chat) */
static int g_in_layer;
static zrn_image_t* g_bd_strip;
static zrn_image_t* g_bd_blur;
static constexpr int BD_GAP = 8;

static constexpr int BD_MAX = 16;
/* the tint over the blur: enough that what the element draws on it reads as on a solid bar */
static constexpr float BD_VEIL = 0.75f;
static Zid g_backdrops[BD_MAX];
static int g_backdrop_count;
/* while the strip of a backdrop is painted: the element it is for, where painting stops (it and what is over it are not under it) */
static bool g_bd_pass, g_bd_stopped;
static const zrn_element_t* g_bd_stop;

void zrn_backdrop_free(zrn_element_t* const e) {
	for (int i = 0; i < g_backdrop_count; i++) {
		if (g_backdrops[i] == e->id) {
			g_backdrops[i] = g_backdrops[--g_backdrop_count];
			break;
		}
	}
	if (!e->backdrop_state) return;
	free(e->backdrop_state);
	e->backdrop_state = nullptr;
}

void Zelement_set_frosted_diffusion(const Zid element, const ZFrostedDiffusion effect, const ZThemeResource tint) {
	zrn_element_t* const e = zrn_get(element, Z_CLASS_ELEMENT);
	if (!e) return;
	if (effect < Z_FROSTED_DIFFUSION_NONE || effect > Z_FROSTED_DIFFUSION_DESCENDING) {
		zrn_error(Z_ERROR_INVALID_ARGUMENT, "unknown frosted diffusion %d", (int)effect);
		return;
	}
	if (effect == Z_FROSTED_DIFFUSION_NONE) {
		zrn_backdrop_free(e);
	} else if (!e->backdrop) {
		if (g_backdrop_count >= BD_MAX) {
			zrn_error(Z_ERROR_INVALID_ARGUMENT, "zrn-frosted-diffusion: at most %d elements have a backdrop effect", BD_MAX);
			return;
		}
		g_backdrops[g_backdrop_count++] = e->id;
	}
	e->backdrop = (uint8_t)effect;
	e->backdrop_tint = (int)tint;
	/* a page gives such a bar no room, a list overlapped by it gets room at its end, the bar paints no background */
	zrn_changed_layout(e);
}

ZFrostedDiffusion Zelement_get_frosted_diffusion(const Zid element) {
	const zrn_element_t* const e = zrn_get(element, Z_CLASS_ELEMENT);
	return e ? (ZFrostedDiffusion)e->backdrop : Z_FROSTED_DIFFUSION_NONE;
}

/* the system-wide switch (the shell's `frost` key): off, backdrops are their tint alone, with no strip and no blur */
static bool g_frost_enabled = true;

void Zset_frosted_diffusion_enabled(const bool enabled) {
	if (g_frost_enabled == enabled) return;
	g_frost_enabled = enabled;
	g_bd_cover_count = 0;
	for (int i = 0; i < g_backdrop_count; i++) {
		zrn_element_t* const b = zrn_lookup(g_backdrops[i]);
		if (!b) continue;
		if (b->backdrop_state) b->backdrop_state->b_valid = false;
		if (b->root) b->root->b_paint_dirty = true;
	}
}

bool Zfrosted_diffusion_enabled(void) { return g_frost_enabled; }

static bool bd_shown(const zrn_element_t* e) {
	for (; e; e = e->parent) if (e->visibility != Z_VISIBLE || e->opacity <= 0.003f) return false;
	return true;
}

static ZRect bd_rect(const zrn_element_t* const e) {
	const ZPoint o = zrn_to_root(e, (ZPoint){ .x = 0, .y = 0 });
	return zrn_rect(o.x, o.y, e->rect.width, e->rect.height);
}

static bool bd_inside(const zrn_element_t* e, const zrn_element_t* const ancestor) {
	for (; e; e = e->parent) if (e == ancestor) return true;
	return false;
}

void zrn_backdrop_insets(zrn_element_t* const sv, float* const top, float* const bottom) {
	*top = *bottom = 0;
	if (!g_backdrop_count || !sv->root) return;
	const ZRect s = bd_rect(sv);
	const float sy0 = s.y, sy1 = s.y + s.height;
	/* a bar laid over the end, then what is laid over the end of that (a composer over an app bar) */
	for (int pass = 0; pass < 4; pass++) {
		bool b_grew = false;
		for (int i = 0; i < g_backdrop_count; i++) {
			const zrn_element_t* const b = zrn_lookup(g_backdrops[i]);
			if (!b || b->root != sv->root || bd_inside(b, sv) || !bd_shown(b)) continue;
			const ZRect r = bd_rect(b);
			if (r.width <= 0 || r.height <= 0 || r.x >= s.x + s.width || r.x + r.width <= s.x) continue;
			const float by0 = r.y, by1 = r.y + r.height;
			/* an element in the lower half of the viewer is over its bottom end, else over its top */
			if (by0 + by1 > sy0 + sy1) {
				const float end = sy1 - *bottom;
				if (by0 < end && by1 >= end - 0.5f && by1 <= sy1 + 0.5f && sy1 - by0 > *bottom + 0.25f) { *bottom = sy1 - by0; b_grew = true; }
			} else {
				const float end = sy0 + *top;
				if (by1 > end && by0 <= end + 0.5f && by0 >= sy0 - 0.5f && by1 - sy0 > *top + 0.25f) { *top = by1 - sy0; b_grew = true; }
			}
		}
		if (!b_grew) break;
	}
	*top = zrn_minf(*top, s.height * 0.5f);
	*bottom = zrn_minf(*bottom, s.height * 0.5f);
}

/* What lies under a backdrop, known without painting it: the leaves (and the rows with a bitmap cache) that reach into its
 * strip, by where they are, how opaque, turned and how often they changed (paint_gen). hit: one of them does. */
typedef struct bd_walk {
	const zrn_element_t* stop;
	bool b_stopped, b_hit;
	uint32_t sig;
	float scale;
} bd_walk_t;

static void bd_mix(bd_walk_t* const w, const void* const data, const size_t n) {
	const uint8_t* const b = (const uint8_t*)data;
	for (size_t i = 0; i < n; i++) w->sig = (w->sig ^ b[i]) * 16777619u;
}

static void bd_walk(bd_walk_t* const w, zrn_element_t* const e, const float ox, const float oy, float alpha, const float clip_in[4]) {
	if (w->b_stopped) return;
	if (e == w->stop) {
		w->b_stopped = true;
		return;
	}
	if (e->visibility != Z_VISIBLE || e->b_is_popup_child) return;
	alpha *= e->opacity;
	if (alpha <= 0.003f) return;
	const float x = ox + e->rect.x * w->scale, y = oy + e->rect.y * w->scale;
	const float x1 = x + e->rect.width * w->scale, y1 = y + e->rect.height * w->scale;
	float clip[4] = { clip_in[0], clip_in[1], clip_in[2], clip_in[3] };
	if (e->b_clip_to_bounds) {
		clip[0] = zrn_maxf(clip[0], x);
		clip[1] = zrn_maxf(clip[1], y);
		clip[2] = zrn_minf(clip[2], x1);
		clip[3] = zrn_minf(clip[3], y1);
	}
	if (clip[2] <= clip[0] || clip[3] <= clip[1]) return;
	const bool b_in = x < clip[2] && x1 > clip[0] && y < clip[3] && y1 > clip[1];
	if (e->b_transformed) {
		/* a turn or a slide: where its content lands is not followed here, so it counts as under and as changed */
		const float xf[6] = { e->translate_x, e->translate_y, e->render_scale, e->scale_y, e->rot_x, e->rot_y };
		bd_mix(w, xf, sizeof xf);
		w->b_hit = true;
	}
	const bool b_leaf = e->child_count == 0 || e->cache_mode == Z_CACHE_BITMAP;
	if (b_in && (e->cls->paint || e->cache_mode == Z_CACHE_BITMAP)) {
		const float k[6] = { x, y, x1, y1, alpha, (float)e->paint_gen };
		bd_mix(w, &e->id, sizeof e->id);
		bd_mix(w, k, sizeof k);
		/* a container's own background (a page's) is the background, not something on it */
		if (b_leaf) w->b_hit = true;
	}
	if (b_leaf && (e->cache_mode == Z_CACHE_BITMAP || e->child_count == 0)) return;
	int count, heap;
	zrn_element_t** const list = zrn_ordered(e, &count, &heap);
	for (int i = 0; i < count && !w->b_stopped; i++) bd_walk(w, list[i], x, y, alpha, clip);
	zrn_ordered_free(list, heap);
}

static ZColor bd_under(const zrn_root_t* const r) { return r->theme.b_has_background ? r->theme.background : r->theme.colors[ZRN_TC_BACKGROUND]; }

/* Before the frame: each backdrop of the root whose underside changed gets its strip painted (the root's tree up to the
 * element, in the frame's own way, cached rows drawn from their textures) and blurred; one that has only the background under
 * it gets nothing. Before the frame's first draw, so the frame stays one render pass on a tiler (no read back, no split). */
static void backdrops_prepare(zrn_root_t* const r, zrn_painter_t* const p);

static void paint_content(zrn_element_t* const e, zrn_painter_t* const p, zrn_root_t* const r) {
	int count, heap;
	if (e->b_clip_to_bounds) zrn_push_clip(p, zrn_local_rect(e));
	if (e->cls->paint) e->cls->paint(e, p);
	const bool b_scroll = e->cls->id == Z_CLASS_SCROLL_VIEWER;
	g_in_scroll += b_scroll;
	zrn_element_t** list = zrn_ordered(e, &count, &heap);
	for (int i = 0; i < count; i++) if (!list[i]->b_is_popup_child) zrn_paint_element(list[i], p, r);
	g_in_scroll -= b_scroll;
	zrn_ordered_free(list, heap);
	if (e->cls->paint_over && !(g_bd_pass && g_bd_stopped)) e->cls->paint_over(e, p);
	if (e->b_clip_to_bounds) zrn_pop_clip(p);
}

uint32_t zrn_cache_paints;
uint32_t zrn_upload_kb_this_frame;
bool zrn_upload_deferred;
extern ZRenderStats zrn_render_stats;

/* New bitmaps made in one frame at most: a list that fills at once (the chats arriving at launch: fifteen rows, 95 ms in one
 * frame) paints the rest directly and caches them over the next frames. */
static constexpr int ZRN_CACHE_NEW_PER_FRAME = 3;
static int g_cache_new_this_frame;
static bool g_cache_deferred;

/* The largest side of a cached bitmap in device pixels: a bigger element is painted directly. */
static constexpr int ZRN_CACHE_MAX_SIDE = 2048;

/* CacheMode = BitmapCache: the subtree is painted into its texture when that is stale, then the texture is drawn. false when the
 * renderer has no layers or the element is too big: it is then painted directly. */
static bool paint_cached(zrn_element_t* const e, zrn_painter_t* const p, zrn_root_t* const r) {
	if (!p->be->layer_begin || !r) return false;
	const int w = (int)ceilf(e->rect.width * p->scale - 0.01f);
	const int h = (int)ceilf(e->rect.height * p->scale - 0.01f);
	if (w <= 0 || h <= 0) return true;
	if (w > ZRN_CACHE_MAX_SIDE || h > ZRN_CACHE_MAX_SIDE) return false;
	const zrn_image_t* const have = e->cache;
	const bool b_fresh = have && e->b_cache_valid && have->gl_owner == p->backend_data && have->width == w && have->height == h &&
		e->cache_scale == p->scale && e->cache_epoch == r->cache_epoch && (!p->be->layer_drawable || p->be->layer_drawable(p, have));
	/* a strip under a backdrop (a quarter of the frame's resolution) makes no bitmap: one made for the frame is drawn into it
	 * scaled down (gl_draw_layer), what has none yet is painted directly */
	if (g_bd_pass) {
		if (!(have && e->b_cache_valid && have->gl_owner == p->backend_data && e->cache_epoch == r->cache_epoch)) return false;
		p->be->draw_layer(p, have);
		return true;
	}
	if (!b_fresh && g_cache_new_this_frame >= ZRN_CACHE_NEW_PER_FRAME) {
		g_cache_deferred = true;
		return false;
	}
	if (!b_fresh) {
		g_cache_new_this_frame++;
		alignas(max_align_t) uint8_t saved[ZRN_LAYER_SAVE];
		if (!p->be->layer_begin(p, &e->cache, w, h, saved)) return false;
		/* marked fresh before painting: a paint hook that asks for another frame (zrn_changed_paint) makes it stale again */
		e->b_cache_valid = true;
		e->paint_gen++;
		e->cache_scale = p->scale;
		e->cache_epoch = r->cache_epoch;
		zrn_cache_paints++;
		static int b_layer_log = -1;
		if (b_layer_log < 0) b_layer_log = getenv("ZRNUI_LAYER_LOG") != nullptr;
		if (b_layer_log) fprintf(stderr, "zrnui-layer %s %dx%d at %.0f,%.0f depth-xf=%d\n", e->cls->name, w, h, p->tx, p->ty, (int)p->b_has_xf);
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		const bool b_upload_was_deferred = zrn_upload_deferred;
		zrn_upload_deferred = false;
		g_in_layer++;
		paint_content(e, p, r);
		g_in_layer--;
		/* a picture in it waits for its upload: the bitmap is made again next frame, with it */
		if (zrn_upload_deferred) e->b_cache_valid = false;
		zrn_upload_deferred = zrn_upload_deferred || b_upload_was_deferred;
		p->be->layer_end(p, saved);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		zrn_render_stats.layer_ms += (float)((double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
	}
	p->be->draw_layer(p, e->cache);
	return true;
}

void zrn_paint_element(zrn_element_t* const e, zrn_painter_t* const p, zrn_root_t* const r) {
	float tx = p->tx, ty = p->ty, alpha = p->alpha;
	float saved_xf[16] = {};
	const int saved_has_xf = p->b_has_xf;
	if (g_bd_pass) {
		if (g_bd_stopped) return;
		if (e == g_bd_stop) {
			g_bd_stopped = true;
			return;
		}
	}
	if (e->visibility != Z_VISIBLE) return;
	/* an element is skipped when it is out of view, unless it holds things that may be in view: a zero-sized Canvas whose children overflow */
	/* a bitmap cache holds only its own box: out of view it has nothing to show, and painting it would make a layer for
	 * every row of a long list (sixty textures in the first frame of a chat) */
	if (zrn_clipped_out(p, e->rect) && (e->b_clip_to_bounds || e->child_count == 0 || e->cache_mode == Z_CACHE_BITMAP)) {
		/* out of view a bitmap cache gives its texture back: kept, every row a long list ever showed would hold one (a few MiB
		 * each at the phone's scale), and the GPU runs out of memory. Back in view the row is painted into a new one once. */
		if (e->cache_mode == Z_CACHE_BITMAP && e->cache && !g_bd_pass) {
			zrn_image_free(e->cache);
			e->cache = nullptr;
			e->b_cache_valid = false;
		}
		return;
	}
	if (g_bd_cover_count && g_in_scroll && !g_in_layer && !g_bd_pass && !p->b_has_xf && !e->b_transformed && (e->child_count == 0 || e->cache_mode == Z_CACHE_BITMAP)) {
		const float x0 = p->tx + e->rect.x * p->scale, y0 = p->ty + e->rect.y * p->scale;
		const float x1 = x0 + e->rect.width * p->scale, y1 = y0 + e->rect.height * p->scale;
		for (int i = 0; i < g_bd_cover_count; i++) {
			const float* const c = g_bd_cover[i];
			if (x0 >= c[0] - 0.5f && y0 >= c[1] - 0.5f && x1 <= c[2] + 0.5f && y1 <= c[3] + 0.5f) return;
		}
	}
	p->tx += e->rect.x * p->scale;
	p->ty += e->rect.y * p->scale;
	p->alpha *= e->opacity;
	/* read once: the transform is pushed and popped as a pair, whatever the paint handlers do to the element */
	const bool b_is_transformed = e->b_transformed;
	/* ZRNUI_XF_LOG: the elements still turned, scaled or moved when the frame is painted at rest (the settle frame) */
	if (b_is_transformed && r && r->b_settle_frame) {
		static int b_xf_log = -1;
		if (b_xf_log < 0) b_xf_log = getenv("ZRNUI_XF_LOG") != nullptr;
		if (b_xf_log) fprintf(stderr, "zrnui-xf-at-rest %s id=%u rect=%.0f,%.0f %.0fx%.0f rx=%.2f ry=%.2f s=%.3f tx=%.1f ty=%.1f c=%.2f,%.2f\n",
			e->cls->name, e->id, e->rect.x, e->rect.y, e->rect.width, e->rect.height, e->rot_x, e->rot_y, e->render_scale,
			e->translate_x, e->translate_y, e->proj_cx, e->proj_cy);
	}
	if (b_is_transformed) {
		/* the transform of this element, about its centre of projection, under the one of its parent */
		float own[16];
		memcpy(saved_xf, p->xf, sizeof saved_xf);
		zrn_xf_of_element(e, p->tx, p->ty, e->rect.width * p->scale, e->rect.height * p->scale, p->scale, own);
		zrn_xf_mul(p->xf, p->xf, own);
		p->b_has_xf = 1;
		p->be->set_transform(p, p->xf);
	}
	/* a cached row is drawn from its bitmap under a transform too (a turnstile, a tilt, a list sliding): the panfrost job
	 * faults that once smeared rows made under one are fixed in the kernel */
	if (e->backdrop && !g_bd_pass && r) {
		const struct zrn_backdrop* const st = e->backdrop_state;
		ZColor tint;
		if (!zrn_res_color(r, (ZThemeResource)e->backdrop_tint, &tint)) tint = r->theme.colors[ZRN_TC_CHROME];
		const ZRect area = zrn_rect(0, 0, e->rect.width, e->rect.height);
		const bool b_blur = st && st->b_hit && st->b_valid && g_bd_blur;
		/* its region of the blur, in texture coordinates (v up): the element is the top left of it */
		float uv[4] = { 0, 0, 1, 1 };
		if (b_blur) {
			const float tw = (float)g_bd_blur->width, th = (float)g_bd_blur->height, q = p->scale * 0.25f;
			uv[0] = (float)st->rx / tw;
			uv[1] = 1.0f - (float)st->ry / th;
			uv[2] = e->rect.width * q / tw;
			uv[3] = e->rect.height * q / th;
		}
		if (!g_frost_enabled) zrn_fill_rect(p, area, tint);
		else if (p->be->backdrop_draw) p->be->backdrop_draw(p, b_blur ? g_bd_blur : nullptr, uv, area, e->backdrop, tint, bd_under(r), BD_VEIL);
		else zrn_fill_rect(p, area, tint);
	}
	if (e->cache_mode != Z_CACHE_BITMAP || !paint_cached(e, p, r)) paint_content(e, p, r);
	if (b_is_transformed) {
		memcpy(p->xf, saved_xf, sizeof saved_xf);
		p->b_has_xf = saved_has_xf;
		p->be->set_transform(p, saved_has_xf ? p->xf : nullptr);
	}
	p->tx = tx;
	p->ty = ty;
	p->alpha = alpha;
}

static void backdrops_prepare(zrn_root_t* const r, zrn_painter_t* const p) {
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	zrn_element_t* todo[BD_MAX];
	uint32_t sigs[BD_MAX];
	int n = 0;
	bool b_changed = false;
	const float s = p->scale, q = s * 0.25f;
	g_bd_cover_count = 0;
	const ZColor under = bd_under(r);
	for (int i = 0; i < g_backdrop_count; i++) {
		zrn_element_t* const b = zrn_lookup(g_backdrops[i]);
		if (!b || b->root != r || !r->visual) continue;
		if (!b->backdrop_state) b->backdrop_state = (struct zrn_backdrop*)calloc(1, sizeof *b->backdrop_state);
		struct zrn_backdrop* const st = b->backdrop_state;
		if (!st) continue;
		const bool b_was_hit = st->b_hit;
		st->b_hit = false;
		if (!bd_shown(b)) continue;
		const ZRect at = bd_rect(b);
		if (at.x != st->at.x || at.y != st->at.y || at.width != st->at.width || at.height != st->at.height) {
			/* it moved or changed size: the lists it overlaps take their room again (zrn_backdrop_insets) */
			st->at = at;
			st->b_valid = false;
			r->b_layout_dirty = true;
		}
		if (!p->be->backdrop_begin || at.width <= 0 || at.height <= 0) continue;
		if (b->backdrop == Z_FROSTED_DIFFUSION_NORMAL && p->be->backdrop_draw) {
			bool b_opaque = true;
			for (const zrn_element_t* a = b; a && b_opaque; a = a->parent) b_opaque = a->opacity >= 0.999f && !a->b_transformed;
			if (b_opaque) {
				float* const c = g_bd_cover[g_bd_cover_count++];
				c[0] = at.x * s;
				c[1] = at.y * s;
				c[2] = (at.x + at.width) * s;
				c[3] = (at.y + at.height) * s;
			}
		}
		const float bx = floorf(at.x * s), by = floorf(at.y * s);
		bd_walk_t walk = { .stop = b, .sig = 2166136261u, .scale = s };
		const float clip[4] = { bx, by, bx + at.width * s, by + at.height * s };
		bd_walk(&walk, r->visual, 0, 0, 1.0f, clip);
		bd_mix(&walk, &r->cache_epoch, sizeof r->cache_epoch);
		bd_mix(&walk, &under, sizeof under);
		if (!walk.b_hit) continue;
		st->b_hit = true;
		if (!b_was_hit || !st->b_valid || st->sig != walk.sig) b_changed = true;
		sigs[n] = walk.sig;
		todo[n++] = b;
	}
	if (n && b_changed) {
		/* one strip for all of them, each in its own band, painted at a quarter of the frame's resolution */
		int width = 1, height = 0;
		for (int i = 0; i < n; i++) {
			struct zrn_backdrop* const st = todo[i]->backdrop_state;
			st->rx = 0;
			st->ry = height;
			st->rw = (int)ceilf(st->at.width * q);
			st->rh = (int)ceilf(st->at.height * q);
			width = st->rw > width ? st->rw : width;
			height += st->rh + BD_GAP;
		}
		alignas(max_align_t) uint8_t saved[ZRN_LAYER_SAVE];
		if (p->be->backdrop_begin(p, &g_bd_strip, width, height, under, saved)) {
			for (int i = 0; i < n; i++) {
				struct zrn_backdrop* const st = todo[i]->backdrop_state;
				p->scale = q;
				p->tx = -floorf(st->at.x * q) + (float)st->rx;
				p->ty = -floorf(st->at.y * q) + (float)st->ry;
				p->alpha = 1.0f;
				p->clip[0] = st->rx;
				p->clip[1] = st->ry;
				p->clip[2] = st->rx + st->rw;
				p->clip[3] = st->ry + st->rh;
				p->clip_depth = 0;
				g_bd_pass = true;
				g_bd_stop = todo[i];
				g_bd_stopped = false;
				zrn_paint_element(r->visual, p, r);
				g_bd_pass = false;
				g_bd_stop = nullptr;
				zrn_render_stats.backdrop_passes++;
			}
			p->be->backdrop_end(p, g_bd_strip, &g_bd_blur, saved);
			for (int i = 0; i < n; i++) {
				struct zrn_backdrop* const st = todo[i]->backdrop_state;
				st->sig = sigs[i];
				st->b_valid = g_bd_blur != nullptr;
			}
		} else {
			for (int i = 0; i < n; i++) todo[i]->backdrop_state->b_hit = false;
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	zrn_render_stats.backdrop_ms += (float)((double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
}

static void paint_tooltip(const zrn_root_t* const r, zrn_painter_t* const p) {
	const zrn_font_t font = { .family = Z_FONT_NORMAL, .size = ZRN_FONT_SMALL, .spacing = 0 };
	float w, h, x, y;
	if (!r->tooltip_target || !r->tooltip_target->tooltip || r->tooltip_timer < 500.0f) return;
	zrn_text_layout_t* l = zrn_text_layout_new(font, r->tooltip_target->tooltip, (int)strlen(r->tooltip_target->tooltip), 300.0f, 1, 0, 0);
	if (!l) return;
	w = l->width + 16;
	h = l->height + 12;
	x = zrn_clampf(r->pointer_x, 0, (float)r->width / r->ui - w);
	y = r->pointer_y + 28;
	if (y + h > (float)r->height / r->ui) y = r->pointer_y - h - 8;
	p->tx = 0; p->ty = 0; p->alpha = 1;
	zrn_fill_rect(p, zrn_rect(x, y, w, h), zrn_theme_color(nullptr, ZRN_TC_CHROME));
	zrn_stroke_rect(p, zrn_rect(x, y, w, h), Zthickness_uniform(1), r->theme.colors[ZRN_TC_BORDER]);
	zrn_draw_text(p, l, x + 8, y + 6, l->width, Z_TEXT_LEFT, Z_TRIM_NONE, r->theme.colors[ZRN_TC_FOREGROUND]);
	zrn_text_layout_free(l);
}

bool zrn_root_paint(zrn_root_t* const r, zrn_painter_t* const p) {
	if (r->b_layout_dirty || r->b_first_frame) Zroot_update_layout(r->id);
	if (!(r->b_paint_dirty || r->b_first_frame)) return false;
	g_cache_new_this_frame = 0;
	g_cache_deferred = false;
	zrn_upload_kb_this_frame = 0;
	zrn_upload_deferred = false;
	g_bd_cover_count = 0;
	if (g_backdrop_count && g_frost_enabled) backdrops_prepare(r, p);
	zrn_paint_clear(p, r->theme.b_has_background ? r->theme.background : r->theme.colors[ZRN_TC_BACKGROUND]);
	if (r->visual) zrn_paint_element(r->visual, p, r);
	for (int i = 0; i < r->popup_count; i++) {
		zrn_element_t* const popup = r->popups[i];
		const ZPoint origin = zrn_to_root(popup, (ZPoint){ .x = 0, .y = 0 });
		if (!popup->child_count) continue;
		if (ZRN_AS(zrn_popup_t, popup)->b_modal) {
			/* ContentDialogDimmingThemeBrush over everything under the dialog */
			p->tx = 0;
			p->ty = 0;
			p->alpha = 1.0f;
			zrn_fill_rect(p, zrn_rect(0, 0, (float)r->width / r->ui, (float)r->height / r->ui), r->theme.colors[ZRN_TC_CONTENT_DIALOG_DIMMING]);
		}
		p->tx = origin.x * r->scale;
		p->ty = origin.y * r->scale;
		p->alpha = 1.0f;
		zrn_paint_element(popup->children[0], p, r);
	}
	paint_tooltip(r, p);
	/* bitmaps left for later: another frame makes the next few */
	r->b_paint_dirty = g_cache_deferred || zrn_upload_deferred;
	r->b_first_frame = false;
	if (!r->b_settle_frame) r->b_settle_armed = true;
	r->b_settle_frame = false;
	r->settle_quiet_ms = 0;
	return true;
}

static const char* g_last_renderer = "software";

void zrn_note_renderer(const char* name) { g_last_renderer = name; }

/* A frame on the off-screen GPU context, read back into the pixels of the root. false when there is none. */
static bool render_gpu(zrn_root_t* const r, const int w, const int h) {
	zrn_painter_t p;
	const bool b_sized = r->pixels_w == w && r->pixels_h == h && r->pixels;
	if (!r->gpu) r->gpu = zrn_gl_create_offscreen(w, h);
	if (!r->gpu) return false;
	if (!b_sized) {
		free(r->pixels);
		r->pixels = (uint8_t*)calloc((size_t)w * (size_t)h, 4);
		r->pixels_w = w;
		r->pixels_h = h;
		if (!r->pixels) return false;
	}
	zrn_gl_begin(r->gpu, &p, w, h, r->scale);
	zrn_root_paint(r, &p);
	zrn_gl_end(r->gpu, 0);
	if (!zrn_gl_read(r->gpu, r->pixels, w, h)) return false;
	g_last_renderer = "gles2";
	return true;
}

bool Zroot_render(const Zid root, ZFrame* const frame) {
	zrn_root_t* const r = get_root(root);
	zrn_painter_t p;
	int w, h;
	bool b_changed;
	if (!r) return false;
	/* the buffer follows the density of the window, not the UI scale: the painter's scale (ui times density) fills it exactly */
	w = (int)ceilf((float)r->width * r->density);
	h = (int)ceilf((float)r->height * r->density);
	if (r->b_layout_dirty || r->b_first_frame) Zroot_update_layout(root);
	if (w <= 0 || h <= 0) return false; /* a root with no size has nothing to draw: the frame is left as it is */
	b_changed = r->b_paint_dirty || r->b_first_frame || r->pixels_w != w || r->pixels_h != h;
	if (b_changed) {
		if (r->renderer == Z_RENDERER_GPU) {
			r->b_paint_dirty = true;
			if (!render_gpu(r, w, h)) {
				zrn_error(Z_ERROR_IO, "no OpenGL ES 2 context for Z_RENDERER_GPU");
				return false;
			}
		} else {
			if (r->pixels_w != w || r->pixels_h != h || !r->pixels) {
				free(r->pixels);
				r->pixels = (uint8_t*)calloc((size_t)w * (size_t)h, 4);
				r->pixels_w = w;
				r->pixels_h = h;
				if (!r->pixels) {
					zrn_error(Z_ERROR_OUT_OF_MEMORY, "out of memory");
					return false;
				}
			}
			zrn_painter_init(&p, r->pixels, w, h, r->scale);
			r->b_paint_dirty = true;
			zrn_root_paint(r, &p);
			g_last_renderer = "software";
		}
	}
	if (frame) {
		frame->pixels = r->pixels;
		frame->width = r->pixels_w;
		frame->height = r->pixels_h;
		frame->stride = r->pixels_w * 4;
	}
	return b_changed;
}

void Zroot_set_renderer(const Zid root, const ZRenderer renderer) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	r->renderer = renderer;
	r->b_paint_dirty = true;
}

ZRenderer Zroot_get_renderer(const Zid root) {
	const zrn_root_t* const r = get_root(root);
	return r ? r->renderer : Z_RENDERER_AUTO;
}

const char* Zroot_get_renderer_name(const Zid root) {
	(void)root;
	return g_last_renderer;
}


/* ============================================================================
 *  time
 * ============================================================================ */

uint32_t zrn_tick_epoch = 1;

/* Ticks a subtree; false when nothing in it ticks any more. Such a subtree is marked with the epoch and skipped until the
 * epoch moves (zrn_want_tick, a tween, a new child): a chat of hundreds of bubbles is not walked sixty times a second. */
/* Zroot_is_animating: what moved in this tick, leaving out the controls that tick for as long as they are shown (an
 * indeterminate progress bar or ring, a caret blinking); a program waiting for the animations to end would wait on them
 * for ever. */
static bool zrn_tick_moved;

static bool zrn_ticks_forever(const zrn_element_t* const e) {
	return zrn_is_a(e, Z_CLASS_PROGRESS_BAR) || zrn_is_a(e, Z_CLASS_PROGRESS_RING) || zrn_is_a(e, Z_CLASS_TEXT_BOX) ||
		zrn_is_a(e, Z_CLASS_PASSWORD_BOX);
}

static bool zrn_tick_tree(zrn_element_t* const e, const float dt) {
	if (e->tick_quiet_epoch == zrn_tick_epoch) return false;
	if (e->tweens) zrn_tweens_tick(e);
	if (e->b_wants_tick && e->cls->tick) e->cls->tick(e, dt);
	/* a class without a tick that asks for one is not ticking */
	bool b_busy = e->tweens != nullptr || (e->b_wants_tick && e->cls->tick);
	if (e->tweens || (e->b_wants_tick && e->cls->tick && !zrn_ticks_forever(e))) zrn_tick_moved = true;
	for (int i = 0; i < e->child_count; i++) b_busy |= zrn_tick_tree(e->children[i], dt);
	if (!b_busy) e->tick_quiet_epoch = zrn_tick_epoch;
	return b_busy;
}

/* ZRNUI_ANIMATION_SPEED: a float multiplier for every animation (0.5 = half speed,
 * 2 = twice as fast; 0 or negative disables the speed override and leaves 1). */
static ZColor zrn_lerp_color(const ZColor a, const ZColor b, const float t) {
	const ZColor c = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
	return c;
}

static void zrn_theme_lerp(zrn_theme_t* const out, const zrn_theme_t* const a, const zrn_theme_t* const b, const float t) {
	out->theme = t >= 0.5f ? b->theme : a->theme;
	out->accent = zrn_lerp_color(a->accent, b->accent, t);
	out->background = zrn_lerp_color(a->background, b->background, t);
	out->b_has_background = b->b_has_background;
	for (int i = 0; i < ZRN_TC_COUNT; i++) out->colors[i] = zrn_lerp_color(a->colors[i], b->colors[i], t);
}

static float zrn_animation_speed(void) {
	const float speed = zrn_config()->animation_speed;
	return speed > 0 ? speed : 1.0f;
}
/* WP's Holding: 500 ms without moving more than 9.5 px. */
static constexpr float ZRN_HOLD_MS = 500.0f;
static constexpr float ZRN_HOLD_SLOP = 9.5f;

static void hold_fire(zrn_root_t* r);

bool Zroot_is_animating(const Zid root) {
	const zrn_root_t* const r = get_root(root);
	return r && r->b_animating;
}

void Zroot_tick(const Zid root, const float delta_ms) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	r->time_ms += delta_ms;
	float speed = zrn_animation_speed();
	/* cross-fade of the theme: the displayed theme moves from theme_from to theme_target */
	if (r->b_theme_transitioning) {
		r->theme_t += (delta_ms * speed) / 250.0f;
		if (r->theme_t >= 1.0f) {
			r->theme_t = 1.0f;
			r->b_theme_transitioning = false;
			r->theme = r->theme_target;
			/* the bindings were applied when the fade began, with the theme it came from: the resources of the theme it
			 * reached go to them now (a bound foreground stayed white on a light page) */
			zrn_root_resources_changed(r);
		} else {
			zrn_theme_lerp(&r->theme, &r->theme_from, &r->theme_target, r->theme_t);
		}
		r->b_paint_dirty = true;
		r->cache_epoch++;
	}
	zrn_tilt_tick(r, delta_ms * speed);
	if (r->b_hold_armed) {
		r->hold_ms += delta_ms;
		if (r->hold_ms >= ZRN_HOLD_MS) {
			hold_fire(r);
			if (!get_root(root)) return;
		}
	}
	/* a handler of a tick may destroy the root: it is looked up again after each tree */
	zrn_tick_moved = r->b_theme_transitioning;
	if (r->visual) zrn_tick_tree(r->visual, delta_ms * speed);
	if (!get_root(root)) return;
	if (r->overlay) zrn_tick_tree(r->overlay, delta_ms * speed);
	if (!get_root(root)) return;
	r->b_animating = zrn_tick_moved || r->capture != nullptr;
	if (r->tooltip_target && r->tooltip_target->tooltip) {
		const float before = r->tooltip_timer;
		r->tooltip_timer += delta_ms;
		if (before < 500.0f && r->tooltip_timer >= 500.0f) r->b_paint_dirty = true;
	}
	/* the settle frame (root.h): 400 ms after the last change, at rest */
	if (r->b_paint_dirty || r->b_layout_dirty) {
		r->settle_quiet_ms = 0;
	} else if (r->b_settle_armed) {
		r->settle_quiet_ms += delta_ms;
		if (r->settle_quiet_ms >= 400.0f) {
			r->b_settle_armed = false;
			/* the GPU faults that smeared rows are fixed in the kernel: nothing is repainted at rest any more */
		}
	}
}


/* ============================================================================
 *  input
 * ============================================================================ */

/* Bubbles an input from `e` up. Pointer positions are given in root coordinates in `in`.
 * The elements are walked by id: a handler may destroy any of them (a tap that opens another page). */
bool zrn_dispatch(zrn_element_t* const e, zrn_input_t* const in) {
	Zid ids[64];
	int n = zrn_chain(e, ids, 64), i;
	float rx = in->x, ry = in->y;
	const bool b_pointer = in->type >= ZRN_IN_POINTER_DOWN && in->type <= ZRN_IN_WHEEL;
	for (i = 0; i < n; i++) {
		zrn_element_t* const a = zrn_lookup(ids[i]);
		zrn_input_t local = *in;
		if (!a) return 1;
		if (!a->cls->input) continue;
		if (b_pointer) {
			ZPoint q = zrn_from_root(a, (ZPoint){ .x = rx, .y = ry });
			local.x = q.x;
			local.y = q.y;
			if (!a->b_enabled) continue;
		}
		if (a->cls->input(a, &local)) return 1;
	}
	return 0;
}

static zrn_element_t* pick(const zrn_root_t* const r, const float x, const float y) {
	for (int i = r->popup_count - 1; i >= 0; i--) {
		zrn_element_t* const popup = r->popups[i];
		zrn_element_t* const child = popup->child_count ? popup->children[0] : nullptr;
		ZPoint origin, q;
		if (!child) continue;
		origin = zrn_to_root(popup, (ZPoint){ .x = 0, .y = 0 });
		q.x = x - origin.x - child->rect.x;
		q.y = y - origin.y - child->rect.y;
		zrn_element_t* hit = zrn_hit_test(child, q);
		if (hit) return hit;
		/* a modal popup is all there is to reach under it */
		if (ZRN_AS(zrn_popup_t, popup)->b_modal) return nullptr;
	}
	if (r->visual) return zrn_hit_test(r->visual, (ZPoint){ x - r->visual->rect.x, y - r->visual->rect.y });
	return nullptr;
}

static bool is_inside(const zrn_element_t* e, const zrn_element_t* ancestor) {
	for (; e; e = e->parent) if (e == ancestor) return 1;
	return 0;
}

/* A press outside the open popups closes the ones that do not stay open. Returns 1 when the press was on the owner of a closed popup. */
static bool close_outside(const zrn_root_t* const r, const zrn_element_t* const target) {
	int i;
	bool b_swallow = false;
	for (i = r->popup_count - 1; i >= 0; i--) {
		const auto popup = ZRN_AS(zrn_popup_t, r->popups[i]);
		if (popup->b_stays_open) continue;
		if (target && is_inside(target, &popup->base)) continue;
		if (target && popup->base.parent && is_inside(target, popup->base.parent)) b_swallow = true;
		/* a tap on the element a flyout is shown at closes it: it does not open it again */
		if (target && popup->target != Z_INVALID_ID && zrn_lookup(popup->target) && is_inside(target, zrn_lookup(popup->target))) b_swallow = true;
		zrn_popup_close(&popup->base);
		i = r->popup_count;
	}
	return b_swallow;
}

static bool in_a_popup(const zrn_root_t* const r, const zrn_element_t* const e) {
	for (int i = 0; i < r->popup_count; i++) if (e && is_inside(e, r->popups[i])) return 1;
	return 0;
}

/* The Back button and Escape: the topmost thing that can be dismissed closes, and nothing else happens. */
bool zrn_root_dismiss_top(zrn_root_t* const r) {
	for (int i = r->popup_count - 1; i >= 0; i--) {
		const auto popup = ZRN_AS(zrn_popup_t, r->popups[i]);
		/* a modal popup (a ContentDialog) closes as if it were told to hide; a popup that stays open is the program's own */
		if (popup->b_stays_open && !popup->b_modal) continue;
		zrn_popup_close(&popup->base);
		return 1;
	}
	if (r->open_bar) {
		zrn_element_t* const bar = r->open_bar;
		if (!ZRN_AS(zrn_app_bar_t, bar)->b_is_sticky) {
			Zapp_bar_set_is_open(bar->id, false);
			return 1;
		}
	}
	return 0;
}

static void update_hover(zrn_root_t* const r, zrn_element_t* const target, const float x, const float y) {
	Zid old_ids[64], new_ids[64];
	int old_n, new_n, i, j;
	zrn_input_t in;
	if (target == r->hover) return;
	/* the hover is drawn (pointer-over looks): the cached bitmaps around the old and the new target are stale */
	zrn_cache_stale(r->hover);
	zrn_cache_stale(target);
	memset(&in, 0, sizeof in);
	in.x = x;
	in.y = y;
	old_n = r->hover ? zrn_chain(r->hover, old_ids, 64) : 0;
	new_n = target ? zrn_chain(target, new_ids, 64) : 0;
	r->hover = target;
	r->b_paint_dirty = true;
	/* the elements are walked by id: a Click-on-hover handler may destroy them */
	for (i = 0; i < old_n; i++) {
		bool b_stays = false;
		zrn_element_t* const a = zrn_lookup(old_ids[i]);
		for (j = 0; j < new_n; j++) if (new_ids[j] == old_ids[i]) b_stays = true;
		if (!a || b_stays) continue;
		a->b_pointer_over = 0;
		in.type = ZRN_IN_POINTER_LEAVE;
		if (a->cls->input) a->cls->input(a, &in);
	}
	for (i = 0; i < new_n; i++) {
		zrn_element_t* const a = zrn_lookup(new_ids[i]);
		if (!a || a->b_pointer_over) continue;
		a->b_pointer_over = 1;
		in.type = ZRN_IN_POINTER_ENTER;
		if (a->cls->input) a->cls->input(a, &in);
	}
}

static zrn_element_t* tooltip_owner(zrn_element_t* e) {
	for (; e; e = e->parent) if (e->tooltip) return e;
	return nullptr;
}

/* Lets the ancestors that observe the pointer see an event first. */
static void preview(const zrn_element_t* const target, const zrn_input_type_t type, const float x, const float y) {
	Zid ids[64];
	zrn_swipe_preview((zrn_element_t*)target, (int)type, x, y);
	if (!zrn_lookup(target->id)) return;
	int n = zrn_chain(target, ids, 64), i;
	for (i = 0; i < n; i++) {
		zrn_input_t in;
		zrn_element_t* const a = zrn_lookup(ids[i]);
		if (!a) return;
		if (!a->b_observes_pointer || !a->b_enabled || !a->cls->input) continue;
		memset(&in, 0, sizeof in);
		ZPoint q = zrn_from_root(a, (ZPoint){ .x = x, .y = y });
		in.type = type;
		in.x = q.x;
		in.y = q.y;
		a->cls->input(a, &in);
	}
}

/* ---- Holding ---- */

/* The innermost element at or above `e` that listens for Holding or has a context flyout. */
static zrn_element_t* hold_owner(zrn_element_t* e) {
	for (; e; e = e->parent) {
		if (!zrn_effectively_enabled(e)) return nullptr;
		if (e->handlers[Z_EVENT_HOLDING - Z_EVENT_CLICK].callback) return e;
		if (e->context_flyout != Z_INVALID_ID && zrn_lookup(e->context_flyout)) return e;
	}
	return nullptr;
}

/* The press has been held: the press is cancelled (no tilt, no click: whoever has the pointer loses it), then Holding is raised and the
 * context flyout shown. */
static void hold_fire(zrn_root_t* const r) {
	const Zid root = r->id;
	r->b_hold_armed = false;
	r->b_hold_fired = true;
	zrn_tilt_release(r);
	if (r->capture) zrn_set_capture(r, nullptr);
	if (!zrn_root_lookup(root)) return;
	zrn_element_t* e = zrn_lookup(r->hold_target);
	if (!e) return;
	const Zid id = e->id;
	if (!zrn_raise(e, Z_EVENT_HOLDING)) return;
	e = zrn_lookup(id);
	if (!e || e->context_flyout == Z_INVALID_ID || !zrn_lookup(e->context_flyout)) return;
	Zflyout_base_show_at(e->context_flyout, id);
}

/* The target again after something that can run user code: NULL if it is gone. */
#define RELOOK(id) ((id) != Z_INVALID_ID ? zrn_lookup(id) : nullptr)

bool Zroot_inject_pointer(const Zid root, const ZPointerEvent event, float x, float y) {
	zrn_root_t* r = get_root(root);
	zrn_input_t in;
	bool b_handled = false;
	if (!r) return false;
	if (r->b_layout_dirty) Zroot_update_layout(root);
	/* the window gives the pointer in its logical pixels; the layout thinks in the grid of the UI scale */
	x /= r->ui;
	y /= r->ui;
	r->pointer_x = x;
	r->pointer_y = y;
	memset(&in, 0, sizeof in);
	in.x = x;
	in.y = y;
	if (event == Z_POINTER_LEAVE) {
		update_hover(r, nullptr, x, y);
		return false;
	}
	zrn_element_t* target = r->capture ? r->capture : pick(r, x, y);
	Zid tid = target ? target->id : Z_INVALID_ID;
	if (event == Z_POINTER_MOVE) {
		zrn_element_t* hit = r->capture ? pick(r, x, y) : target;
		zrn_element_t* owner = tooltip_owner(hit);
		update_hover(r, hit, x, y);
		if (!zrn_root_lookup(root)) return false;
		if (owner != r->tooltip_target) {
			if (r->tooltip_timer >= 500.0f) r->b_paint_dirty = true;
			r->tooltip_target = owner && zrn_lookup(owner->id) ? owner : nullptr;
			r->tooltip_timer = 0;
		}
		zrn_tilt_move(r, x, y);
		if (r->b_hold_armed && hypotf(x - r->hold_x, y - r->hold_y) > ZRN_HOLD_SLOP) r->b_hold_armed = false;
		target = RELOOK(tid);
		if (target) preview(target, ZRN_IN_PREVIEW_MOVE, x, y);
		if (r->capture) target = r->capture;
		else target = RELOOK(tid);
		in.type = ZRN_IN_POINTER_MOVE;
		if (target) b_handled = zrn_dispatch(target, &in);
		return b_handled;
	}
	if (event == Z_POINTER_DOWN) {
		r->b_hold_armed = false;
		r->b_hold_fired = false;
		{
			const zrn_element_t* const owner = hold_owner(target);
			if (owner) {
				r->hold_target = owner->id;
				r->hold_x = x;
				r->hold_y = y;
				r->hold_ms = 0;
				r->b_hold_armed = true;
			}
		}
		r->tooltip_timer = 0;
		r->tooltip_target = nullptr;
		if (!r->capture && r->open_bar && !ZRN_AS(zrn_app_bar_t, r->open_bar)->b_is_sticky && !is_inside(target, r->open_bar) && !in_a_popup(r, target)) {
			/* light dismiss: the press that closes the bar is not given to what is under it */
			Zapp_bar_set_is_open(r->open_bar->id, false);
			close_outside(r, target);
			return true;
		}
		if (!r->capture && close_outside(r, target)) return true;
		target = RELOOK(tid);
		if (tid != Z_INVALID_ID && !target) return true;
		update_hover(r, target, x, y);
		target = RELOOK(tid);
		if (tid != Z_INVALID_ID && !target) return true;
		/* a press in an app bar leaves the focus where it is (WP's application bar never takes it): a send button pressed
		 * while typing must not take the focus from the text box, put the keyboard away and move from under the finger */
		bool b_in_bar = false;
		for (const zrn_element_t* a = target; a && !b_in_bar; a = a->parent) b_in_bar = zrn_is_a(a, Z_CLASS_APP_BAR);
		const bool b_typing = r->focus && (zrn_is_a(r->focus, Z_CLASS_TEXT_BOX) || zrn_is_a(r->focus, Z_CLASS_PASSWORD_BOX));
		for (zrn_element_t* f = target; f && !b_in_bar; f = f->parent) {
			if (f->b_focusable && zrn_effectively_enabled(f)) {
				/* IsTabStop false: a control beside a text box (an emoji or a mic button) is pressed without taking the
				 * focus from it, so the keyboard does not go and the control does not move from under the finger */
				if (!f->b_tab_stop && b_typing && f != r->focus) break;
				zrn_set_focus(r, f);
				break;
			}
		}
		target = RELOOK(tid);
		if (tid != Z_INVALID_ID && !target) return true;
		const zrn_element_t* const held_before = r->capture;
		if (target) preview(target, ZRN_IN_PREVIEW_DOWN, x, y);
		if (!zrn_root_lookup(root)) return true;
		target = RELOOK(tid);
		in.type = ZRN_IN_POINTER_DOWN;
		/* the preview took the press (a list stopped from coasting): it is that one's, nothing under it is pressed */
		if (r->capture && r->capture != held_before) {
			r->b_hold_armed = false;
			return zrn_dispatch(r->capture, &in);
		}
		{
			for (zrn_element_t* f2 = target; f2; f2 = f2->parent) {
				if (f2->tilt && zrn_effectively_enabled(f2)) {
					zrn_tilt_press(r, f2, x, y);
					break;
				}
			}
		}
		if (target) b_handled = zrn_dispatch(target, &in);
		return b_handled;
	}
	zrn_tilt_release(r);
	r->b_hold_armed = false;
	if (r->b_hold_fired) {
		/* the lift of a held press does nothing more: the press was cancelled when it was held */
		r->b_hold_fired = false;
		if (r->capture) zrn_set_capture(r, nullptr);
		return true;
	}
	if (target) preview(target, ZRN_IN_PREVIEW_UP, x, y);
	target = r->capture ? r->capture : RELOOK(tid);
	in.type = ZRN_IN_POINTER_UP;
	if (target) b_handled = zrn_dispatch(target, &in);
	return b_handled;
}

bool Zroot_inject_wheel(const Zid root, const float delta) {
	zrn_root_t* const r = get_root(root);
	zrn_input_t in;
	if (!r) return false;
	zrn_element_t* target = pick(r, r->pointer_x, r->pointer_y);
	if (!target) return false;
	memset(&in, 0, sizeof in);
	in.type = ZRN_IN_WHEEL;
	in.x = r->pointer_x;
	in.y = r->pointer_y;
	in.delta = delta;
	return zrn_dispatch(target, &in);
}

bool Zroot_back(const Zid root) {
	zrn_root_t* const r = get_root(root);
	/* back first puts the keyboard away (WP): the text box loses the focus, the window hides the keyboard, the page stays */
	if (r && r->input_pane > 0 && r->focus && (zrn_is_a(r->focus, Z_CLASS_TEXT_BOX) || zrn_is_a(r->focus, Z_CLASS_PASSWORD_BOX))) {
		zrn_set_focus(r, nullptr);
		return true;
	}
	if (r && r->back_callback && r->back_callback(root, r->back_user_data)) return true;
	/* (the callback may have done anything to the root) */
	zrn_root_t* const rr = get_root(root);
	if (rr && zrn_root_dismiss_top(rr)) return true;
	if (!rr || !rr->visual || !zrn_is_a(rr->visual, Z_CLASS_FRAME)) return false;
	return Zframe_go_back(rr->visual->id);
}

void Zroot_on_back(const Zid root, const ZBackCallback callback, void* const user_data) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	r->back_callback = callback;
	r->back_user_data = user_data;
}

bool Zroot_inject_key(const Zid root, const ZKey key, const int modifiers, const bool pressed) {
	zrn_root_t* const r = get_root(root);
	zrn_input_t in;
	bool b_handled = false;
	if (!r) return false;
	memset(&in, 0, sizeof in);
	in.type = pressed ? ZRN_IN_KEY_DOWN : ZRN_IN_KEY_UP;
	in.key = key;
	in.mods = modifiers;
	if (r->focus) b_handled = zrn_dispatch(r->focus, &in);
	if (!b_handled && pressed && key == Z_KEY_TAB) {
		Zroot_move_focus(root, !(modifiers & Z_MOD_SHIFT));
		b_handled = true;
	}
	if (!b_handled && pressed && key == Z_KEY_ESCAPE) b_handled = Zroot_back(root);
	return b_handled;
}

bool Zroot_inject_text(const Zid root, const char* const text) {
	zrn_root_t* const r = get_root(root);
	zrn_input_t in;
	if (!r || !r->focus || !text) return false;
	memset(&in, 0, sizeof in);
	in.type = ZRN_IN_TEXT;
	in.text = text;
	return zrn_dispatch(r->focus, &in);
}
