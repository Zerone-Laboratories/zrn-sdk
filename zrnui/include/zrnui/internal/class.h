/**
 * zrnui - class.h
 * The class of an element: a vtable with a base.
 */

#pragma once

#include "zrnui/element.h"
#include "zrnui/internal/input.h"

typedef struct zrn_element zrn_element_t;
typedef struct zrn_class zrn_class_t;
typedef struct zrn_painter zrn_painter_t;

struct zrn_class {
	/**
	 * The class this one derives from, NULL for Element. Slots left NULL are copied from it when the class is first used (everything but init
	 * and destroy, which are chained).
	 */
	const zrn_class_t* base;
	/** The name Zelement_get_class_name reports and the errors print. */
	const char* name;
	/** The ZClass this object stands for: zrn_class_of(id) returns it. */
	ZClass id;
	/** sizeof the instance struct, which starts with the struct of the base class; zrn_create callocs it, so every field starts zero. */
	size_t size;
	/** zrn_create refuses it (Z_ERROR_INVALID_ARGUMENT): only a class that derives from it is made. */
	bool b_abstract_class;
	/** Set once the empty slots have been filled from the base. */
	bool b_resolved;

	/** Called base first, then derived. */
	void (*init)(zrn_element_t* e);
	/** Called derived first, then base, after the children are gone; the handle is still valid. */
	void (*destroy)(zrn_element_t* e);

	/**
	 * Measure and arrange overrides. `available` has the margin removed and
	 * Width / Height / Min / Max applied; return the content size.
	 * `final` of arrange is the size the element will have (rect is already set); zrn_arrange ignores the return value.
	 */
	ZSize (*measure)(zrn_element_t* e, ZSize available);
	ZSize (*arrange)(zrn_element_t* e, ZSize final);
	/** Draws the element itself in its own coordinates (logical px, origin at its top left); the children are drawn after. */
	void (*paint)(zrn_element_t* e, zrn_painter_t* p);
	/** Draws over the children (scroll bars, focus marks). */
	void (*paint_over)(zrn_element_t* e, zrn_painter_t* p);
	/** Returns nonzero when the input was handled and must not bubble. A handler can destroy the element: look it up again after raising an event. */
	int (*input)(zrn_element_t* e, zrn_input_t* in);
	/**
	 * Called by Zroot_tick only while b_wants_tick is set, with the frame time in ms times the animation speed; a control that is done
	 * animating clears b_wants_tick.
	 */
	void (*tick)(zrn_element_t* e, float delta_ms);
	/** A child is about to be detached (moved, removed or destroyed): drop pointers to it. Called on the parent while the child is still in children[]. */
	void (*child_removed)(zrn_element_t* e, zrn_element_t* child);
	/** Whether the element itself (not its children) catches the pointer; nonzero yes. NULL (also inherited) means yes. */
	int (*hit_self)(zrn_element_t* e);

	/** Extra room taken inside the element by the chrome a control draws (a Button's touch target). */
	ZThickness (*chrome)(zrn_element_t* e);
	/** What a TextBlock inside this control inherits; the nearest ancestor that has the hook wins (zrn_inherited_*). */
	ZColor (*content_foreground)(zrn_element_t* e);
	float (*content_font_size)(zrn_element_t* e);
	ZFontFamily (*content_font_family)(zrn_element_t* e);
};

/** The class object of an id, NULL when the id is no class (nothing is reported). */
const zrn_class_t* zrn_class_of(ZClass id);
/** true when `cls` is `base` or derives from it. */
bool zrn_class_derives(const zrn_class_t* cls, const zrn_class_t* base);
