// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Text: HarfBuzz shapes in font units (so a layout is the same at every
 * scale, on the laptop and on the phone), FreeType draws each glyph once at
 * the size it is shown, into the GPU atlas. Characters a style's font does
 * not have come from whatever fontconfig finds that has them (Sinhala, CJK).
 */
#define _GNU_SOURCE
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <hb.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zui-internal.h"

#define MAX_FACES 32
struct face {
	char *path;
	FT_Face ft;
	hb_font_t *hb;
	float upem, asc, desc;
	bool ok;
};

static struct {
	FT_Library ft;
	struct face faces[MAX_FACES];
	int n;
	int role[ZT_FONTS];	/* face index of each role */
	float spacing;
	unsigned fonts_gen;	/* bumps when the fonts change: layouts are made again */
} T = { .spacing = 1.3f };

/* fallbacks found for characters, so fontconfig is asked once per character */
#define FALLBACK_CACHE 512
static struct { uint32_t cp; int face; } fallback_seen[FALLBACK_CACHE];
static int fallback_n;

static int face_open(const char *path) {
	for (int i = 0; i < T.n; i++)
		if (strcmp(T.faces[i].path, path) == 0) return i;
	if (T.n >= MAX_FACES) return -1;
	if (!T.ft && FT_Init_FreeType(&T.ft)) return -1;
	struct face *f = &T.faces[T.n];
	memset(f, 0, sizeof(*f));
	f->path = strdup(path);
	if (FT_New_Face(T.ft, path, 0, &f->ft) || !FT_IS_SCALABLE(f->ft)) {
		/* bitmap-only fonts (colour emoji) are not drawn yet */
		if (f->ft) FT_Done_Face(f->ft);
		f->ft = NULL;
		T.n++;
		return T.n - 1;
	}
	hb_blob_t *blob = hb_blob_create_from_file(path);
	hb_face_t *hface = hb_face_create(blob, 0);
	hb_blob_destroy(blob);
	f->hb = hb_font_create(hface);
	hb_face_destroy(hface);
	f->upem = (float)f->ft->units_per_EM;
	hb_font_set_scale(f->hb, (int)f->upem, (int)f->upem);
	f->asc = f->ft->ascender / f->upem;
	f->desc = -f->ft->descender / f->upem;
	f->ok = true;
	T.n++;
	return T.n - 1;
}

static char *fc_find(const char *pattern, uint32_t cp) {
	FcInit();
	FcPattern *p = FcNameParse((const FcChar8 *)(pattern ? pattern : "sans-serif"));
	if (!p) return NULL;
	if (cp) {
		FcCharSet *cs = FcCharSetCreate();
		FcCharSetAddChar(cs, cp);
		FcPatternAddCharSet(p, FC_CHARSET, cs);
		FcCharSetDestroy(cs);
		FcPatternAddBool(p, FC_SCALABLE, FcTrue);
	}
	FcConfigSubstitute(NULL, p, FcMatchPattern);
	FcDefaultSubstitute(p);
	FcResult res;
	FcPattern *m = FcFontMatch(NULL, p, &res);
	FcPatternDestroy(p);
	char *out = NULL;
	FcChar8 *file;
	if (m && FcPatternGetString(m, FC_FILE, 0, &file) == FcResultMatch) {
		bool has = true;
		if (cp) {
			FcCharSet *cs;
			has = FcPatternGetCharSet(m, FC_CHARSET, 0, &cs) == FcResultMatch && FcCharSetHasChar(cs, cp);
		}
		if (has) out = strdup((const char *)file);
	}
	if (m) FcPatternDestroy(m);
	return out;
}

/* "Inter-Regular.ttf;Inter:weight=80": the file in libzrnui's fonts, else
 * what fontconfig makes of the pattern */
