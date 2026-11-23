#ifndef MANGO_WORKSPACE_OVERVIEW_LIVE_PROJECTION_H
#define MANGO_WORKSPACE_OVERVIEW_LIVE_PROJECTION_H

#include <stdbool.h>
#include <stdint.h>

struct wlr_scene_node;
struct wlr_scene_tree;

/*
 * Check the scene-layer placement of a live workspace projection. Floating,
 * fullscreen and other client-state changes can alter the expected layer
 * without changing workspace membership, so the projection cache must compare
 * both.
 */
bool wsov_live_projection_layer_matches(
	const struct wlr_scene_node *node,
	struct wlr_scene_tree *const *layer_trees, uint32_t layer,
	uint32_t layer_count);

#endif
