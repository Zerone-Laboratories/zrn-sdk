/**
 * zrnui fuzzers - shared setup: software renderer, no window, errors silenced.
 */

#pragma once

#include <zrnui/controls.h>
#include <zrnui/host.h>
#include <zrnui/platform.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void zrn_fuzz_quiet(ZError, const char*, void*) {}

/** Once per process: the renderer is the software one so that no GPU is touched, and the errors are not printed. */
static inline void zrn_fuzz_start(void) {
	setenv("ZRNUI_RENDERER", "software", 1);
	setenv("ZRNUI_ANIMATIONS", "off", 1);
	(void)Zinit();
	Zset_error_callback(zrn_fuzz_quiet, nullptr);
}

/** A NUL-terminated copy of the input, so that the library sees a C string. */
static inline char* zrn_fuzz_string(const uint8_t* const data, const size_t size) {
	char* const text = (char*)malloc(size + 1);
	if (!text) return nullptr;
	memcpy(text, data, size);
	text[size] = '\0';
	return text;
}
