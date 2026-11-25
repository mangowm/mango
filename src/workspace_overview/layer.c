#include "mango/workspace_overview/workspace_overview_internal.h"

typedef struct {
	struct wsov_cell *cell;
	LayerSurface *layer;
	int layer_index;
} wsov_layer_iter_data;

bool wsov_layer_has_preview(int layer_index) {
	return layer_index == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND ||
		   layer_index == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
}

struct wlr_scene_tree *wsov_layer_normal_parent(LayerSurface *layer) {
	if (!layer || !layer->layer_surface)
		return NULL;

	switch (layer->layer_surface->current.layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return server.layers[LyrBg];
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return server.layers[LyrBottom];
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return server.layers[LyrTop];
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return server.layers[LyrSpecialOverlay];
	default:
		return NULL;
	}
}

void wsov_sync_direct_layers(void) {
	if (!wsov.mon || !wsov.direct_top_tree || !wsov.direct_overlay_tree)
		return;

	for (int layer_index = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
		 layer_index <= ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY; layer_index++) {
		struct wlr_scene_tree *target =
			layer_index == ZWLR_LAYER_SHELL_V1_LAYER_TOP ? wsov.direct_top_tree
			: layer_index == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY
				? wsov.direct_overlay_tree
				: NULL;
		if (!target)
			continue;

		LayerSurface *layer;
		wl_list_for_each_reverse(layer, &wsov.mon->layers[layer_index], link) {
			if (layer->scene->node.parent != target)
				wlr_scene_node_reparent(&layer->scene->node, target);
		}
		wl_list_for_each_reverse(layer, &wsov.mon->layers[layer_index], link) {
			if (layer->popups->node.parent != target)
				wlr_scene_node_reparent(&layer->popups->node, target);
		}
	}
}

void wsov_restore_direct_layers(void) {
	if (!wsov.mon)
		return;

	for (int layer_index = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
		 layer_index <= ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY; layer_index++) {
		LayerSurface *layer;
		wl_list_for_each_reverse(layer, &wsov.mon->layers[layer_index], link) {
			struct wlr_scene_tree *target = wsov_layer_normal_parent(layer);
			if (target && layer->scene->node.parent != target)
				wlr_scene_node_reparent(&layer->scene->node, target);
		}
		wl_list_for_each_reverse(layer, &wsov.mon->layers[layer_index], link) {
			struct wlr_scene_tree *target = wsov_layer_normal_parent(layer);
			if (target && layer->popups->node.parent != target)
				wlr_scene_node_reparent(&layer->popups->node, target);
		}
	}
}

void wsov_destroy_layer_fadeouts(void) {
	if (!wsov.direct_fadeout_tree)
		return;

	LayerSurface *layer, *tmp;
	wl_list_for_each_safe(layer, tmp, &server.fadeout_layers, fadeout_link) {
		if (layer->scene &&
			layer->scene->node.parent == wsov.direct_fadeout_tree) {
			wl_list_remove(&layer->fadeout_link);
			wlr_scene_node_destroy(&layer->scene->node);
			free(layer);
		}
	}
}

void wsov_layer_preview_add(struct wsov_cell *cell,
							struct wlr_scene_buffer *source,
							LayerSurface *layer, int layer_index, int sx,
							int sy) {
	if (!wsov_layer_has_preview(layer_index) || !source || !source->buffer)
		return;

	if (cell->layer_count == cell->layer_capacity) {
		int capacity = cell->layer_capacity ? cell->layer_capacity * 2 : 8;
		struct wsov_layer_preview *layers =
			realloc(cell->layers, capacity * sizeof(*layers));
		if (!layers)
			return;
		cell->layers = layers;
		cell->layer_capacity = capacity;
	}

	struct wlr_scene_buffer *node =
		wlr_scene_buffer_create(wsov.root, source->buffer);
	if (!node)
		return;
	wlr_scene_buffer_set_source_box(node, &source->src_box);
	wlr_scene_buffer_set_transform(node, source->transform);
	wlr_scene_buffer_set_filter_mode(node, WLR_SCALE_FILTER_BILINEAR);
	wlr_scene_buffer_set_opacity(node, 1.0f);
	wlr_scene_buffer_set_transfer_function(node, source->transfer_function);
	wlr_scene_buffer_set_primaries(node, source->primaries);
	wlr_scene_buffer_set_color_encoding(node, source->color_encoding);
	wlr_scene_buffer_set_color_range(node, source->color_range);
	wlr_scene_node_place_below(&node->node, &cell->contents->node);

	struct wlr_fbox src_box = source->src_box;
	if (src_box.width <= 0 || src_box.height <= 0)
		src_box = (struct wlr_fbox){0, 0, source->buffer->width,
									source->buffer->height};
	cell->layers[cell->layer_count++] = (struct wsov_layer_preview){
		.node = node,
		.layer = layer,
		.workspace_x = sx - wsov.mon->m.x,
		.workspace_y = sy - wsov.mon->m.y,
		.width =
			source->dst_width > 0 ? source->dst_width : source->buffer->width,
		.height = source->dst_height > 0 ? source->dst_height
										 : source->buffer->height,
		.src_box = src_box,
	};
}

