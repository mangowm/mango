#include "mango/animation/common.h"
#include "mango/common/scene_node.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/parse.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include <math.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_subcompositor.h>

static struct dvec2 calculate_bezier_curve_for(double t,
											   const double curve[4]) {
	struct dvec2 point;
	point.x = 3 * t * (1 - t) * (1 - t) * curve[0] +
			  3 * t * t * (1 - t) * curve[2] + t * t * t;

	point.y = 3 * t * (1 - t) * (1 - t) * curve[1] +
			  3 * t * t * (1 - t) * curve[3] + t * t * t;
	return point;
}

static const AnimationCurve *animation_curve_for_type(int32_t type) {
	if (type == OPEN)
		return &config.animation_curve_open;
	if (type == TAG)
		return &config.animation_curve_tag;
	if (type == CLOSE)
		return &config.animation_curve_close;
	if (type == FOCUS)
		return &config.animation_curve_focus;
	if (type == OPAFADEIN)
		return &config.animation_curve_opafadein;
	if (type == OPAFADEOUT)
		return &config.animation_curve_opafadeout;
	return &config.animation_curve_move;
}

static double apply_named_easing(enum animation_easing easing, double t);

double animation_curve_ease(const AnimationCurve *curve, double progress) {
	if (!curve)
		return fmax(0.0, fmin(1.0, progress));
	progress = fmax(0.0, fmin(1.0, progress));
	if (curve->type == ANIMATION_CURVE_NAMED)
		return apply_named_easing(curve->easing, progress);

	const double *bezier = curve->bezier;
	double low = 0.0;
	double high = 1.0;
	for (int i = 0; i < 24; i++) {
		double mid = (low + high) / 2.0;
		if (calculate_bezier_curve_for(mid, bezier).x < progress)
			low = mid;
		else
			high = mid;
	}
	return calculate_bezier_curve_for((low + high) / 2.0, bezier).y;
}

uint32_t animation_curve_duration_ms(const AnimationCurve *curve,
									 uint32_t fallback_ms) {
	return fallback_ms;
}

uint32_t animation_duration_for_type(int32_t type, uint32_t fallback_ms) {
	return animation_curve_duration_ms(animation_curve_for_type(type),
									   fallback_ms);
}

static double ease_out_bounce(double t) {
	const double n1 = 7.5625;
	const double d1 = 2.75;
	if (t < 1.0 / d1)
		return n1 * t * t;
	if (t < 2.0 / d1) {
		t -= 1.5 / d1;
		return n1 * t * t + 0.75;
	}
	if (t < 2.5 / d1) {
		t -= 2.25 / d1;
		return n1 * t * t + 0.9375;
	}
	t -= 2.625 / d1;
	return n1 * t * t + 0.984375;
}

