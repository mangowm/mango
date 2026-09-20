#include "mango/draw/text-node.h"

#include <cairo.h>
#include <drm_fourcc.h>
#include <glib.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_buffer.h>

#if defined(__has_include) && __has_include(<scenefx/types/fx/clipped_region.h>)
#include <scenefx/types/fx/clipped_region.h>
#define TEXT_NODE_CORNER_RADIUS 1
#else
#define TEXT_NODE_CORNER_RADIUS 0
#endif

static GHashTable *font_desc_cache = NULL;

PangoFontDescription *get_cached_font_desc(const char *font_desc) {
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

void text_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	if (buf->surface) {
		cairo_surface_destroy(buf->surface);
	}
	free(buf);
}

bool text_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
									   uint32_t flags, void **data,
									   uint32_t *format, size_t *stride) {
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}

	struct mango_text_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	if (!buf->surface) {
		return false;
	}
	*data = cairo_image_surface_get_data(buf->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = cairo_image_surface_get_stride(buf->surface);
	return true;
}

void text_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {}

static const struct wlr_buffer_impl text_buffer_impl = {
	.destroy = text_buffer_destroy,
	.begin_data_ptr_access = text_buffer_begin_data_ptr_access,
	.end_data_ptr_access = text_buffer_end_data_ptr_access,
};

static void rect_apply(struct wlr_scene_rect *rect, const float color[4],
					   int32_t radius, int32_t x, int32_t y, int32_t width,
					   int32_t height) {
	float premultiplied[4] = {
		color[0] * color[3],
		color[1] * color[3],
		color[2] * color[3],
		color[3],
	};
	wlr_scene_rect_set_color(rect, premultiplied);

	if (width < 0) {
		width = 0;
	}
	if (height < 0) {
		height = 0;
	}

#if TEXT_NODE_CORNER_RADIUS
	int32_t limit = (width < height ? width : height) / 2;
	if (radius < 0) {
		radius = limit;
	} else if (radius > limit) {
		radius = limit;
	}
	wlr_scene_rect_set_corner_radii(rect, corner_radii_all(radius));
#endif

	wlr_scene_node_set_position(&rect->node, x, y);
	wlr_scene_rect_set_size(rect, width, height);
}

static void layout_configure(PangoLayout *layout, const char *font_desc,
							 const char *text) {
	pango_layout_set_font_description(layout, get_cached_font_desc(font_desc));
	pango_layout_set_text(layout, text, -1);
}

static void measure_init(struct mango_text_measure *measure) {
	measure->surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	measure->cr = cairo_create(measure->surface);
	measure->context = pango_cairo_create_context(measure->cr);
	measure->layout = pango_layout_new(measure->context);
	measure->scale = 1.0f;
}

static void measure_finish(struct mango_text_measure *measure) {
	if (measure->layout) {
		g_object_unref(measure->layout);
		measure->layout = NULL;
	}
	if (measure->context) {
		g_object_unref(measure->context);
		measure->context = NULL;
	}
	if (measure->cr) {
		cairo_destroy(measure->cr);
		measure->cr = NULL;
	}
	if (measure->surface) {
		cairo_surface_destroy(measure->surface);
		measure->surface = NULL;
	}
}

static void measure_text(struct mango_text_measure *measure,
						 const char *font_desc, const char *text, float scale,
						 int32_t *out_pixel_w, int32_t *out_pixel_h) {
	if (scale <= 0.0f) {
		scale = 1.0f;
	}
	if (measure->scale != scale) {
		pango_cairo_context_set_resolution(measure->context, 96.0 * scale);
		measure->scale = scale;
	}

	layout_configure(measure->layout, font_desc, text);
	pango_layout_get_pixel_size(measure->layout, out_pixel_w, out_pixel_h);
}

static char *text_ellipsize(struct mango_text_measure *measure,
							const char *font_desc, const char *text,
							float scale, int32_t max_pixel_w) {
	int32_t pixel_w = 0, pixel_h = 0;
	int32_t best = 0;
	const char *end = text + strlen(text);

	measure_text(measure, font_desc, "…", scale, &pixel_w, &pixel_h);
	if (pixel_w > max_pixel_w) {
		return g_strdup("…");
	}

	for (const char *pos = g_utf8_next_char(text); pos <= end;
		 pos = g_utf8_next_char(pos)) {
		int32_t length = (int32_t)(pos - text);
		char *candidate = g_strdup_printf("%.*s…", length, text);
		measure_text(measure, font_desc, candidate, scale, &pixel_w, &pixel_h);
		g_free(candidate);
		if (pixel_w > max_pixel_w) {
			break;
		}
		best = length;
	}

	return g_strdup_printf("%.*s…", best, text);
}