static int font_resolve(const char *spec) {
	char file[256] = "", pattern[256] = "sans-serif";
	const char *semi = strchr(spec, ';');
	if (semi) {
		snprintf(file, sizeof(file), "%.*s", (int)(semi - spec), spec);
		snprintf(pattern, sizeof(pattern), "%s", semi + 1);
	} else {
		snprintf(file, sizeof(file), "%s", spec);
	}
	if (file[0]) {
		char path[1024];
		const char *env = getenv("ZUI_FONT_DIR");
		if (env) {
			snprintf(path, sizeof(path), "%s/%s", env, file);
			if (access(path, R_OK) == 0) return face_open(path);
		}
		snprintf(path, sizeof(path), "%s/fonts/%s", zui_data_dir(), file);
		if (access(path, R_OK) == 0) return face_open(path);
	}
	char *found = fc_find(pattern, 0);
	int i = found ? face_open(found) : -1;
	free(found);
	return i;
}

void zt_set_fonts(const char *const files[ZT_FONTS], float spacing) {
	for (int r = 0; r < ZT_FONTS; r++) {
		int i = font_resolve(files[r]);
		if (i < 0 || !T.faces[i].ok) i = font_resolve(";sans-serif");
		T.role[r] = i < 0 ? 0 : i;
	}
	T.spacing = spacing;
	T.fonts_gen++;
}

static bool face_has(int i, uint32_t cp) {
	return i >= 0 && i < T.n && T.faces[i].ok && FT_Get_Char_Index(T.faces[i].ft, cp) != 0;
}

static int face_for(int primary, uint32_t cp) {
	if (face_has(primary, cp) || cp < 0x20) return primary;
	for (int i = 0; i < fallback_n; i++)
		if (fallback_seen[i].cp == cp) return fallback_seen[i].face < 0 ? primary : fallback_seen[i].face;
	/* faces already open first: one fallback font usually covers a whole script */
	int found = -1;
	for (int i = 0; i < T.n && found < 0; i++)
		if (face_has(i, cp)) found = i;
	if (found < 0) {
		char *path = fc_find("sans-serif", cp);
		if (path) {
			int i = face_open(path);
			if (face_has(i, cp)) found = i;
			free(path);
		}
	}
	if (fallback_n < FALLBACK_CACHE) {
		fallback_seen[fallback_n].cp = cp;
		fallback_seen[fallback_n++].face = found;
	}
	return found < 0 ? primary : found;
}

static uint32_t utf8_next(const char *s, int *i, int len) {
	const unsigned char *u = (const unsigned char *)s;
	uint32_t c = u[*i];
	int n = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
	if (*i + n > len) n = 1;
	if (n == 1) { (*i)++; return c; }
	c &= n == 2 ? 0x1f : n == 3 ? 0x0f : 0x07;
	for (int k = 1; k < n; k++) c = (c << 6) | (u[*i + k] & 0x3f);
	*i += n;
	return c;
}

/* one paragraph shaped: glyphs, their advances, and which ones are spaces */
struct shaped {
	struct zt_glyph *g;
	float *adv;
	bool *space;
	int n, cap;
};

static void shaped_push(struct shaped *s, int face, uint32_t gid, float x_off, float y_off, float adv, bool space) {
	if (s->n == s->cap) {
		s->cap = s->cap ? s->cap * 2 : 64;
		s->g = realloc(s->g, sizeof(*s->g) * s->cap);
		s->adv = realloc(s->adv, sizeof(float) * s->cap);
		s->space = realloc(s->space, sizeof(bool) * s->cap);
	}
	s->g[s->n] = (struct zt_glyph){ (uint16_t)face, gid, x_off, y_off };
	s->adv[s->n] = adv;
	s->space[s->n] = space;
	s->n++;
}

