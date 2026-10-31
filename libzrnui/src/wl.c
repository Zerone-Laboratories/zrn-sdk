// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * The window: a plain Wayland client (xdg-shell, wl_egl, GLES2), the frame
 * clock (a frame is drawn only when something changed or moves, paced by the
 * compositor's frame callbacks), touch, pointer and keyboard, and the main
 * loop with the app's file descriptors, timers and posts from other threads.
 * The Wayland and EGL setup follows ZrnMetroCore's (ztmetro/core).
 */
#define _GNU_SOURCE
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <errno.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"
#include "zui-internal.h"

void zui_run_posts(zui_app *app);

struct zui_wl {
	zui_app *app;
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct xdg_wm_base *wm_base;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_touch *touch;
	struct wl_keyboard *keyboard;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	struct wl_callback *frame;
	struct wl_egl_window *egl_window;
	EGLDisplay egl_display;
	EGLContext egl_context;
	EGLSurface egl_surface;
	EGLConfig egl_config;
	bool configured;
	int fixed_scale, preferred_scale, output_scale;
	int pending_w, pending_h;
	double px, py;
	bool pointer_down;
	int32_t touch_id;
	bool touch_down;
	struct xkb_context *xkb;
	struct xkb_keymap *keymap;
	struct xkb_state *xkb_state;
	unsigned frames;
	/* ZUI_SCRIPT: steps for tests and evidence ("wait 500; tap 180 300; snap a.png; back; quit") */
	char *script, *script_at;
	uint64_t script_wait_until;
	char *snap_pending;
};

static uint64_t now_ms64(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int scale_of(struct zui_wl *w) {
	if (w->fixed_scale) return w->fixed_scale;
	if (w->preferred_scale) return w->preferred_scale;
	return w->output_scale ? w->output_scale : 1;
}

/* ---- pointer: a mouse on the laptop works as a finger ---- */
static void p_enter(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
	struct zui_wl *w = d;
	w->px = wl_fixed_to_double(x);
	w->py = wl_fixed_to_double(y);
}
static void p_leave(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
	struct zui_wl *w = d;
	if (w->pointer_down) {
		w->pointer_down = false;
		zui_touch(w->app, 3, (float)w->px, (float)w->py, 0);
	}
}
static void p_motion(void *d, struct wl_pointer *p, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
	struct zui_wl *w = d;
	w->px = wl_fixed_to_double(x);
	w->py = wl_fixed_to_double(y);
	if (w->pointer_down) zui_touch(w->app, 1, (float)w->px, (float)w->py, time);
}
static void p_button(void *d, struct wl_pointer *p, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	struct zui_wl *w = d;
	if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		zui_back(w->app);	/* the right button is Back on the laptop */
		return;
	}
	if (button != BTN_LEFT) return;
	w->pointer_down = state == WL_POINTER_BUTTON_STATE_PRESSED;
	zui_touch(w->app, w->pointer_down ? 0 : 2, (float)w->px, (float)w->py, time);
}
static void p_axis(void *d, struct wl_pointer *p, uint32_t time, uint32_t axis, wl_fixed_t value) {
	struct zui_wl *w = d;
	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) zui_scroll_by(w->app, (float)wl_fixed_to_double(value) * 2.5f);
}
static void p_frame(void *d, struct wl_pointer *p) {}
static void p_axis_source(void *d, struct wl_pointer *p, uint32_t s) {}
static void p_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) {}
static void p_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v) {}
static const struct wl_pointer_listener pointer_listener = {
	.enter = p_enter, .leave = p_leave, .motion = p_motion, .button = p_button, .axis = p_axis,
	.frame = p_frame, .axis_source = p_axis_source, .axis_stop = p_axis_stop, .axis_discrete = p_axis_discrete,
};

