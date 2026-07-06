#include "effect_pass.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <dirent.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <limits.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/egl.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/log.h>

bool wlr_texture_is_fx(struct wlr_texture *texture);
bool wlr_renderer_is_fx(struct wlr_renderer *renderer);

static struct wlr_buffer *effect_alloc_render_buffer(int w, int h);

static struct {
	bool ready;
	struct wlr_renderer *renderer;
	struct wlr_allocator *alloc;
	EGLDisplay egl_display;
	EGLContext egl_context;
} state;

bool effect_pass_init(struct wlr_renderer *renderer,
					  struct wlr_allocator *alloc) {
	if (!wlr_renderer_is_fx(renderer)) {
		wlr_log(WLR_ERROR, "effect_pass: renderer is not a SceneFX renderer, "
						   "shaders disabled");
		return false;
	}
	state.renderer = renderer;
	state.alloc = alloc;
	state.egl_display = EGL_NO_DISPLAY;
	state.egl_context = EGL_NO_CONTEXT;
	state.ready = true;

	struct wlr_buffer *probe = effect_alloc_render_buffer(1, 1);
	if (probe) {
		struct wlr_render_pass *pass =
			wlr_renderer_begin_buffer_pass(renderer, probe, NULL);
		if (pass) {
			state.egl_display = eglGetCurrentDisplay();
			state.egl_context = eglGetCurrentContext();
			wlr_render_pass_submit(pass);
		}
		wlr_buffer_drop(probe);
	}

	if (state.egl_display == EGL_NO_DISPLAY) {
		wlr_log(WLR_ERROR,
				"effect_pass: could not acquire EGL context, shaders disabled");
		state.ready = false;
		return false;
	}
	return true;
}

struct effect_shader {
	GLuint program;
	GLint loc_tex, loc_progress, loc_time, loc_size, loc_pos, loc_uv;
};

static const char *VERT_SRC =
	"#version 300 es\n"
	"in vec2 a_pos;\n"
	"in vec2 a_uv;\n"
	"out vec2 v_texcoord;\n"
	"void main() { v_texcoord = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *INJECT_HEADER = "#version 300 es\n"
								   "precision highp float;\n"
								   "uniform sampler2D u_texture;\n"
								   "uniform float u_progress;\n"
								   "uniform float u_time;\n"
								   "uniform vec2 u_size;\n"
								   "in vec2 v_texcoord;\n"
								   "out vec4 fragColor;\n";

static GLuint effect_compile_shader(GLenum type, const char *src1,
									const char *src2) {
	GLuint s = glCreateShader(type);
	const char *srcs[2] = {src1, src2 ? src2 : ""};
	glShaderSource(s, src2 ? 2 : 1, srcs, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		wlr_log(WLR_ERROR, "effect_pass: shader compile failed: %s", log);
		glDeleteShader(s);
		return 0;
	}
	return s;
}

static struct effect_shader *make_shader(const char *frag_body) {
	GLuint vs = effect_compile_shader(GL_VERTEX_SHADER, VERT_SRC, NULL);
	GLuint fs =
		effect_compile_shader(GL_FRAGMENT_SHADER, INJECT_HEADER, frag_body);
	if (!vs || !fs) {
		if (vs)
			glDeleteShader(vs);
		if (fs)
			glDeleteShader(fs);
		return NULL;
	}
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint ok = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		glDeleteProgram(prog);
		return NULL;
	}
	struct effect_shader *sh = calloc(1, sizeof(*sh));
	sh->program = prog;
	sh->loc_tex = glGetUniformLocation(prog, "u_texture");
	sh->loc_progress = glGetUniformLocation(prog, "u_progress");
	sh->loc_time = glGetUniformLocation(prog, "u_time");
	sh->loc_size = glGetUniformLocation(prog, "u_size");
	sh->loc_pos = glGetAttribLocation(prog, "a_pos");
	sh->loc_uv = glGetAttribLocation(prog, "a_uv");
	return sh;
}

struct registry_entry {
	char name[64];
	struct effect_shader *shader;
};

static struct registry_entry *registry;
static size_t registry_count;
static size_t registry_cap;

