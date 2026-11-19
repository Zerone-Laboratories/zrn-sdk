/**
 * zrnui - resources.c
 * ThemeResource: the values of the keys of themeresources.xaml (theme_resources.c) for the theme and the accent
 * of a root, the overrides an application sets, its own keys, and the bindings of properties to keys.
 */

#include "zrnui/internal/resources.h"
#include "zrnui/internal/theme_resources.h"
#include "zrnui/internal/util.h"
#include "zrnui/internal/element.h"
#include "zrnui/internal/theme.h"
#include "zrnui/internal/root.h"
#include "zrnui/internal/controls/control.h"
#include "zrnui/internal/controls/text_block.h"
#include "zrnui/internal/controls/panels.h"

#include <stdlib.h>
#include <string.h>

typedef struct zrn_res_override {
	ZThemeResource key;
	zrn_res_type_t type;
	union { ZColor color; float d; int i; ZThickness thickness; } v;
} zrn_res_override_t;

typedef struct zrn_binding {
	zrn_bind_prop_t prop;
	ZThemeResource key;
} zrn_binding_t;

#define USER_KEY(key) ((int)(key) >= (int)Z_RES_USER_BASE)
#define BUILTIN_KEY(key) ((int)(key) >= (int)Z_RES_FIRST_ && (int)(key) < (int)Z_RES_LAST_)

static zrn_root_t* get_root(const Zid id) { return zrn_root_lookup(id); }

static const zrn_res_def_t* def_of(const ZThemeResource key) {
	return BUILTIN_KEY(key) ? &zrn_res_table[(int)key - (int)Z_RES_FIRST_] : nullptr;
}

static const zrn_res_override_t* find_override(const zrn_root_t* const r, const ZThemeResource key) {
	if (!r) return nullptr;
	for (int i = 0; i < r->override_count; i++) if (r->overrides[i].key == key) return &r->overrides[i];
	return nullptr;
}

static bool is_light(const zrn_root_t* r) { return r && r->theme.theme == Z_THEME_LIGHT; }

/* the table value of a key of a type for the theme of the root, or NULL */
static const zrn_res_value_t* table_value(const zrn_root_t* const r, const ZThemeResource key, const zrn_res_type_t type, int* const accent) {
	const zrn_res_def_t* d = def_of(key);
	const bool b_light = is_light(r);
	if (!d || d->type != type) return nullptr;
	if (accent) *accent = (d->accent & (b_light ? 2 : 1)) != 0;
	return b_light ? &d->light : &d->dark;
}

bool zrn_res_color(const zrn_root_t* const r, const ZThemeResource key, ZColor* const out) {
	const zrn_res_override_t* o = find_override(r, key);
	const zrn_res_value_t* v;
	int accent = 0;
	if (o) {
		if (o->type != ZRN_RT_COLOR) return false;
		*out = o->v.color;
		return true;
	}
	v = table_value(r, key, ZRN_RT_COLOR, &accent);
	if (!v) return false;
	if (accent) {
		ZColor a = r ? r->theme.accent : zrn_accent_color(Z_ACCENT_COBALT);
		a.a *= (float)(v->argb >> 24) / 255.0f;
		*out = a;
	} else {
		*out = Zcolor_argb(v->argb);
	}
	return true;
}

bool zrn_res_double(const zrn_root_t* const r, const ZThemeResource key, float* const out) {
	const zrn_res_override_t* o = find_override(r, key);
	const zrn_res_value_t* v;
	if (o) {
		if (o->type != ZRN_RT_DOUBLE) return false;
		*out = o->v.d;
		return true;
	}
	v = table_value(r, key, ZRN_RT_DOUBLE, nullptr);
	if (!v) return false;
	*out = v->f[0];
	return true;
}

bool zrn_res_int(const zrn_root_t* const r, const ZThemeResource key, int* const out) {
	const zrn_res_override_t* o = find_override(r, key);
	const zrn_res_value_t* v;
	if (o) {
		if (o->type != ZRN_RT_INT) return false;
		*out = o->v.i;
		return true;
	}
	v = table_value(r, key, ZRN_RT_INT, nullptr);
	if (!v) return false;
	*out = v->i;
	return true;
}