/* ---- touch: the first finger only ---- */
static void t_down(void *d, struct wl_touch *t, uint32_t serial, uint32_t time, struct wl_surface *s,
		int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct zui_wl *w = d;
	if (w->touch_down) return;
	w->touch_down = true;
	w->touch_id = id;
	zui_touch(w->app, 0, (float)wl_fixed_to_double(x), (float)wl_fixed_to_double(y), time);
}
static void t_up(void *d, struct wl_touch *t, uint32_t serial, uint32_t time, int32_t id) {
	struct zui_wl *w = d;
	if (!w->touch_down || id != w->touch_id) return;
	w->touch_down = false;
	zui_touch(w->app, 2, w->app->last_x, w->app->last_y, time);
}
static void t_motion(void *d, struct wl_touch *t, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct zui_wl *w = d;
	if (!w->touch_down || id != w->touch_id) return;
	zui_touch(w->app, 1, (float)wl_fixed_to_double(x), (float)wl_fixed_to_double(y), time);
}
static void t_frame(void *d, struct wl_touch *t) {}
static void t_cancel(void *d, struct wl_touch *t) {
	struct zui_wl *w = d;
	if (!w->touch_down) return;
	w->touch_down = false;
	zui_touch(w->app, 3, w->app->last_x, w->app->last_y, 0);
}
static const struct wl_touch_listener touch_listener = {
	.down = t_down, .up = t_up, .motion = t_motion, .frame = t_frame, .cancel = t_cancel,
};

/* ---- keyboard ---- */
static void k_keymap(void *d, struct wl_keyboard *k, uint32_t format, int32_t fd, uint32_t size) {
	struct zui_wl *w = d;
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || !size) {
		close(fd);
		return;
	}
	char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED) return;
	if (!w->xkb) w->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *next = w->xkb ? xkb_keymap_new_from_buffer(w->xkb, map, size - 1,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
	munmap(map, size);
	if (!next) return;
	struct xkb_state *state = xkb_state_new(next);
	if (!state) {
		xkb_keymap_unref(next);
		return;
	}
	xkb_state_unref(w->xkb_state);
	xkb_keymap_unref(w->keymap);
	w->keymap = next;
	w->xkb_state = state;
}
static void k_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys) {}
static void k_leave(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) {}
static void k_key(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	struct zui_wl *w = d;
	zui_app *app = w->app;
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
	if (key == KEY_ESC || key == KEY_BACK) {
		if (app->focus) {
			app->focus->focused = false;
			zanim_go(&app->focus->focus_anim, 0, 150, zui_now());
			app->focus = NULL;
			zui_osk(app, false);
			app->dirty = true;
		} else {
			zui_back(app);
		}
		return;
	}
	if (!app->focus) return;
	if (key == KEY_BACKSPACE) { zui_key_text(app, "\b"); return; }
	if (key == KEY_ENTER || key == KEY_KPENTER) { zui_key_text(app, "\n"); return; }
	if (!w->xkb_state) return;
	char utf8[64];
	int n = xkb_state_key_get_utf8(w->xkb_state, key + 8, utf8, sizeof(utf8));
	if (n > 0 && n < (int)sizeof(utf8) && (unsigned char)utf8[0] >= 0x20 && utf8[0] != 0x7f) zui_key_text(app, utf8);
}
static void k_modifiers(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t dep, uint32_t lat,
		uint32_t lock, uint32_t group) {
	struct zui_wl *w = d;
	if (w->xkb_state) xkb_state_update_mask(w->xkb_state, dep, lat, lock, 0, 0, group);
}
static void k_repeat(void *d, struct wl_keyboard *k, int32_t rate, int32_t delay) {}
static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = k_keymap, .enter = k_enter, .leave = k_leave, .key = k_key,
	.modifiers = k_modifiers, .repeat_info = k_repeat,
};