static void registry_put(const char *name, struct effect_shader *shader) {
	for (size_t i = 0; i < registry_count; i++) {
		if (strcmp(registry[i].name, name) == 0) {
			wlr_log(WLR_INFO,
					"effect_pass: shader '%s' redefined, replacing previous",
					name);
			glDeleteProgram(registry[i].shader->program);
			free(registry[i].shader);
			registry[i].shader = shader;
			return;
		}
	}
	if (registry_count == registry_cap) {
		size_t new_cap = registry_cap ? registry_cap * 2 : 8;
		struct registry_entry *grown =
			realloc(registry, new_cap * sizeof(*grown));
		if (!grown) {
			wlr_log(WLR_ERROR,
					"effect_pass: out of memory growing shader registry, "
					"dropping '%s'",
					name);
			glDeleteProgram(shader->program);
			free(shader);
			return;
		}
		registry = grown;
		registry_cap = new_cap;
	}
	snprintf(registry[registry_count].name,
			 sizeof(registry[registry_count].name), "%s", name);
	registry[registry_count].shader = shader;
	registry_count++;
}

void effect_pass_load_dir(const char *dir) {
	if (!state.ready || !dir) {
		return;
	}
	DIR *d = opendir(dir);
	if (!d) {
		wlr_log(
			WLR_INFO,
			"effect_pass: shader directory '%s' not available (%s), skipping",
			dir, strerror(errno));
		return;
	}

	EGLContext prev_ctx = eglGetCurrentContext();
	EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
	EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);
	EGLDisplay prev_dpy = eglGetCurrentDisplay();
	eglMakeCurrent(state.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
				   state.egl_context);

	static const char ext[] = ".frag";
	const size_t ext_len = sizeof(ext) - 1;

	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		const char *fname = ent->d_name;
		size_t len = strlen(fname);
		if (len <= ext_len || strcmp(fname + len - ext_len, ext) != 0) {
			continue;
		}
		size_t name_len = len - ext_len;
		if (name_len >= sizeof(registry[0].name)) {
			wlr_log(WLR_ERROR,
					"effect_pass: shader filename '%s' too long, skipping",
					fname);
			continue;
		}

		char path[PATH_MAX];
		int n = snprintf(path, sizeof(path), "%s/%s", dir, fname);
		if (n < 0 || (size_t)n >= sizeof(path)) {
			wlr_log(WLR_ERROR,
					"effect_pass: shader path '%s/%s' too long, skipping", dir,
					fname);
			continue;
		}

		FILE *f = fopen(path, "rb");
		if (!f) {
			wlr_log(WLR_ERROR,
					"effect_pass: failed to open '%s' (%s), skipping", path,
					strerror(errno));
			continue;
		}
		if (fseek(f, 0, SEEK_END) != 0) {
			wlr_log(WLR_ERROR, "effect_pass: failed to read '%s', skipping",
					path);
			fclose(f);
			continue;
		}
		long size = ftell(f);
		if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
			wlr_log(WLR_ERROR, "effect_pass: failed to read '%s', skipping",
					path);
			fclose(f);
			continue;
		}
		char *buf = malloc((size_t)size + 1);
		if (!buf) {
			wlr_log(WLR_ERROR,
					"effect_pass: out of memory reading '%s', skipping", path);
			fclose(f);
			continue;
		}
		size_t got = fread(buf, 1, (size_t)size, f);
		fclose(f);
		buf[got] = '\0';

		char name[sizeof(registry[0].name)];
		snprintf(name, sizeof(name), "%.*s", (int)name_len, fname);

		struct effect_shader *sh = make_shader(buf);
		free(buf);
		if (!sh) {
			wlr_log(WLR_ERROR,
					"effect_pass: shader '%s' failed to compile, skipping",
					name);
			continue;
		}
		registry_put(name, sh);
		wlr_log(WLR_INFO, "effect_pass: registered shader '%s'", name);
	}
	closedir(d);
	wlr_log(WLR_INFO, "effect_pass: %zu shader(s) registered from '%s'",
			registry_count, dir);

	eglMakeCurrent(prev_dpy == EGL_NO_DISPLAY ? state.egl_display : prev_dpy,
				   prev_draw, prev_read, prev_ctx);
}

struct effect_shader *effect_pass_get(const char *name) {
	if (!name) {
		return NULL;
	}
	for (size_t i = 0; i < registry_count; i++) {
		if (strcmp(registry[i].name, name) == 0) {
			return registry[i].shader;
		}
	}
	return NULL;
}

