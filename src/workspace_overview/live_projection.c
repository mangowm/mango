#include "mango/workspace_overview/live_projection.h"

#include <wlr/types/wlr_scene.h>

bool wsov_live_projection_layer_matches(
	const struct wlr_scene_node *node,
	struct wlr_scene_tree *const *layer_trees, uint32_t layer,
	uint32_t layer_count) {
	if (!node || !layer_trees || layer >= layer_count)
		return false;

	struct wlr_scene_tree *expected = layer_trees[layer];
	return expected && node->parent == expected;
}
