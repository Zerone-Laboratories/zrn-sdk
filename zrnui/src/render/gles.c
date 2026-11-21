/**
 * zrnui - render/gles.c
 * The OpenGL ES 2 renderer: the painter (render/painter.c) on the GPU.
 *
 * Every shape is a quad with a distance function evaluated in the fragment
 * shader (rounded rectangles, ellipses, lines), so the edges are anti-aliased
 * like those of the software painter. Glyphs come from an atlas, images from
 * one texture each. A transform (u_xf, a 4x4 matrix applied to the device
 * position, with perspective) is the way to animate: the painter draws a page
 * under a rotation without the controls knowing.
 *
 * The context is one that the platform made and gives (zrn_gl_create_external: zrnapp, a shell), or
 * an off-screen one with a framebuffer object (zrn_gl_create_offscreen), which
 * Zroot_render uses for a screenshot and the tests use to compare with the
 * software painter. Without EGL and GLES2 at build time every function here
 * says it is not available, and the software painter is all there is.
 */

#include "zrnui/internal/render/gles.h"
#include "zrnui/internal/util.h"
#include "zrnui/internal/image.h"
#include "zrnui/internal/xform.h"
#include "zrnui/internal/render/painter.h"
#include "zrnui/internal/render/backend.h"
#include "zrnui/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef ZRNUI_HAVE_GL

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

static constexpr int FLOATS = 19; /* x y  lx ly  hx hy  r0 r1 r2 r3  aux mode  r g b a  u v  w */
static constexpr int MAX_QUADS = 4096;
/* The glyph atlas is pages of 512 x 512: a driver that copies a texture the GPU still reads before it is written
 * (panfrost does) copies 256 KiB for a new glyph, not the 4 MiB of one 2048 x 2048 atlas, which made a page full of new
 * text take hundreds of ms in its first frame. */
static constexpr int ATLAS_SIZE = 512;
static constexpr int ATLAS_PAGES = 16;
/* the slots of the glyph atlas are found by a hash of their key: a power of two well above what the atlas holds */
static constexpr size_t ATLAS_INDEX = 1u << 15;

enum { MODE_SOLID = 0, MODE_RRECT, MODE_ELLIPSE, MODE_RING, MODE_GLYPH, MODE_IMAGE, MODE_LAYER };

/* the counters of Zrender_take_stats (platform.c) */
extern ZRenderStats zrn_render_stats;

typedef struct atlas_slot {
	const uint8_t* key;
	int w, h, x, y;
	int page;
} atlas_slot_t;

struct zrn_gl {
	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
	EGLConfig config;
	bool b_offscreen;
	/** A context that the platform (zrnapp, a shell) made: the renderer only draws in it. `current` makes it current. */
	bool b_external;
	void (*current)(void* user);
	void* current_user;
	GLuint program, fbo, fbo_texture;
	/** The pages of the glyph atlas, made as they are needed; `atlas_page` is the one being filled. */
	GLuint atlas[ATLAS_PAGES];
	int atlas_page;
	/** The vertex buffer the batches go through: client-side arrays made the driver copy every attribute of every
	 * draw on its own (Mesa's u_vbuf), a quarter of the CPU time of a frame on the phone. */
	GLuint vbo;
	/** The samples of the off-screen framebuffer (GL_EXT_multisampled_render_to_texture), 0 when it has none. */
	GLsizei fbo_samples;
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC fbo_multisample;
	GLint loc_size, loc_tex;
	int width, height;
	float* verts;
	int quads;
	int batch_clip[4], batch_clip_set;
	GLuint batch_texture;
	/* the atlas of glyphs: shelf packing, keyed by the bitmap of the glyph cache */
	atlas_slot_t* slots;
	size_t slot_count, slot_cap;
	/** The index of the slots by their key: open addressing, slot index + 1 (0 is empty), ATLAS_INDEX entries. */
	uint32_t* slot_index;
	int shelf_x, shelf_y, shelf_h;
	float xf[16];
	bool b_has_xf;
	zrn_image_t** images;
	int image_count, image_cap;
	/** How many layers (CacheMode) are being painted into, one inside the other: the blending of a layer keeps its alpha. */
	int layer_depth;
	/** The eye distance of the perspective, from the height of the surface: a layer (a smaller target) keeps the surface's. */
	float eye;
	/** A framebuffer object for each depth of layers, made once and kept: not one made and deleted for every layer painted. */
	GLuint layer_fbos[8];
	/** The layer atlas: the bitmap caches painted outside any other share one texture (and its framebuffer), shelf packed.
	 * A texture made for every cache cost 2.6 ms each on the phone (sixty for a chat coming in) and a draw call each. */
	GLuint latlas, latlas_fbo;
	int latlas_x, latlas_y, latlas_h;
	/** The origin of the region being painted in the target (a layer of the atlas), in GL coordinates: what the scissor adds. */
	int vp_x, vp_y;
	/** zrn-frosted-diffusion: the programs (made on first use), the grain, the framebuffer the strips and blurs are painted
	 * through; bd_failed when a program did not build (the backdrops are then filled with their tint). */
	GLuint bd_down, bd_frost, bd_grain, bd_fbo;
	bool bd_failed;
	/** While a strip is painted (a quarter of the frame's resolution): bitmap caches, made at the frame's, are drawn at this scale. */
	float bd_layer_scale;
};

static const char* VERTEX_SHADER =
	"attribute vec2 a_pos; attribute vec2 a_local; attribute vec2 a_half; attribute vec4 a_radii;\n"
	"attribute vec2 a_misc; attribute vec4 a_color; attribute vec2 a_uv; attribute float a_w;\n"
	"uniform vec2 u_size;\n"
	"varying vec2 v_local; varying vec2 v_half; varying vec4 v_radii; varying float v_aux; varying float v_mode;\n"
	"varying vec4 v_color; varying vec2 v_uv;\n"
	"void main() {\n"
	/* the position comes transformed (the CPU applies the transform, so that a new one does not end the batch) */
	"  gl_Position = vec4(a_pos.x / u_size.x * 2.0 - 1.0, 1.0 - a_pos.y / u_size.y * 2.0, 0.0, a_w);\n"
	"  v_local = a_local; v_half = a_half; v_radii = a_radii; v_aux = a_misc.x; v_mode = a_misc.y;\n"
	"  v_color = a_color; v_uv = a_uv;\n"
	"}\n";

static const char* FRAGMENT_SHADER =
	"precision highp float;\n"
	"varying vec2 v_local; varying vec2 v_half; varying vec4 v_radii; varying float v_aux; varying float v_mode;\n"
	"varying vec4 v_color; varying vec2 v_uv;\n"
	"uniform sampler2D u_tex;\n"
	"float rrect(vec2 p, vec2 h, vec4 r) {\n"
	"  float rad = p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z);\n"
	"  vec2 q = abs(p) - h + rad;\n"
	"  return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - rad;\n"
	"}\n"
	"float ellipse(vec2 p, vec2 r) {\n"
	"  float len = length(p / r);\n"
	"  return (len - 1.0) * min(r.x, r.y);\n"
	"}\n"
	"void main() {\n"
	"  float cov = 1.0;\n"
	"  if (v_mode < 0.5) cov = 1.0;\n"
	"  else if (v_mode < 1.5) cov = clamp(0.5 - rrect(v_local, v_half, v_radii), 0.0, 1.0);\n"
	"  else if (v_mode < 2.5) cov = clamp(0.5 - ellipse(v_local, v_half), 0.0, 1.0);\n"
	"  else if (v_mode < 3.5) { float d = ellipse(v_local, v_half); cov = clamp(0.5 - (abs(d + v_aux * 0.5) - v_aux * 0.5), 0.0, 1.0); }\n"
	"  else if (v_mode < 4.5) cov = texture2D(u_tex, v_uv).a;\n"
	"  else if (v_mode < 5.5) { vec4 t = texture2D(u_tex, v_uv); gl_FragColor = vec4(t.rgb, t.a * v_color.a); return; }\n"
	/* a layer holds premultiplied color: back to straight for the blending of the frame */
	"  else { vec4 t = texture2D(u_tex, v_uv); gl_FragColor = t.a > 0.0 ? vec4(t.rgb / t.a, t.a * v_color.a) : vec4(0.0); return; }\n"
	"  gl_FragColor = vec4(v_color.rgb, v_color.a * cov);\n"
	"}\n";