static double apply_named_easing(enum animation_easing easing, double t) {
	const double pi = acos(-1.0);
	switch (easing) {
	case ANIMATION_EASING_LINEAR:
		return t;
	case ANIMATION_EASING_EASE_IN_SINE:
		return 1.0 - cos(t * pi / 2.0);
	case ANIMATION_EASING_EASE_OUT_SINE:
		return sin(t * pi / 2.0);
	case ANIMATION_EASING_EASE_IN_OUT_SINE:
		return -(cos(pi * t) - 1.0) / 2.0;
	case ANIMATION_EASING_EASE_IN_QUAD:
		return t * t;
	case ANIMATION_EASING_EASE_OUT_QUAD:
		return 1.0 - (1.0 - t) * (1.0 - t);
	case ANIMATION_EASING_EASE_IN_OUT_QUAD:
		return t < 0.5 ? 2.0 * t * t : 1.0 - pow(-2.0 * t + 2.0, 2) / 2.0;
	case ANIMATION_EASING_EASE_IN_CUBIC:
		return t * t * t;
	case ANIMATION_EASING_EASE_OUT_CUBIC:
		return 1.0 - pow(1.0 - t, 3);
	case ANIMATION_EASING_EASE_IN_OUT_CUBIC:
		return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3) / 2.0;
	case ANIMATION_EASING_EASE_IN_QUART:
		return pow(t, 4);
	case ANIMATION_EASING_EASE_OUT_QUART:
		return 1.0 - pow(1.0 - t, 4);
	case ANIMATION_EASING_EASE_IN_OUT_QUART:
		return t < 0.5 ? 8.0 * pow(t, 4) : 1.0 - pow(-2.0 * t + 2.0, 4) / 2.0;
	case ANIMATION_EASING_EASE_IN_QUINT:
		return pow(t, 5);
	case ANIMATION_EASING_EASE_OUT_QUINT:
		return 1.0 - pow(1.0 - t, 5);
	case ANIMATION_EASING_EASE_IN_OUT_QUINT:
		return t < 0.5 ? 16.0 * pow(t, 5) : 1.0 - pow(-2.0 * t + 2.0, 5) / 2.0;
	case ANIMATION_EASING_EASE_IN_EXPO:
		return t <= 0.0 ? 0.0 : pow(2.0, 10.0 * t - 10.0);
	case ANIMATION_EASING_EASE_OUT_EXPO:
		return t >= 1.0 ? 1.0 : 1.0 - pow(2.0, -10.0 * t);
	case ANIMATION_EASING_EASE_IN_OUT_EXPO:
		if (t <= 0.0)
			return 0.0;
		if (t >= 1.0)
			return 1.0;
		return t < 0.5 ? pow(2.0, 20.0 * t - 10.0) / 2.0
					   : (2.0 - pow(2.0, -20.0 * t + 10.0)) / 2.0;
	case ANIMATION_EASING_EASE_IN_CIRC:
		return 1.0 - sqrt(1.0 - pow(t, 2));
	case ANIMATION_EASING_EASE_OUT_CIRC:
		return sqrt(1.0 - pow(t - 1.0, 2));
	case ANIMATION_EASING_EASE_IN_OUT_CIRC:
		return t < 0.5 ? (1.0 - sqrt(1.0 - pow(2.0 * t, 2))) / 2.0
					   : (sqrt(1.0 - pow(-2.0 * t + 2.0, 2)) + 1.0) / 2.0;
	case ANIMATION_EASING_EASE_IN_BACK: {
		const double c1 = 1.70158;
		const double c3 = c1 + 1.0;
		return c3 * t * t * t - c1 * t * t;
	}
	case ANIMATION_EASING_EASE_OUT_BACK: {
		const double c1 = 1.70158;
		const double c3 = c1 + 1.0;
		const double x = t - 1.0;
		return 1.0 + c3 * x * x * x + c1 * x * x;
	}
	case ANIMATION_EASING_EASE_IN_OUT_BACK: {
		const double c1 = 1.70158 * 1.525;
		const double c2 = c1 + 1.0;
		return t < 0.5 ? (pow(2.0 * t, 2) * ((c2 + 1.0) * 2.0 * t - c2)) / 2.0
					   : (pow(2.0 * t - 2.0, 2) *
							  ((c2 + 1.0) * (2.0 * t - 2.0) + c2) +
						  2.0) /
							 2.0;
	}
	case ANIMATION_EASING_EASE_IN_ELASTIC: {
		const double c4 = 2.0 * pi / 3.0;
		if (t <= 0.0)
			return 0.0;
		if (t >= 1.0)
			return 1.0;
		return -pow(2.0, 10.0 * t - 10.0) * sin((t * 10.0 - 10.75) * c4);
	}
	case ANIMATION_EASING_EASE_OUT_ELASTIC: {
		const double c4 = 2.0 * pi / 3.0;
		if (t <= 0.0)
			return 0.0;
		if (t >= 1.0)
			return 1.0;
		return pow(2.0, -10.0 * t) * sin((t * 10.0 - 0.75) * c4) + 1.0;
	}
	case ANIMATION_EASING_EASE_IN_OUT_ELASTIC: {
		const double c5 = 2.0 * pi / 4.5;
		if (t <= 0.0)
			return 0.0;
		if (t >= 1.0)
			return 1.0;
		return t < 0.5 ? -(pow(2.0, 20.0 * t - 10.0) *
						   sin((20.0 * t - 11.125) * c5)) /
							 2.0
					   : (pow(2.0, -20.0 * t + 10.0) *
						  sin((20.0 * t - 11.125) * c5)) /
								 2.0 +
							 1.0;
	}
	case ANIMATION_EASING_EASE_IN_BOUNCE:
		return 1.0 - ease_out_bounce(1.0 - t);
	case ANIMATION_EASING_EASE_OUT_BOUNCE:
		return ease_out_bounce(t);
	case ANIMATION_EASING_EASE_IN_OUT_BOUNCE:
		return t < 0.5 ? (1.0 - ease_out_bounce(1.0 - 2.0 * t)) / 2.0
					   : (1.0 + ease_out_bounce(2.0 * t - 1.0)) / 2.0;
	case ANIMATION_EASING_SNAPPY: {
		const double curve[4] = {0.05, 0.9, 0.1, 1.05};
		double low = 0.0;
		double high = 1.0;
		for (int i = 0; i < 24; i++) {
			double mid = (low + high) / 2.0;
			if (calculate_bezier_curve_for(mid, curve).x < t)
				low = mid;
			else
				high = mid;
		}
		return calculate_bezier_curve_for((low + high) / 2.0, curve).y;
	}
	}
	return t;
}