static cairo_surface_t *
text_surface_create(struct mango_text_measure *measure, const char *font_desc,
					const char *text, float scale, const float color[4],
					int32_t *out_pixel_w, int32_t *out_pixel_h) {
	if (scale <= 0.0f) {
		scale = 1.0f;
	}

	int32_t width = 0, height = 0;
	measure_text(measure, font_desc, text, scale, &width, &height);
	*out_pixel_w = 0;
	*out_pixel_h = 0;
	if (width <= 0 || height <= 0) {
		return NULL;
	}

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	cairo_t *cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	PangoContext *context = pango_cairo_create_context(cr);
	pango_cairo_context_set_resolution(context, 96.0 * scale);
	PangoLayout *layout = pango_layout_new(context);
	layout_configure(layout, font_desc, text);
	cairo_move_to(cr, 0.0, 0.0);
	cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
	pango_cairo_show_layout(cr, layout);
	cairo_surface_flush(surface);

	g_object_unref(layout);
	g_object_unref(context);
	cairo_destroy(cr);

	*out_pixel_w = width;
	*out_pixel_h = height;
	return surface;
}

static void text_node_install(struct wlr_scene_buffer *scene_buffer,
							  struct mango_text_buffer **slot,
							  cairo_surface_t *surface, int32_t pixel_w,
							  int32_t pixel_h) {
	if (*slot) {
		wlr_buffer_drop(&(*slot)->base);
		*slot = NULL;
	}

	if (!surface) {
		wlr_scene_buffer_set_buffer(scene_buffer, NULL);
		wlr_scene_buffer_set_dest_size(scene_buffer, 0, 0);
		return;
	}

	struct mango_text_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf) {
		cairo_surface_destroy(surface);
		return;
	}
	wlr_buffer_init(&buf->base, &text_buffer_impl, pixel_w, pixel_h);
	buf->surface = surface;
	*slot = buf;

	wlr_scene_buffer_set_buffer(scene_buffer, &buf->base);
}

static void text_node_apply_size(struct wlr_scene_buffer *scene_buffer,
								 int32_t pixel_w, int32_t pixel_h, float scale,
								 int32_t max_logical_w, int32_t max_logical_h,
								 int32_t *out_logical_w,
								 int32_t *out_logical_h) {
	if (scale <= 0.0f) {
		scale = 1.0f;
	}

	int32_t clip_pixel_w = pixel_w;
	int32_t clip_pixel_h = pixel_h;

	if (max_logical_w > 0) {
		int32_t max_pixel_w = (int32_t)(max_logical_w * scale + 0.5f);
		if (max_pixel_w < clip_pixel_w) {
			clip_pixel_w = max_pixel_w;
		}
	}
	if (max_logical_h > 0) {
		int32_t max_pixel_h = (int32_t)(max_logical_h * scale + 0.5f);
		if (max_pixel_h < clip_pixel_h) {
			clip_pixel_h = max_pixel_h;
		}
	}
	if (clip_pixel_w < 0) {
		clip_pixel_w = 0;
	}
	if (clip_pixel_h < 0) {
		clip_pixel_h = 0;
	}

	if (pixel_w > 0 && (clip_pixel_w < pixel_w || clip_pixel_h < pixel_h)) {
		const struct wlr_fbox box = {
			.width = clip_pixel_w,
			.height = clip_pixel_h,
		};
		wlr_scene_buffer_set_source_box(scene_buffer, &box);
	} else {
		wlr_scene_buffer_set_source_box(scene_buffer, NULL);
	}

	int32_t logical_w = (int32_t)(clip_pixel_w / scale + 0.5f);
	int32_t logical_h = (int32_t)(clip_pixel_h / scale + 0.5f);
	wlr_scene_buffer_set_dest_size(scene_buffer, logical_w, logical_h);

	*out_logical_w = logical_w;
	*out_logical_h = logical_h;
}