static void seat_caps(void *d, struct wl_seat *seat, uint32_t caps) {
	struct zui_wl *w = d;
	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->pointer) {
		w->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(w->pointer, &pointer_listener, w);
	}
	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !w->touch) {
		w->touch = wl_seat_get_touch(seat);
		wl_touch_add_listener(w->touch, &touch_listener, w);
	}
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w->keyboard) {
		w->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(w->keyboard, &keyboard_listener, w);
	}
}
static void seat_name(void *d, struct wl_seat *seat, const char *name) {}
static const struct wl_seat_listener seat_listener = { .capabilities = seat_caps, .name = seat_name };

/* ---- outputs, for the scale when the compositor does not say one ---- */
static void o_geometry(void *d, struct wl_output *o, int32_t x, int32_t y, int32_t pw, int32_t ph,
		int32_t sub, const char *make, const char *model, int32_t tr) {}
static void o_mode(void *d, struct wl_output *o, uint32_t flags, int32_t w, int32_t h, int32_t r) {}
static void o_done(void *d, struct wl_output *o) {}
static void o_scale(void *d, struct wl_output *o, int32_t factor) {
	struct zui_wl *w = d;
	if (factor > w->output_scale) {
		w->output_scale = factor;
		w->app->dirty = true;
	}
}
static const struct wl_output_listener output_listener = {
	.geometry = o_geometry, .mode = o_mode, .done = o_done, .scale = o_scale,
};

static void s_enter(void *d, struct wl_surface *s, struct wl_output *o) {}
static void s_leave(void *d, struct wl_surface *s, struct wl_output *o) {}
static void s_preferred_scale(void *d, struct wl_surface *s, int32_t factor) {
	struct zui_wl *w = d;
	w->preferred_scale = factor;
	w->app->dirty = true;
}
static void s_preferred_transform(void *d, struct wl_surface *s, uint32_t t) {}
static const struct wl_surface_listener surface_listener = {
	.enter = s_enter, .leave = s_leave,
#ifdef WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION
	.preferred_buffer_scale = s_preferred_scale,
	.preferred_buffer_transform = s_preferred_transform,
#endif
};

/* ---- xdg ---- */
static void xs_configure(void *d, struct xdg_surface *xs, uint32_t serial) {
	struct zui_wl *w = d;
	xdg_surface_ack_configure(xs, serial);
	if (w->pending_w > 0 && w->pending_h > 0) {
		if (w->app->width != w->pending_w || w->app->height != w->pending_h) w->app->laid_out = false;
		w->app->width = (float)w->pending_w;
		w->app->height = (float)w->pending_h;
	}
	w->configured = true;
	w->app->dirty = true;
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xs_configure };

static void xt_configure(void *d, struct xdg_toplevel *t, int32_t width, int32_t height, struct wl_array *states) {
	struct zui_wl *w = d;
	w->pending_w = width;
	w->pending_h = height;
	bool active = false;
	uint32_t *st;
	wl_array_for_each(st, states) active = active || *st == XDG_TOPLEVEL_STATE_ACTIVATED;
	w->app->active = active;
}
static void xt_close(void *d, struct xdg_toplevel *t) {
	struct zui_wl *w = d;
	w->app->running = false;
}
static void xt_bounds(void *d, struct xdg_toplevel *t, int32_t width, int32_t height) {}
static void xt_caps(void *d, struct xdg_toplevel *t, struct wl_array *caps) {}
static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = xt_configure, .close = xt_close,
#ifdef XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION
	.configure_bounds = xt_bounds,
#endif
#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
	.wm_capabilities = xt_caps,
#endif
};