static GLuint compile(const GLenum type, const char* const source) {
	const GLuint s = glCreateShader(type);
	GLint ok = 0;
	glShaderSource(s, 1, &source, nullptr);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(s, sizeof log, nullptr, log);
		zrn_error(Z_ERROR_IO, "OpenGL shader: %s", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static bool build_program(zrn_gl_t* const gl) {
	const GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SHADER), fs = compile(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
	GLint ok = 0;
	if (!vs || !fs) return 0;
	gl->program = glCreateProgram();
	glAttachShader(gl->program, vs);
	glAttachShader(gl->program, fs);
	glBindAttribLocation(gl->program, 0, "a_pos");
	glBindAttribLocation(gl->program, 1, "a_local");
	glBindAttribLocation(gl->program, 2, "a_half");
	glBindAttribLocation(gl->program, 3, "a_radii");
	glBindAttribLocation(gl->program, 4, "a_misc");
	glBindAttribLocation(gl->program, 5, "a_color");
	glBindAttribLocation(gl->program, 6, "a_uv");
	glBindAttribLocation(gl->program, 7, "a_w");
	glLinkProgram(gl->program);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glGetProgramiv(gl->program, GL_LINK_STATUS, &ok);
	if (!ok) {
		zrn_error(Z_ERROR_IO, "OpenGL program did not link");
		return 0;
	}
	gl->loc_size = glGetUniformLocation(gl->program, "u_size");
	gl->loc_tex = glGetUniformLocation(gl->program, "u_tex");
	return 1;
}


/* ============================================================================
 *  EGL
 * ============================================================================ */

static constexpr GLsizei MSAA_SAMPLES = 4;

/* A config with 4x MSAA; a driver without one gets a plain config, so that the GPU renderer still works (without anti-aliased edges). */
static bool choose(zrn_gl_t* const gl) {
	const EGLint surface_type = EGL_PBUFFER_BIT;
	const EGLint multisampled[] = {
		EGL_SURFACE_TYPE, surface_type,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, MSAA_SAMPLES, EGL_NONE
	};
	const EGLint plain[] = {
		EGL_SURFACE_TYPE, surface_type,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE
	};
	EGLint n = 0;
	if (eglChooseConfig(gl->display, multisampled, &gl->config, 1, &n) && n > 0) return 1;
	Z_DEBUG_LOG("no 4x MSAA config: the edges are not anti-aliased\n");
	if (eglChooseConfig(gl->display, plain, &gl->config, 1, &n) && n > 0) return 1;
	zrn_error(Z_ERROR_IO, "no usable EGL config");
	return 0;
}

/* The off-screen framebuffer is a texture, which has no samples of its own: where the driver can multisample a texture
 * attachment (GL_EXT_multisampled_render_to_texture, resolved when the frame ends) the framebuffer gets the samples. */
static void find_framebuffer_multisample(zrn_gl_t* const gl) {
	const char* const extensions = (const char*)glGetString(GL_EXTENSIONS);
	gl->fbo_samples = 0;
	gl->fbo_multisample = nullptr;
	if (!extensions || !strstr(extensions, "GL_EXT_multisampled_render_to_texture")) return;
	gl->fbo_multisample = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");
	if (!gl->fbo_multisample) return;
	GLint most = 0;
	glGetIntegerv(GL_MAX_SAMPLES_EXT, &most);
	gl->fbo_samples = most < MSAA_SAMPLES ? most : MSAA_SAMPLES;
}

/* A page of the glyph atlas, empty. */
static bool atlas_page_make(zrn_gl_t* const gl, const int page) {
	if (gl->atlas[page]) return true;
	glGenTextures(1, &gl->atlas[page]);
	glBindTexture(GL_TEXTURE_2D, gl->atlas[page]);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_ALPHA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return gl->atlas[page] != 0;
}

static bool init_gl(zrn_gl_t* const gl) {
	if (!build_program(gl)) return 0;
	gl->verts = (float*)malloc((size_t)MAX_QUADS * 6 * FLOATS * sizeof(float));
	if (!gl->verts) return 0;
	glGenBuffers(1, &gl->vbo);
	return atlas_page_make(gl, 0);
}

/* Roots share the EGLDisplay of a platform: it is terminated when the last of them lets it go, not by the first that is destroyed. */
typedef struct display_use {
	EGLDisplay display;
	int users;
} display_use_t;

static display_use_t g_displays[8];

static void display_acquire(const EGLDisplay display) {
	display_use_t* free_slot = nullptr;
	for (size_t i = 0; i < sizeof g_displays / sizeof g_displays[0]; i++) {
		if (g_displays[i].display == display && g_displays[i].users > 0) {
			g_displays[i].users++;
			return;
		}
		if (g_displays[i].users == 0 && !free_slot) free_slot = &g_displays[i];
	}
	if (!free_slot) return;
	free_slot->display = display;
	free_slot->users = 1;
}

/* Terminates the display when nobody else uses it. A display that was never counted (the table was full) is terminated as before. */
static void display_release(const EGLDisplay display) {
	for (size_t i = 0; i < sizeof g_displays / sizeof g_displays[0]; i++) {
		if (g_displays[i].display != display || g_displays[i].users == 0) continue;
		if (--g_displays[i].users > 0) return;
		break;
	}
	eglTerminate(display);
}

static bool open_display(zrn_gl_t* const gl, const EGLenum platform, void* const native) {
	EGLint major, minor;
	const PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	gl->display = EGL_NO_DISPLAY;
	if (get_platform && platform) gl->display = get_platform(platform, native, nullptr);
	if (gl->display == EGL_NO_DISPLAY && !platform) gl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (gl->display == EGL_NO_DISPLAY || !eglInitialize(gl->display, &major, &minor)) {
		zrn_error(Z_ERROR_IO, "no EGL display");
		return 0;
	}
	display_acquire(gl->display);
	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		zrn_error(Z_ERROR_IO, "no OpenGL ES");
		display_release(gl->display);
		gl->display = EGL_NO_DISPLAY;
		return 0;
	}
	return 1;
}

static bool make_context(zrn_gl_t* const gl) {
	constexpr EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	gl->context = eglCreateContext(gl->display, gl->config, EGL_NO_CONTEXT, ctx_attribs);
	if (gl->context == EGL_NO_CONTEXT) {
		zrn_error(Z_ERROR_IO, "cannot create an OpenGL ES 2 context");
		return 0;
	}
	return 1;
}

static void gl_free(zrn_gl_t* gl);

/* Makes the context of the renderer current: ours through EGL, the one of the platform through its callback. */
static void make_current(zrn_gl_t* const gl) {
	if (!gl->b_external) eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context);
	else if (gl->current) gl->current(gl->current_user);
}

zrn_gl_t* zrn_gl_create_offscreen(const int width, const int height) {
	zrn_gl_t* gl = calloc(1, sizeof *gl);
	const EGLint pb[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
	if (!gl) return nullptr;
	gl->b_offscreen = true;
	if (!open_display(gl, EGL_PLATFORM_SURFACELESS_MESA, nullptr) && !open_display(gl, 0, nullptr)) goto fail;
	if (!choose(gl) || !make_context(gl)) goto fail;
	gl->surface = eglCreatePbufferSurface(gl->display, gl->config, pb);
	if (gl->surface == EGL_NO_SURFACE || !eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context)) {
		zrn_error(Z_ERROR_IO, "cannot make the OpenGL context current");
		goto fail;
	}
	if (!init_gl(gl)) goto fail;
	find_framebuffer_multisample(gl);
	glGenTextures(1, &gl->fbo_texture);
	glGenFramebuffers(1, &gl->fbo);
	zrn_gl_resize(gl, width, height);
	return gl;
fail:
	gl_free(gl);
	return nullptr;
}

zrn_gl_t* zrn_gl_create_external(void (*const current)(void* user), void* const user) {
	zrn_gl_t* const gl = (zrn_gl_t*)calloc(1, sizeof *gl);
	if (!gl) return nullptr;
	gl->b_external = true;
	gl->current = current;
	gl->current_user = user;
	make_current(gl);
	if (!init_gl(gl)) {
		gl_free(gl);
		return nullptr;
	}
	/* the layers (CacheMode) are textures: multisampled where the driver can */
	find_framebuffer_multisample(gl);
	return gl;
}

void zrn_gl_resize(zrn_gl_t* const gl, const int width, const int height) {
	if (!gl) return;
	if (gl->b_offscreen) {
		make_current(gl);
		glBindTexture(GL_TEXTURE_2D, gl->fbo_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, gl->fbo);
		if (gl->fbo_samples > 0) gl->fbo_multisample(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture, 0, gl->fbo_samples);
		else glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture, 0);
	}
	gl->width = width;
	gl->height = height;
}

