/* SPDX-License-Identifier: LGPL-2.1-or-later */
/**
 * zrnapp - the theme and the accent of the shell, read from its config file as ztMETRO's Settings writes it.
 */

#include "shell_theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_checks, g_failures;

#define CHECK(condition) do { \
	g_checks++; \
	if (!(condition)) { g_failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); } \
} while (0)

static void write_file(const char* const path, const char* const text) {
	FILE* const f = fopen(path, "w");
	if (!f) return;
	fputs(text, f);
	fclose(f);
}

static void test_light_and_named_accent(const char* const path) {
	zrnapp_shell_theme_t t;
	write_file(path, "# ztmetro\nstart_wallpaper = /x.jpg\ntheme = light\naccent = lime\ncolumns = 4\n");
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(t.b_has_theme && t.theme == Z_THEME_LIGHT);
	CHECK(t.b_has_accent);
	CHECK(t.keyboard_height == 3 + 4 * 56);   /* no key_height: the shell's default */
	write_file(path, "key_height = 48\n");
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(t.keyboard_height == 3 + 4 * 48);
	write_file(path, "# ztmetro\nstart_wallpaper = /x.jpg\ntheme = light\naccent = lime\ncolumns = 4\n");
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(fabsf(t.accent.r - 0xa4 / 255.0f) < 0.001f && fabsf(t.accent.g - 0xc4 / 255.0f) < 0.001f && t.accent.b == 0.0f);
}

static void test_dark_hex_and_unknown(const char* const path) {
	zrnapp_shell_theme_t t;
	write_file(path, "theme=dark\naccent=#25d366\n");
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(t.theme == Z_THEME_DARK && t.b_has_accent);
	CHECK(fabsf(t.accent.g - 0xd3 / 255.0f) < 0.001f);
	write_file(path, "accent = nosuchcolor\n");
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(!t.b_has_accent && !t.b_has_theme);
}

static void test_session_stands_in(const char* const path) {
	zrnapp_shell_theme_t t;
	write_file(path, "accent = red\n");
	setenv("ZTRF_SHELL_THEME", "light", 1);
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(t.b_has_theme && t.theme == Z_THEME_LIGHT);
	write_file(path, "theme = dark\n");
	CHECK(zrnapp_shell_theme_read(path, &t));
	CHECK(t.theme == Z_THEME_DARK);   /* the file wins: the shell changed it since the session started */
	unsetenv("ZTRF_SHELL_THEME");
	CHECK(!zrnapp_shell_theme_read("/nonexistent/zrn/config", &t));
	CHECK(!t.b_has_theme && !t.b_has_accent);
}

static void test_path(void) {
	char path[256];
	setenv("ZRNAPP_SHELL_CONFIG", "/tmp/own-config", 1);
	CHECK(zrnapp_shell_theme_path(path, sizeof path));
	CHECK(strcmp(path, "/tmp/own-config") == 0);
	unsetenv("ZRNAPP_SHELL_CONFIG");
	setenv("HOME", "/home/someone", 1);
	CHECK(zrnapp_shell_theme_path(path, sizeof path));
	CHECK(strcmp(path, "/home/someone/.config/ztmetro/config") == 0);
}

static void test_osk_file(const char* const path) {
	float h = -1;
	bool b_visible = false;
	write_file(path, "254 1\n");
	CHECK(zrnapp_shell_osk_read(path, &h, &b_visible));
	CHECK(h == 254 && b_visible);
	write_file(path, "0 0\n");
	CHECK(zrnapp_shell_osk_read(path, &h, &b_visible));
	CHECK(h == 0 && !b_visible);
	write_file(path, "garbage\n");
	CHECK(!zrnapp_shell_osk_read(path, &h, &b_visible));
	CHECK(!zrnapp_shell_osk_read("/nonexistent/ztmetro-osk", &h, &b_visible));
	char p[64];
	setenv("XDG_RUNTIME_DIR", "/run/user/7", 1);
	CHECK(zrnapp_shell_osk_path(p, sizeof p) && strcmp(p, "/run/user/7/ztmetro-osk") == 0);
}

int main(void) {
	char path[] = "/tmp/zrnapp-theme-XXXXXX";
	const int fd = mkstemp(path);
	if (fd < 0) return 1;
	close(fd);
	unsetenv("ZTRF_SHELL_THEME");
	test_light_and_named_accent(path);
	test_dark_hex_and_unknown(path);
	test_session_stands_in(path);
	test_path();
	test_osk_file(path);
	unlink(path);
	printf("test_shell_theme: %d checks, %d failed\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