static void shape_para(struct shaped *out, const char *text, int len, int primary, float size) {
	hb_buffer_t *buf = hb_buffer_create();
	int i = 0;
	while (i < len) {
		/* a run: characters the same face draws (marks stay with what they mark) */
		int start = i, face = -1;
		while (i < len) {
			int at = i;
			uint32_t cp = utf8_next(text, &i, len);
			hb_unicode_general_category_t cat = hb_unicode_general_category(hb_unicode_funcs_get_default(), cp);
			bool mark = cat == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ||
				cat == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK || cp == 0x200d || cp == 0x200c;
			int f = (mark && face >= 0) ? face : face_for(primary, cp);
			if (face < 0) face = f;
			else if (f != face) { i = at; break; }
		}
		if (face < 0 || !T.faces[face].ok) continue;
		hb_buffer_clear_contents(buf);
		hb_buffer_add_utf8(buf, text, len, start, i - start);
		hb_buffer_guess_segment_properties(buf);
		hb_shape(T.faces[face].hb, buf, NULL, 0);
		unsigned count;
		hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buf, &count);
		hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(buf, &count);
		float k = size / T.faces[face].upem;
		for (unsigned g = 0; g < count; g++) {
			bool space = text[info[g].cluster] == ' ';
			shaped_push(out, face, info[g].codepoint, pos[g].x_offset * k, -pos[g].y_offset * k,
				pos[g].x_advance * k, space);
		}
	}
	hb_buffer_destroy(buf);
}

float zt_line_height(float size) {
	return roundf(size * T.spacing * 4) / 4;
}

static void layout_add(struct zt_layout *L, int *cap, const struct zt_glyph *g) {
	if (L->n == *cap) {
		*cap = *cap ? *cap * 2 : 64;
		L->g = realloc(L->g, sizeof(*L->g) * *cap);
	}
	L->g[L->n++] = *g;
}

struct zt_layout *zt_layout_make(const char *utf8, int font, float size, float max_w, int max_lines) {
	struct zt_layout *L = calloc(1, sizeof(*L));
	int cap = 0;
	if (!utf8) utf8 = "";
	int primary = T.role[font < 0 || font >= ZT_FONTS ? 0 : font];
	float asc = T.n ? T.faces[primary].asc : 0.9f, desc = T.n ? T.faces[primary].desc : 0.25f;
	L->size = size;
	L->line_h = zt_line_height(size);
	/* the baseline within a line: the font's own box centred in it */
	L->ascent = roundf(((L->line_h - (asc + desc) * size) / 2 + asc * size) * 4) / 4;
	bool wrap = max_w > 0 && max_lines != 1;
	bool ellipsis = max_w > 0 && max_lines >= 1;
	int line = 0;
	bool stopped = false;
	const char *p = utf8;
	while (!stopped) {
		const char *nl = strchr(p, '\n');
		int len = nl ? (int)(nl - p) : (int)strlen(p);
		struct shaped s = { 0 };
		if (T.n) shape_para(&s, p, len, primary, size);
		int i = 0;
		do {
			/* one line: as many glyphs as fit, broken after a space */
			float x = 0, w_at_break = 0;
			int j = i, brk = -1;
			while (j < s.n) {
				if (wrap && x + s.adv[j] > max_w && j > i && !s.space[j]) {
					break;
				}
				x += s.adv[j];
				if (s.space[j]) { brk = j + 1; w_at_break = x; }
				j++;
			}
			int end = j;
			if (j < s.n && brk > i) end = brk;
			(void)w_at_break;
			bool last = max_lines > 0 && line == max_lines - 1;
			bool more = end < s.n || nl;
			float lx = 0, y = line * L->line_h + L->ascent;
			if (last && more && max_lines > 1) end = s.n;	/* the rest goes on the last line, cut */
			/* cut at the width with an ellipsis */
			float total = 0;
			for (int k = i; k < end; k++) total += s.adv[k];
			bool cut = ellipsis && (total > max_w + 0.01f || (last && more));
			int stop = end;
			float ell_w = 0;
			uint32_t ell = 0;
			if (cut && T.n && T.faces[primary].ok) {
				ell = FT_Get_Char_Index(T.faces[primary].ft, 0x2026);
				hb_position_t a = ell ? hb_font_get_glyph_h_advance(T.faces[primary].hb, ell) : 0;
				ell_w = a * size / T.faces[primary].upem;
				float acc = 0;
				stop = i;
				while (stop < end && acc + s.adv[stop] + ell_w <= max_w) acc += s.adv[stop++];
				while (stop > i && s.space[stop - 1]) stop--;
			}
			for (int k = i; k < stop; k++) {
				struct zt_glyph g = s.g[k];
				g.x += lx;
				g.y += y;
				lx += s.adv[k];
				layout_add(L, &cap, &g);
			}
			/* trailing spaces do not count toward the width */
			float vis = lx;
			for (int k = stop - 1; k >= i && s.space[k]; k--) vis -= s.adv[k];
			if (cut && ell) {
				struct zt_glyph g = { (uint16_t)primary, ell, vis, y };
				layout_add(L, &cap, &g);
				vis += ell_w;
			}
			if (vis > L->w) L->w = vis;
			line++;
			i = end;
			if (cut && (last || !wrap)) { stopped = true; break; }
			if (max_lines > 0 && line >= max_lines) { stopped = nl || i < s.n; if (stopped) break; }
		} while (i < s.n);
		free(s.g);
		free(s.adv);
		free(s.space);
		if (!nl || stopped) break;
		if (max_lines > 0 && line >= max_lines) break;
		p = nl + 1;
	}
	if (line == 0) line = 1;
	L->lines = line;
	L->h = line * L->line_h;
	return L;
}