bool zrn_res_thickness(const zrn_root_t* const r, const ZThemeResource key, ZThickness* const out) {
	const zrn_res_override_t* o = find_override(r, key);
	const zrn_res_value_t* v;
	if (o) {
		if (o->type != ZRN_RT_THICKNESS) return false;
		*out = o->v.thickness;
		return true;
	}
	v = table_value(r, key, ZRN_RT_THICKNESS, nullptr);
	if (!v) return false;
	*out = Zthickness(v->f[0], v->f[1], v->f[2], v->f[3]);
	return true;
}

bool zrn_res_font_family(const zrn_root_t* const r, const ZThemeResource key, ZFontFamily* const out) {
	const zrn_res_override_t* o = find_override(r, key);
	const zrn_res_value_t* v;
	if (o) {
		if (o->type != ZRN_RT_FONT_FAMILY) return false;
		*out = (ZFontFamily)o->v.i;
		return true;
	}
	v = table_value(r, key, ZRN_RT_FONT_FAMILY, nullptr);
	if (!v) return false;
	*out = (ZFontFamily)v->i;
	return true;
}


/* ============================================================================
 *  public: values
 * ============================================================================ */

ZColor Zroot_get_theme_color(const Zid root, const ZThemeResource key) {
	ZColor c = Zcolor_rgba(0, 0, 0, 0);
	const zrn_root_t* r = get_root(root);
	if (r) zrn_res_color(r, key, &c);
	return c;
}

float Zroot_get_theme_double(const Zid root, const ZThemeResource key) {
	float v = 0;
	const zrn_root_t* r = get_root(root);
	if (r) zrn_res_double(r, key, &v);
	return v;
}

int Zroot_get_theme_int(const Zid root, const ZThemeResource key) {
	int v = 0;
	const zrn_root_t* r = get_root(root);
	if (r) zrn_res_int(r, key, &v);
	return v;
}

ZThickness Zroot_get_theme_thickness(const Zid root, const ZThemeResource key) {
	ZThickness v = zrn_thickness_zero();
	const zrn_root_t* r = get_root(root);
	if (r) zrn_res_thickness(r, key, &v);
	return v;
}

ZFontFamily Zroot_get_theme_font_family(const Zid root, const ZThemeResource key) {
	ZFontFamily v = Z_FONT_NORMAL;
	const zrn_root_t* r = get_root(root);
	if (r) zrn_res_font_family(r, key, &v);
	return v;
}


/* ============================================================================
 *  public: overrides
 * ============================================================================ */

/* the override slot of a key for a value of a type, made when there is none; NULL when the key cannot take it */
static zrn_res_override_t* override_slot(zrn_root_t* const r, const ZThemeResource key, const zrn_res_type_t type) {
	zrn_res_override_t* o = nullptr;
	if (BUILTIN_KEY(key)) {
		if (def_of(key)->type != type) {
			zrn_error(Z_ERROR_INVALID_ARGUMENT, "resource %s is not of that type", def_of(key)->name);
			return nullptr;
		}
	} else if (!USER_KEY(key)) {
		zrn_error(Z_ERROR_INVALID_ARGUMENT, "invalid resource key 0x%X", (unsigned)key);
		return nullptr;
	}
	for (int i = 0; i < r->override_count; i++) if (r->overrides[i].key == key) o = &r->overrides[i];
	if (!o) {
		if (r->override_count == r->override_cap) {
			const int cap = r->override_cap ? r->override_cap * 2 : 8;
			zrn_res_override_t* grown = realloc(r->overrides, (size_t)cap * sizeof *grown);
			if (!grown) {
				zrn_error(Z_ERROR_OUT_OF_MEMORY, "out of memory");
				return nullptr;
			}
			r->overrides = grown;
			r->override_cap = cap;
		}
		o = &r->overrides[r->override_count++];
		o->key = key;
	}
	/* a user key takes the type of the value set now */
	memset(&o->v, 0, sizeof o->v);
	o->type = type;
	return o;
}