bool effect_pass_texture_usable(struct wlr_texture *src) {
	if (!src || !wlr_texture_is_fx(src))
		return false;
	struct fx_texture_attribs sa;
	fx_texture_get_attribs(src, &sa);
	return sa.target == GL_TEXTURE_2D;
}

bool effect_pass_run(struct effect_shader *shader, struct wlr_texture *src,
					 struct wlr_buffer *dst, struct effect_uniforms u) {
	if (!state.ready || !shader || !src || !dst)
		return false;
	struct fx_texture_attribs sa;
	if (!effect_pass_texture_usable(src))
		return false;
	fx_texture_get_attribs(src, &sa);
	GLuint fbo = fx_renderer_get_buffer_fbo(state.renderer, dst);
	if (!fbo)
		return false;

	EGLContext prev_ctx = eglGetCurrentContext();
	EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
	EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);
	EGLDisplay prev_dpy = eglGetCurrentDisplay();
	eglMakeCurrent(state.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
				   state.egl_context);

	while (glGetError() != GL_NO_ERROR)
		;

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, dst->width, dst->height);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(shader->program);

	static const GLfloat pos[] = {-1, -1, 1, -1, -1, 1, 1, 1};
	static const GLfloat uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
	if (shader->loc_pos >= 0) {
		glVertexAttribPointer(shader->loc_pos, 2, GL_FLOAT, GL_FALSE, 0, pos);
		glEnableVertexAttribArray(shader->loc_pos);
	}
	if (shader->loc_uv >= 0) {
		glVertexAttribPointer(shader->loc_uv, 2, GL_FLOAT, GL_FALSE, 0, uv);
		glEnableVertexAttribArray(shader->loc_uv);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(sa.target, sa.tex);
	glTexParameteri(sa.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(sa.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glUniform1i(shader->loc_tex, 0);
	glUniform1f(shader->loc_progress, u.progress);
	glUniform1f(shader->loc_time, u.time);
	glUniform2f(shader->loc_size, (float)dst->width, (float)dst->height);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	if (shader->loc_pos >= 0)
		glDisableVertexAttribArray(shader->loc_pos);
	if (shader->loc_uv >= 0)
		glDisableVertexAttribArray(shader->loc_uv);
	glBindTexture(sa.target, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
	glFlush();
	GLenum err = glGetError();

	eglMakeCurrent(prev_dpy == EGL_NO_DISPLAY ? state.egl_display : prev_dpy,
				   prev_draw, prev_read, prev_ctx);
	return err == GL_NO_ERROR;
}

static const struct wlr_drm_format *effect_render_format(void) {
	const struct wlr_drm_format_set *texture_formats =
		wlr_renderer_get_texture_formats(state.renderer, WLR_BUFFER_CAP_DMABUF);
	if (!texture_formats) {
		return NULL;
	}
	return wlr_drm_format_set_get(texture_formats, DRM_FORMAT_ABGR8888);
}

static struct wlr_buffer *effect_alloc_render_buffer(int w, int h) {
	const struct wlr_drm_format *fmt = effect_render_format();
	if (!fmt) {
		return NULL;
	}
	return wlr_allocator_create_buffer(state.alloc, w, h, fmt);
}

struct wlr_swapchain *effect_pass_create_swapchain(int width, int height) {
	if (!state.ready || width <= 0 || height <= 0) {
		return NULL;
	}
	const struct wlr_drm_format *fmt = effect_render_format();
	if (!fmt) {
		return NULL;
	}
	return wlr_swapchain_create(state.alloc, width, height, fmt);
}

static void effect_flatten_bounds(struct wlr_scene_node *node, int *min_x,
								  int *min_y, bool *found) {
	if (!node->enabled) {
		return;
	}
	if (node->type == WLR_SCENE_NODE_BUFFER ||
		node->type == WLR_SCENE_NODE_RECT) {
		int ax = 0, ay = 0;
		wlr_scene_node_coords(node, &ax, &ay);
		if (!*found) {
			*min_x = ax;
			*min_y = ay;
			*found = true;
		} else {
			if (ax < *min_x)
				*min_x = ax;
			if (ay < *min_y)
				*min_y = ay;
		}
	} else if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			effect_flatten_bounds(child, min_x, min_y, found);
		}
	}
}

static void effect_flatten_paint(struct wlr_scene_node *node,
								 struct wlr_render_pass *pass, int ox, int oy,
								 bool include_rects) {
	if (!node->enabled) {
		return;
	}
	int abs_x = 0, abs_y = 0;
	wlr_scene_node_coords(node, &abs_x, &abs_y);
	int nx = abs_x - ox, ny = abs_y - oy;
	switch (node->type) {
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *sb = wlr_scene_buffer_from_node(node);
		if (sb->buffer) {
			struct wlr_texture *t =
				wlr_texture_from_buffer(state.renderer, sb->buffer);
			if (t) {
				int dw = sb->dst_width > 0 ? sb->dst_width : t->width;
				int dh = sb->dst_height > 0 ? sb->dst_height : t->height;
				wlr_render_pass_add_texture(
					pass, &(struct wlr_render_texture_options){
							  .texture = t,
							  .dst_box =
								  {
									  .x = nx,
									  .y = ny,
									  .width = dw,
									  .height = dh,
								  },
						  });
				wlr_texture_destroy(t);
			} else {
				wlr_log(WLR_ERROR,
						"effect_pass: flatten failed to create texture from "
						"buffer, snapshot will be incomplete");
			}
		}
		break;
	}
	case WLR_SCENE_NODE_RECT: {
		if (!include_rects)
			break;
		struct wlr_scene_rect *r = wlr_scene_rect_from_node(node);
		wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
										   .box = {.x = nx,
												   .y = ny,
												   .width = r->width,
												   .height = r->height},
										   .color = {r->color[0], r->color[1],
													 r->color[2], r->color[3]},
									   });
		break;
	}
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			effect_flatten_paint(child, pass, ox, oy, include_rects);
		}
		break;
	}
	case WLR_SCENE_NODE_SHADOW:
	case WLR_SCENE_NODE_OPTIMIZED_BLUR:
	case WLR_SCENE_NODE_BLUR:
		break;
	}
}

