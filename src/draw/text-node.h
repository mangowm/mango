#ifndef TEXT_NODE_H
#define TEXT_NODE_H

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wlr/types/wlr_scene.h>

// 原有结构体，假设已存在
typedef struct {
	float fg_color[4];
	float bg_color[4];
	float focus_fg_color[4];
	float focus_bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	const char *font_desc;
} TextDrawData;

struct mango_text_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

struct mango_text_node {
	struct wlr_scene_buffer *scene_buffer;
	struct mango_text_buffer *buffer;
	cairo_surface_t *surface;
	int surface_pixel_w, surface_pixel_h;

	float fg_color[4];
	float bg_color[4];
	float focus_fg_color[4];
	float focus_bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	char *font_desc;

	// 缓存
	char *cached_text;
	char *cached_font_desc;
	float cached_scale;
	float cached_fg_color[4];
	float cached_bg_color[4];
	float cached_focus_fg_color[4];
	float cached_focus_bg_color[4];
	float cached_border_color[4];
	int32_t cached_border_width;
	int32_t cached_corner_radius;
	int32_t cached_padding_x;
	int32_t cached_padding_y;
	bool cached_focused;

	bool focused;

	// 测量
	cairo_surface_t *measure_surface;
	cairo_t *measure_cr;
	PangoContext *measure_context;
	PangoLayout *measure_layout;
	float measure_scale;

	int32_t logical_width;
	int32_t logical_height;
};

struct mango_titlebar_node {
	struct wlr_scene_buffer *scene_buffer;
	struct mango_text_buffer *buffer;
	cairo_surface_t *surface;
	int surface_pixel_w, surface_pixel_h;

	// 初始配置
	float fg_color[4];
	float bg_color[4];
	float focus_fg_color[4];
	float focus_bg_color[4];
	float border_color[4];
	int32_t border_width;
	int32_t corner_radius;
	int32_t padding_x;
	int32_t padding_y;
	char *font_desc;

	// 尺寸
	int32_t target_width;
	int32_t target_height;

	// 缓存
	char *cached_text;
	char *cached_font_desc;
	float cached_scale;
	float cached_fg_color[4];
	float cached_bg_color[4];
	float cached_focus_fg_color[4];
	float cached_focus_bg_color[4];
	float cached_border_color[4];
	int32_t cached_border_width;
	int32_t cached_corner_radius;
	int32_t cached_padding_x;
	int32_t cached_padding_y;
	int32_t cached_target_width;
	int32_t cached_target_height;
	bool cached_focused;

	bool focused;

	// 上次绘制参数（用于尺寸变化重绘）
	char *last_text;
	float last_scale;

	// 测量
	cairo_surface_t *measure_surface;
	cairo_t *measure_cr;
	PangoContext *measure_context;
	PangoLayout *measure_layout;
	float measure_scale;

	int32_t logical_width;
	int32_t logical_height;
};

void mango_text_global_finish(void);
struct mango_text_node *mango_text_node_create(struct wlr_scene_tree *parent,
											   TextDrawData data);
void mango_text_node_destroy(struct mango_text_node *node);
void mango_text_node_set_background(struct mango_text_node *node, float r,
									float g, float b, float a);
void mango_text_node_set_border(struct mango_text_node *node, float r, float g,
								float b, float a, int32_t width,
								int32_t radius);
void mango_text_node_set_padding(struct mango_text_node *node, int32_t pad_x,
								 int32_t pad_y);
void mango_text_node_update(struct mango_text_node *node, const char *text,
							float scale);

struct mango_titlebar_node *
mango_titlebar_node_create(void *mango_node_data, struct wlr_scene_tree *parent,
						   TextDrawData data, int32_t width, int32_t height);
void mango_titlebar_node_destroy(struct mango_titlebar_node *node);
void mango_titlebar_node_set_size(struct mango_titlebar_node *node,
								  int32_t width, int32_t height);
void mango_titlebar_node_update(struct mango_titlebar_node *node,
								const char *text, float scale);

void mango_text_node_set_focus(struct mango_text_node *node, bool focused);
void mango_titlebar_node_set_focus(struct mango_titlebar_node *node,
								   bool focused);

void mango_titlebar_node_set_colors(struct mango_titlebar_node *node,
									const float fg[4], const float bg[4]);
void mango_text_node_apply_config(struct mango_text_node *node,
								  const TextDrawData *data);
void mango_titlebar_node_apply_config(struct mango_titlebar_node *node,
									  const TextDrawData *data);
#endif // TEXT_NODE_H