#define RES_SET(name, ctype, rtype, assign) \
	bool name(Zid root, ZThemeResource key, ctype value) { \
		zrn_root_t* r = get_root(root); \
		zrn_res_override_t* o; \
		if (!r) return false; \
		o = override_slot(r, key, rtype); \
		if (!o) return false; \
		assign; \
		zrn_root_resources_changed(r); \
		return true; \
	}

RES_SET(Zroot_set_resource_color, ZColor, ZRN_RT_COLOR, o->v.color = value)
RES_SET(Zroot_set_resource_double, float, ZRN_RT_DOUBLE, o->v.d = value)
RES_SET(Zroot_set_resource_int, int, ZRN_RT_INT, o->v.i = value)
RES_SET(Zroot_set_resource_thickness, ZThickness, ZRN_RT_THICKNESS, o->v.thickness = value)
RES_SET(Zroot_set_resource_font_family, ZFontFamily, ZRN_RT_FONT_FAMILY, o->v.i = (int)value)

void Zroot_clear_resource(const Zid root, const ZThemeResource key) {
	zrn_root_t* const r = get_root(root);
	if (!r) return;
	for (int i = 0; i < r->override_count; i++) {
		if (r->overrides[i].key != key) continue;
		r->overrides[i] = r->overrides[--r->override_count];
		zrn_root_resources_changed(r);
		return;
	}
}

void zrn_root_resources_free(zrn_root_t* const r) {
	free(r->overrides);
	r->overrides = nullptr;
	r->override_count = r->override_cap = 0;
}


/* ============================================================================
 *  bindings
 * ============================================================================ */

static zrn_res_type_t prop_type(const zrn_bind_prop_t prop) {
	switch (prop) {
		case ZRN_BP_ELEMENT_MARGIN:
		case ZRN_BP_CONTROL_BORDER_THICKNESS:
		case ZRN_BP_CONTROL_PADDING:
		case ZRN_BP_BORDER_BORDER_THICKNESS:
		case ZRN_BP_BORDER_PADDING:
			return ZRN_RT_THICKNESS;
		case ZRN_BP_TEXT_FONT_SIZE:
		case ZRN_BP_CONTROL_FONT_SIZE:
			return ZRN_RT_DOUBLE;
		case ZRN_BP_TEXT_CHARACTER_SPACING:
			return ZRN_RT_INT;
		default:
			return ZRN_RT_COLOR;
	}
}