static void ping(void *d, struct xdg_wm_base *b, uint32_t serial) { xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener wm_base_listener = { .ping = ping };

static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version) {
	struct zui_wl *w = d;
	if (!strcmp(iface, wl_compositor_interface.name)) {
		w->compositor = wl_registry_bind(r, name, &wl_compositor_interface, version < 6 ? version : 6);
	} else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		w->wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(w->wm_base, &wm_base_listener, w);
	} else if (!strcmp(iface, wl_seat_interface.name) && !w->seat) {
		w->seat = wl_registry_bind(r, name, &wl_seat_interface, version < 5 ? version : 5);
		wl_seat_add_listener(w->seat, &seat_listener, w);
	} else if (!strcmp(iface, wl_output_interface.name)) {
		struct wl_output *o = wl_registry_bind(r, name, &wl_output_interface, version < 2 ? version : 2);
		wl_output_add_listener(o, &output_listener, w);
	}
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t name) {}
static const struct wl_registry_listener registry_listener = { .global = reg_global, .global_remove = reg_remove };

/* ---- drawing a frame ---- */
static void frame_done(void *d, struct wl_callback *cb, uint32_t time);
static const struct wl_callback_listener frame_listener = { .done = frame_done };

/* ZUI_PERFLOG=1: what building and submitting a frame costs the CPU (every
 * 60 frames, and at exit) */
static struct { int on, n, fw, fh; double sum, worst; } perf = { .on = -1 };

static void perf_report(void) {
	if (!perf.n) return;
	fprintf(stderr, "zrnui: frame CPU %.2f ms average, %.2f ms worst (%d frames, %dx%d px)\n",
		perf.sum / perf.n, perf.worst, perf.n, perf.fw, perf.fh);
	perf.sum = perf.worst = 0;
	perf.n = 0;
}

static void render(struct zui_wl *w) {
	zui_app *app = w->app;
	int scale = scale_of(w);
	if (scale != app->scale) {
		app->scale = scale;
	}
	int fw = (int)lroundf(app->width) * scale, fh = (int)lroundf(app->height) * scale;
	wl_egl_window_resize(w->egl_window, fw, fh, 0, 0);
	wl_surface_set_buffer_scale(w->surface, scale);
	uint32_t now = zui_now();
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	zui_step(app, now);
	app->dirty = false;
	struct zui_palette pal = zui_pal(app);
	zg_begin(fw, fh, (float)scale, pal.bg);
	zui_draw(app, now);
	zg_end();
	clock_gettime(CLOCK_MONOTONIC, &t1);
	if (perf.on < 0) perf.on = getenv("ZUI_PERFLOG") != NULL;
	{
		/* ZUI_FRAMELOG: one line a frame, for measuring on the phone */
		static int every = -1;
		if (every < 0) every = getenv("ZUI_FRAMELOG") != NULL;
		if (every) {
			struct timespec t2;
			clock_gettime(CLOCK_MONOTONIC, &t2);
			static FILE *log;
			if (!log) log = fopen(getenv("ZUI_FRAMELOG"), "a");
			if (log) {
				fprintf(log, "%.1f %s: frame %.1f ms, swap 0.0 ms\n", t0.tv_sec * 1e3 + t0.tv_nsec / 1e6, app->app_id,
					(t2.tv_sec - t0.tv_sec) * 1e3 + (t2.tv_nsec - t0.tv_nsec) / 1e6);
				fflush(log);
			}
		}
	}
	if (perf.on) {
		double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
		perf.sum += ms;
		if (ms > perf.worst) perf.worst = ms;
		perf.fw = fw;
		perf.fh = fh;
		if (++perf.n == 60) perf_report();
	}
	if (w->snap_pending) {
		uint8_t *px = malloc((size_t)fw * fh * 4);
		glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px);
		for (size_t i = 3; i < (size_t)fw * fh * 4; i += 4) px[i] = 255;
		if (zui_png_write(w->snap_pending, px, fw, fh, true) == 0)
			fprintf(stderr, "zrnui: snapshot %s (%dx%d, scale %d)\n", w->snap_pending, fw, fh, scale);
		free(px);
		free(w->snap_pending);
		w->snap_pending = NULL;
	}
	w->frame = wl_surface_frame(w->surface);
	wl_callback_add_listener(w->frame, &frame_listener, w);
	eglSwapBuffers(w->egl_display, w->egl_surface);
	w->frames++;
}

