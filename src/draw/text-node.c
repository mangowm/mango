#include "text-node.h"

#include <cairo.h>
#include <drm_fourcc.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>

static GHashTable *font_desc_cache = NULL;

static PangoFontDescription *get_cached_font_desc(const char *font_desc) {
	if (!font_desc_cache) {
		font_desc_cache =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
								  (GDestroyNotify)pango_font_description_free);
	}

	PangoFontDescription *desc =
		g_hash_table_lookup(font_desc_cache, font_desc);
	if (!desc) {
		desc = pango_font_description_from_string(font_desc);
		g_hash_table_insert(font_desc_cache, g_strdup(font_desc), desc);
	}
	return desc;
}

void mango_text_global_finish(void) {
	if (font_desc_cache) {
		g_hash_table_destroy(font_desc_cache);
		font_desc_cache = NULL;
	}
}

static void text_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	free(buf);
}

static bool text_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
											  uint32_t flags, void **data,
											  uint32_t *format,
											  size_t *stride) {
	(void)flags;
	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	*data = cairo_image_surface_get_data(buf->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = cairo_image_surface_get_stride(buf->surface);
	return true;
}

static void text_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {}

static const struct wlr_buffer_impl text_buffer_impl = {
	.destroy = text_buffer_destroy,
	.begin_data_ptr_access = text_buffer_begin_data_ptr_access,
	.end_data_ptr_access = text_buffer_end_data_ptr_access,
};

struct mango_text_node *mango_text_node_create(struct wlr_scene_tree *parent,
											   JumphitData data) {
	struct mango_text_node *node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;

	node->scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (!node->scene_buffer) {
		free(node);
		return NULL;
	}

	memcpy(node->fg_color, data.fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data.bg_color, sizeof(node->bg_color));
	memcpy(node->border_color, data.border_color, sizeof(node->border_color));
	node->border_width = data.border_width;
	node->corner_radius = data.corner_radius;
	node->padding_x = data.padding_x;
	node->padding_y = data.padding_y;
	node->font_desc =
		g_strdup(data.font_desc ? data.font_desc : "monospace Bold 24");

	node->cached_text = NULL;
	node->cached_scale = -1.0f;
	node->cached_font_desc = NULL;

	node->measure_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	node->measure_cr = cairo_create(node->measure_surface);
	node->measure_context = pango_cairo_create_context(node->measure_cr);
	node->measure_layout = pango_layout_new(node->measure_context);
	node->measure_scale = 1.0f;

	return node;
}

void mango_text_node_destroy(struct mango_text_node *node) {
	if (!node)
		return;

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}

	if (node->surface) {
		cairo_surface_destroy(node->surface);
		node->surface = NULL;
	}

	if (node->measure_layout)
		g_object_unref(node->measure_layout);
	if (node->measure_context)
		g_object_unref(node->measure_context);
	if (node->measure_cr)
		cairo_destroy(node->measure_cr);
	if (node->measure_surface)
		cairo_surface_destroy(node->measure_surface);

	wlr_scene_node_destroy(&node->scene_buffer->node);

	g_free(node->font_desc);
	g_free(node->cached_text);
	g_free(node->cached_font_desc);

	free(node);
}

void mango_text_node_set_background(struct mango_text_node *node, float r,
									float g, float b, float a) {
	if (!node)
		return;
	node->bg_color[0] = r;
	node->bg_color[1] = g;
	node->bg_color[2] = b;
	node->bg_color[3] = a;
}

void mango_text_node_set_border(struct mango_text_node *node, float r, float g,
								float b, float a, int32_t width,
								int32_t radius) {
	if (!node)
		return;
	node->border_color[0] = r;
	node->border_color[1] = g;
	node->border_color[2] = b;
	node->border_color[3] = a;
	node->border_width = width > 0 ? width : 0;
	node->corner_radius = radius;
}

void mango_text_node_set_padding(struct mango_text_node *node, int32_t pad_x,
								 int32_t pad_y) {
	if (!node)
		return;
	node->padding_x = pad_x >= 0 ? pad_x : 0;
	node->padding_y = pad_y >= 0 ? pad_y : 0;
}

