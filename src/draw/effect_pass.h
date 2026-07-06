#ifndef MANGO_EFFECT_PASS_H
#define MANGO_EFFECT_PASS_H
#include <scenefx/types/wlr_scene.h>
#include <stdbool.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>

bool effect_pass_init(struct wlr_renderer *renderer,
					  struct wlr_allocator *alloc);

void effect_pass_finish(void);

struct effect_uniforms {
	float progress;
	float time;
};
struct effect_shader;
void effect_pass_load_dir(const char *dir);
struct effect_shader *effect_pass_get(const char *name);
bool effect_pass_run(struct effect_shader *shader, struct wlr_texture *src,
					 struct wlr_buffer *dst, struct effect_uniforms u);
bool effect_pass_texture_usable(struct wlr_texture *src);
struct wlr_buffer *effect_pass_flatten(struct wlr_scene_node *node, int width,
									   int height, bool include_rects);
struct wlr_swapchain *effect_pass_create_swapchain(int width, int height);

#endif