static void gl_free(zrn_gl_t* const gl) {
	int i;
	if (!gl) return;
	if (gl->b_external || (gl->display != EGL_NO_DISPLAY && gl->context != EGL_NO_CONTEXT)) {
		make_current(gl);
		for (i = 0; i < gl->image_count; i++) {
			const GLuint t = gl->images[i]->gl_texture;
			if (!gl->images[i]->b_atlas) glDeleteTextures(1, &t);
			gl->images[i]->gl_texture = 0;
			gl->images[i]->gl_owner = nullptr;
		}
		if (gl->vbo) glDeleteBuffers(1, &gl->vbo);
		if (gl->latlas) glDeleteTextures(1, &gl->latlas);
		if (gl->latlas_fbo) glDeleteFramebuffers(1, &gl->latlas_fbo);
		if (gl->program) glDeleteProgram(gl->program);
		if (gl->bd_down) glDeleteProgram(gl->bd_down);
		if (gl->bd_frost) glDeleteProgram(gl->bd_frost);
		if (gl->bd_grain) glDeleteTextures(1, &gl->bd_grain);
		if (gl->bd_fbo) glDeleteFramebuffers(1, &gl->bd_fbo);
		for (int k = 0; k < ATLAS_PAGES; k++) if (gl->atlas[k]) glDeleteTextures(1, &gl->atlas[k]);
		if (gl->fbo_texture) glDeleteTextures(1, &gl->fbo_texture);
		if (gl->fbo) glDeleteFramebuffers(1, &gl->fbo);
		for (size_t k = 0; k < sizeof gl->layer_fbos / sizeof gl->layer_fbos[0]; k++) if (gl->layer_fbos[k]) glDeleteFramebuffers(1, &gl->layer_fbos[k]);
		if (!gl->b_external) eglMakeCurrent(gl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	}
	if (gl->display != EGL_NO_DISPLAY) {
		if (gl->surface != EGL_NO_SURFACE && gl->surface) eglDestroySurface(gl->display, gl->surface);
		if (gl->context != EGL_NO_CONTEXT && gl->context) eglDestroyContext(gl->display, gl->context);
		display_release(gl->display);
	}
	free(gl->verts);
	free(gl->slots);
	free(gl->slot_index);
	free(gl->images);
	free(gl);
}

void zrn_gl_destroy(zrn_gl_t* gl) { gl_free(gl); }

void zrn_gl_forget_image(zrn_image_t* const image) {
	const auto gl = (zrn_gl_t*)image->gl_owner;
	if (!gl) return;
	make_current(gl);
	/* a region of the layer atlas: the texture is the atlas's */
	if (!image->b_atlas) {
		const GLuint t = image->gl_texture;
		glDeleteTextures(1, &t);
	}
	for (int i = 0; i < gl->image_count; i++) {
		if (gl->images[i] == image) {
			gl->images[i] = gl->images[--gl->image_count];
			break;
		}
	}
	image->gl_texture = 0;
	image->gl_owner = nullptr;
}


/* ============================================================================
 *  batching
 * ============================================================================ */

/* ZRNUI_VERTEX_CHECK: a line on stderr for each batch with a vertex that is not finite, far off the target or behind the
 * eye (what a GPU draws as triangles across the screen). A debugging aid: off, it costs one branch a batch. */
static void vertex_check(const zrn_gl_t* const gl) {
	static int b_on = -1;
	if (b_on < 0) b_on = getenv("ZRNUI_VERTEX_CHECK") != nullptr;
	if (!b_on) return;
	const int n = gl->quads * 6;
	int bad = 0, first = -1;
	for (int i = 0; i < n; i++) {
		const float* const v = gl->verts + (size_t)i * FLOATS;
		const float x = v[0], y = v[1], w = v[18];
		const float lim = 4.0f * (float)(gl->width > gl->height ? gl->width : gl->height) * (w > 0 ? w : 1);
		if (!isfinite(x) || !isfinite(y) || !isfinite(w) || w < 0.05f || fabsf(x) > lim || fabsf(y) > lim || !isfinite(v[2]) || !isfinite(v[3]) || !isfinite(v[16]) || !isfinite(v[17])) {
			if (first < 0) first = i;
			bad++;
		}
	}
	if (!bad) return;
	const float* const v = gl->verts + (size_t)first * FLOATS;
	fprintf(stderr, "zrnui-badvertex %d of %d in a batch: depth=%d target=%dx%d vertex %d pos=%g,%g w=%g local=%g,%g uv=%g,%g mode=%g xf=%d\n",
		bad, n, gl->layer_depth, gl->width, gl->height, first, (double)v[0], (double)v[1], (double)v[18], (double)v[2], (double)v[3], (double)v[16], (double)v[17], (double)v[11], (int)gl->b_has_xf);
}

static void flush(zrn_gl_t* const gl) {
	static constexpr GLsizei stride = (GLsizei)(FLOATS * sizeof(float));
	if (!gl->quads) return;
	vertex_check(gl);
	/* A clip with no area (an element scrolled or slid past the edge of its clip) draws nothing: the batch is dropped,
	 * not sent to the GPU as a draw with an empty scissor. */
	if (gl->batch_clip_set && (gl->batch_clip[2] <= gl->batch_clip[0] || gl->batch_clip[3] <= gl->batch_clip[1])) {
		static int b_log = -1;
		if (b_log < 0) b_log = getenv("ZRNUI_VERTEX_CHECK") != nullptr;
		if (b_log) fprintf(stderr, "zrnui-emptyclip %d quads dropped: clip %d,%d-%d,%d depth=%d\n", gl->quads, gl->batch_clip[0], gl->batch_clip[1], gl->batch_clip[2], gl->batch_clip[3], gl->layer_depth);
		gl->quads = 0;
		return;
	}
	/* a new store each time (orphaned): the driver need not wait for the draw that reads the last one */
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)gl->quads * 6 * FLOATS * sizeof(float)), gl->verts, GL_STREAM_DRAW);
	glUseProgram(gl->program);
	glUniform2f(gl->loc_size, (float)gl->width, (float)gl->height);
	glUniform1i(gl->loc_tex, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->batch_texture ? gl->batch_texture : gl->atlas[0]);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(0 * sizeof(float)));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(2 * sizeof(float)));
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(4 * sizeof(float)));
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(6 * sizeof(float)));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(10 * sizeof(float)));
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(12 * sizeof(float)));
	glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(16 * sizeof(float)));
	glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)(18 * sizeof(float)));
	{ int i; for (i = 0; i < 8; i++) glEnableVertexAttribArray((GLuint)i); }
	glDrawArrays(GL_TRIANGLES, 0, gl->quads * 6);
	zrn_render_stats.draws++;
	zrn_render_stats.quads += (uint32_t)gl->quads;
	{ int i; for (i = 0; i < 8; i++) glDisableVertexAttribArray((GLuint)i); }
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	gl->quads = 0;
}

/* Gets the batch ready for a draw with this clip and texture: a change of either draws what is waiting. */
static float* reserve(const zrn_painter_t* const p, const GLuint texture) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	/* the shapes without a texture (0) go with any: only two different textures part a batch */
	if (gl->quads >= MAX_QUADS || (gl->quads && texture && gl->batch_texture && gl->batch_texture != texture)) flush(gl);
	if (gl->quads && memcmp(gl->batch_clip, p->clip, sizeof p->clip) != 0) flush(gl);
	if (!gl->batch_clip_set || memcmp(gl->batch_clip, p->clip, sizeof p->clip) != 0) {
		memcpy(gl->batch_clip, p->clip, sizeof p->clip);
		gl->batch_clip_set = 1;
		glEnable(GL_SCISSOR_TEST);
		glScissor(gl->vp_x + p->clip[0], gl->vp_y + gl->height - p->clip[3], p->clip[2] - p->clip[0] > 0 ? p->clip[2] - p->clip[0] : 0, p->clip[3] - p->clip[1] > 0 ? p->clip[3] - p->clip[1] : 0);
	}
	if (texture || !gl->quads) gl->batch_texture = texture;
	return gl->verts + (size_t)gl->quads * 6 * FLOATS;
}

typedef struct corner {
	float x, y, u, v;
} corner_t;

static void put(const zrn_gl_t* const gl, float* const out, const float x, const float y, const float lx, const float ly, const float hx, const float hy, const float radii[4], const float aux, const float mode, const ZColor c, const float alpha, const float u, const float v) {
	if (gl->b_has_xf) {
		/* the column-major matrix of the device position, perspective in its last row */
		const float* const m = gl->xf;
		out[0] = m[0] * x + m[4] * y + m[12];
		out[1] = m[1] * x + m[5] * y + m[13];
		out[18] = m[3] * x + m[7] * y + m[15];
	} else {
		out[0] = x;
		out[1] = y;
		out[18] = 1.0f;
	}
	out[2] = lx; out[3] = ly; out[4] = hx; out[5] = hy;
	out[6] = radii[0]; out[7] = radii[1]; out[8] = radii[2]; out[9] = radii[3];
	out[10] = aux; out[11] = mode;
	out[12] = c.r; out[13] = c.g; out[14] = c.b; out[15] = c.a * alpha;
	out[16] = u; out[17] = v;
}

/* A device rectangle [x0,x1] x [y0,y1] whose local coordinates are centred on (cx, cy). */
static void quad(zrn_painter_t* p, const GLuint texture, const float x0, const float y0, const float x1, const float y1, const float cx, const float cy, const float hx, const float hy,
		const float radii[4], const float aux, const float mode, const ZColor c, const float u0, const float v0, const float u1, const float v1) {
	float* const o = reserve(p, texture);
	const auto gl = (zrn_gl_t*)p->backend_data;
	const float a = p->alpha;
#define V(i, X, Y, U, V_) put(gl, o + (i) * FLOATS, X, Y, (X) - cx, (Y) - cy, hx, hy, radii, aux, mode, c, a, U, V_)
	V(0, x0, y0, u0, v0); V(1, x1, y0, u1, v0); V(2, x1, y1, u1, v1);
	V(3, x0, y0, u0, v0); V(4, x1, y1, u1, v1); V(5, x0, y1, u0, v1);
#undef V
	gl->quads++;
}

