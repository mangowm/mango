#ifndef MANGO_DRAW_DIM_NODE_H
#define MANGO_DRAW_DIM_NODE_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wlr/types/wlr_buffer.h>

#if defined(__has_include) && __has_include(<scenefx/types/wlr_scene.h>)
#include <scenefx/types/wlr_scene.h>
#else
#include <wlr/types/wlr_scene.h>
#endif

struct mango_dim_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

typedef struct MangoDimNode {
	struct wlr_scene_buffer *scene_buffer;
	struct mango_dim_buffer *buffer;
	float color[4];
} MangoDimNode;

MangoDimNode *mango_dim_node_create(struct wlr_scene_tree *parent,
									const float color[4]);
void mango_dim_node_destroy(MangoDimNode *node);

void mango_dim_node_set_color(MangoDimNode *node, const float color[4]);
void mango_dim_node_set_radius(MangoDimNode *node, int32_t radius);
void mango_dim_node_set_box(MangoDimNode *node, int32_t x, int32_t y,
							int32_t width, int32_t height);
void mango_dim_node_set_enabled(MangoDimNode *node, bool enabled);

#endif
