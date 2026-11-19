/**
 * zrnui - element.h
 * The element: the base of every class, and the tree, layout and handles of core.c.
 */

#pragma once

#include "zrnui/element.h"
#include "zrnui/internal/id.h"
#include "zrnui/internal/util.h"
#include "zrnui/internal/class.h"

typedef struct zrn_element zrn_element_t;
typedef struct zrn_root zrn_root_t;

/** One handler slot per routed event, from Z_EVENT_CLICK on. */
#define ZRN_HANDLER_COUNT (Z_EVENT_LAST_ - Z_EVENT_CLICK)

/** The one callback of an event on an element: Zelement_on replaces it. */
typedef struct zrn_handler {
	ZRoutedCallback callback;
	void* user_data;
} zrn_handler_t;

struct zrn_element {
	/** The handle of this element; it is freed with the element, so a stale Zid stops resolving. */
	Zid id;
	const zrn_class_t* cls;
	/** The root whose tree holds the element, NULL while it is detached. */
	zrn_root_t* root;
	/** NULL while detached and for the visual of a root. */
	zrn_element_t* parent;
	/** The children in insertion order: owned by the element (destroying it destroys them); child_cap is the allocated size. */
	zrn_element_t** children;
	int child_count;
	int child_cap;

	/** name and tooltip are heap copies owned by the element, NULL when unset; tag is the program's own pointer and is never freed. */
	char* name;
	void* tag;
	char* tooltip;

	/* FrameworkElement */
	/** Z_AUTO sizes to the content. min_* default to 0 and max_* to Z_INFINITE. */
	float width, height;
	float min_width, min_height, max_width, max_height;
	ZThickness margin;
	ZHorizontalAlignment halign;
	ZVerticalAlignment valign;
	ZVisibility visibility;
	/** 0..1, multiplied into the alpha of the element and everything under it. */
	float opacity;
	bool b_enabled;
	/** false: hit testing skips the element and its whole subtree; it is still painted. */
	bool b_hit_visible;
	/** Clips the painting of the element and its children to its rect. */
	bool b_clip_to_bounds;

	/* attached properties */
	/** Grid cell and spans (1 by default), Canvas.Left and Top (Z_AUTO is 0) and Panel.ZIndex (the higher paints later, on top). */
	int grid_row, grid_column, grid_row_span, grid_column_span;
	float canvas_left, canvas_top;
	int zindex;

	/* layout results */
	/** The size the last measure asked for, margin included. */
	ZSize desired;
	/** Set by zrn_arrange: margin excluded, in the coordinates of the parent (logical px). */
	ZRect rect;
	/** The `available` of the last measure: while it is the same and b_measure_valid, measuring is skipped. */
	ZSize last_available;
	/** Cleared by zrn_changed_layout on the element and every ancestor. */
	bool b_measure_valid;

	/* state */
	/** Kept by the root's hover tracking: true for the hovered element and its ancestors. */
	bool b_pointer_over;
	/** Can take the focus by a press; it is a Tab stop only with b_tab_stop too (then ordered by tab_index, lower first). */
	bool b_focusable;
	bool b_tab_stop;
	int tab_index;
	/** The class's tick is called by Zroot_tick only while this is true; a control that is done animating must clear it. */
	bool b_wants_tick;
	/** Gets ZRN_IN_PREVIEW_* of the presses, moves and releases that land on its descendants, before the target sees them. */
	bool b_observes_pointer;
	/** A drag that starts on this element is its own: a ScrollViewer around it does not take the pointer away. */
	bool b_blocks_scroll;

	/* render transform (anim.c), not layout: rotation in degrees about the point (proj_cx, proj_cy) of the element, as fractions of its size;
	   render_scale 1 is none; translate_x and translate_y in logical px */
	float rot_x, rot_y, proj_cx, proj_cy, render_scale, translate_x, translate_y;
	/** true when any of the above is not the identity. */
	bool b_transformed;
	/** Leans when pressed (Zelement_set_tilt); on by default for list items, check and radio buttons, hyperlinks and menu items. */
	int tilt;
	/** Animations running on this element: one slot per ZAnimatedProperty, NULL when it has none. */
	struct zrn_tween* tweens;
	/** Laid out and painted by its parent itself (a popup's content, a Viewbox's child), not by the generic walk; not hit tested through the tree. */
	bool b_is_popup_child;
	/** Set when zrn_destroy starts: what an element does as it goes must not destroy what is already going. */
	bool b_destroying;

	zrn_handler_t handlers[ZRN_HANDLER_COUNT];

	/** Properties bound to a ThemeResource (resources.c): a small list, NULL when there is none. */
	struct zrn_binding* bindings;
	int binding_count;
};

/** The element of a handle; NULL when the handle is invalid, stale or a root. Reports nothing (zrn_get does). */
zrn_element_t* zrn_lookup(Zid id);
/**
 * Children in paint order (Panel.ZIndex, then insertion). `*heap` says whether the list was allocated: when no sorting is needed (or no memory)
 * it is e->children itself, so the children must not change while it is used. Free with zrn_ordered_free(list, *heap).
 */