static constexpr float NO_RADII[4] = { 0, 0, 0, 0 };

static inline float dx(const zrn_painter_t* p, const float x) { return p->tx + x * p->scale; }
static inline float dy(const zrn_painter_t* p, const float y) { return p->ty + y * p->scale; }


/* ============================================================================
 *  the backend
 * ============================================================================ */

static void gl_paint_clear(zrn_painter_t* const p, const ZColor c) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	flush(gl);
	glDisable(GL_SCISSOR_TEST);
	gl->batch_clip_set = 0;
	glClearColor(c.r, c.g, c.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void gl_fill_rect(zrn_painter_t* const p, const ZRect r, const ZColor c) {
	/* edges on whole device pixels, as the software painter does */
	const float x0 = floorf(dx(p, r.x) + 0.5f), y0 = floorf(dy(p, r.y) + 0.5f);
	const float x1 = floorf(dx(p, r.x + r.width) + 0.5f), y1 = floorf(dy(p, r.y + r.height) + 0.5f);
	if (x1 <= x0 || y1 <= y0) return;
	quad(p, 0, x0, y0, x1, y1, 0, 0, 0, 0, NO_RADII, 0, MODE_SOLID, c, 0, 0, 0, 0);
}

static void gl_fill_rrect(zrn_painter_t* const p, const ZRect r, const ZCornerRadius radius, const ZColor c) {
	float x0, y0, x1, y1, rad[4];
	if (radius.top_left <= 0 && radius.top_right <= 0 && radius.bottom_right <= 0 && radius.bottom_left <= 0) {
		gl_fill_rect(p, r, c);
		return;
	}
	x0 = dx(p, r.x); y0 = dy(p, r.y); x1 = dx(p, r.x + r.width); y1 = dy(p, r.y + r.height);
	rad[0] = radius.top_left * p->scale; rad[1] = radius.top_right * p->scale;
	rad[2] = radius.bottom_right * p->scale; rad[3] = radius.bottom_left * p->scale;
	quad(p, 0, x0 - 1, y0 - 1, x1 + 1, y1 + 1, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (x1 - x0) * 0.5f, (y1 - y0) * 0.5f, rad, 0, MODE_RRECT, c, 0, 0, 0, 0);
}

static void ellipse_quad(zrn_painter_t* const p, const ZRect r, const float stroke, const ZColor c) {
	const float x0 = dx(p, r.x), y0 = dy(p, r.y), x1 = dx(p, r.x + r.width), y1 = dy(p, r.y + r.height);
	const float rx = zrn_maxf((x1 - x0) * 0.5f, 0.01f), ry = zrn_maxf((y1 - y0) * 0.5f, 0.01f);
	quad(p, 0, x0 - 1, y0 - 1, x1 + 1, y1 + 1, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, rx, ry, NO_RADII, stroke, stroke > 0 ? MODE_RING : MODE_ELLIPSE, c, 0, 0, 0, 0);
}

static void gl_fill_ellipse(zrn_painter_t* p, const ZRect r, const ZColor c) { ellipse_quad(p, r, 0, c); }
static void gl_stroke_ellipse(zrn_painter_t* p, const ZRect r, const float thickness, const ZColor c) { ellipse_quad(p, r, thickness * p->scale, c); }

/* A polygon by ear clipping, in device pixels: the fill of a simple polygon, convex or not. */
static void gl_fill_polygon(zrn_painter_t* const p, const float* const xy, const int count, const ZColor c) {
	float pts[64][2];
	int idx[64], n = count, i, guard = 0;
	float area = 0;
	if (count < 3 || count > 64) return;
	for (i = 0; i < count; i++) {
		pts[i][0] = dx(p, xy[i * 2]);
		pts[i][1] = dy(p, xy[i * 2 + 1]);
		idx[i] = i;
	}
	for (i = 0; i < count; i++) {
		int j = (i + 1) % count;
		area += pts[i][0] * pts[j][1] - pts[j][0] * pts[i][1];
	}
	while (n > 3 && guard++ < 256) {
		int ear = -1;
		for (i = 0; i < n && ear < 0; i++) {
			int a = idx[(i + n - 1) % n], b = idx[i], cc = idx[(i + 1) % n], k;
			bool b_inside = false;
			float cross = (pts[b][0] - pts[a][0]) * (pts[cc][1] - pts[a][1]) - (pts[b][1] - pts[a][1]) * (pts[cc][0] - pts[a][0]);
			if ((cross > 0) != (area > 0) || cross == 0) continue;
			for (k = 0; k < n && !b_inside; k++) {
				int t = idx[k];
				float d0, d1, d2;
				if (t == a || t == b || t == cc) continue;
				d0 = (pts[b][0] - pts[a][0]) * (pts[t][1] - pts[a][1]) - (pts[b][1] - pts[a][1]) * (pts[t][0] - pts[a][0]);
				d1 = (pts[cc][0] - pts[b][0]) * (pts[t][1] - pts[b][1]) - (pts[cc][1] - pts[b][1]) * (pts[t][0] - pts[b][0]);
				d2 = (pts[a][0] - pts[cc][0]) * (pts[t][1] - pts[cc][1]) - (pts[a][1] - pts[cc][1]) * (pts[t][0] - pts[cc][0]);
				if ((d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0)) b_inside = true;
			}
			if (!b_inside) ear = i;
		}
		if (ear < 0) ear = 0;
		{
			int a = idx[(ear + n - 1) % n], b = idx[ear], cc = idx[(ear + 1) % n];
			float* const o = reserve(p, 0);
			const auto gl = (zrn_gl_t*)p->backend_data;
			int v;
			const int vs[3] = { a, b, cc };
			for (v = 0; v < 3; v++) put(gl, o + v * FLOATS, pts[vs[v]][0], pts[vs[v]][1], 0, 0, 0, 0, NO_RADII, 0, MODE_SOLID, c, p->alpha, 0, 0);
			/* the quad of the batch has six vertices: a degenerate second triangle fills the rest */
			for (v = 3; v < 6; v++) put(gl, o + v * FLOATS, pts[a][0], pts[a][1], 0, 0, 0, 0, NO_RADII, 0, MODE_SOLID, c, p->alpha, 0, 0);
			gl->quads++;
		}
		for (i = ear; i < n - 1; i++) idx[i] = idx[i + 1];
		n--;
	}
	if (n == 3) {
		float* const o = reserve(p, 0);
		const auto gl = (zrn_gl_t*)p->backend_data;
		for (int v = 0; v < 6; v++) put(gl, o + v * FLOATS, pts[idx[v % 3]][0], pts[idx[v % 3]][1], 0, 0, 0, 0, NO_RADII, 0, MODE_SOLID, c, p->alpha, 0, 0);
		gl->quads++;
	}
}

static void gl_draw_line(zrn_painter_t* const p, const float x0, const float y0, const float x1, const float y1, const float thickness, const ZColor c) {
	float ax = dx(p, x0), ay = dy(p, y0), bx = dx(p, x1), by = dy(p, y1);
	float lx = bx - ax, ly = by - ay, len = sqrtf(lx * lx + ly * ly), h = thickness * p->scale * 0.5f, ux, uy, nx, ny, mx, my, e;
	const auto gl = (zrn_gl_t*)p->backend_data;
	float corners[4][2];
	const int order[6] = { 0, 1, 2, 0, 2, 3 };
	if (len <= 0.0001f) return;
	ux = lx / len; uy = ly / len; nx = -uy; ny = ux;
	mx = (ax + bx) * 0.5f; my = (ay + by) * 0.5f;
	e = 1.0f; /* room for the soft edge */
	corners[0][0] = ax - ux * e + nx * (h + e); corners[0][1] = ay - uy * e + ny * (h + e);
	corners[1][0] = bx + ux * e + nx * (h + e); corners[1][1] = by + uy * e + ny * (h + e);
	corners[2][0] = bx + ux * e - nx * (h + e); corners[2][1] = by + uy * e - ny * (h + e);
	corners[3][0] = ax - ux * e - nx * (h + e); corners[3][1] = ay - uy * e - ny * (h + e);
	float* o = reserve(p, 0);
	for (int i = 0; i < 6; i++) {
		float x = corners[order[i]][0], y = corners[order[i]][1];
		put(gl, o + i * FLOATS, x, y, (x - mx) * ux + (y - my) * uy, (x - mx) * nx + (y - my) * ny, len * 0.5f, h, NO_RADII, 0, MODE_RRECT, c, p->alpha, 0, 0);
	}
	gl->quads++;
}

static size_t atlas_hash(const uint8_t* const key) {
	uint64_t h = (uint64_t)(uintptr_t)key;
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccdull;
	h ^= h >> 33;
	return (size_t)h & (ATLAS_INDEX - 1);
}

/* The atlas starts again empty: its slots and their index. */
static void atlas_clear(zrn_gl_t* const gl) {
	gl->slot_count = 0;
	gl->shelf_x = gl->shelf_y = gl->shelf_h = 0;
	gl->atlas_page = 0;
	if (gl->slot_index) memset(gl->slot_index, 0, ATLAS_INDEX * sizeof *gl->slot_index);
}

/* The atlas slot of a glyph bitmap, uploading it the first time. Found through the index: a linear search over
 * thousands of slots for every glyph of every frame was the cost of a frame full of text. */
static atlas_slot_t* atlas_find(zrn_gl_t* const gl, const uint8_t* const alpha, const int w, const int h) {
	atlas_slot_t* s;
	if (!gl->slot_index) {
		gl->slot_index = (uint32_t*)calloc(ATLAS_INDEX, sizeof *gl->slot_index);
		if (!gl->slot_index) return nullptr;
	}
	size_t at = atlas_hash(alpha);
	for (;;) {
		const uint32_t v = gl->slot_index[at];
		if (!v) break;
		s = &gl->slots[v - 1];
		if (s->key == alpha && s->w == w && s->h == h) return s;
		at = (at + 1) & (ATLAS_INDEX - 1);
	}
	/* the index stays at most half full: past that the atlas starts again, as when it is out of room */
	if (gl->slot_count >= ATLAS_INDEX / 2) return nullptr;
	if (gl->shelf_x + w + 1 > ATLAS_SIZE) {
		gl->shelf_x = 0;
		gl->shelf_y += gl->shelf_h + 1;
		gl->shelf_h = 0;
	}
	if (w + 1 > ATLAS_SIZE || h + 1 > ATLAS_SIZE) return nullptr;
	if (gl->shelf_y + h + 1 > ATLAS_SIZE) {
		/* the page is full: the next one */
		if (gl->atlas_page + 1 >= ATLAS_PAGES || !atlas_page_make(gl, gl->atlas_page + 1)) return nullptr;
		gl->atlas_page++;
		gl->shelf_x = gl->shelf_y = gl->shelf_h = 0;
	}
	if (gl->slot_count == gl->slot_cap) {
		const size_t cap = gl->slot_cap ? gl->slot_cap * 2 : 512;
		atlas_slot_t* slots = (atlas_slot_t*)realloc(gl->slots, cap * sizeof *slots);
		if (!slots) return nullptr;
		gl->slots = slots;
		gl->slot_cap = cap;
	}
	s = &gl->slots[gl->slot_count++];
	gl->slot_index[at] = (uint32_t)gl->slot_count;
	s->key = alpha; s->w = w; s->h = h; s->x = gl->shelf_x; s->y = gl->shelf_y; s->page = gl->atlas_page;
	gl->shelf_x += w + 1;
	if (h > gl->shelf_h) gl->shelf_h = h;
	glBindTexture(GL_TEXTURE_2D, gl->atlas[s->page]);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, s->x, s->y, w, h, GL_ALPHA, GL_UNSIGNED_BYTE, alpha);
	zrn_render_stats.glyph_uploads++;
	zrn_render_stats.upload_kb += (uint32_t)(w * h) / 1024u;
	return s;
}