static void text_node_clear(struct wlr_scene_buffer *scene_buffer,
							struct mango_text_buffer **slot,
							struct wlr_scene_rect *border,
							struct wlr_scene_rect *bg) {
	text_node_install(scene_buffer, slot, NULL, 0, 0);
	wlr_scene_buffer_set_source_box(scene_buffer, NULL);
	wlr_scene_rect_set_size(border, 0, 0);
	wlr_scene_rect_set_size(bg, 0, 0);
}

MangoJumpLabel *mango_jump_label_node_create(struct wlr_scene_tree *parent,
											 DecorateDrawData data) {
	MangoJumpLabel *node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;

	node->scene = wlr_scene_tree_create(parent);
	if (!node->scene) {
		free(node);
		return NULL;
	}

	node->border = wlr_scene_rect_create(node->scene, 0, 0, (float[4]){0});
	node->bg = wlr_scene_rect_create(node->scene, 0, 0, (float[4]){0});
	node->scene_buffer = wlr_scene_buffer_create(node->scene, NULL);
	if (!node->border || !node->bg || !node->scene_buffer) {
		wlr_scene_node_destroy(&node->scene->node);
		free(node);
		return NULL;
	}

	memcpy(node->fg_color, data.fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data.bg_color, sizeof(node->bg_color));
	memcpy(node->focus_fg_color, data.focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->focus_bg_color, data.focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->border_color, data.border_color, sizeof(node->border_color));
	node->border_width = data.border_width;
	node->corner_radius = data.corner_radius;
	node->padding_x = data.padding_x;
	node->padding_y = data.padding_y;
	node->font_desc =
		g_strdup(data.font_desc ? data.font_desc : "monospace Bold 16");

	node->cached_scale = -1.0f;
	node->scene_buffer->node.data = NULL;
	measure_init(&node->measure);

	return node;
}

void mango_jump_label_node_destroy(MangoJumpLabel *node) {
	if (!node)
		return;

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}
	if (node->scene) {
		wlr_scene_node_destroy(&node->scene->node);
		node->scene = NULL;
	}

	measure_finish(&node->measure);

	g_free(node->font_desc);
	g_free(node->cached_text);
	g_free(node->cached_font_desc);

	free(node);
}

static void jump_label_apply_geometry(MangoJumpLabel *node) {
	float scale = node->cached_scale > 0.0f ? node->cached_scale : 1.0f;

	text_node_apply_size(node->scene_buffer, node->surface_pixel_w,
						 node->surface_pixel_h, scale, 0, 0,
						 &node->text_logical_w, &node->text_logical_h);

	if (node->text_logical_w <= 0 || node->text_logical_h <= 0) {
		wlr_scene_rect_set_size(node->border, 0, 0);
		wlr_scene_rect_set_size(node->bg, 0, 0);
		node->logical_width = 0;
		node->logical_height = 0;
		return;
	}

	int32_t border = node->border_width > 0 ? node->border_width : 0;
	int32_t width = node->text_logical_w + 2 * node->padding_x + 2 * border;
	int32_t height = node->text_logical_h + 2 * node->padding_y + 2 * border;

	node->logical_width = width;
	node->logical_height = height;

	rect_apply(node->border, node->border_color, node->corner_radius + border,
			   0, 0, width, height);
	rect_apply(node->bg, node->focused ? node->focus_bg_color : node->bg_color,
			   node->corner_radius, border, border, width - 2 * border,
			   height - 2 * border);

	wlr_scene_node_set_position(&node->scene_buffer->node,
								border + node->padding_x,
								border + node->padding_y);
}