void wsov_collect_layer_buffer(struct wlr_scene_buffer *buffer, int sx, int sy,
							   void *data) {
	wsov_layer_iter_data *iter = data;
	wsov_layer_preview_add(iter->cell, buffer, iter->layer, iter->layer_index,
						   sx, sy);
}

void wsov_create_layer_previews_for_layer(struct wsov_cell *cell,
										  LayerSurface *layer,
										  int layer_index) {
	if (!wsov_layer_has_preview(layer_index) || !cell || !layer ||
		!layer->scene || !layer->scene->node.enabled)
		return;
	wsov_layer_iter_data iter = {
		.cell = cell,
		.layer = layer,
		.layer_index = layer_index,
	};
	wlr_scene_node_for_each_buffer(&layer->scene->node,
								   wsov_collect_layer_buffer, &iter);
}

void wsov_create_layer_previews(void) {
	for (int cell_index = 0; cell_index < wsov.cell_count; cell_index++) {
		struct wsov_cell *cell = &wsov.cells[cell_index];
		for (int layer_index = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
			 layer_index <= ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM; layer_index++) {
			LayerSurface *layer;
			wl_list_for_each_reverse(layer, &wsov.mon->layers[layer_index],
									 link)
				wsov_create_layer_previews_for_layer(cell, layer, layer_index);
		}
	}
}

void wsov_remove_layer_previews(struct wsov_cell *cell, LayerSurface *layer) {
	if (!cell || !layer)
		return;
	int write = 0;
	for (int read = 0; read < cell->layer_count; read++) {
		struct wsov_layer_preview *preview = &cell->layers[read];
		if (preview->layer == layer) {
			wlr_scene_node_destroy(&preview->node->node);
			continue;
		}
		if (write != read)
			cell->layers[write] = cell->layers[read];
		write++;
	}
	cell->layer_count = write;
}

void wsov_destroy_layer_previews(struct wsov_cell *cell) {
	for (int i = 0; i < cell->layer_count; i++)
		wlr_scene_node_destroy(&cell->layers[i].node->node);
	free(cell->layers);
	cell->layers = NULL;
	cell->layer_count = 0;
	cell->layer_capacity = 0;
}

void wsov_refresh_layer_previews(void) {
	if (!workspace_overview_is_active())
		return;
	for (int i = 0; i < wsov.cell_count; i++)
		wsov_destroy_layer_previews(&wsov.cells[i]);
	wsov_create_layer_previews();
	wsov_order_live_projections();
	wsov_update_styles();
	/*
	 * Recreated preview nodes start at their unscaled layer-shell geometry.
	 * The output frame may commit before the workspace-overview frame listener
	 * advances the open/close animation, so position them at the current
	 * camera progress immediately to avoid exposing a full-sized bar for one
	 * frame.
	 */
	wsov_apply_camera(wsov.progress);
}

void wsov_refresh_layer_preview(LayerSurface *layer) {
	if (!workspace_overview_is_active_on(layer ? layer->mon : NULL) ||
		wsov.closing)
		return;
	int layer_index = layer->layer_surface->current.layer;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		wsov_remove_layer_previews(cell, layer);
		wsov_create_layer_previews_for_layer(cell, layer, layer_index);
	}
	wsov_order_live_projections();
	wlr_scene_node_raise_to_top(&wsov.root->node);
	if (wsov.direct_layer_tree)
		wlr_scene_node_raise_to_top(&wsov.direct_layer_tree->node);
	wsov_apply_camera(wsov.progress);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}