static void gl_blit_alpha(zrn_painter_t* const p, const int x, const int y, const int w, const int h, const uint8_t* const alpha, const int stride, const ZColor c) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	constexpr float inv = 1.0f / (float)ATLAS_SIZE;
	if (w <= 0 || h <= 0 || stride != w) return;
	/* a glyph bigger than a page has no place: starting the atlas again would not make one */
	if (w + 1 > ATLAS_SIZE || h + 1 > ATLAS_SIZE) return;
	atlas_slot_t* s = atlas_find(gl, alpha, w, h);
	if (!s) {
		/* the atlas is full: start it again */
		flush(gl);
		atlas_clear(gl);
		s = atlas_find(gl, alpha, w, h);
		if (!s) return;
	}
	quad(p, gl->atlas[s->page], (float)x, (float)y, (float)(x + w), (float)(y + h), 0, 0, 0, 0, NO_RADII, 0, MODE_GLYPH, c,
		(float)s->x * inv, (float)s->y * inv, (float)(s->x + w) * inv, (float)(s->y + h) * inv);
}

/* Makes room in the list of the images that have a texture of this renderer. */
static bool images_room(zrn_gl_t* const gl) {
	if (gl->image_count < gl->image_cap) return true;
	const int cap = gl->image_cap ? gl->image_cap * 2 : 16;
	zrn_image_t** images = realloc(gl->images, (size_t)cap * sizeof *images);
	if (!images) return false;
	gl->images = images;
	gl->image_cap = cap;
	return true;
}

/* Kilobytes of pictures uploaded in this frame (host.c resets it): past the budget a big one waits for the next frame,
 * so a chat scrolled to three new photos uploads them over three frames instead of stalling one (4-8 MB, 50-190 ms). */
extern uint32_t zrn_upload_kb_this_frame;
extern bool zrn_upload_deferred;
static constexpr uint32_t UPLOAD_BIG_KB = 512;
static constexpr uint32_t UPLOAD_FRAME_KB = 1536;

static GLuint image_texture(zrn_gl_t* const gl, const zrn_image_t* const image) {
	const auto im = (zrn_image_t*)image;
	GLuint t;
	if (im->gl_owner == gl) return im->gl_texture;
	if (!im->rgba || !images_room(gl)) return 0;
	const uint32_t kb = (uint32_t)(im->width * im->height * 4) / 1024u;
	if (kb >= UPLOAD_BIG_KB && zrn_upload_kb_this_frame > 0 && zrn_upload_kb_this_frame + kb > UPLOAD_FRAME_KB) {
		zrn_upload_deferred = true;
		return 0;
	}
	zrn_upload_kb_this_frame += kb;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->width, im->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, im->rgba);
	zrn_render_stats.image_uploads++;
	zrn_render_stats.upload_kb += (uint32_t)(im->width * im->height * 4) / 1024u;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	im->gl_texture = t;
	im->gl_owner = gl;
	gl->images[gl->image_count++] = im;
	return t;
}

static void gl_draw_image(zrn_painter_t* const p, const zrn_image_t* const image, const ZRect dest, const ZRect source) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	const float x0 = dx(p, dest.x), y0 = dy(p, dest.y), x1 = dx(p, dest.x + dest.width), y1 = dy(p, dest.y + dest.height);
	const ZColor white = { .r = 1, .g = 1, .b = 1, .a = 1 };
	if (!image || x1 <= x0 || y1 <= y0) return;
	flush(gl);
	GLuint t = image_texture(gl, image);
	if (!t) return;
	quad(p, t, x0, y0, x1, y1, 0, 0, 0, 0, NO_RADII, 0, MODE_IMAGE, white,
		source.x / (float)image->width, source.y / (float)image->height,
		(source.x + source.width) / (float)image->width, (source.y + source.height) / (float)image->height);
	flush(gl);
}

/* An affine transform of device pixels (z toward the viewer), seen from an eye a 34 degree field of view away: the matrix of the shader has the perspective in it. */
static void gl_set_transform(zrn_painter_t* const p, const float* const m) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	/* inside a layer gl->height is the layer's: a row's few hundred pixels would put the eye inside the rows that turn
	 * (corners behind it, triangles across the screen), so a layer sees from where the surface does */
	if (gl->layer_depth == 0 || gl->eye <= 0) gl->eye = ((float)gl->height * 0.5f) / tanf(17.0f * ZRN_PI / 180.0f);
	float persp[16];
	const float eye = gl->eye;
	/* no flush: the vertices are transformed as they are put in the batch (put), so rows that each turn their own way
	 * still go to the GPU in one draw */
	if (!m) {
		zrn_xf_identity(gl->xf);
		gl->b_has_xf = false;
		return;
	}
	zrn_xf_identity(persp);
	persp[11] = -1.0f / eye;
	zrn_xf_mul(gl->xf, persp, m);
	gl->b_has_xf = true;
}

/* ---- layers (CacheMode) ---- */

/* What layer_begin saves and layer_end puts back. */
typedef struct layer_save {
	zrn_painter_t painter;
	GLint framebuffer;
	GLuint fbo;
	int width, height;
	float xf[16];
	bool b_has_xf;
	int vp_x, vp_y;
} layer_save_t;

static_assert(sizeof(layer_save_t) <= ZRN_LAYER_SAVE, "ZRN_LAYER_SAVE is too small for the state of a layer");

/* The blending of the frame keeps the alpha of the target; that of a layer adds up its coverage, so that its color comes out premultiplied. */
static void blend_for(const zrn_gl_t* const gl) {
	if (gl->layer_depth > 0) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	else glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
}

/* A texture of the size for a layer, owned by a pixel-less image of this renderer; NULL when there is no memory. */
static zrn_image_t* layer_make(zrn_gl_t* const gl, const int width, const int height) {
	if (!images_room(gl)) return nullptr;
	zrn_image_t* const im = (zrn_image_t*)calloc(1, sizeof *im);
	if (!im) return nullptr;
	GLuint t;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	im->width = width;
	im->height = height;
	im->gl_texture = t;
	im->gl_owner = gl;
	gl->images[gl->image_count++] = im;
	return im;
}

