#include "mango/animation/common.h"
#include "mango/common/scene_node.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/parse.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_subcompositor.h>

struct dvec2 calculate_animation_curve_at(double t, int32_t type) {
	struct dvec2 point;
	double *animation_curve;
	if (type == MOVE) {
		animation_curve = config.animation_curve_move;
	} else if (type == OPEN) {
		animation_curve = config.animation_curve_open;
	} else if (type == TAG) {
		animation_curve = config.animation_curve_tag;
	} else if (type == CLOSE) {
		animation_curve = config.animation_curve_close;
	} else if (type == FOCUS) {
		animation_curve = config.animation_curve_focus;
	} else if (type == OPAFADEIN) {
		animation_curve = config.animation_curve_opafadein;
	} else if (type == OPAFADEOUT) {
		animation_curve = config.animation_curve_opafadeout;
	} else {
		animation_curve = config.animation_curve_move;
	}

	point.x = 3 * t * (1 - t) * (1 - t) * animation_curve[0] +
			  3 * t * t * (1 - t) * animation_curve[2] + t * t * t;

	point.y = 3 * t * (1 - t) * (1 - t) * animation_curve[1] +
			  3 * t * t * (1 - t) * animation_curve[3] + t * t * t;

	return point;
}
void handle_snapshot_meta_destroy(struct wl_listener *listener, void *data) {
	SnapshotMetadata *meta = wl_container_of(listener, meta, destroy);
	wl_list_remove(&meta->destroy.link);
	free(meta);
}
void init_baked_points(void) {
	server.baked_points_move =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_move));
	server.baked_points_open =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_open));
	server.baked_points_tag =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_tag));
	server.baked_points_close =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_close));
	server.baked_points_focus =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_focus));
	server.baked_points_opafadein =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_opafadein));
	server.baked_points_opafadeout =
		calloc(BAKED_POINTS_COUNT, sizeof(*server.baked_points_opafadeout));

	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_move[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), MOVE);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_open[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), OPEN);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_tag[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), TAG);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_close[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), CLOSE);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_focus[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), FOCUS);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_opafadein[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), OPAFADEIN);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		server.baked_points_opafadeout[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), OPAFADEOUT);
	}
}
double find_animation_curve_at(double t, int32_t type) {
	int32_t down = 0;
	int32_t up = BAKED_POINTS_COUNT - 1;

	int32_t middle = (up + down) / 2;
	struct dvec2 *baked_points;
	if (type == MOVE) {
		baked_points = server.baked_points_move;
	} else if (type == OPEN) {
		baked_points = server.baked_points_open;
	} else if (type == TAG) {
		baked_points = server.baked_points_tag;
	} else if (type == CLOSE) {
		baked_points = server.baked_points_close;
	} else if (type == FOCUS) {
		baked_points = server.baked_points_focus;
	} else if (type == OPAFADEIN) {
		baked_points = server.baked_points_opafadein;
	} else if (type == OPAFADEOUT) {
		baked_points = server.baked_points_opafadeout;
	} else {
		baked_points = server.baked_points_move;
	}

	while (up - down != 1) {
		if (baked_points[middle].x <= t) {
			down = middle;
		} else {
			up = middle;
		}
		middle = (up + down) / 2;
	}
	return baked_points[up].y;
}
bool scene_node_snapshot(struct wlr_scene_node *node, int32_t lx, int32_t ly,
						 struct wlr_scene_tree *snapshot_tree) {
	if (!node->enabled && node->type != WLR_SCENE_NODE_TREE) {
		return true;
	}

	lx += node->x;
	ly += node->y;

	struct wlr_scene_node *snapshot_node = NULL;
	switch (node->type) {
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);

		struct wlr_scene_node *child;
		wl_list_for_each(child, &scene_tree->children, link) {
			scene_node_snapshot(child, lx, ly, snapshot_tree);
		}
		break;
	}
	case WLR_SCENE_NODE_RECT: {
		// struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);

		// struct wlr_scene_rect *snapshot_rect =
		// 	wlr_scene_rect_create(snapshot_tree, scene_rect->width,
		// 						  scene_rect->height, scene_rect->color);
		// snapshot_rect->node.data = scene_rect->node.data;
		// if (snapshot_rect == NULL) {
		// 	return false;
		// }

		// wlr_scene_rect_set_clipped_region(scene_rect,
		// 								  snapshot_rect->clipped_region);
		// wlr_scene_rect_set_backdrop_blur(scene_rect, false);
		// wlr_scene_rect_set_backdrop_blur_optimized(
		// 	scene_rect, snapshot_rect->backdrop_blur_optimized);
		// wlr_scene_rect_set_corner_radius(
		// 	scene_rect, snapshot_rect->corner_radius, snapshot_rect->corners);
		// wlr_scene_rect_set_color(scene_rect, snapshot_rect->color);

		// snapshot_node = &snapshot_rect->node;
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *scene_buffer =
			wlr_scene_buffer_from_node(node);

		// Creates an intermediate wrapper tree node.
		struct wlr_scene_tree *wrapper = wlr_scene_tree_create(snapshot_tree);
		if (wrapper == NULL) {
			return false;
		}
		snapshot_node = &wrapper->node; // Applies the coordinate offset to the
										// outer wrapper box.

		// Collects surface state and stores it as metadata.
		SnapshotMetadata *meta = calloc(1, sizeof(SnapshotMetadata));
		if (meta == NULL) {
			wlr_scene_node_destroy(&wrapper->node);
			return false;
		}
		meta->orig_width = scene_buffer->dst_width;
		meta->orig_height = scene_buffer->dst_height;
		meta->orig_src_box = scene_buffer->src_box;
		meta->orig_x = 0;
		meta->orig_y = 0;
		meta->type = Snapshot;

		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(scene_buffer);
		if (scene_surface != NULL) {
			meta->is_subsurface =
				!!wlr_subsurface_try_from_wlr_surface(scene_surface->surface);
		}

		// bind a destruction callback listener to free memory when the wrapper
		// node is destroyed
		meta->destroy.notify = handle_snapshot_meta_destroy;
		wl_signal_add(&wrapper->node.events.destroy, &meta->destroy);
		mango_scene_node_set(&wrapper->node, Snapshot, meta);

		// attach the real buffer underneath the wrapper (relative coordinates
		// 0,0)
		struct wlr_scene_buffer *snapshot_buffer =
			wlr_scene_buffer_create(wrapper, NULL);
		if (snapshot_buffer == NULL) {
			wlr_scene_node_destroy(&wrapper->node);
			return false;
		}

		/*
		 * Do not copy the source node's payload onto the snapshot buffer. A
		 * MangoSceneNode is owned by (and freed together with) its original
		 * node, so aliasing it here would dangle once that node is destroyed
		 * and would make an ancestor lookup resolve to the wrong root. The
		 * wrapping tree already carries its own Snapshot payload, so walking
		 * up from this buffer still resolves correctly.
		 */

		wlr_scene_buffer_set_dest_size(snapshot_buffer, scene_buffer->dst_width,
									   scene_buffer->dst_height);
		wlr_scene_buffer_set_opaque_region(snapshot_buffer,
										   &scene_buffer->opaque_region);
		wlr_scene_buffer_set_source_box(snapshot_buffer,
										&scene_buffer->src_box);
		wlr_scene_buffer_set_transform(snapshot_buffer,
									   scene_buffer->transform);
		wlr_scene_buffer_set_filter_mode(snapshot_buffer,
										 scene_buffer->filter_mode);

		// Effects
		wlr_scene_buffer_set_opacity(snapshot_buffer, scene_buffer->opacity);
		wlr_scene_buffer_set_corner_radii(snapshot_buffer,
										  scene_buffer->corners);

		if (scene_surface != NULL && scene_surface->surface->buffer != NULL) {
			wlr_scene_buffer_set_buffer(snapshot_buffer,
										&scene_surface->surface->buffer->base);
		} else {
			wlr_scene_buffer_set_buffer(snapshot_buffer, scene_buffer->buffer);
		}
		break;
	}
	case WLR_SCENE_NODE_SHADOW: {
		struct wlr_scene_shadow *scene_shadow =
			wlr_scene_shadow_from_node(node);

		struct wlr_scene_shadow *snapshot_shadow = wlr_scene_shadow_create(
			snapshot_tree, scene_shadow->width, scene_shadow->height,
			scene_shadow->corner_radius, scene_shadow->blur_sigma,
			scene_shadow->color);
		if (snapshot_shadow == NULL) {
			return false;
		}
		snapshot_node = &snapshot_shadow->node;

		wlr_scene_shadow_set_clipped_region(snapshot_shadow,
											scene_shadow->clipped_region);

		wlr_scene_node_set_enabled(&snapshot_shadow->node, false);

		break;
	}
	case WLR_SCENE_NODE_BLUR:
		break;
	case WLR_SCENE_NODE_OPTIMIZED_BLUR:
		return true;
	}

	if (snapshot_node != NULL) {
		wlr_scene_node_set_position(snapshot_node, lx, ly);
	}

	return true;
}
struct wlr_scene_tree *wlr_scene_tree_snapshot(struct wlr_scene_node *node,
											   struct wlr_scene_tree *parent) {
	struct wlr_scene_tree *snapshot = wlr_scene_tree_create(parent);
	if (snapshot == NULL) {
		return NULL;
	}

	// Disable and enable the snapshot tree like so to atomically update
	// the scene-graph. This will prevent over-damaging or other weirdness.
	wlr_scene_node_set_enabled(&snapshot->node, false);

	if (!scene_node_snapshot(node, 0, 0, snapshot)) {
		wlr_scene_node_destroy(&snapshot->node);
		return NULL;
	}

	wlr_scene_node_set_enabled(&snapshot->node, true);

	return snapshot;
}