static void frame_done(void *d, struct wl_callback *cb, uint32_t time) {
	struct zui_wl *w = d;
	wl_callback_destroy(cb);
	w->frame = NULL;
}

static bool egl_setup(struct zui_wl *w) {
	w->egl_display = eglGetDisplay((EGLNativeDisplayType)w->display);
	if (w->egl_display == EGL_NO_DISPLAY || !eglInitialize(w->egl_display, NULL, NULL)) return false;
	if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;
	EGLint attrs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0, EGL_NONE };
	EGLint count = 0;
	if (!eglChooseConfig(w->egl_display, attrs, &w->egl_config, 1, &count) || count < 1) return false;
	EGLint ctx[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	w->egl_context = eglCreateContext(w->egl_display, w->egl_config, EGL_NO_CONTEXT, ctx);
	if (w->egl_context == EGL_NO_CONTEXT) return false;
	int scale = scale_of(w);
	w->egl_window = wl_egl_window_create(w->surface, (int)w->app->width * scale, (int)w->app->height * scale);
	if (!w->egl_window) return false;
	w->egl_surface = eglCreateWindowSurface(w->egl_display, w->egl_config, (EGLNativeWindowType)w->egl_window, NULL);
	if (w->egl_surface == EGL_NO_SURFACE) return false;
	if (!eglMakeCurrent(w->egl_display, w->egl_surface, w->egl_surface, w->egl_context)) return false;
	/* the frame callback is the clock */
	eglSwapInterval(w->egl_display, 0);
	return zg_init();
}

/* ---- ZUI_SCRIPT ---- */
static void script_step(struct zui_wl *w) {
	zui_app *app = w->app;
	while (w->script_at && *w->script_at) {
		if (w->snap_pending) return;	/* the picture is taken at the next frame */
		if (w->script_wait_until && now_ms64() < w->script_wait_until) return;
		w->script_wait_until = 0;
		char *end = strchr(w->script_at, ';');
		char cmd[512];
		size_t len = end ? (size_t)(end - w->script_at) : strlen(w->script_at);
		if (len >= sizeof(cmd)) len = sizeof(cmd) - 1;
		memcpy(cmd, w->script_at, len);
		cmd[len] = 0;
		w->script_at = end ? end + 1 : w->script_at + strlen(w->script_at);
		char *c = cmd;
		while (*c == ' ') c++;
		double x, y;
		int ms;
		char path[480];
		if (sscanf(c, "wait %d", &ms) == 1) {
			w->script_wait_until = now_ms64() + (uint64_t)ms;
			return;
		} else if (sscanf(c, "tap %lf %lf", &x, &y) == 2) {
			uint32_t t = zui_now();
			zui_touch(app, 0, (float)x, (float)y, t);
			zui_touch(app, 2, (float)x, (float)y, t + 60);
		} else if (sscanf(c, "drag %lf %lf %d", &x, &y, &ms) == 3) {
			/* a finger from (x, y) moving ms points up, over 200 ms */
			uint32_t t = zui_now();
			zui_touch(app, 0, (float)x, (float)y, t);
			for (int i = 1; i <= 10; i++) zui_touch(app, 1, (float)x, (float)(y - ms * i / 10.0), t + i * 20);
			zui_touch(app, 2, (float)x, (float)(y - ms), t + 200);
		} else if (sscanf(c, "type %479[^\n]", path) == 1) {
			zui_key_text(app, path);
		} else if (sscanf(c, "snap %479s", path) == 1) {
			w->snap_pending = strdup(path);
			app->dirty = true;
		} else if (strcmp(c, "back") == 0) {
			zui_back(app);
		} else if (strcmp(c, "quit") == 0) {
			zui_quit(app);
		}
		app->dirty = true;
	}
}