/* Resolves a binding and writes the property. A key that has no value (a user key not set) leaves the property as it is. */
static void apply_binding(zrn_element_t* const e, const zrn_binding_t* const b) {
	const zrn_root_t* r = e->root;
	ZColor c = Zcolor_rgba(0, 0, 0, 0);
	ZThickness t = zrn_thickness_zero();
	float d = 0;
	int i = 0;
	switch (prop_type(b->prop)) {
		case ZRN_RT_COLOR: if (!zrn_res_color(r, b->key, &c)) return; break;
		case ZRN_RT_THICKNESS: if (!zrn_res_thickness(r, b->key, &t)) return; break;
		case ZRN_RT_DOUBLE: if (!zrn_res_double(r, b->key, &d)) return; break;
		default: if (!zrn_res_int(r, b->key, &i)) return; break;
	}
	switch (b->prop) {
		case ZRN_BP_ELEMENT_MARGIN:
			e->margin = t;
			zrn_changed_layout(e);
			break;
		case ZRN_BP_TEXT_FOREGROUND: {
			const auto tb = ZRN_AS(zrn_text_block_t, e);
			tb->foreground = c;
			tb->set_flags |= ZRN_TB_FOREGROUND;
			zrn_changed_paint(e);
			break;
		}
		case ZRN_BP_TEXT_FONT_SIZE: {
			const auto tb = ZRN_AS(zrn_text_block_t, e);
			tb->font_size = d;
			tb->set_flags |= ZRN_TB_FONT_SIZE;
			zrn_changed_layout(e);
			break;
		}
		case ZRN_BP_TEXT_CHARACTER_SPACING:
			ZRN_AS(zrn_text_block_t, e)->character_spacing = i;
			zrn_changed_layout(e);
			break;
		case ZRN_BP_CONTROL_FOREGROUND:
			ZRN_AS(zrn_control_t, e)->foreground = c;
			ZRN_AS(zrn_control_t, e)->set_flags |= ZRN_C_FOREGROUND;
			zrn_changed_paint(e);
			break;
		case ZRN_BP_CONTROL_BACKGROUND:
			ZRN_AS(zrn_control_t, e)->background = c;
			ZRN_AS(zrn_control_t, e)->set_flags |= ZRN_C_BACKGROUND;
			zrn_changed_paint(e);
			break;
		case ZRN_BP_CONTROL_BORDER_BRUSH:
			ZRN_AS(zrn_control_t, e)->border_brush = c;
			ZRN_AS(zrn_control_t, e)->set_flags |= ZRN_C_BORDER_BRUSH;
			zrn_changed_paint(e);
			break;
		case ZRN_BP_CONTROL_BORDER_THICKNESS:
			ZRN_AS(zrn_control_t, e)->border_thickness = t;
			ZRN_AS(zrn_control_t, e)->set_flags |= ZRN_C_BORDER_THICKNESS;
			zrn_changed_layout(e);
			break;
		case ZRN_BP_CONTROL_PADDING:
			ZRN_AS(zrn_control_t, e)->padding = t;
			ZRN_AS(zrn_control_t, e)->set_flags |= ZRN_C_PADDING;
			zrn_changed_layout(e);
			break;
		case ZRN_BP_CONTROL_FONT_SIZE:
			ZRN_AS(zrn_control_t, e)->font_size = d;
			ZRN_AS(zrn_control_t, e)->set_flags |= ZRN_C_FONT_SIZE;
			zrn_invalidate_deep(e);
			break;
		case ZRN_BP_BORDER_BACKGROUND:
			ZRN_AS(zrn_border_t, e)->background = c;
			ZRN_AS(zrn_border_t, e)->b_has_background = true;
			zrn_changed_paint(e);
			break;
		case ZRN_BP_BORDER_BORDER_BRUSH:
			ZRN_AS(zrn_border_t, e)->border_brush = c;
			ZRN_AS(zrn_border_t, e)->b_has_border_brush = true;
			zrn_changed_paint(e);
			break;
		case ZRN_BP_BORDER_BORDER_THICKNESS:
			ZRN_AS(zrn_border_t, e)->border_thickness = t;
			zrn_changed_layout(e);
			break;
		case ZRN_BP_BORDER_PADDING:
			ZRN_AS(zrn_border_t, e)->padding = t;
			zrn_changed_layout(e);
			break;
	}
}

bool zrn_bind(zrn_element_t* const e, const zrn_bind_prop_t prop, const ZThemeResource key) {
	zrn_binding_t* b = nullptr;
	if (!e) return false;
	if (BUILTIN_KEY(key)) {
		if (def_of(key)->type != prop_type(prop)) {
			zrn_error(Z_ERROR_INVALID_ARGUMENT, "resource %s is not of the type of the property", def_of(key)->name);
			return false;
		}
	} else if (!USER_KEY(key)) {
		zrn_error(Z_ERROR_INVALID_ARGUMENT, "invalid resource key 0x%X", (unsigned)key);
		return false;
	}
	for (int i = 0; i < e->binding_count; i++) if (e->bindings[i].prop == prop) b = &e->bindings[i];
	if (!b) {
		/* grown one by one: an element binds a few properties at most */
		zrn_binding_t* grown = realloc(e->bindings, (size_t)(e->binding_count + 1) * sizeof *grown);
		if (!grown) {
			zrn_error(Z_ERROR_OUT_OF_MEMORY, "out of memory");
			return false;
		}
		e->bindings = grown;
		b = &e->bindings[e->binding_count++];
		b->prop = prop;
	}
	b->key = key;
	apply_binding(e, b);
	return true;
}