static void snapshot_dest_to_source(enum wl_output_transform transform, float x,
									float y, float *src_x, float *src_y) {
	switch (transform) {
	case WL_OUTPUT_TRANSFORM_90:
		*src_x = 1.0f - y;
		*src_y = x;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		*src_x = 1.0f - x;
		*src_y = 1.0f - y;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		*src_x = y;
		*src_y = 1.0f - x;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		*src_x = 1.0f - x;
		*src_y = y;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		*src_x = y;
		*src_y = x;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		*src_x = x;
		*src_y = 1.0f - y;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		*src_x = 1.0f - y;
		*src_y = 1.0f - x;
		break;
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		*src_x = x;
		*src_y = y;
		break;
	}
}

struct SnapshotClipData {
	struct wlr_box bounds;
	int32_t width;
	int32_t height;
	bool scale;
};

static void scene_snapshot_enable_buffers(struct wlr_scene_node *node) {
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		if (!node->enabled) {
			wlr_scene_node_set_enabled(node, true);
		}
		return;
	}
	if (node->type != WLR_SCENE_NODE_TREE) {
		return;
	}

	struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
	struct wlr_scene_node *child;
	wl_list_for_each(child, &tree->children, link) {
		scene_snapshot_enable_buffers(child);
	}
}

static void scene_snapshot_clip_buffer(struct wlr_scene_buffer *buffer,
									   int32_t sx, int32_t sy, void *data) {
	struct SnapshotClipData *clip = data;
	MangoSceneNode *snapshot_node = mango_scene_node_find(&buffer->node);
	if (!snapshot_node || snapshot_node->type != Snapshot ||
		!snapshot_node->owner) {
		return;
	}

	SnapshotMetadata *meta = snapshot_node->owner;
	int32_t child_x = buffer->node.x;
	int32_t child_y = buffer->node.y;
	int32_t base_x = sx - child_x + meta->orig_x;
	int32_t base_y = sy - child_y + meta->orig_y;
	int32_t dest_width = meta->orig_width;
	int32_t dest_height = meta->orig_height;

	if (clip->scale) {
		dest_width = clip->width;
		dest_height = clip->height;
	}
	if (dest_width <= 0 || dest_height <= 0) {
		dest_width = buffer->WLR_PRIVATE.buffer_width;
		dest_height = buffer->WLR_PRIVATE.buffer_height;
		if (buffer->transform & WL_OUTPUT_TRANSFORM_90) {
			int32_t tmp = dest_width;
			dest_width = dest_height;
			dest_height = tmp;
		}
	}

	wlr_scene_node_set_position(&buffer->node, meta->orig_x, meta->orig_y);
	if (dest_width <= 0 || dest_height <= 0) {
		wlr_scene_node_set_enabled(&buffer->node, false);
		return;
	}

	struct wlr_box buffer_box = {
		.x = base_x,
		.y = base_y,
		.width = dest_width,
		.height = dest_height,
	};
	struct wlr_box visible;
	if (!wlr_box_intersection(&visible, &buffer_box, &clip->bounds)) {
		wlr_scene_node_set_enabled(&buffer->node, false);
		return;
	}

	wlr_scene_node_set_enabled(&buffer->node, true);
	int32_t left = visible.x - buffer_box.x;
	int32_t top = visible.y - buffer_box.y;
	wlr_scene_node_set_position(&buffer->node, meta->orig_x + left,
								meta->orig_y + top);
	wlr_scene_buffer_set_dest_size(buffer, visible.width, visible.height);

	struct wlr_fbox src_box = meta->orig_src_box;
	int32_t src_width = buffer->WLR_PRIVATE.buffer_width;
	int32_t src_height = buffer->WLR_PRIVATE.buffer_height;
	if (!wlr_fbox_empty(&src_box)) {
		src_width = src_box.width;
		src_height = src_box.height;
	} else {
		src_box.x = 0;
		src_box.y = 0;
		src_box.width = src_width;
		src_box.height = src_height;
	}

	if (left == 0 && top == 0 && visible.width == dest_width &&
		visible.height == dest_height) {
		wlr_scene_buffer_set_source_box(
			buffer,
			wlr_fbox_empty(&meta->orig_src_box) ? NULL : &meta->orig_src_box);
		return;
	}

	float nx0 = (float)left / (float)dest_width;
	float ny0 = (float)top / (float)dest_height;
	float nx1 = (float)(left + visible.width) / (float)dest_width;
	float ny1 = (float)(top + visible.height) / (float)dest_height;
	float ax, ay, bx, by;
	snapshot_dest_to_source(buffer->transform, nx0, ny0, &ax, &ay);
	snapshot_dest_to_source(buffer->transform, nx1, ny1, &bx, &by);

	float sx0 = MANGO_MIN(ax, bx);
	float sy0 = MANGO_MIN(ay, by);
	float sx1 = MANGO_MAX(ax, bx);
	float sy1 = MANGO_MAX(ay, by);
	struct wlr_fbox clipped_src = {
		.x = src_box.x + sx0 * src_width,
		.y = src_box.y + sy0 * src_height,
		.width = (sx1 - sx0) * src_width,
		.height = (sy1 - sy0) * src_height,
	};
	wlr_scene_buffer_set_source_box(buffer, &clipped_src);
}

void scene_snapshot_clip_to_box(struct wlr_scene_tree *snapshot,
								const struct wlr_box *bounds, bool scale,
								int32_t width, int32_t height) {
	if (!snapshot || !bounds || bounds->width <= 0 || bounds->height <= 0) {
		return;
	}

	struct SnapshotClipData clip = {
		.bounds = *bounds,
		.width = width,
		.height = height,
		.scale = scale,
	};
	scene_snapshot_enable_buffers(&snapshot->node);
	wlr_scene_node_for_each_buffer(&snapshot->node, scene_snapshot_clip_buffer,
								   &clip);
}

void request_fresh_all_monitors(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		wlr_output_schedule_frame(m->wlr_output);
	}
}
