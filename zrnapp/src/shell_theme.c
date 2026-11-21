/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * zrnapp - shell_theme.c
 * Reads the theme and the accent of the shell from its config file (see shell_theme.h).
 */

#include "shell_theme.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The accents as the shell names them, with the colors ZrnMetroCore gives them (zmc_config_accent). */
typedef struct accent_name {
	const char* name;
	uint32_t rgb;
} accent_name_t;

static const accent_name_t g_accents[] = {
	{ "lime", 0xa4c400 }, { "green", 0x60a917 }, { "emerald", 0x008a00 },
	{ "teal", 0x00aba9 }, { "cyan", 0x1ba1e2 }, { "cobalt", 0x0050ef },
	{ "indigo", 0x6a00ff }, { "violet", 0xaa00ff }, { "pink", 0xf472d0 },
	{ "magenta", 0xd80073 }, { "crimson", 0xa20025 }, { "red", 0xe51400 },
	{ "orange", 0xfa6800 }, { "amber", 0xf0a30a }, { "yellow", 0xe3c800 },
	{ "brown", 0x825a2c }, { "olive", 0x6d8764 }, { "steel", 0x647687 },
	{ "mauve", 0x76608a }, { "taupe", 0x87794e },
};

static ZColor rgb_color(const uint32_t rgb) {
	const ZColor c = { .r = (float)((rgb >> 16) & 0xff) / 255.0f, .g = (float)((rgb >> 8) & 0xff) / 255.0f, .b = (float)(rgb & 0xff) / 255.0f, .a = 1.0f };
	return c;
}

static char* trim(char* s) {
	while (*s && isspace((unsigned char)*s)) s++;
	char* end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) end--;
	*end = '\0';
	return s;
}

static bool parse_accent(const char* const value, ZColor* const out) {
	for (size_t i = 0; i < sizeof g_accents / sizeof g_accents[0]; i++) {
		if (strcmp(g_accents[i].name, value) != 0) continue;
		*out = rgb_color(g_accents[i].rgb);
		return true;
	}
	if (value[0] != '#' || strlen(value) != 7) return false;
	char* end = nullptr;
	const unsigned long rgb = strtoul(value + 1, &end, 16);
	if (!end || *end) return false;
	*out = rgb_color((uint32_t)rgb);
	return true;
}

static bool is_light(const char* const value) { return strcmp(value, "light") == 0 || strcmp(value, "1") == 0; }

bool zrnapp_shell_theme_read(const char* const path, zrnapp_shell_theme_t* const out) {
	memset(out, 0, sizeof *out);
	out->theme = Z_THEME_DARK;
	out->b_frost = true;
	int key_height = 56;
	FILE* const f = path ? fopen(path, "r") : nullptr;
	char line[256];
	while (f && fgets(line, sizeof line, f)) {
		char* const eq = strchr(line, '=');
		if (line[0] == '#' || !eq) continue;
		*eq = '\0';
		const char* const key = trim(line);
		const char* const value = trim(eq + 1);
		if (strcmp(key, "theme") == 0) {
			out->b_has_theme = true;
			out->theme = is_light(value) ? Z_THEME_LIGHT : Z_THEME_DARK;
		} else if (strcmp(key, "frost") == 0) {
			out->b_frost = !(strcmp(value, "0") == 0 || strcmp(value, "off") == 0);
		} else if (strcmp(key, "key_height") == 0) {
			const int n = atoi(value);
			key_height = n < 42 ? 42 : n > 72 ? 72 : n;
		} else if (strcmp(key, "accent") == 0) {
			ZColor c;
			if (!parse_accent(value, &c)) continue;
			out->b_has_accent = true;
			out->accent = c;
		}
	}
	if (f) fclose(f);
	/* osk_layout of ztmetro.c: a gap of 3 above four rows of keys */
	out->keyboard_height = 3.0f + 4.0f * (float)key_height;
	const char* const session = getenv("ZTRF_SHELL_THEME");
	if (!out->b_has_theme && session && *session) {
		out->b_has_theme = true;
		out->theme = is_light(session) ? Z_THEME_LIGHT : Z_THEME_DARK;
	}
	return f != nullptr;
}

bool zrnapp_shell_theme_path(char* const out, const size_t capacity) {
	const char* const own = getenv("ZRNAPP_SHELL_CONFIG");
	if (own && *own) return snprintf(out, capacity, "%s", own) < (int)capacity;
	const char* const home = getenv("HOME");
	if (!home || !*home) return false;
	return snprintf(out, capacity, "%s/.config/ztmetro/config", home) < (int)capacity;
}

bool zrnapp_shell_osk_read(const char* const path, float* const height, bool* const b_visible) {
	FILE* const f = fopen(path, "r");
	if (!f) return false;
	int h = 0, v = 0;
	const bool b_ok = fscanf(f, "%d %d", &h, &v) == 2 && h >= 0 && h < 10000 && (v == 0 || v == 1);
	fclose(f);
	if (!b_ok) return false;
	*height = (float)h;
	*b_visible = v == 1;
	return true;
}

bool zrnapp_shell_osk_path(char* const out, const size_t capacity) {
	const char* const runtime = getenv("XDG_RUNTIME_DIR");
	if (!runtime || !*runtime) return false;
	return snprintf(out, capacity, "%s/ztmetro-osk", runtime) < (int)capacity;
}