void zui_wl_wake(zui_app *app) {
	if (app->post_fd >= 0) {
		uint64_t one = 1;
		if (write(app->post_fd, &one, sizeof(one)) < 0) {}
	}
}

static int timers_due(zui_app *app, uint64_t now) {
	int soonest = -1;
	for (int i = 0; i < ZUI_MAX_TIMERS; i++) {
		struct zui_timer_entry *t = &app->timers[i];
		if (!t->id) continue;
		if (t->due <= now) {
			zui_fn fn = t->fn;
			void *data = t->data;
			if (t->every) t->due = now + t->every;
			else t->id = 0;
			fn(data);
			app->dirty = true;
			if (!t->id) continue;
		}
		int left = (int)(t->due - now);
		if (soonest < 0 || left < soonest) soonest = left;
	}
	return soonest;
}

ZUI_EXPORT int zui_timer(zui_app *app, int ms, bool repeat, zui_fn fn, void *data) {
	for (int i = 0; i < ZUI_MAX_TIMERS; i++) {
		if (app->timers[i].id) continue;
		app->timers[i] = (struct zui_timer_entry){ app->next_timer_id++, now_ms64() + (uint64_t)(ms < 0 ? 0 : ms),
			repeat ? (uint64_t)(ms < 1 ? 1 : ms) : 0, fn, data };
		return app->timers[i].id;
	}
	return 0;
}

ZUI_EXPORT void zui_timer_cancel(zui_app *app, int id) {
	for (int i = 0; i < ZUI_MAX_TIMERS; i++)
		if (app->timers[i].id == id) app->timers[i].id = 0;
}

ZUI_EXPORT void zui_watch_fd(zui_app *app, int fd, void (*ready)(int, void *), void *data) {
	for (int i = 0; i < ZUI_MAX_FDS; i++) {
		if (app->fds[i].ready && app->fds[i].fd == fd) {
			if (!ready) app->fds[i].ready = NULL;
			else { app->fds[i].ready = ready; app->fds[i].data = data; }
			return;
		}
	}
	if (!ready) return;
	for (int i = 0; i < ZUI_MAX_FDS; i++) {
		if (!app->fds[i].ready) {
			app->fds[i].fd = fd;
			app->fds[i].ready = ready;
			app->fds[i].data = data;
			return;
		}
	}
}

