#ifndef jump_label_node_H
#define jump_label_node_H

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__has_include) && __has_include(<scenefx/types/wlr_scene.h>)
#include <scenefx/types/wlr_scene.h>
#else
#include <wlr/types/wlr_scene.h>
#endif

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
} DecorateDrawData;

struct mango_text_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

struct mango_text_measure {
	cairo_surface_t *surface;
	cairo_t *cr;
	PangoContext *context;
	PangoLayout *layout;
	float scale;
};

typedef struct MangoJumpLabel {
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4];
	struct wlr_scene_rect *bg;
	struct wlr_scene_buffer *scene_buffer;
	struct mango_text_buffer *buffer;
	struct mango_text_measure measure;

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

	char *cached_text;
	char *cached_font_desc;
	float cached_scale;
	float cached_fg_color[4];
	bool cached_focused;
	int32_t surface_pixel_w;
	int32_t surface_pixel_h;

	bool focused;

	int32_t text_logical_w;
	int32_t text_logical_h;
	int32_t logical_width;
	int32_t logical_height;
} MangoJumpLabel;

typedef struct MangoBarDecoration {
	uint32_t type;
	bool is_tab;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4];
	struct wlr_scene_rect *bg;
	struct wlr_scene_buffer *scene_buffer;
	struct mango_text_buffer *buffer;
	void *node_data;
	struct mango_text_measure measure;

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

	int32_t target_width;
	int32_t target_height;

	char *cached_text;
	char *cached_font_desc;
	float cached_scale;
	float cached_fg_color[4];
	bool cached_focused;
	int32_t cached_clip_pixel_w;
	int32_t surface_pixel_w;
	int32_t surface_pixel_h;

	bool focused;

	char *last_text;
	float last_scale;

	int32_t text_logical_w;
	int32_t text_logical_h;
	int32_t logical_width;
	int32_t logical_height;

	struct wlr_scene_buffer *close_buffer;
	struct mango_text_buffer *close_texture;
	bool close_enabled;
	bool close_hover;
	int32_t close_size;
	int32_t close_margin;
	float close_color[4];
	float close_hover_color[4];
	float close_x_color[4];
	int32_t close_cached_size;
	float close_cached_scale;
	bool close_cached_hover;
	bool close_cached_valid;
} MangoBarDecoration;

void mango_text_global_finish(void);
MangoJumpLabel *mango_jump_label_node_create(struct wlr_scene_tree *parent,
											 DecorateDrawData data);
void mango_jump_label_node_destroy(MangoJumpLabel *node);
void mango_jump_label_node_set_background(MangoJumpLabel *node, float r,
										  float g, float b, float a);
void mango_jump_label_node_set_border(MangoJumpLabel *node, float r, float g,
									  float b, float a, int32_t width,
									  int32_t radius);
void mango_jump_label_node_set_padding(MangoJumpLabel *node, int32_t pad_x,
									   int32_t pad_y);
void mango_jump_label_node_update(MangoJumpLabel *node, const char *text,
								  float scale);

MangoBarDecoration *mango_bar_decoration_create(void *cdata, uint32_t type,
												bool is_tab,
												struct wlr_scene_tree *parent,
												DecorateDrawData data,
												int32_t width, int32_t height);
void mango_bar_decoration_destroy(MangoBarDecoration *node);
void mango_bar_decoration_set_size(MangoBarDecoration *node, int32_t width,
								   int32_t height);
void mango_bar_decoration_update(MangoBarDecoration *node, const char *text,
								 float scale);

void mango_jump_label_node_set_focus(MangoJumpLabel *node, bool focused);
void mango_bar_decoration_set_focus(MangoBarDecoration *node, bool focused);

void mango_bar_decoration_set_colors(MangoBarDecoration *node,
									 const float fg[4], const float bg[4]);

void mango_bar_decoration_set_close(MangoBarDecoration *node, bool enabled,
									int32_t size, int32_t margin);
void mango_bar_decoration_set_close_color(MangoBarDecoration *node,
										  const float circle[4]);
void mango_bar_decoration_set_close_hover(MangoBarDecoration *node, bool hover);
bool mango_bar_decoration_close_contains(MangoBarDecoration *node, double lx,
										 double ly);

void mango_jump_label_node_apply_config(MangoJumpLabel *node,
										const DecorateDrawData *data);
void mango_bar_decoration_apply_config(MangoBarDecoration *node,
									   const DecorateDrawData *data);
#endif // jump_label_node_H