void mango_jump_label_node_update(MangoJumpLabel *node, const char *text,
								  float scale) {
	if (!node || !text)
		return;
	if (scale <= 0.0f) {
		scale = 1.0f;
	}

	const float *fg_color =
		node->focused ? node->focus_fg_color : node->fg_color;

	bool dirty =
		node->cached_scale != scale || !node->cached_text ||
		strcmp(node->cached_text, text) != 0 || !node->cached_font_desc ||
		strcmp(node->cached_font_desc, node->font_desc) != 0 ||
		memcmp(node->cached_fg_color, fg_color, sizeof(node->fg_color)) != 0 ||
		node->cached_focused != node->focused;

	if (dirty) {
		g_free(node->cached_text);
		node->cached_text = g_strdup(text);
		g_free(node->cached_font_desc);
		node->cached_font_desc = g_strdup(node->font_desc);
		node->cached_scale = scale;
		memcpy(node->cached_fg_color, fg_color, sizeof(node->cached_fg_color));
		node->cached_focused = node->focused;

		cairo_surface_t *surface = text_surface_create(
			&node->measure, node->font_desc, text, scale, fg_color,
			&node->surface_pixel_w, &node->surface_pixel_h);
		text_node_install(node->scene_buffer, &node->buffer, surface,
						  node->surface_pixel_w, node->surface_pixel_h);
	}

	jump_label_apply_geometry(node);
}

void mango_jump_label_node_set_focus(MangoJumpLabel *node, bool focused) {
	if (!node || node->focused == focused)
		return;
	node->focused = focused;
	if (node->cached_text) {
		mango_jump_label_node_update(
			node, node->cached_text,
			node->cached_scale > 0.0f ? node->cached_scale : 1.0f);
	}
}

void mango_jump_label_node_set_background(MangoJumpLabel *node, float r,
										  float g, float b, float a) {
	if (!node)
		return;
	node->bg_color[0] = r;
	node->bg_color[1] = g;
	node->bg_color[2] = b;
	node->bg_color[3] = a;
	if (node->cached_text) {
		mango_jump_label_node_update(
			node, node->cached_text,
			node->cached_scale > 0.0f ? node->cached_scale : 1.0f);
	}
}

void mango_jump_label_node_set_border(MangoJumpLabel *node, float r, float g,
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
	if (node->cached_text) {
		mango_jump_label_node_update(
			node, node->cached_text,
			node->cached_scale > 0.0f ? node->cached_scale : 1.0f);
	}
}

void mango_jump_label_node_set_padding(MangoJumpLabel *node, int32_t pad_x,
									   int32_t pad_y) {
	if (!node)
		return;
	node->padding_x = pad_x >= 0 ? pad_x : 0;
	node->padding_y = pad_y >= 0 ? pad_y : 0;
	if (node->cached_text) {
		mango_jump_label_node_update(
			node, node->cached_text,
			node->cached_scale > 0.0f ? node->cached_scale : 1.0f);
	}
}

MangoGroupBar *mango_group_bar_create(void *cdata, uint32_t type,
									  struct wlr_scene_tree *parent,
									  DecorateDrawData data, int32_t width,
									  int32_t height) {
	MangoGroupBar *mangobar = calloc(1, sizeof(*mangobar));
	if (!mangobar)
		return NULL;

	mangobar->scene = wlr_scene_tree_create(parent);
	if (!mangobar->scene) {
		free(mangobar);
		return NULL;
	}

	mangobar->border =
		wlr_scene_rect_create(mangobar->scene, 0, 0, (float[4]){0});
	mangobar->bg = wlr_scene_rect_create(mangobar->scene, 0, 0, (float[4]){0});
	mangobar->scene_buffer = wlr_scene_buffer_create(mangobar->scene, NULL);
	if (!mangobar->border || !mangobar->bg || !mangobar->scene_buffer) {
		wlr_scene_node_destroy(&mangobar->scene->node);
		free(mangobar);
		return NULL;
	}

	memcpy(mangobar->fg_color, data.fg_color, sizeof(mangobar->fg_color));
	memcpy(mangobar->bg_color, data.bg_color, sizeof(mangobar->bg_color));
	memcpy(mangobar->focus_fg_color, data.focus_fg_color,
		   sizeof(mangobar->focus_fg_color));
	memcpy(mangobar->focus_bg_color, data.focus_bg_color,
		   sizeof(mangobar->focus_bg_color));
	memcpy(mangobar->border_color, data.border_color,
		   sizeof(mangobar->border_color));
	mangobar->border_width = data.border_width;
	mangobar->corner_radius = data.corner_radius;
	mangobar->padding_x = data.padding_x;
	mangobar->padding_y = data.padding_y;
	mangobar->font_desc =
		g_strdup(data.font_desc ? data.font_desc : "monospace Bold 16");

	mangobar->target_width = width;
	mangobar->target_height = height;
	mangobar->type = type;
	mangobar->node_data = cdata;

	mangobar->cached_scale = -1.0f;

	mangobar->scene->node.data = mangobar;
	measure_init(&mangobar->measure);

	return mangobar;
}