void zt_layout_free(struct zt_layout *L) {
	if (!L) return;
	free(L->g);
	free(L);
}

float zt_width(const char *utf8, int font, float size) {
	struct zt_layout *L = zt_layout_make(utf8, font, size, 0, 0);
	float w = L->w;
	zt_layout_free(L);
	return w;
}

/* ---- glyphs in the atlas ---- */
#define CACHE 8192
struct cached {
	uint32_t gid, px64;
	uint16_t face;
	uint8_t bin, used;
	unsigned gen;
	int16_t ax, ay, w, h, left, top;
};
static struct cached cache[CACHE];
static unsigned cache_gen = 1;
static unsigned cache_count;
static unsigned cache_zg = ~0u;

static void cache_clear(void) {
	cache_gen++;
	cache_count = 0;
	zg_atlas_reset();
}

static const struct cached *glyph_get(int face, uint32_t gid, uint32_t px64, int bin) {
	if (cache_zg != zg_generation()) {
		cache_zg = zg_generation();
		cache_clear();
	}
	uint32_t h = (gid * 2654435761u) ^ (px64 * 40503u) ^ ((uint32_t)face << 20) ^ ((uint32_t)bin << 28);
	for (int probe = 0; probe < 64; probe++) {
		struct cached *c = &cache[(h + probe) % CACHE];
		if (c->used && c->gen == cache_gen) {
			if (c->gid == gid && c->px64 == px64 && c->face == face && c->bin == bin) return c;
			continue;
		}
		/* not there: draw it */
		if (cache_count > CACHE * 3 / 4) {
			cache_clear();
			return glyph_get(face, gid, px64, bin);
		}
		struct face *f = &T.faces[face];
		if (!f->ok) return NULL;
		if (FT_Set_Char_Size(f->ft, 0, px64, 72, 72)) return NULL;
		if (FT_Load_Glyph(f->ft, gid, FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_LIGHT)) return NULL;
		FT_GlyphSlot slot = f->ft->glyph;
		if (slot->format == FT_GLYPH_FORMAT_OUTLINE && bin)
			FT_Outline_Translate(&slot->outline, bin * 64 / 3, 0);
		if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL)) return NULL;
		FT_Bitmap *bm = &slot->bitmap;
		int ax = 0, ay = 0;
		if (bm->width && bm->rows) {
			if (!zg_atlas_alloc((int)bm->width, (int)bm->rows, &ax, &ay)) {
				cache_clear();
				return glyph_get(face, gid, px64, bin);
			}
			zg_atlas_upload(ax, ay, (int)bm->width, (int)bm->rows, bm->buffer, bm->pitch);
		}
		*c = (struct cached){ gid, px64, (uint16_t)face, (uint8_t)bin, 1, cache_gen,
			(int16_t)ax, (int16_t)ay, (int16_t)bm->width, (int16_t)bm->rows,
			(int16_t)slot->bitmap_left, (int16_t)slot->bitmap_top };
		cache_count++;
		return c;
	}
	cache_clear();
	return NULL;
}

