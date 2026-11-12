/**
 * zrnui - font.h
 * Font loading: the inventory of the fonts that exist (the font directory and the system),
 * the five WP faces resolved from it and the faces that stand in for the code points they lack.
 * Text layout and drawing (text.c) only asks this module for an FT_Face.
 */

#pragma once

#include "zrnui/element.h"

#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H

/** The five WP weights; the index of zrn_face(). */
enum zrn_face { ZRN_FACE_REGULAR, ZRN_FACE_LIGHT, ZRN_FACE_SEMILIGHT, ZRN_FACE_SEMIBOLD, ZRN_FACE_BOLD, ZRN_FACE_COUNT };

/**
 * Starts FreeType and fontconfig, scans every font there is (font directory, then the system) and
 * resolves the five faces from that inventory. Nothing is searched for after this call. Fails when
 * not even the regular face can be found.
 */
bool zrn_fonts_init(void);
/** Closes every face, frees the inventory and the path of Zset_font_directory. Every FT_Face is invalid afterwards. */
void zrn_fonts_shutdown(void);

/** The face index of a font family; an unknown family is the regular face. */
uint8_t zrn_face_index(ZFontFamily family);
/** The face; a weight that has no font uses the regular one. Never NULL after zrn_fonts_init. Owned here and valid until zrn_fonts_shutdown: never FT_Done_Face it, and set the size before every use (all users share it). */
FT_Face zrn_face(uint8_t face);
/**
 * The face that draws `cp` for `face`: the face itself when it has the glyph, else the best font of
 * the inventory that has it, else the face itself (the missing-glyph box).
 */
FT_Face zrn_face_for_char(uint8_t face, uint32_t cp);