static constexpr int LATLAS_SIZE = 2048;
/* a cache bigger than this a side gets a texture of its own */
static constexpr int LATLAS_MAX = 1024;

/* The layer atlas, made the first time: cleared transparent once, so that the gaps between regions read as nothing. */
static bool latlas_make(zrn_gl_t* const gl) {
	if (gl->latlas) return true;
	glGenTextures(1, &gl->latlas);
	glBindTexture(GL_TEXTURE_2D, gl->latlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LATLAS_SIZE, LATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenFramebuffers(1, &gl->latlas_fbo);
	GLint before = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &before);
	glBindFramebuffer(GL_FRAMEBUFFER, gl->latlas_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->latlas, 0);
	const bool b_ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	if (b_ok) {
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)before);
	gl->batch_clip_set = 0;
	if (!b_ok) {
		glDeleteFramebuffers(1, &gl->latlas_fbo);
		glDeleteTextures(1, &gl->latlas);
		gl->latlas = gl->latlas_fbo = 0;
	}
	return b_ok;
}

/* The atlas is full: every region in it is let go (the caches that had one are painted again, each in a new region) and
 * the packing starts over. What was drawn from it this frame is flushed first, so that it reads the old pixels. */
static void latlas_reset(zrn_gl_t* const gl) {
	flush(gl);
	for (int i = 0; i < gl->image_count;) {
		zrn_image_t* const im = gl->images[i];
		if (!im->b_atlas) { i++; continue; }
		im->gl_owner = nullptr;
		im->gl_texture = 0;
		gl->images[i] = gl->images[--gl->image_count];
	}
	gl->latlas_x = gl->latlas_y = gl->latlas_h = 0;
}

/* A region of the atlas for a layer, shelf packed with a pixel between regions; NULL when it does not fit (too big, or the
 * atlas cannot be made). */
static zrn_image_t* latlas_layer(zrn_gl_t* const gl, const int width, const int height) {
	if (width > LATLAS_MAX || height > LATLAS_MAX || !latlas_make(gl) || !images_room(gl)) return nullptr;
	if (gl->latlas_x + width + 1 > LATLAS_SIZE) {
		gl->latlas_x = 0;
		gl->latlas_y += gl->latlas_h + 1;
		gl->latlas_h = 0;
	}
	if (gl->latlas_y + height + 1 > LATLAS_SIZE) {
		latlas_reset(gl);
		if (!images_room(gl)) return nullptr;
	}
	zrn_image_t* const im = (zrn_image_t*)calloc(1, sizeof *im);
	if (!im) return nullptr;
	im->width = width;
	im->height = height;
	im->gl_texture = gl->latlas;
	im->gl_owner = gl;
	im->b_atlas = true;
	im->atlas_x = gl->latlas_x;
	im->atlas_y = gl->latlas_y;
	gl->latlas_x += width + 1;
	if (height > gl->latlas_h) gl->latlas_h = height;
	gl->images[gl->image_count++] = im;
	return im;
}

static bool gl_layer_begin(zrn_painter_t* const p, zrn_image_t** const layer, const int width, const int height, void* const saved_state) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	const auto saved = (layer_save_t*)saved_state;
	flush(gl);
	zrn_image_t* im = *layer;
	/* a region of the atlas is painted only outside other layers: inside one, the atlas is the target (or may be read) */
	if (im && (im->gl_owner != gl || im->width != width || im->height != height || im->rgba || (im->b_atlas && gl->layer_depth > 0))) {
		zrn_image_free(im);
		im = nullptr;
		*layer = nullptr;
	}
	if (!im) {
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
		if (gl->layer_depth == 0) im = latlas_layer(gl, width, height);
		if (!im) im = layer_make(gl, width, height);
		clock_gettime(CLOCK_MONOTONIC, &t1);
		zrn_render_stats.alloc_ms += (float)((double)(t1.tv_sec - t0.tv_sec) * 1e3 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e6);
	}
	if (!im) return false;
	*layer = im;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved->framebuffer);
	const int depth = gl->layer_depth;
	saved->fbo = 0;
	if (im->b_atlas) {
		glBindFramebuffer(GL_FRAMEBUFFER, gl->latlas_fbo);
	} else if (depth < (int)(sizeof gl->layer_fbos / sizeof gl->layer_fbos[0])) {
		if (!gl->layer_fbos[depth]) glGenFramebuffers(1, &gl->layer_fbos[depth]);
		glBindFramebuffer(GL_FRAMEBUFFER, gl->layer_fbos[depth]);
	} else {
		/* deeper than the kept ones: one of its own, deleted at the end */
		glGenFramebuffers(1, &saved->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, saved->fbo);
	}
	/* a layer is painted without multisampling: the shapes are anti-aliased by their distance functions and the text by
	 * its coverage, and a multisampled layer costs a resolve each time it is painted (sixty rows of a chat coming in) */
	if (!im->b_atlas) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, im->gl_texture, 0);
	if (!im->b_atlas && glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)saved->framebuffer);
		if (saved->fbo) glDeleteFramebuffers(1, &saved->fbo);
		return false;
	}
	saved->painter = *p;
	saved->width = gl->width;
	saved->height = gl->height;
	memcpy(saved->xf, gl->xf, sizeof saved->xf);
	saved->b_has_xf = gl->b_has_xf;
	saved->vp_x = gl->vp_x;
	saved->vp_y = gl->vp_y;
	gl->vp_x = im->b_atlas ? im->atlas_x : 0;
	gl->vp_y = im->b_atlas ? im->atlas_y : 0;
	gl->width = width;
	gl->height = height;
	zrn_xf_identity(gl->xf);
	gl->b_has_xf = false;
	gl->batch_clip_set = 0;
	gl->batch_texture = 0;
	gl->layer_depth++;
	glViewport(gl->vp_x, gl->vp_y, width, height);
	/* only the region: the rest of the atlas holds the other layers */
	glEnable(GL_SCISSOR_TEST);
	glScissor(gl->vp_x, gl->vp_y, width, height);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	gl->batch_clip_set = 0;
	blend_for(gl);
	p->tx = 0;
	p->ty = 0;
	p->alpha = 1.0f;
	p->width = width;
	p->height = height;
	p->clip[0] = 0;
	p->clip[1] = 0;
	p->clip[2] = width;
	p->clip[3] = height;
	p->clip_depth = 0;
	p->clip_skipped = 0;
	zrn_xf_identity(p->xf);
	p->b_has_xf = false;
	return true;
}

static void gl_layer_end(zrn_painter_t* const p, const void* const saved_state) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	const auto saved = (const layer_save_t*)saved_state;
	flush(gl);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)saved->framebuffer);
	if (saved->fbo) {
		const GLuint fbo = saved->fbo;
		glDeleteFramebuffers(1, &fbo);
	}
	gl->layer_depth--;
	gl->width = saved->width;
	gl->height = saved->height;
	memcpy(gl->xf, saved->xf, sizeof gl->xf);
	gl->b_has_xf = saved->b_has_xf;
	gl->vp_x = saved->vp_x;
	gl->vp_y = saved->vp_y;
	gl->batch_clip_set = 0;
	gl->batch_texture = 0;
	glViewport(gl->vp_x, gl->vp_y, gl->width, gl->height);
	blend_for(gl);
	*p = saved->painter;
}

static void gl_draw_layer(zrn_painter_t* const p, const zrn_image_t* const layer) {
	const auto gl = (const zrn_gl_t*)p->backend_data;
	const ZColor white = { .r = 1, .g = 1, .b = 1, .a = 1 };
	float x0 = p->tx, y0 = p->ty;
	if (!layer || !layer->gl_texture) return;
	zrn_render_stats.layer_draws++;
	/* untransformed, on whole device pixels: the texture is copied, not resampled */
	if (!p->b_has_xf) {
		x0 = floorf(x0 + 0.5f);
		y0 = floorf(y0 + 0.5f);
	}
	/* in a strip under a frosted element, at its quarter resolution */
	const float k = gl->bd_layer_scale > 0 ? gl->bd_layer_scale : 1.0f;
	/* the first row of the texture is the bottom of the layer */
	if (layer->b_atlas) {
		constexpr float inv = 1.0f / (float)LATLAS_SIZE;
		quad(p, layer->gl_texture, x0, y0, x0 + (float)layer->width * k, y0 + (float)layer->height * k, 0, 0, 0, 0, NO_RADII, 0, MODE_LAYER, white,
			(float)layer->atlas_x * inv, (float)(layer->atlas_y + layer->height) * inv, (float)(layer->atlas_x + layer->width) * inv, (float)layer->atlas_y * inv);
		return;
	}
	quad(p, layer->gl_texture, x0, y0, x0 + (float)layer->width * k, y0 + (float)layer->height * k, 0, 0, 0, 0, NO_RADII, 0, MODE_LAYER, white, 0, 1, 1, 0);
}

/* A region of the layer atlas cannot be drawn while a layer is being painted: the target may be the atlas itself. */
static bool gl_layer_drawable(const zrn_painter_t* const p, const zrn_image_t* const layer) {
	const auto gl = (const zrn_gl_t*)p->backend_data;
	return !layer->b_atlas || gl->layer_depth == 0;
}