static void draw_range(const struct zt_layout *L, float x, float y, uint32_t rgba, float x0, float x1);

void zt_draw(const struct zt_layout *L, float x, float y, uint32_t rgba) {
	draw_range(L, x, y, rgba, -1e9f, 1e9f);
}

/* only the glyphs that lie wholly between x0 and x1 (layout coordinates) */
void zt_draw_clipped(const struct zt_layout *L, float x, float y, uint32_t rgba, float x0, float x1) {
	draw_range(L, x, y, rgba, x0, x1);
}

static void draw_range(const struct zt_layout *L, float x, float y, uint32_t rgba, float x0, float x1) {
	if (!L || !T.n) return;
	float scale = zui_gfx_scale();
	bool snap = zg_xf_is_identity(zg_get_xf());
	/* sizes in quarter pixels */
	uint32_t px64 = (uint32_t)lroundf(L->size * scale * 4) * 16;
	for (int i = 0; i < L->n; i++) {
		const struct zt_glyph *g = &L->g[i];
		float gx = (x + g->x) * scale, gy = (y + g->y) * scale;
		int bin = 0;
		if (snap) {
			float fx = floorf(gx);
			bin = (int)((gx - fx) * 3);
			if (bin > 2) bin = 2;
			gx = fx;
			gy = roundf(gy);
		}
		const struct cached *c = glyph_get(g->face, g->gid, px64, bin);
		if (!c || !c->w) continue;
		if (g->x + c->left / scale < x0 - 0.5f || g->x + (c->left + c->w) / scale > x1 + 0.5f) continue;
		float qx = (gx + c->left) / scale, qy = (gy - c->top) / scale;
		float u0 = (float)c->ax / ZG_ATLAS, v0 = (float)c->ay / ZG_ATLAS;
		float u1 = (float)(c->ax + c->w) / ZG_ATLAS, v1 = (float)(c->ay + c->h) / ZG_ATLAS;
		zg_glyph(qx, qy, c->w / scale, c->h / scale, u0, v0, u1, v1, rgba);
	}
}

/* ---- layouts kept on nodes ---- */
struct zt_layout *zt_slot_get(struct zt_slot *s, const char *text, int font, float size,
		float max_w, int max_lines) {
	if (!text) text = "";
	if (s->layout && s->style_gen == T.fonts_gen && s->font == font && s->size == size &&
			s->max_w == max_w && s->max_lines == max_lines && s->text && strcmp(s->text, text) == 0)
		return s->layout;
	zt_layout_free(s->layout);
	free(s->text);
	s->text = strdup(text);
	s->font = font;
	s->size = size;
	s->max_w = max_w;
	s->max_lines = max_lines;
	s->style_gen = T.fonts_gen;
	s->layout = zt_layout_make(text, font, size, max_w, max_lines);
	return s->layout;
}

void zt_slot_free(struct zt_slot *s) {
	zt_layout_free(s->layout);
	free(s->text);
	memset(s, 0, sizeof(*s));
}

void zt_fini(void) {
	for (int i = 0; i < T.n; i++) {
		if (T.faces[i].hb) hb_font_destroy(T.faces[i].hb);
		if (T.faces[i].ft) FT_Done_Face(T.faces[i].ft);
		free(T.faces[i].path);
	}
	T.n = 0;
	fallback_n = 0;
	if (T.ft) FT_Done_FreeType(T.ft);
	T.ft = NULL;
	T.fonts_gen++;
}
