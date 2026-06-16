#ifndef MANGO_TEXT_NODE_H
#define MANGO_TEXT_NODE_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>

struct mango_text_node {
	struct wlr_scene_buffer *scene_buffer;
	int logical_width;
	int logical_height;
	const char *font_desc;

	// 外观属性
	float fg_color[4];	   // 文本颜色 RGBA，默认白色
	float bg_color[4];	   // 背景色 RGBA，默认透明
	float border_color[4]; // 边框色 RGBA，默认透明
	int32_t border_width;  // 边框宽度（逻辑像素），<=0 则不绘制
	int32_t corner_radius; // 圆角半径（逻辑像素），<0 时自动取
						   // min(width,height)/2 形成胶囊
	int32_t padding_x;	   // 文本左右内边距（逻辑像素）
	int32_t padding_y;	   // 文本上下内边距（逻辑像素）
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

// 外观设置接口
void mango_text_node_set_background(struct mango_text_node *node, float r,
									float g, float b, float a);
void mango_text_node_set_border(struct mango_text_node *node, float r, float g,
								float b, float a, float width, float radius);
void mango_text_node_set_padding(struct mango_text_node *node, float pad_x,
								 float pad_y);

void mango_text_node_destroy(struct mango_text_node *node);
void mango_text_global_finish(void);

#endif