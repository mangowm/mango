#include "mango/common/scene_node.h"
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/util/transform.h>

static void handle_scene_node_destroy(struct wl_listener *listener,
									  void *data) {
	MangoSceneNode *node = wl_container_of(listener, node, destroy);
	wl_list_remove(&node->destroy.link);
	free(node);
}

MangoSceneNode *mango_scene_node_set(struct wlr_scene_node *node, uint32_t type,
									 void *owner) {
	if (!node)
		return NULL;

	/*
	 * A node owns at most one payload. Drop an existing one so a repeated
	 * call cannot leave a stale listener behind.
	 */
	MangoSceneNode *old = node->data;
	if (old) {
		wl_list_remove(&old->destroy.link);
		free(old);
	}

	MangoSceneNode *data = calloc(1, sizeof(*data));
	if (!data)
		return NULL;

	data->type = type;
	data->owner = owner;
	data->destroy.notify = handle_scene_node_destroy;
	wl_signal_add(&node->events.destroy, &data->destroy);
	node->data = data;
	return data;
}

MangoSceneNode *mango_scene_node_find(struct wlr_scene_node *node) {
	for (; node; node = node->parent ? &node->parent->node : NULL) {
		if (node->data) {
			return node->data;
		}
	}
	return NULL;
}

void mango_scene_node_set_ignore_hit(struct wlr_scene_node *node,
									 bool ignore_hit) {
	if (!node || !node->data) {
		return;
	}
	((MangoSceneNode *)node->data)->ignore_hit = ignore_hit;
}

bool mango_scene_node_get_ignore_hit(struct wlr_scene_node *node) {
	if (!node || !node->data) {
		return false;
	}
	return ((MangoSceneNode *)node->data)->ignore_hit;
}

/* Size of a scene node, mirroring wlroots' internal scene_node_get_size() for
 * the node kinds that can actually take pointer input. */
static void scene_node_size(struct wlr_scene_node *node, int32_t *width,
							int32_t *height) {
	*width = 0;
	*height = 0;

	switch (node->type) {
	case WLR_SCENE_NODE_RECT: {
		struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
		*width = rect->width;
		*height = rect->height;
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
		if (buffer->dst_width > 0 && buffer->dst_height > 0) {
			*width = buffer->dst_width;
			*height = buffer->dst_height;
		} else {
			*width = buffer->WLR_PRIVATE.buffer_width;
			*height = buffer->WLR_PRIVATE.buffer_height;
			wlr_output_transform_coords(buffer->transform, width, height);
		}
		break;
	}
	default:
		break;
	}
}

/* Mirrors wlroots' scene_node_at_iterator() acceptance rules. On success
 * *rx / *ry hold the node-local coordinates (a callback may rewrite them). */
static bool scene_node_accepts_input(struct wlr_scene_node *node, double *rx,
									 double *ry) {
	switch (node->type) {
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
		return !buffer->point_accepts_input ||
			   buffer->point_accepts_input(buffer, rx, ry);
	}
	case WLR_SCENE_NODE_RECT:
		/* Native wlroots rects always take input; scenefx's accepts_input /
		 * clipped_region do not exist here. */
		return true;
	default:
		/* Trees (and any unknown node kind) never take input. */
		return false;
	}
}

static bool scene_node_contains_point(struct wlr_scene_node *node, double px,
									  double py, int32_t lx, int32_t ly) {
	int32_t width = 0, height = 0;
	struct wlr_box node_box = {.x = lx, .y = ly};

	scene_node_size(node, &width, &height);
	node_box.width = width;
	node_box.height = height;

	return wlr_box_contains_point(&node_box, px, py);
}

/*
 * wlroots offers no hook to skip a node during hit-testing, so walk the tree
 * ourselves and prune pointer-transparent subtrees instead. wlr_scene_node_at()
 * would otherwise return a node with ignore_hit set.
 */
static bool scene_node_search(struct wlr_scene_node *node, double px, double py,
							  int32_t lx, int32_t ly,
							  struct wlr_scene_node **result, double *rx,
							  double *ry) {
	if (!node->enabled) {
		return false;
	}

	MangoSceneNode *data = node->data;
	if (data && data->ignore_hit) {
		return false;
	}

	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each_reverse(child, &tree->children, link) {
			if (scene_node_search(child, px, py, lx + child->x, ly + child->y,
								  result, rx, ry)) {
				return true;
			}
		}
		return false;
	}

	if (node->type != WLR_SCENE_NODE_BUFFER &&
		node->type != WLR_SCENE_NODE_RECT) {
		return false;
	}

	if (!scene_node_contains_point(node, px, py, lx, ly)) {
		return false;
	}

	double local_x = px - lx;
	double local_y = py - ly;
	if (!scene_node_accepts_input(node, &local_x, &local_y)) {
		return false;
	}

	*result = node;
	*rx = local_x;
	*ry = local_y;
	return true;
}

struct wlr_scene_node *mango_scene_node_at(struct wlr_scene_node *node,
										   double lx, double ly, double *nx,
										   double *ny) {
	struct wlr_scene_node *result = NULL;
	int32_t node_x = 0, node_y = 0;
	double rx = 0, ry = 0;

	wlr_scene_node_coords(node, &node_x, &node_y);

	if (!scene_node_search(node, lx, ly, node_x, node_y, &result, &rx, &ry)) {
		return NULL;
	}

	if (nx) {
		*nx = rx;
	}
	if (ny) {
		*ny = ry;
	}
	return result;
}