/* ---- zrn-frosted-diffusion (the ztMETRO shell's bars: frost_init, frost_prepare, bar_layer) ---- */

/* a quad over the whole target; uv from the source's corner */
static const char* BD_PASS_VERTEX =
	"attribute vec2 a_pos;\n"
	"varying vec2 v_uv;\n"
	"void main() { v_uv = a_pos; gl_Position = vec4(a_pos * 2.0 - 1.0, 0.0, 1.0); }\n";
/* the strip (painted at a quarter of the frame's resolution) blurred in one pass: the 9-tap Gaussian both ways in five
 * filtered taps each, 25 taps over a few thousand pixels; one small pass, not three (each a render pass the tiler would flush) */
static const char* BD_DOWN_FRAGMENT =
	"precision mediump float;\n"
	"varying vec2 v_uv;\n"
	"uniform sampler2D u_tex;\n"
	"uniform vec2 u_step;\n"
	"float off(int i) { return i == 0 ? -3.2307692 : i == 1 ? -1.3846154 : i == 2 ? 0.0 : i == 3 ? 1.3846154 : 3.2307692; }\n"
	"float wt(int i) { return i == 2 ? 0.2270270 : (i == 1 || i == 3) ? 0.3162162 : 0.0702703; }\n"
	"void main() {\n"
	"  vec4 c = vec4(0.0);\n"
	"  for (int i = 0; i < 5; i++)\n"
	"    for (int j = 0; j < 5; j++) c += wt(i) * wt(j) * texture2D(u_tex, v_uv + vec2(off(i) * u_step.x, off(j) * u_step.y));\n"
	"  gl_FragColor = c;\n"
	"}\n";
/* the element: positions come transformed as the batch's do; v_f is where in the element (0 to 1 each way, y down) */
static const char* BD_FROST_VERTEX =
	"attribute vec3 a_pos;\n"
	"attribute vec2 a_f;\n"
	"uniform vec2 u_size;\n"
	"varying vec2 v_f;\n"
	"void main() { v_f = a_f; gl_Position = vec4(a_pos.x / u_size.x * 2.0 - 1.0, 1.0 - a_pos.y / u_size.y * 2.0, 0.0, a_pos.z); }\n";
/* frosted glass: the blur under a veil of the tint, with grain only where the blur is not the page's own background (with
 * nothing under it, it is that background under the tint); even (NORMAL) or fading out toward one edge */
static const char* BD_FROST_FRAGMENT =
	"precision mediump float;\n"
	"#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
	"#define HP highp\n"
	"#else\n"
	"#define HP mediump\n"
	"#endif\n"
	"varying vec2 v_f;\n"
	"uniform sampler2D u_tex;\n"
	"uniform sampler2D u_grain;\n"
	"uniform vec3 u_tint;\n"
	"uniform vec3 u_under;\n"
	"uniform vec4 u_uv;\n"
	"uniform float u_veil, u_grain_amt, u_grain_px, u_alpha, u_use_tex, u_mode;\n"
	"void main() {\n"
	/* NORMAL even; ASCENDING full at the top (t 0) and none at the bottom, DESCENDING the other way, smoothstep from a tenth in */
	"  float t = u_mode < 1.5 ? 0.0 : (u_mode < 2.5 ? v_f.y : 1.0 - v_f.y);\n"
	"  float r = 1.0 - smoothstep(0.1, 1.0, t);\n"
	"  vec3 b = u_use_tex > 0.5 ? texture2D(u_tex, vec2(u_uv.x + v_f.x * u_uv.z, u_uv.y - v_f.y * u_uv.w)).rgb : u_under;\n"
	"  vec3 c = mix(b, u_tint, u_veil);\n"
	"  vec3 d = abs(b - u_under);\n"
	"  HP vec2 g = gl_FragCoord.xy / (64.0 * u_grain_px);\n"
	"  float n = texture2D(u_grain, g).r - 0.5;\n"
	"  c += n * u_grain_amt * min(1.0, (d.r + d.g + d.b) * 2.0);\n"
	"  gl_FragColor = vec4(clamp(c, 0.0, 1.0), r * u_alpha);\n"
	"}\n";
static constexpr float BD_SPREAD = 1.6f;
static constexpr float BD_GRAIN = 0.10f;

static GLuint bd_link(const char* const vertex, const char* const fragment, const char* const a0, const char* const a1) {
	const GLuint vs = compile(GL_VERTEX_SHADER, vertex), fs = compile(GL_FRAGMENT_SHADER, fragment);
	if (!vs || !fs) return 0;
	const GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, a0);
	if (a1) glBindAttribLocation(prog, 1, a1);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}

static bool bd_ready(zrn_gl_t* const gl) {
	if (gl->bd_failed) return false;
	if (gl->bd_down && gl->bd_frost && gl->bd_grain && gl->bd_fbo) return true;
	gl->bd_down = bd_link(BD_PASS_VERTEX, BD_DOWN_FRAGMENT, "a_pos", nullptr);
	gl->bd_frost = bd_link(BD_FROST_VERTEX, BD_FROST_FRAGMENT, "a_pos", "a_f");
	if (!gl->bd_down || !gl->bd_frost) {
		zrn_error(Z_ERROR_IO, "zrn-frosted-diffusion: a shader did not build; backdrops are filled with their tint");
		gl->bd_failed = true;
		return false;
	}
	/* the grain: fixed noise, the same every start, tiled one texel to a device pixel's worth of points */
	static uint8_t noise[64 * 64];
	uint32_t x = 0x9e3779b9u;
	for (int i = 0; i < 64 * 64; i++) {
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		noise[i] = (uint8_t)(x >> 24);
	}
	glGenTextures(1, &gl->bd_grain);
	glBindTexture(GL_TEXTURE_2D, gl->bd_grain);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 64, 64, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, noise);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glGenFramebuffers(1, &gl->bd_fbo);
	return true;
}

/* a strip or a blur of the given size, kept while it stays that size */
static bool bd_texture(zrn_gl_t* const gl, zrn_image_t** const im, const int width, const int height) {
	zrn_image_t* have = *im;
	if (have && (have->gl_owner != gl || have->width != width || have->height != height || have->b_atlas)) {
		zrn_image_free(have);
		have = nullptr;
		*im = nullptr;
	}
	if (!have) have = layer_make(gl, width, height);
	*im = have;
	return have != nullptr;
}

static bool gl_backdrop_begin(zrn_painter_t* const p, zrn_image_t** const strip, const int width, const int height, const ZColor under, void* const saved_state) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	const auto saved = (layer_save_t*)saved_state;
	if (gl->layer_depth != 0 || width <= 0 || height <= 0 || !bd_ready(gl)) return false;
	flush(gl);
	if (!bd_texture(gl, strip, width, height)) return false;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved->framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gl->bd_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (*strip)->gl_texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)saved->framebuffer);
		return false;
	}
	saved->fbo = 0;
	saved->painter = *p;
	saved->width = gl->width;
	saved->height = gl->height;
	memcpy(saved->xf, gl->xf, sizeof saved->xf);
	saved->b_has_xf = gl->b_has_xf;
	saved->vp_x = gl->vp_x;
	saved->vp_y = gl->vp_y;
	/* not a layer: layer_depth stays 0, so the bitmap caches (the atlas among them) are drawn as in the frame, at a quarter */
	gl->bd_layer_scale = 0.25f;
	gl->vp_x = gl->vp_y = 0;
	gl->width = width;
	gl->height = height;
	zrn_xf_identity(gl->xf);
	gl->b_has_xf = false;
	gl->batch_clip_set = 0;
	gl->batch_texture = 0;
	glViewport(0, 0, width, height);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(under.r, under.g, under.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	blend_for(gl);
	p->tx = 0;
	p->ty = 0;
	p->alpha = 1.0f;
	p->width = width;
	p->height = height;
	p->clip[0] = 0;
	p->clip[1] = 0;
	p->clip[2] = width;
	p->clip[3] = height;
	p->clip_depth = 0;
	p->clip_skipped = 0;
	zrn_xf_identity(p->xf);
	p->b_has_xf = false;
	return true;
}

static void gl_backdrop_end(zrn_painter_t* const p, zrn_image_t* const strip, zrn_image_t** const blur, const void* const saved_state) {
	static const GLfloat corners[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
	const auto gl = (zrn_gl_t*)p->backend_data;
	const auto saved = (const layer_save_t*)saved_state;
	flush(gl);
	const int w = strip ? strip->width : 0, h = strip ? strip->height : 0;
	gl->bd_layer_scale = 0;
	/* the strip is painted at a quarter already: the blur is the same size */
	if (strip && bd_texture(gl, blur, w, h)) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (*blur)->gl_texture, 0);
		glViewport(0, 0, (*blur)->width, (*blur)->height);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_BLEND);
		glUseProgram(gl->bd_down);
		glUniform1i(glGetUniformLocation(gl->bd_down, "u_tex"), 0);
		glUniform2f(glGetUniformLocation(gl->bd_down, "u_step"), BD_SPREAD / (float)w, BD_SPREAD / (float)h);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, strip->gl_texture);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, corners);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glDisableVertexAttribArray(0);
		zrn_render_stats.draws++;
		glEnable(GL_BLEND);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)saved->framebuffer);
	gl->width = saved->width;
	gl->height = saved->height;
	memcpy(gl->xf, saved->xf, sizeof gl->xf);
	gl->b_has_xf = saved->b_has_xf;
	gl->vp_x = saved->vp_x;
	gl->vp_y = saved->vp_y;
	gl->batch_clip_set = 0;
	gl->batch_texture = 0;
	glViewport(gl->vp_x, gl->vp_y, gl->width, gl->height);
	blend_for(gl);
	*p = saved->painter;
}