void mango_group_bar_destroy(MangoGroupBar *node) {
	if (!node)
		return;

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}
	if (node->scene) {
		wlr_scene_node_destroy(&node->scene->node);
		node->scene = NULL;
	}

	measure_finish(&node->measure);

	g_free(node->font_desc);
	g_free(node->cached_text);
	g_free(node->cached_font_desc);
	g_free(node->last_text);
	free(node);
}

static int32_t group_bar_avail_width(MangoGroupBar *node) {
	int32_t border = node->border_width > 0 ? node->border_width : 0;
	int32_t avail = node->target_width - 2 * border - 2 * node->padding_x;
	return avail > 0 ? avail : 0;
}

static void group_bar_apply_geometry(MangoGroupBar *node) {
	int32_t border = node->border_width > 0 ? node->border_width : 0;
	int32_t width = node->target_width > 0 ? node->target_width : 0;
	int32_t height = node->target_height > 0 ? node->target_height : 0;
	int32_t inner_w = width - 2 * border;
	int32_t inner_h = height - 2 * border;
	int32_t avail_w = group_bar_avail_width(node);
	int32_t avail_h = inner_h - 2 * node->padding_y;

	if (avail_w <= 0 || avail_h <= 0) {
		text_node_clear(node->scene_buffer, &node->buffer, node->border,
						node->bg);
		node->cached_scale = -1.0f;
		node->cached_clip_pixel_w = -1;
		node->text_logical_w = 0;
		node->text_logical_h = 0;
		node->logical_width = 0;
		node->logical_height = 0;
		return;
	}

	rect_apply(node->border, node->border_color, node->corner_radius + border,
			   0, 0, width, height);
	rect_apply(node->bg, node->focused ? node->focus_bg_color : node->bg_color,
			   node->corner_radius, border, border, inner_w, inner_h);

	node->logical_width = width;
	node->logical_height = height;

	float scale = node->cached_scale > 0.0f ? node->cached_scale : 1.0f;
	text_node_apply_size(node->scene_buffer, node->surface_pixel_w,
						 node->surface_pixel_h, scale, 0, avail_h,
						 &node->text_logical_w, &node->text_logical_h);

	int32_t offset_x = (avail_w - node->text_logical_w) / 2;
	int32_t offset_y = (avail_h - node->text_logical_h) / 2;
	if (offset_x < 0) {
		offset_x = 0;
	}
	if (offset_y < 0) {
		offset_y = 0;
	}

	wlr_scene_node_set_position(&node->scene_buffer->node,
								border + node->padding_x + offset_x,
								border + node->padding_y + offset_y);
}

void mango_group_bar_set_size(MangoGroupBar *node, int32_t width,
							  int32_t height) {
	if (!node)
		return;

	if (width < 0)
		width = 0;
	if (height < 0)
		height = 0;

	if (node->target_width == width && node->target_height == height)
		return;

	node->target_width = width;
	node->target_height = height;

	const char *redraw_text = node->last_text ? node->last_text : "";
	float redraw_scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;

	mango_group_bar_update(node, redraw_text, redraw_scale);
}