zrn_element_t** zrn_ordered(zrn_element_t* e, int* count, int* heap);
/** Frees a list of zrn_ordered when `b_heap` says it was allocated. */
void zrn_ordered_free(zrn_element_t** list, bool b_heap);
/** Looks an element up and checks its class; NULL on failure, reported as Z_ERROR_INVALID_ID or Z_ERROR_WRONG_CLASS. */
zrn_element_t* zrn_get(Zid id, ZClass cls);
/**
 * Makes an element of a concrete class, detached and owned by the caller until it is added to a tree. NULL with zrn_error for an unknown or
 * abstract class or no memory.
 */
zrn_element_t* zrn_create(ZClass id);
/**
 * Destroys the element and its whole subtree: removes it from its parent (child_removed runs there), detaches it from the root, runs the
 * destroy hooks and frees the handles. NULL is allowed; `e` is invalid afterwards.
 */
void zrn_destroy(zrn_element_t* e);
/** true when the class of `e` is `id` or derives from it. */
bool zrn_is_a(const zrn_element_t* e, ZClass id);
/**
 * Adds `child` at `index` (-1 or past the end appends), first taking it from its old parent; the parent owns it from then on. A cycle is
 * refused (Z_ERROR_CYCLE). On no memory nothing is added and the child is already out of its old parent: check child->parent afterwards. A
 * Loaded handler may destroy the parent.
 */
void zrn_add_child(zrn_element_t* parent, zrn_element_t* child, int index);
/**
 * Detaches `child` without destroying it (the caller owns it again): child_removed runs first and b_is_popup_child is cleared. Does nothing
 * when it is not a child.
 */
void zrn_remove_child(zrn_element_t* parent, zrn_element_t* child);
/** Destroys every child. */
void zrn_clear_children(zrn_element_t* parent);
/**
 * Replaces the one child of a single-child element: `slot` is the field that keeps it (updated here), NULL `child` clears it. The new child is taken out of its
 * old place first, then the old one is destroyed. A cycle is refused (Z_ERROR_CYCLE) and the old child stays; on no memory *slot ends NULL and `child` is detached.
 */
void zrn_set_only_child(zrn_element_t* parent, zrn_element_t** slot, zrn_element_t* child);
/** The position in children[], -1 when `child` is not a child of `parent`. */
int zrn_child_index(const zrn_element_t* parent, const zrn_element_t* child);

/* layout */
/**
 * The measure pass: `available` includes the margin. Applies Width, Height, Min and Max, calls the class hook and stores `desired` (margin
 * included). A Collapsed element measures 0. Skipped while the result is valid and `available` is the same.
 */
void zrn_measure(zrn_element_t* e, ZSize available);
/**
 * The arrange pass: `slot` (in the coordinates of the parent) includes the margin. Aligns the element inside it, sets `rect` and calls the
 * class hook.
 */
void zrn_arrange(zrn_element_t* e, ZRect slot);
/** Measures every child with `available` and returns the widest and the tallest desired size. */
ZSize zrn_measure_children_max(zrn_element_t* e, ZSize available);
/** The size of the element may have changed: marks it and every ancestor unmeasured and the layout and the paint of the root dirty. */
void zrn_changed_layout(zrn_element_t* e);
/** Only the look changed: the root repaints, nothing is laid out again. */
void zrn_changed_paint(zrn_element_t* e);
/** Arrange again without measuring again (a scroll offset changed). */
void zrn_changed_arrange(zrn_element_t* e);
/** true when the element and all its ancestors are enabled. */
bool zrn_effectively_enabled(const zrn_element_t* e);
/** true when the element and all its ancestors are Z_VISIBLE. */
bool zrn_effectively_visible(const zrn_element_t* e);
/** Converts a point in the coordinates of `e` to root coordinates (the layout grid of the UI scale); render transforms are ignored. */
ZPoint zrn_to_root(const zrn_element_t* e, ZPoint p);
/** The inverse of zrn_to_root. */
ZPoint zrn_from_root(const zrn_element_t* e, ZPoint p);
/**
 * The deepest element under `p` (in the coordinates of `e`), NULL when there is none. Honours visibility, b_hit_visible, the clip and
 * hit_self; popup children are skipped.
 */
zrn_element_t* zrn_hit_test(zrn_element_t* e, ZPoint p);
/** Calls the handler of an event (none: nothing happens). @return false when the handler destroyed the element: the caller must then touch nothing of it. */
bool zrn_raise(zrn_element_t* e, ZEvent ev);
/** A change of an inherited property (font) can change the measure of everything below. */
void zrn_invalidate_deep(zrn_element_t* e);
/** The ids of an element and its ancestors, innermost first (at most `max`; returns how many): safe to walk while callbacks destroy things. */
int zrn_chain(const zrn_element_t* e, Zid* ids, int max);

/** Walks up for what a TextBlock inherits: the nearest ancestor class with the hook, else the theme foreground, ZRN_FONT_NORMAL, Z_FONT_NORMAL. */
ZColor zrn_inherited_foreground(const zrn_element_t* e);
float zrn_inherited_font_size(const zrn_element_t* e);
ZFontFamily zrn_inherited_font_family(const zrn_element_t* e);

/** Element rectangle in its own coordinates. */
static inline ZRect zrn_local_rect(const zrn_element_t* e) { return zrn_rect(0, 0, e->rect.width, e->rect.height); }

extern zrn_class_t zrn_class_element;