static void get_text_pixel_size(struct mango_text_node *node, const char *text,
								float scale, int32_t *out_w, int32_t *out_h) {
	if (node->measure_scale != scale) {
		pango_cairo_context_set_resolution(node->measure_context, 96.0 * scale);
		node->measure_scale = scale;
	}

	PangoFontDescription *desc = get_cached_font_desc(node->font_desc);
	pango_layout_set_font_description(node->measure_layout, desc);
	pango_layout_set_text(node->measure_layout, text, -1);

	pango_layout_get_pixel_size(node->measure_layout, out_w, out_h);
}

static void draw_rounded_rect(cairo_t *cr, double x, double y, double w,
							  double h, double r) {
	double degrees = G_PI / 180.0;
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);
	cairo_arc(cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);
	cairo_arc(cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);
	cairo_arc(cr, x + r, y + r, r, 180 * degrees, 270 * degrees);
	cairo_close_path(cr);
}

void mango_text_node_update(struct mango_text_node *node, const char *text,
							float scale) {
	if (!node || !text)
		return;
	if (scale <= 0.0f)
		scale = 1.0f;

	/* 脏检查 */
	if (node->cached_scale == scale && node->cached_font_desc &&
		strcmp(node->cached_font_desc, node->font_desc) == 0 &&
		node->cached_text && strcmp(node->cached_text, text) == 0 &&
		memcmp(node->cached_fg_color, node->fg_color, sizeof(node->fg_color)) ==
			0 &&
		memcmp(node->cached_bg_color, node->bg_color, sizeof(node->bg_color)) ==
			0 &&
		memcmp(node->cached_border_color, node->border_color,
			   sizeof(node->border_color)) == 0 &&
		node->cached_border_width == node->border_width &&
		node->cached_corner_radius == node->corner_radius &&
		node->cached_padding_x == node->padding_x &&
		node->cached_padding_y == node->padding_y) {
		return;
	}

	/* 更新缓存 */
	g_free(node->cached_text);
	node->cached_text = g_strdup(text);
	g_free(node->cached_font_desc);
	node->cached_font_desc = g_strdup(node->font_desc);
	node->cached_scale = scale;
	memcpy(node->cached_fg_color, node->fg_color, sizeof(node->fg_color));
	memcpy(node->cached_bg_color, node->bg_color, sizeof(node->bg_color));
	memcpy(node->cached_border_color, node->border_color,
		   sizeof(node->border_color));
	node->cached_border_width = node->border_width;
	node->cached_corner_radius = node->corner_radius;
	node->cached_padding_x = node->padding_x;
	node->cached_padding_y = node->padding_y;

	int32_t text_pixel_w, text_pixel_h;
	get_text_pixel_size(node, text, scale, &text_pixel_w, &text_pixel_h);

	if (text_pixel_w <= 0 || text_pixel_h <= 0) {
		wlr_scene_buffer_set_buffer(node->scene_buffer, NULL);
		if (node->buffer) {
			wlr_buffer_drop(&node->buffer->base);
			node->buffer = NULL;
		}
		if (node->surface) {
			cairo_surface_destroy(node->surface);
			node->surface = NULL;
		}
		node->logical_width = 0;
		node->logical_height = 0;
		wlr_scene_buffer_set_dest_size(node->scene_buffer, 0, 0);
		return;
	}

	/* 逻辑尺寸：文本 + 内边距 + 边框（整数计算） */
	int32_t logical_text_w = (int32_t)(text_pixel_w / scale + 0.5f);
	int32_t logical_text_h = (int32_t)(text_pixel_h / scale + 0.5f);
	int32_t box_logical_w = logical_text_w + 2 * node->padding_x;
	int32_t box_logical_h = logical_text_h + 2 * node->padding_y;

	/* 加上边框后，乘以 scale 得到物理像素（表面尺寸），向上取整 */
	int32_t required_pixel_w =
		(int32_t)((box_logical_w + 2 * node->border_width) * scale + 0.5f);
	int32_t required_pixel_h =
		(int32_t)((box_logical_h + 2 * node->border_width) * scale + 0.5f);
	if (required_pixel_w < 1)
		required_pixel_w = 1;
	if (required_pixel_h < 1)
		required_pixel_h = 1;

	bool surface_size_changed = (!node->surface) ||
								(node->surface_pixel_w != required_pixel_w) ||
								(node->surface_pixel_h != required_pixel_h);

	if (surface_size_changed) {
		if (node->buffer) {
			wlr_buffer_drop(&node->buffer->base);
			node->buffer = NULL;
		}
		if (node->surface) {
			cairo_surface_destroy(node->surface);
			node->surface = NULL;
		}

		node->surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, required_pixel_w, required_pixel_h);
		node->surface_pixel_w = required_pixel_w;
		node->surface_pixel_h = required_pixel_h;
	}

	cairo_t *cr = cairo_create(node->surface);

	/* 清空为透明 */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	/* 计算背景矩形（物理像素） */
	double border = node->border_width * scale;
	double bg_x = border;
	double bg_y = border;
	double bg_w = box_logical_w * scale;
	double bg_h = box_logical_h * scale;

	/* 圆角半径（物理像素） */
	double radius;
	if (node->corner_radius < 0) {
		/* 负数表示自动取半宽/半高作为圆角 */
		radius = (bg_w < bg_h ? bg_w : bg_h) / 2.0;
	} else {
		radius = node->corner_radius * scale;
	}
	/* 限制最大圆角 */
	if (radius > bg_w / 2.0)
		radius = bg_w / 2.0;
	if (radius > bg_h / 2.0)
		radius = bg_h / 2.0;

	bool draw_bg = (node->bg_color[3] > 0.0f);
	bool draw_border =
		(node->border_width > 0) && (node->border_color[3] > 0.0f);

	/* 绘制背景 */
	if (draw_bg) {
		cairo_set_source_rgba(cr, node->bg_color[0], node->bg_color[1],
							  node->bg_color[2], node->bg_color[3]);
		if (radius > 0.0) {
			draw_rounded_rect(cr, bg_x, bg_y, bg_w, bg_h, radius);
			cairo_fill(cr);
		} else {
			cairo_rectangle(cr, bg_x, bg_y, bg_w, bg_h);
			cairo_fill(cr);
		}
	}

	/* 绘制文本 */
	cairo_save(cr);
	double text_x = (node->border_width + node->padding_x) * scale;
	double text_y = (node->border_width + node->padding_y) * scale;
	cairo_translate(cr, text_x, text_y);

	PangoContext *ctx = pango_cairo_create_context(cr);
	pango_cairo_context_set_resolution(ctx, 96.0 * scale);
	PangoLayout *layout = pango_layout_new(ctx);
	PangoFontDescription *desc = get_cached_font_desc(node->font_desc);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, text, -1);

	cairo_set_source_rgba(cr, node->fg_color[0], node->fg_color[1],
						  node->fg_color[2], node->fg_color[3]);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	g_object_unref(ctx);
	cairo_restore(cr);

	/* 绘制边框 */
	if (draw_border) {
		cairo_set_source_rgba(cr, node->border_color[0], node->border_color[1],
							  node->border_color[2], node->border_color[3]);
		cairo_set_line_width(cr, border);

		double half_lw = border * 0.5;
		double bx = bg_x - half_lw;
		double by = bg_y - half_lw;
		double bw = bg_w + border;
		double bh = bg_h + border;

		if (radius > 0.0) {
			double outer_radius = radius + half_lw;
			if (outer_radius < 0.0)
				outer_radius = 0.0;
			draw_rounded_rect(cr, bx, by, bw, bh, outer_radius);
		} else {
			cairo_rectangle(cr, bx, by, bw, bh);
		}
		cairo_stroke(cr);
	}

	cairo_surface_flush(node->surface);
	cairo_destroy(cr);

	/* 更新 wlr_buffer */
	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}

	struct mango_text_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf) {
		return;
	}
	wlr_buffer_init(&buf->base, &text_buffer_impl, node->surface_pixel_w,
					node->surface_pixel_h);
	buf->surface = node->surface;
	node->buffer = buf;

	wlr_scene_buffer_set_buffer(node->scene_buffer, &buf->base);

	node->logical_width = box_logical_w + 2 * node->border_width;
	node->logical_height = box_logical_h + 2 * node->border_width;
	wlr_scene_buffer_set_dest_size(node->scene_buffer, node->logical_width,
								   node->logical_height);
}