void mango_group_bar_update(MangoGroupBar *node, const char *text,
							float scale) {
	if (!node || !text)
		return;
	if (scale <= 0.0f) {
		scale = 1.0f;
	}

	if (!node->last_text || strcmp(node->last_text, text) != 0) {
		g_free(node->last_text);
		node->last_text = g_strdup(text);
	}
	node->last_scale = scale;

	const float *fg_color =
		node->focused ? node->focus_fg_color : node->fg_color;

	int32_t avail_w = group_bar_avail_width(node);
	int32_t avail_pixel_w = (int32_t)(avail_w * scale + 0.5f);

	int32_t natural_pixel_w = 0, natural_pixel_h = 0;
	measure_text(&node->measure, node->font_desc, text, scale,
				 &natural_pixel_w, &natural_pixel_h);

	bool overlong = avail_pixel_w > 0 && natural_pixel_w > avail_pixel_w;
	int32_t clip_pixel_w = overlong ? avail_pixel_w : 0;

	bool dirty =
		node->cached_scale != scale || !node->cached_text ||
		strcmp(node->cached_text, text) != 0 || !node->cached_font_desc ||
		strcmp(node->cached_font_desc, node->font_desc) != 0 ||
		memcmp(node->cached_fg_color, fg_color, sizeof(node->fg_color)) != 0 ||
		node->cached_focused != node->focused ||
		node->cached_clip_pixel_w != clip_pixel_w;

	if (dirty) {
		char *ellipsized = NULL;
		const char *display = text;
		if (overlong) {
			ellipsized = text_ellipsize(&node->measure, node->font_desc, text,
										scale, avail_pixel_w);
			display = ellipsized;
		}

		g_free(node->cached_text);
		node->cached_text = g_strdup(text);
		g_free(node->cached_font_desc);
		node->cached_font_desc = g_strdup(node->font_desc);
		node->cached_scale = scale;
		memcpy(node->cached_fg_color, fg_color, sizeof(node->cached_fg_color));
		node->cached_focused = node->focused;
		node->cached_clip_pixel_w = clip_pixel_w;

		cairo_surface_t *surface = text_surface_create(
			&node->measure, node->font_desc, display, scale, fg_color,
			&node->surface_pixel_w, &node->surface_pixel_h);
		text_node_install(node->scene_buffer, &node->buffer, surface,
						  node->surface_pixel_w, node->surface_pixel_h);
		g_free(ellipsized);
	}

	group_bar_apply_geometry(node);
}

void mango_group_bar_set_focus(MangoGroupBar *node, bool focused) {
	if (!node || node->focused == focused)
		return;
	node->focused = focused;
	if (node->last_text) {
		float scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;
		mango_group_bar_update(node, node->last_text, scale);
	}
}

void mango_group_bar_set_colors(MangoGroupBar *node, const float fg[4],
								const float bg[4]) {
	if (!node)
		return;

	memcpy(node->fg_color, fg, sizeof(node->fg_color));
	memcpy(node->bg_color, bg, sizeof(node->bg_color));

	if (node->last_text) {
		float scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;
		mango_group_bar_update(node, node->last_text, scale);
	}
}

void mango_jump_label_node_apply_config(MangoJumpLabel *node,
										const DecorateDrawData *data) {
	if (!node || !data)
		return;

	memcpy(node->fg_color, data->fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data->bg_color, sizeof(node->bg_color));
	memcpy(node->focus_fg_color, data->focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->focus_bg_color, data->focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->border_color, data->border_color, sizeof(node->border_color));
	node->border_width = data->border_width;
	node->corner_radius = data->corner_radius;
	node->padding_x = data->padding_x;
	node->padding_y = data->padding_y;

	g_free(node->font_desc);
	node->font_desc =
		g_strdup(data->font_desc ? data->font_desc : "monospace Bold 16");

	if (node->cached_text) {
		mango_jump_label_node_update(
			node, node->cached_text,
			node->cached_scale > 0.0f ? node->cached_scale : 1.0f);
	}
}

void mango_group_bar_apply_config(MangoGroupBar *node,
								  const DecorateDrawData *data) {
	if (!node || !data)
		return;

	memcpy(node->fg_color, data->fg_color, sizeof(node->fg_color));
	memcpy(node->bg_color, data->bg_color, sizeof(node->bg_color));
	memcpy(node->focus_fg_color, data->focus_fg_color,
		   sizeof(node->focus_fg_color));
	memcpy(node->focus_bg_color, data->focus_bg_color,
		   sizeof(node->focus_bg_color));
	memcpy(node->border_color, data->border_color, sizeof(node->border_color));
	node->border_width = data->border_width;
	node->corner_radius = data->corner_radius;
	node->padding_x = data->padding_x;
	node->padding_y = data->padding_y;

	g_free(node->font_desc);
	node->font_desc =
		g_strdup(data->font_desc ? data->font_desc : "monospace Bold 16");

	if (node->last_text) {
		float scale = node->last_scale > 0.0f ? node->last_scale : 1.0f;
		mango_group_bar_update(node, node->last_text, scale);
	}
}