static void gl_backdrop_draw(zrn_painter_t* const p, const zrn_image_t* const blur, const float uv[4], const ZRect r, const int mode, const ZColor tint, const ZColor under, const float veil) {
	const auto gl = (zrn_gl_t*)p->backend_data;
	if (!bd_ready(gl) || p->clip[2] <= p->clip[0] || p->clip[3] <= p->clip[1]) {
		gl_fill_rect(p, r, tint);
		return;
	}
	flush(gl);
	const float x0 = dx(p, r.x), y0 = dy(p, r.y), x1 = dx(p, r.x + r.width), y1 = dy(p, r.y + r.height);
	const float xs[4] = { x0, x1, x0, x1 }, ys[4] = { y0, y0, y1, y1 }, fx[4] = { 0, 1, 0, 1 }, fy[4] = { 0, 0, 1, 1 };
	GLfloat v[4 * 5];
	for (int i = 0; i < 4; i++) {
		float* const o = v + i * 5;
		if (gl->b_has_xf) {
			const float* const m = gl->xf;
			o[0] = m[0] * xs[i] + m[4] * ys[i] + m[12];
			o[1] = m[1] * xs[i] + m[5] * ys[i] + m[13];
			o[2] = m[3] * xs[i] + m[7] * ys[i] + m[15];
		} else {
			o[0] = xs[i];
			o[1] = ys[i];
			o[2] = 1.0f;
		}
		o[3] = fx[i];
		o[4] = fy[i];
	}
	const GLuint prog = gl->bd_frost;
	const bool b_tex = blur && blur->gl_texture && blur->gl_owner == gl;
	glEnable(GL_SCISSOR_TEST);
	glScissor(gl->vp_x + p->clip[0], gl->vp_y + gl->height - p->clip[3], p->clip[2] - p->clip[0], p->clip[3] - p->clip[1]);
	gl->batch_clip_set = 0;
	glUseProgram(prog);
	glUniform2f(glGetUniformLocation(prog, "u_size"), (float)gl->width, (float)gl->height);
	glUniform3f(glGetUniformLocation(prog, "u_tint"), tint.r, tint.g, tint.b);
	glUniform3f(glGetUniformLocation(prog, "u_under"), under.r, under.g, under.b);
	glUniform4f(glGetUniformLocation(prog, "u_uv"), uv[0], uv[1], uv[2], uv[3]);
	glUniform1f(glGetUniformLocation(prog, "u_veil"), veil);
	glUniform1f(glGetUniformLocation(prog, "u_grain_amt"), BD_GRAIN);
	glUniform1f(glGetUniformLocation(prog, "u_grain_px"), fmaxf(1.0f, roundf(p->scale)));
	glUniform1f(glGetUniformLocation(prog, "u_alpha"), p->alpha);
	glUniform1f(glGetUniformLocation(prog, "u_use_tex"), b_tex ? 1.0f : 0.0f);
	glUniform1f(glGetUniformLocation(prog, "u_mode"), (float)mode);
	glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
	glUniform1i(glGetUniformLocation(prog, "u_grain"), 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gl->bd_grain);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, b_tex ? blur->gl_texture : gl->bd_grain);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), v);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), v + 3);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	zrn_render_stats.draws++;
	zrn_render_stats.quads++;
}

const zrn_backend_t zrn_backend_gles = {
	.name = "gles2",
	.paint_clear = gl_paint_clear,
	.fill_rect = gl_fill_rect,
	.fill_rrect = gl_fill_rrect,
	.fill_ellipse = gl_fill_ellipse,
	.stroke_ellipse = gl_stroke_ellipse,
	.fill_polygon = gl_fill_polygon,
	.draw_line = gl_draw_line,
	.blit_alpha = gl_blit_alpha,
	.draw_image = gl_draw_image,
	/* the clip is the painter's own state: only the scissor follows it */
	.push_clip = zrn_clip_push,
	.pop_clip = zrn_clip_pop,
	.clipped_out = zrn_clip_out,
	.set_transform = gl_set_transform,
	.layer_begin = gl_layer_begin,
	.layer_end = gl_layer_end,
	.draw_layer = gl_draw_layer,
	.layer_drawable = gl_layer_drawable,
	.backdrop_begin = gl_backdrop_begin,
	.backdrop_end = gl_backdrop_end,
	.backdrop_draw = gl_backdrop_draw,
};


/* ============================================================================
 *  a frame
 * ============================================================================ */

void zrn_gl_begin(zrn_gl_t* const gl, zrn_painter_t* const p, const int width, const int height, const float scale) {
	make_current(gl);
	if (width != gl->width || height != gl->height) zrn_gl_resize(gl, width, height);
	if (gl->b_offscreen) glBindFramebuffer(GL_FRAMEBUFFER, gl->fbo);
	memset(p, 0, sizeof *p);
	p->be = &zrn_backend_gles;
	p->backend_data = gl;
	p->width = width;
	p->height = height;
	p->scale = scale;
	p->alpha = 1.0f;
	p->clip[2] = width;
	p->clip[3] = height;
	zrn_xf_identity(p->xf);
	zrn_xf_identity(gl->xf);
	gl->b_has_xf = false;
	gl->vp_x = gl->vp_y = 0;
	gl->batch_clip_set = 0;
	gl->batch_texture = 0;
	gl->quads = 0;
	glViewport(0, 0, width, height);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glEnable(0x809D); /* GL_MULTISAMPLE */
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
}

void zrn_gl_end(zrn_gl_t* const gl, const bool b_swap) {
	flush(gl);
	glDisable(GL_SCISSOR_TEST);
	if (b_swap && !gl->b_offscreen && !gl->b_external) eglSwapBuffers(gl->display, gl->surface);
}

void zrn_gl_swap(zrn_gl_t* const gl) {
	if (gl && !gl->b_offscreen && !gl->b_external) eglSwapBuffers(gl->display, gl->surface);
}

bool zrn_gl_read(zrn_gl_t* const gl, uint8_t* const rgba, const int width, const int height) {
	glBindFramebuffer(GL_FRAMEBUFFER, gl->b_offscreen ? gl->fbo : 0); /* the window swaps through the default framebuffer */
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	/* GL's rows run bottom to top */
	uint8_t* row = (uint8_t*)malloc((size_t)width * 4);
	if (!row) return 0;
	for (int y = 0; y < height / 2; y++) {
		uint8_t* const a = rgba + (size_t)y * (size_t)width * 4;
		uint8_t* const b = rgba + (size_t)(height - 1 - y) * (size_t)width * 4;
		memcpy(row, a, (size_t)width * 4);
		memcpy(a, b, (size_t)width * 4);
		memcpy(b, row, (size_t)width * 4);
	}
	free(row);
	return 1;
}

bool zrn_gl_available(void) { return 1; }
int zrn_gl_width(const zrn_gl_t* const gl) { return gl->width; }
int zrn_gl_height(const zrn_gl_t* const gl) { return gl->height; }

#else /* no OpenGL ES at build time */

zrn_gl_t* zrn_gl_create_offscreen(int width, int height) { (void)width; (void)height; return nullptr; }
zrn_gl_t* zrn_gl_create_external(void (*const current)(void* user), void* const user) { (void)current; (void)user; return nullptr; }
void zrn_gl_resize(zrn_gl_t* gl, int width, int height) { (void)gl; (void)width; (void)height; }
void zrn_gl_destroy(zrn_gl_t* gl) { (void)gl; }
void zrn_gl_begin(zrn_gl_t* gl, zrn_painter_t* p, int width, int height, float scale) { (void)gl; (void)p; (void)width; (void)height; (void)scale; }
void zrn_gl_end(zrn_gl_t* gl, bool b_swap) { (void)gl; (void)b_swap; }
void zrn_gl_swap(zrn_gl_t* gl) { (void)gl; }
bool zrn_gl_read(zrn_gl_t* gl, uint8_t* rgba, int width, int height) { (void)gl; (void)rgba; (void)width; (void)height; return 0; }
void zrn_gl_forget_image(zrn_image_t* image) { (void)image; }
bool zrn_gl_available(void) { return 0; }
int zrn_gl_width(const zrn_gl_t* gl) { (void)gl; return 0; }
int zrn_gl_height(const zrn_gl_t* gl) { (void)gl; return 0; }

#endif