int zui_wl_run(zui_app *app) {
	struct zui_wl *w = calloc(1, sizeof(*w));
	w->app = app;
	app->wl = w;
	const char *env_scale = getenv("ZUI_SCALE");
	if (!env_scale) env_scale = getenv("ZTMETRO_SCALE");	/* the shell's, on the phone */
	if (env_scale && strtod(env_scale, NULL) >= 1.0) {
		double s = ceil(strtod(env_scale, NULL) - 0.01);
		w->fixed_scale = s > 4 ? 4 : (int)s;
	}
	const char *win = getenv("ZUI_WINDOW");
	int ww = 360, wh = 740;
	if (win) sscanf(win, "%dx%d", &ww, &wh);
	app->width = (float)ww;
	app->height = (float)wh;
	w->display = wl_display_connect(NULL);
	if (!w->display) {
		fprintf(stderr, "%s: no Wayland display (WAYLAND_DISPLAY is %s); on the laptop use `zrn run`\n",
			app->app_id, getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "not set");
		app->wl = NULL;
		free(w);
		return 1;
	}
	struct wl_registry *reg = wl_display_get_registry(w->display);
	wl_registry_add_listener(reg, &registry_listener, w);
	wl_display_roundtrip(w->display);
	if (!w->compositor || !w->wm_base) {
		fprintf(stderr, "%s: the compositor has no xdg-shell\n", app->app_id);
		return 1;
	}
	w->surface = wl_compositor_create_surface(w->compositor);
	wl_surface_add_listener(w->surface, &surface_listener, w);
	w->xdg_surface = xdg_wm_base_get_xdg_surface(w->wm_base, w->surface);
	xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);
	w->toplevel = xdg_surface_get_toplevel(w->xdg_surface);
	xdg_toplevel_add_listener(w->toplevel, &toplevel_listener, w);
	xdg_toplevel_set_app_id(w->toplevel, app->app_id);
	xdg_toplevel_set_title(w->toplevel, app->title);
	wl_surface_commit(w->surface);
	wl_display_roundtrip(w->display);
	if (!egl_setup(w)) {
		fprintf(stderr, "%s: EGL/GLES2 setup failed\n", app->app_id);
		return 1;
	}
	const char *script = getenv("ZUI_SCRIPT");
	if (script) {
		w->script = strdup(script);
		w->script_at = w->script;
	}
	app->dirty = true;
	while (app->running) {
		uint64_t now = now_ms64();
		int timeout = timers_due(app, now);
		if (w->script_at && *w->script_at && w->configured && w->frames > 0) {
			script_step(w);
			if (w->script_wait_until) {
				int left = (int)(w->script_wait_until - now_ms64());
				if (left < 0) left = 0;
				if (timeout < 0 || left < timeout) timeout = left;
			}
		}
		if (!app->running) break;
		bool moving = zui_animating(app);
		if (w->configured && !w->frame && (app->dirty || moving || w->snap_pending)) render(w);
		/* moving: the frame callback wakes us; still: sleep until something happens */
		if (w->frame && moving && (timeout < 0 || timeout > 100)) timeout = 100;
		if (!w->frame && moving) timeout = 4;
		while (wl_display_prepare_read(w->display) != 0) wl_display_dispatch_pending(w->display);
		if (wl_display_flush(w->display) < 0 && errno != EAGAIN) {
			wl_display_cancel_read(w->display);
			break;
		}
		struct pollfd pfd[2 + ZUI_MAX_FDS];
		int map[2 + ZUI_MAX_FDS];
		int n = 0;
		pfd[n] = (struct pollfd){ wl_display_get_fd(w->display), POLLIN, 0 };
		map[n++] = -1;
		pfd[n] = (struct pollfd){ app->post_fd, POLLIN, 0 };
		map[n++] = -2;
		for (int i = 0; i < ZUI_MAX_FDS; i++) {
			if (!app->fds[i].ready) continue;
			pfd[n] = (struct pollfd){ app->fds[i].fd, POLLIN, 0 };
			map[n++] = i;
		}
		int r = poll(pfd, (nfds_t)n, timeout);
		if (r > 0 && (pfd[0].revents & POLLIN)) {
			if (wl_display_read_events(w->display) < 0) break;
		} else {
			wl_display_cancel_read(w->display);
		}
		if (pfd[0].revents & (POLLERR | POLLHUP)) break;
		if (wl_display_dispatch_pending(w->display) < 0) break;
		for (int i = 1; r > 0 && i < n; i++) {
			if (!(pfd[i].revents & (POLLIN | POLLHUP))) continue;
			if (map[i] == -2) zui_run_posts(app);
			else if (map[i] >= 0 && app->fds[map[i]].ready) {
				app->fds[map[i]].ready(app->fds[map[i]].fd, app->fds[map[i]].data);
				app->dirty = true;
			}
		}
	}
	if (perf.on > 0) perf_report();
	zg_fini();
	eglMakeCurrent(w->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (w->egl_surface) eglDestroySurface(w->egl_display, w->egl_surface);
	if (w->egl_context) eglDestroyContext(w->egl_display, w->egl_context);
	eglTerminate(w->egl_display);
	if (w->egl_window) wl_egl_window_destroy(w->egl_window);
	xdg_toplevel_destroy(w->toplevel);
	xdg_surface_destroy(w->xdg_surface);
	wl_surface_destroy(w->surface);
	wl_display_disconnect(w->display);
	free(w->script);
	app->wl = NULL;
	free(w);
	return app->status;
}
