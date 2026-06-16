#include "text-node.h"
#include <cairo.h>
#include <drm_fourcc.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>

// 自定义 wlr_buffer，用于将 Cairo Surface 包装并注入 wlr_scene
struct mango_text_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

// 全局字体描述缓存
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

// wlr_buffer 实现
static void text_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	cairo_surface_destroy(buf->surface);
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

	node->fg_color[0] = data.fg_color[0];
	node->fg_color[1] = data.fg_color[1];
	node->fg_color[2] = data.fg_color[2];
	node->fg_color[3] = data.fg_color[3];
	node->bg_color[0] = data.bg_color[0];
	node->bg_color[1] = data.bg_color[1];
	node->bg_color[2] = data.bg_color[2];
	node->bg_color[3] = data.bg_color[3];
	node->border_color[0] = data.border_color[0];
	node->border_color[1] = data.border_color[1];
	node->border_color[2] = data.border_color[2];
	node->border_color[3] = data.border_color[3];
	node->border_width = data.border_width;
	node->corner_radius = data.corner_radius;
	node->padding_x = data.padding_x;
	node->padding_y = data.padding_y;
	node->font_desc = data.font_desc ? data.font_desc : "monospace Bold 24";

	return node;
}

void mango_text_node_destroy(struct mango_text_node *node) {
	if (!node)
		return;

	wlr_scene_node_destroy(&node->scene_buffer->node);
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
								float b, float a, float width, float radius) {
	if (!node)
		return;
	node->border_color[0] = r;
	node->border_color[1] = g;
	node->border_color[2] = b;
	node->border_color[3] = a;
	node->border_width = width > 0.0f ? width : 0.0f;
	node->corner_radius = radius >= 0.0f ? radius : 0.0f;
}

void mango_text_node_set_padding(struct mango_text_node *node, float pad_x,
								 float pad_y) {
	if (!node)
		return;
	node->padding_x = pad_x >= 0.0f ? pad_x : 0.0f;
	node->padding_y = pad_y >= 0.0f ? pad_y : 0.0f;
}

static void draw_rounded_rect(cairo_t *cr, double x, double y, double w,
							  double h, double r) {
	// 使用 Cairo 的标准圆角矩形路径
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

	const char *font_desc = node->font_desc;
	PangoFontDescription *desc = get_cached_font_desc(font_desc);

	// 测量文字像素尺寸（无缩放的实际像素）
	cairo_surface_t *dummy_surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *dummy_cr = cairo_create(dummy_surface);
	PangoContext *dummy_ctx = pango_cairo_create_context(dummy_cr);
	pango_cairo_context_set_resolution(dummy_ctx, 96.0 * scale);
	PangoLayout *dummy_layout = pango_layout_new(dummy_ctx);
	pango_layout_set_font_description(dummy_layout, desc);
	pango_layout_set_text(dummy_layout, text, -1);

	int text_pixel_w, text_pixel_h;
	pango_layout_get_pixel_size(dummy_layout, &text_pixel_w, &text_pixel_h);

	g_object_unref(dummy_layout);
	g_object_unref(dummy_ctx);
	cairo_destroy(dummy_cr);
	cairo_surface_destroy(dummy_surface);

	if (text_pixel_w <= 0 || text_pixel_h <= 0) {
		wlr_scene_buffer_set_buffer(node->scene_buffer, NULL);
		node->logical_width = 0;
		node->logical_height = 0;
		return;
	}

	float logical_text_w = text_pixel_w / scale;
	float logical_text_h = text_pixel_h / scale;

	float pad_x = node->padding_x;
	float pad_y = node->padding_y;
	float box_logical_w = logical_text_w + 2.0f * pad_x;
	float box_logical_h = logical_text_h + 2.0f * pad_y;

	float border = node->border_width;
	bool draw_border = (border > 0.0f) && (node->border_color[3] > 0.0f);
	bool draw_bg = (node->bg_color[3] > 0.0f);

	int surface_pixel_w = (int)((box_logical_w + 2.0f * border) * scale + 0.5f);
	int surface_pixel_h = (int)((box_logical_h + 2.0f * border) * scale + 0.5f);

	if (surface_pixel_w < 1)
		surface_pixel_w = 1;
	if (surface_pixel_h < 1)
		surface_pixel_h = 1;

	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, surface_pixel_w, surface_pixel_h);
	cairo_t *cr = cairo_create(surface);

	// 清空为全透明
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	double bg_x = border * scale;
	double bg_y = border * scale;
	double bg_w = box_logical_w * scale;
	double bg_h = box_logical_h * scale;

	double radius = node->corner_radius * scale;

	if (radius < 0) {
		radius = (bg_w < bg_h ? bg_w : bg_h) / 2.0;
	}
	if (radius > bg_w / 2.0)
		radius = bg_w / 2.0;
	if (radius > bg_h / 2.0)
		radius = bg_h / 2.0;

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

	cairo_save(cr);
	cairo_translate(cr, (border + pad_x) * scale, (border + pad_y) * scale);

	PangoContext *ctx = pango_cairo_create_context(cr);
	pango_cairo_context_set_resolution(ctx, 96.0 * scale);
	PangoLayout *layout = pango_layout_new(ctx);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_text(layout, text, -1);

	cairo_set_source_rgba(cr, node->fg_color[0], node->fg_color[1],
						  node->fg_color[2], node->fg_color[3]);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	g_object_unref(ctx);
	cairo_restore(cr);

	if (draw_border) {
		cairo_set_source_rgba(cr, node->border_color[0], node->border_color[1],
							  node->border_color[2], node->border_color[3]);
		cairo_set_line_width(cr, border * scale);

		double half_lw = border * scale * 0.5;
		double bx = bg_x - half_lw;
		double by = bg_y - half_lw;
		double bw = bg_w + border * scale;
		double bh = bg_h + border * scale;

		if (radius > 0.0) {
			double outer_radius = radius + half_lw;
			if (outer_radius < 0.0)
				outer_radius = 0.0;
			draw_rounded_rect(cr, bx, by, bw, bh, outer_radius);
			cairo_stroke(cr);
		} else {
			cairo_rectangle(cr, bx, by, bw, bh);
			cairo_stroke(cr);
		}
	}

	cairo_surface_flush(surface);

	// 包装成 wlr_buffer
	struct mango_text_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf) {
		cairo_surface_destroy(surface);
		cairo_destroy(cr);
		return;
	}
	wlr_buffer_init(&buf->base, &text_buffer_impl, surface_pixel_w,
					surface_pixel_h);
	buf->surface = surface;

	wlr_scene_buffer_set_buffer(node->scene_buffer, &buf->base);
	wlr_buffer_drop(&buf->base);

	// 最终逻辑大小 = 背景框逻辑尺寸 + 边框逻辑尺寸
	node->logical_width = (int)(box_logical_w + 2.0f * border);
	node->logical_height = (int)(box_logical_h + 2.0f * border);
	wlr_scene_buffer_set_dest_size(node->scene_buffer, node->logical_width,
								   node->logical_height);

	cairo_destroy(cr);
}