struct wlr_buffer *effect_pass_flatten(struct wlr_scene_node *node, int width,
									   int height, bool include_rects) {
	if (!state.ready || !node || width <= 0 || height <= 0) {
		return NULL;
	}

	struct wlr_buffer *dst = effect_alloc_render_buffer(width, height);
	if (!dst) {
		wlr_log(WLR_ERROR, "effect_pass: failed to allocate flatten buffer");
		return NULL;
	}

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(state.renderer, dst, NULL);
	if (!pass) {
		wlr_buffer_drop(dst);
		return NULL;
	}

	wlr_render_pass_add_rect(
		pass, &(struct wlr_render_rect_options){
				  .box = {.x = 0, .y = 0, .width = width, .height = height},
				  .color = {0, 0, 0, 0},
				  .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
			  });

	int ox = 0, oy = 0;
	bool found = false;
	effect_flatten_bounds(node, &ox, &oy, &found);
	if (!found) {
		wlr_scene_node_coords(node, &ox, &oy);
	}
	effect_flatten_paint(node, pass, ox, oy, include_rects);

	if (!wlr_render_pass_submit(pass)) {
		wlr_buffer_drop(dst);
		return NULL;
	}

	return dst;
}

void effect_pass_finish(void) {
	if (registry_count > 0) {
		EGLContext prev_ctx = eglGetCurrentContext();
		EGLSurface prev_draw = eglGetCurrentSurface(EGL_DRAW);
		EGLSurface prev_read = eglGetCurrentSurface(EGL_READ);
		EGLDisplay prev_dpy = eglGetCurrentDisplay();
		eglMakeCurrent(state.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
					   state.egl_context);
		for (size_t i = 0; i < registry_count; i++) {
			if (registry[i].shader) {
				glDeleteProgram(registry[i].shader->program);
				free(registry[i].shader);
			}
		}

		eglMakeCurrent(prev_dpy == EGL_NO_DISPLAY ? state.egl_display
												  : prev_dpy,
					   prev_draw, prev_read, prev_ctx);
	}
	free(registry);
	registry = NULL;
	registry_count = 0;
	registry_cap = 0;

	state.ready = false;
}
