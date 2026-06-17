#ifndef MANGO_TEXT_NODE_H
#define MANGO_TEXT_NODE_H

#include <cairo.h>
#include <pango/pangocairo.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

// 自定义 wlr_buffer，仅用于包装 cairo surface，不负责 surface 的销毁
struct mango_text_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

struct mango_text_node {
	struct wlr_scene_buffer *scene_buffer;

	float fg_color[4];
	float bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x, padding_y;
	char *font_desc;

	char *cached_text;
	float cached_scale;
	float cached_fg_color[4];
	float cached_bg_color[4];
	float cached_border_color[4];
	float cached_border_width;
	float cached_corner_radius;
	float cached_padding_x, cached_padding_y;
	char *cached_font_desc;

	cairo_surface_t *surface;
	struct mango_text_buffer *buffer;
	int32_t surface_pixel_w, surface_pixel_h;

	cairo_surface_t *measure_surface;
	cairo_t *measure_cr;
	PangoContext *measure_context;
	PangoLayout *measure_layout;
	float measure_scale;

	int32_t logical_width;
	int32_t logical_height;
};

typedef struct {
	float fg_color[4];
	float bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	const char *font_desc;
} JumphitData;

struct mango_text_node *mango_text_node_create(struct wlr_scene_tree *parent,
											   JumphitData data);
void mango_text_node_destroy(struct mango_text_node *node);
void mango_text_node_update(struct mango_text_node *node, const char *text,
							float scale);

void mango_text_node_set_background(struct mango_text_node *node, float r,
									float g, float b, float a);
void mango_text_node_set_border(struct mango_text_node *node, float r, float g,
								float b, float a, int32_t width,
								int32_t radius);

void mango_text_node_set_padding(struct mango_text_node *node, int32_t pad_x,
								 int32_t pad_y);

void mango_text_global_finish(void);

#endif