void zrn_unbind(zrn_element_t* const e, const zrn_bind_prop_t prop) {
	if (!e || !e->binding_count) return;
	for (int i = 0; i < e->binding_count; i++) {
		if (e->bindings[i].prop != prop) continue;
		e->bindings[i] = e->bindings[--e->binding_count];
		if (!e->binding_count) {
			free(e->bindings);
			e->bindings = nullptr;
		}
		return;
	}
}

void zrn_bindings_apply(zrn_element_t* const e) {
	/* applying changes no list: a copy is not needed, but the count is read each time */
	for (int i = 0; i < e->binding_count; i++) apply_binding(e, &e->bindings[i]);
}

void zrn_bindings_free(zrn_element_t* const e) {
	free(e->bindings);
	e->bindings = nullptr;
	e->binding_count = 0;
}

static void apply_tree(zrn_element_t* const e) {
	if (!e) return;
	zrn_bindings_apply(e);
	for (int i = 0; i < e->child_count; i++) apply_tree(e->children[i]);
}

void zrn_root_resources_changed(zrn_root_t* const r) {
	/* the popups that are open may be in no tree of the root's own: they are listed apart */
	apply_tree(r->visual);
	apply_tree(r->overlay);
	for (int i = 0; i < r->popup_count; i++) apply_tree(r->popups[i]);
	r->b_paint_dirty = true;
}

#define BIND_FN(name, cls, prop) \
	bool name(Zid id, ZThemeResource key) { \
		zrn_element_t* e = zrn_get(id, cls); \
		return e ? zrn_bind(e, prop, key) : false; \
	}

BIND_FN(Zelement_bind_margin, Z_CLASS_ELEMENT, ZRN_BP_ELEMENT_MARGIN)
BIND_FN(Ztext_block_bind_foreground, Z_CLASS_TEXT_BLOCK, ZRN_BP_TEXT_FOREGROUND)
BIND_FN(Ztext_block_bind_font_size, Z_CLASS_TEXT_BLOCK, ZRN_BP_TEXT_FONT_SIZE)
BIND_FN(Ztext_block_bind_character_spacing, Z_CLASS_TEXT_BLOCK, ZRN_BP_TEXT_CHARACTER_SPACING)
BIND_FN(Zcontrol_bind_foreground, Z_CLASS_CONTROL, ZRN_BP_CONTROL_FOREGROUND)
BIND_FN(Zcontrol_bind_background, Z_CLASS_CONTROL, ZRN_BP_CONTROL_BACKGROUND)
BIND_FN(Zcontrol_bind_border_brush, Z_CLASS_CONTROL, ZRN_BP_CONTROL_BORDER_BRUSH)
BIND_FN(Zcontrol_bind_border_thickness, Z_CLASS_CONTROL, ZRN_BP_CONTROL_BORDER_THICKNESS)
BIND_FN(Zcontrol_bind_padding, Z_CLASS_CONTROL, ZRN_BP_CONTROL_PADDING)
BIND_FN(Zcontrol_bind_font_size, Z_CLASS_CONTROL, ZRN_BP_CONTROL_FONT_SIZE)
BIND_FN(Zborder_bind_background, Z_CLASS_BORDER, ZRN_BP_BORDER_BACKGROUND)
BIND_FN(Zborder_bind_border_brush, Z_CLASS_BORDER, ZRN_BP_BORDER_BORDER_BRUSH)
BIND_FN(Zborder_bind_border_thickness, Z_CLASS_BORDER, ZRN_BP_BORDER_BORDER_THICKNESS)
BIND_FN(Zborder_bind_padding, Z_CLASS_BORDER, ZRN_BP_BORDER_PADDING)
