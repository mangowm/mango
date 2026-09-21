#include "mango/draw/dim-node.h"

#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/interfaces/wlr_buffer.h>

#if defined(__has_include) && __has_include(<scenefx/types/fx/clipped_region.h>)
#include <scenefx/types/fx/clipped_region.h>
#define DIM_NODE_CORNER_RADIUS 1
#else
#define DIM_NODE_CORNER_RADIUS 0
#endif

static void dim_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct mango_dim_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	if (buf->surface) {
		cairo_surface_destroy(buf->surface);
	}
	free(buf);
}

static bool dim_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
											 uint32_t flags, void **data,
											 uint32_t *format, size_t *stride) {
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}

	struct mango_dim_buffer *buf = wl_container_of(wlr_buffer, buf, base);
	if (!buf->surface) {
		return false;
	}
	*data = cairo_image_surface_get_data(buf->surface);
	*format = DRM_FORMAT_ARGB8888;
	*stride = cairo_image_surface_get_stride(buf->surface);
	return true;
}

static void dim_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {}

static const struct wlr_buffer_impl dim_buffer_impl = {
	.destroy = dim_buffer_destroy,
	.begin_data_ptr_access = dim_buffer_begin_data_ptr_access,
	.end_data_ptr_access = dim_buffer_end_data_ptr_access,
};

static void dim_node_build_pixel(MangoDimNode *node) {
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return;
	}

	cairo_t *cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, node->color[0], node->color[1], node->color[2],
						  node->color[3]);
	cairo_paint(cr);
	cairo_surface_flush(surface);
	cairo_destroy(cr);

	struct mango_dim_buffer *buf = calloc(1, sizeof(*buf));
	if (!buf) {
		cairo_surface_destroy(surface);
		return;
	}
	wlr_buffer_init(&buf->base, &dim_buffer_impl, 1, 1);
	buf->surface = surface;

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
	}
	node->buffer = buf;

	wlr_scene_buffer_set_buffer(node->scene_buffer, &buf->base);
}

static bool dim_node_point_accepts_input(struct wlr_scene_buffer *buffer,
										 double *sx, double *sy) {
	return false;
}

MangoDimNode *mango_dim_node_create(struct wlr_scene_tree *parent,
									const float color[4]) {
	if (!parent) {
		return NULL;
	}

	MangoDimNode *node = calloc(1, sizeof(*node));
	if (!node) {
		return NULL;
	}

	node->scene_buffer = wlr_scene_buffer_create(parent, NULL);
	if (!node->scene_buffer) {
		free(node);
		return NULL;
	}

	if (color) {
		memcpy(node->color, color, sizeof(node->color));
	} else {
		node->color[0] = 0.0f;
		node->color[1] = 0.0f;
		node->color[2] = 0.0f;
		node->color[3] = 0x55 / 255.0f;
	}

	node->scene_buffer->point_accepts_input = dim_node_point_accepts_input;

	dim_node_build_pixel(node);
	wlr_scene_node_set_position(&node->scene_buffer->node, 0, 0);

	return node;
}

void mango_dim_node_destroy(MangoDimNode *node) {
	if (!node) {
		return;
	}

	if (node->buffer) {
		wlr_buffer_drop(&node->buffer->base);
		node->buffer = NULL;
	}
	if (node->scene_buffer) {
		wlr_scene_node_destroy(&node->scene_buffer->node);
		node->scene_buffer = NULL;
	}

	free(node);
}

void mango_dim_node_set_color(MangoDimNode *node, const float color[4]) {
	if (!node || !color) {
		return;
	}

	bool color_changed =
		!node->buffer || memcmp(node->color, color, sizeof(node->color)) != 0;
	memcpy(node->color, color, sizeof(node->color));

	if (color_changed) {
		dim_node_build_pixel(node);
	}
}

void mango_dim_node_set_radius(MangoDimNode *node, int32_t radius) {
#if DIM_NODE_CORNER_RADIUS
	if (!node) {
		return;
	}

	if (radius < 0) {
		radius = 0;
	}
	wlr_scene_buffer_set_corner_radii(node->scene_buffer,
									  corner_radii_all(radius));
#endif
}

void mango_dim_node_set_box(MangoDimNode *node, int32_t x, int32_t y,
							int32_t width, int32_t height) {
	if (!node) {
		return;
	}
	if (width < 0) {
		width = 0;
	}
	if (height < 0) {
		height = 0;
	}

	if (node->scene_buffer->node.x != x || node->scene_buffer->node.y != y) {
		wlr_scene_node_set_position(&node->scene_buffer->node, x, y);
	}

	wlr_scene_buffer_set_dest_size(node->scene_buffer, width, height);
}

void mango_dim_node_set_enabled(MangoDimNode *node, bool enabled) {
	if (!node) {
		return;
	}
	wlr_scene_node_set_enabled(&node->scene_buffer->node, enabled);
}