double find_animation_curve_for(double t, const AnimationCurve *curve) {
	return animation_curve_ease(curve, t);
}
void handle_snapshot_meta_destroy(struct wl_listener *listener, void *data) {
	SnapshotMetadata *meta = wl_container_of(listener, meta, destroy);
	wl_list_remove(&meta->destroy.link);
	free(meta);
}
double find_animation_curve_at(double t, int32_t type) {
	return animation_curve_ease(animation_curve_for_type(type), t);
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

static void scene_snapshot_for_each_buffer_node(
	struct wlr_scene_node *node, int32_t sx, int32_t sy,
	wlr_scene_buffer_iterator_func_t iterator, void *data) {
	if (!node)
		return;
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		iterator(wlr_scene_buffer_from_node(node), sx, sy, data);
		return;
	}
	if (node->type != WLR_SCENE_NODE_TREE)
		return;

	struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
	struct wlr_scene_node *child;
	wl_list_for_each(child, &tree->children, link) {
		scene_snapshot_for_each_buffer_node(child, sx + child->x, sy + child->y,
											iterator, data);
	}
}

/*
 * Snapshot trees can be disabled while still being composed manually. The
 * wlroots iterator skips disabled subtrees, so walk the snapshot children
 * directly to keep their geometry and opacity in sync.
 */
static void
scene_snapshot_for_each_buffer(struct wlr_scene_tree *snapshot,
							   wlr_scene_buffer_iterator_func_t iterator,
							   void *data) {
	if (!snapshot)
		return;
	scene_snapshot_for_each_buffer_node(&snapshot->node, 0, 0, iterator, data);
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

	/*
	 * The snapshot iterator reports child coordinates relative to the snapshot
	 * root. Translate the monitor bounds into the same coordinate space.
	 */
	struct wlr_box local_bounds = *bounds;
	local_bounds.x -= snapshot->node.x;
	local_bounds.y -= snapshot->node.y;
	struct SnapshotClipData clip = {
		.bounds = local_bounds,
		.width = width,
		.height = height,
		.scale = scale,
	};
	scene_snapshot_enable_buffers(&snapshot->node);
	scene_snapshot_for_each_buffer(snapshot, scene_snapshot_clip_buffer, &clip);
}

void scene_snapshot_apply_opacity(struct wlr_scene_tree *snapshot,
								  double opacity) {
	scene_snapshot_for_each_buffer(snapshot, scene_buffer_apply_opacity,
								   &opacity);
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
