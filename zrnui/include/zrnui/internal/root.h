/**
 * zrnui - root.h
 * The root: the tree, the focus, the popups and the painting (host.c).
 */

#pragma once

#include "zrnui/element.h"
#include "zrnui/internal/theme.h"
#include "zrnui/internal/input.h"

typedef struct zrn_element zrn_element_t;
typedef struct zrn_painter zrn_painter_t;
typedef struct zrn_root zrn_root_t;

struct zrn_root {
	Zid id;
	/** The window size in logical pixels (Zroot_set_size); the layout grid is this divided by `ui`. */
	int width, height;
	/** The painter's scale: the UI scale times the density of the buffer. */
	float scale;
	/** The UI scale: the layout grid is the window divided by it; the shell owns it. */
	float ui;
	/** The density of the buffer against the logical size: only crispness. */
	float density;
	/** The theme in effect: the cross-faded value while b_theme_transitioning. */
	zrn_theme_t theme;
	/** The theme we are leaving, the one we are going to, and the progress between them. */
	zrn_theme_t theme_from, theme_target;
	bool b_theme_transitioning;
	float theme_t;
	/** The root of the element tree, owned by the root (destroyed with it or when replaced). */
	zrn_element_t* visual;
	/** The open popups in opening order, which is paint order; not owned. A popup is listed only while it is open and in this root. */
	zrn_element_t** popups;
	int popup_count, popup_cap;
	/**
	 * focus, capture and hover are not owned: zrn_element_forget clears them when the element leaves the root. capture gets all pointer input,
	 * hover is the element under the pointer.
	 */
	zrn_element_t* focus;
	zrn_element_t* capture;
	zrn_element_t* hover;
	zrn_element_t* tooltip_target;
	/** ms the pointer has rested on tooltip_target; the tooltip shows from 500. */
	float tooltip_timer;
	/** The AppBar that is open: a press outside it closes it unless it is sticky. */
	zrn_element_t* open_bar;
	/** Where the flyouts shown at an element are hosted while they show: a hidden Canvas that belongs to the root. */
	zrn_element_t* overlay;
	/** The last pointer position in layout units (window / ui). */
	float pointer_x, pointer_y;
	/** Set by zrn_changed_layout and zrn_changed_arrange, cleared by Zroot_update_layout. */
	bool b_layout_dirty;
	/** Something changed: the next frame must be painted. */
	bool b_paint_dirty;
	/**
	 * The software frame (RGBA8, pixels_w x pixels_h), owned; the GPU frame is read back into it too. Zroot_render hands it out in a ZFrame,
	 * valid until the next render.
	 */
	uint8_t* pixels;
	int pixels_w, pixels_h;
	/** true until the first frame has been painted. */
	bool b_first_frame;
	/** The modes of the root, taken from the Zinit_ex config when it was created. */
	ZRenderer renderer;
	ZAnimations animations;
	/** true while the platform presents the root with the OpenGL renderer (Zroot_report_presenter). */
	bool b_gpu_window;
	/** The clock of animations, in ms: the sum of the Zroot_tick deltas. */
	double time_ms;
	/** Tilt in progress: the pressed element, how far the finger is from its centre (-1..1), how much it leans (0..1). */
	zrn_element_t* tilt_element;
	float tilt_fx, tilt_fy, tilt_amount, tilt_target;
	/** The off-screen GPU context of Zroot_render, made when it is first asked for. */
	struct zrn_gl* gpu;
	/** The overrides of ThemeResource keys the application set (resources.c). */
	struct zrn_res_override* overrides;
	int override_count, override_cap;
};

/** The root of a handle; NULL when it is invalid or an element. Reports nothing. */
zrn_root_t* zrn_root_lookup(Zid id);
/**
 * Lays the tree out when it needs it and, when something changed, paints it with `p` (the clear
 * and every element, then the open popups and the tooltip). true when it painted; false when nothing changed. The software and the GPU renderer both go through it.
 */
bool zrn_root_paint(zrn_root_t* r, zrn_painter_t* p);
/** The name Zroot_get_renderer_name reports, for all roots. Only the pointer is kept: pass a string that lives on. */
void zrn_note_renderer(const char* name);

/** Closes what the Back button closes first: a dialog, a popup, an open app bar. true when it closed something; a popup that stays open (not a dialog) and a sticky bar are skipped. */
bool zrn_root_dismiss_top(zrn_root_t* r);

/**
 * Puts `e` and its subtree in the root: sets `root`, applies the theme bindings, registers open popups and raises Loaded, children first. A
 * Loaded handler may destroy things.
 */
void zrn_root_attach(zrn_root_t* r, zrn_element_t* e);
/** Takes `e` and its subtree out of the root, forgetting them (zrn_element_forget). No LostFocus and no Unloaded are raised. */
void zrn_root_detach(zrn_element_t* e);
/** Lists an open popup (once) and lays it out; on no memory it is silently not listed. */
void zrn_root_register_popup(zrn_root_t* r, zrn_element_t* popup);
/** Removes a popup from the list; does nothing when it is not there. */
void zrn_root_unregister_popup(zrn_root_t* r, const zrn_element_t* popup);
/**
 * Moves the keyboard focus (NULL clears it; b_focusable is not checked). FOCUS_OUT and LostFocus go to the old element, then FOCUS_IN and
 * GotFocus to the new one, unless a handler destroyed it or moved the focus meanwhile.
 */
void zrn_set_focus(zrn_root_t* r, zrn_element_t* e);
/** Gives `e` all the pointer input until it is released; the previous holder gets ZRN_IN_CAPTURE_LOST. */
void zrn_set_capture(zrn_root_t* r, zrn_element_t* e);
/** Releases the capture when `e` holds it; unlike zrn_set_capture it sends no event. */
void zrn_release_capture(zrn_root_t* r, zrn_element_t* e);
/**
 * Clears every reference the root keeps to `e` (focus, capture, hover, tilt, tooltip, visual, open bar, popup list) without raising events.
 * Called when an element leaves the root.
 */
void zrn_element_forget(zrn_element_t* e);
/**
 * Bubbles `in` from `e` up the ancestors. A pointer event comes in root coordinates and is converted for each ancestor; a disabled ancestor
 * gets no pointer input. true when an element handled it or one on the chain was destroyed on the way; only the first 64 ancestors are
 * reached.
 */
bool zrn_dispatch(zrn_element_t* e, zrn_input_t* in);
/**
 * Paints `e` and its subtree with `p` at the origin of its parent (p->tx and p->ty move by e->rect): opacity, render transform and clip are
 * applied; hidden and clipped-out elements and popup children are skipped. `r` is only passed down.
 */
void zrn_paint_element(zrn_element_t* e, zrn_painter_t* p, zrn_root_t* r);
