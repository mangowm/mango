#include "mango/workspace_overview/workspace_overview_internal.h"

bool wsov_item_is_live(const struct wsov_cell *cell,
					   const struct wsov_item *item) {
	if (!cell || !item || item->is_snapshot || !item->client)
		return false;
	for (int i = 0; i < cell->live_surface_count; i++) {
		if (cell->live_surfaces[i]->client == item->client)
			return true;
	}
	return false;
}

struct wsov_live_client *wsov_cell_live_client_for(const struct wsov_cell *cell,
												   const Client *client) {
	if (!cell || !client)
		return NULL;
	for (int i = 0; i < cell->live_client_count; i++) {
		if (cell->live_clients[i].client == client)
			return &cell->live_clients[i];
	}
	return NULL;
}

bool wsov_item_has_effect_border(const struct wsov_cell *cell,
								 const struct wsov_item *item) {
	if (!cell || !item || !item->client)
		return false;
	for (int i = 0; i < cell->live_client_count; i++) {
		struct wsov_live_client *live = &cell->live_clients[i];
		if (live->client == item->client &&
			wsov_live_effects_has_border(live->effects))
			return true;
	}
	return false;
}

bool wsov_cell_contains_live_client(const struct wsov_cell *cell,
									const Client *client) {
	if (!cell || !client)
		return false;
	for (int i = 0; i < cell->item_count; i++) {
		if (!cell->items[i].is_snapshot && cell->items[i].client == client)
			return true;
	}
	return false;
}

struct wsov_cell *wsov_live_owner_for_client(Client *client) {
	struct wsov_cell *owner = NULL;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!cell->live_desired ||
			!wsov_cell_contains_live_client(cell, client))
			continue;
		if (!owner || cell->live_priority > owner->live_priority ||
			(cell->live_priority == owner->live_priority &&
			 cell->tag == wsov.target_tag))
			owner = cell;
	}
	return owner;
}

void wsov_remove_live_surface(struct wsov_cell *cell,
							  struct wsov_live_surface *entry,
							  bool destroy_node) {
	if (!cell || !entry || !cell->live_surfaces)
		return;

	int index = -1;
	for (int i = 0; i < cell->live_surface_count; i++) {
		if (cell->live_surfaces[i] == entry) {
			index = i;
			break;
		}
	}
	if (index < 0)
		return;

	if (entry->commit.link.prev && entry->commit.link.next)
		wl_list_remove(&entry->commit.link);
	if (entry->destroy.link.prev && entry->destroy.link.next)
		wl_list_remove(&entry->destroy.link);
	if (entry->frame_done.link.prev && entry->frame_done.link.next)
		wl_list_remove(&entry->frame_done.link);
	if (destroy_node && entry->scene_surface)
		wlr_scene_node_destroy(&entry->scene_surface->buffer->node);

	if (index + 1 < cell->live_surface_count) {
		memmove(&cell->live_surfaces[index], &cell->live_surfaces[index + 1],
				(cell->live_surface_count - index - 1) *
					sizeof(*cell->live_surfaces));
	}
	cell->live_surface_count--;
	free(entry);
}

void wsov_live_surface_commit(struct wl_listener *listener, void *data) {
	struct wsov_live_surface *entry = wl_container_of(listener, entry, commit);
	if (entry->client == wsov.drag_client && wsov.moved)
		return;
	wsov_mark_live_layout_dirty();
}

void wsov_live_surface_destroy(struct wl_listener *listener, void *data) {
	struct wsov_live_surface *entry = wl_container_of(listener, entry, destroy);
	/* The scene surface helper owns and is already tearing down its buffer. */
	wsov_remove_live_surface(entry->cell, entry, false);
}

void wsov_live_surface_frame_done(struct wl_listener *listener, void *data) {
	struct wsov_live_surface *entry =
		wl_container_of(listener, entry, frame_done);
	struct wlr_scene_frame_done_event *event = data;
	if (entry->scene_surface->buffer->primary_output == event->output)
		wlr_surface_send_frame_done(entry->surface, &event->when);
}

struct wsov_live_client *
wsov_live_client_add(struct wsov_cell *cell, Client *client,
					 struct wlr_scene_tree *layer_tree) {
	if (!cell || !client || !layer_tree)
		return NULL;

	if (cell->live_client_count == cell->live_client_capacity) {
		int capacity =
			cell->live_client_capacity ? cell->live_client_capacity * 2 : 8;
		struct wsov_live_client *clients =
			realloc(cell->live_clients, capacity * sizeof(*clients));
		if (!clients)
			return NULL;
		cell->live_clients = clients;
		cell->live_client_capacity = capacity;
	}

	struct wlr_scene_tree *tree = wlr_scene_tree_create(layer_tree);
	if (!tree)
		return NULL;
	struct wsov_live_effects *effects = wsov_live_effects_create(tree, client);

	struct wsov_live_client *live =
		&cell->live_clients[cell->live_client_count++];
	*live = (struct wsov_live_client){
		.client = client,
		.tree = tree,
		.effects = effects,
	};
	return live;
}

void wsov_live_surface_add(struct wlr_surface *surface, int sx, int sy,
						   void *data) {
	struct wsov_live_surface_data *live = data;
	struct wsov_cell *cell = live->cell;
	if (!surface || !cell || !live->tree)
		return;

	if (cell->live_surface_count == cell->live_surface_capacity) {
		int capacity =
			cell->live_surface_capacity ? cell->live_surface_capacity * 2 : 8;
		struct wsov_live_surface **surfaces =
			realloc(cell->live_surfaces, capacity * sizeof(*surfaces));
		if (!surfaces)
			return;
		cell->live_surfaces = surfaces;
		cell->live_surface_capacity = capacity;
	}

	struct wsov_live_surface *entry = calloc(1, sizeof(*entry));
	if (!entry)
		return;
	entry->client = live->client;
	entry->cell = cell;
	entry->effects = live->effects;
	entry->surface = surface;
	entry->sx = sx;
	entry->sy = sy;
	entry->destroy.notify = wsov_live_surface_destroy;
	/*
	 * Remove our buffer listener before the scene surface's own destroy
	 * listener tears the buffer down.
	 */
	wl_signal_add(&surface->events.destroy, &entry->destroy);

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_create(live->tree, surface);
	if (!scene_surface) {
		wl_list_remove(&entry->destroy.link);
		free(entry);
		return;
	}
	entry->scene_surface = scene_surface;

	cell->live_surfaces[cell->live_surface_count++] = entry;
	wlr_scene_buffer_set_filter_mode(scene_surface->buffer,
									 WLR_SCALE_FILTER_BILINEAR);
	wsov_live_effects_apply_surface(live->effects, scene_surface->buffer, 1.0);
	entry->effect_frame_serial =
		cell->live_lower_tree && wsov.mon ? wsov.mon->frame_serial : 0;
	entry->commit.notify = wsov_live_surface_commit;
	wl_signal_add(&surface->events.commit, &entry->commit);
	entry->frame_done.notify = wsov_live_surface_frame_done;
	wl_signal_add(&scene_surface->buffer->events.frame_done,
				  &entry->frame_done);
}

void wsov_clear_cell_live_surfaces(struct wsov_cell *cell, bool mark_dirty) {
	if (!cell)
		return;

	if (mark_dirty && cell->live_surface_count > 0) {
		cell->contents_dirty = true;
		cell->foreground_dirty = true;
	}
	while (cell->live_surface_count > 0)
		wsov_remove_live_surface(cell, cell->live_surfaces[0], true);
	free(cell->live_surfaces);
	cell->live_surfaces = NULL;
	cell->live_surface_capacity = 0;

	for (int i = 0; i < cell->live_client_count; i++) {
		struct wsov_live_client *live = &cell->live_clients[i];
		wsov_live_effects_destroy(live->effects);
		if (live->tree)
			wlr_scene_node_destroy(&live->tree->node);
	}
	free(cell->live_clients);
	cell->live_clients = NULL;
	cell->live_client_count = 0;
	cell->live_client_capacity = 0;
}

void wsov_clear_live_projection(bool mark_dirty) {
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		wsov_clear_cell_live_surfaces(cell, mark_dirty);
		cell->live_enabled = false;
		cell->live_desired = false;
		cell->live_rebuild_pending = false;
		cell->live_priority = 0;
	}
	wsov.live_cell = NULL;
}

void wsov_destroy_cell_live_trees(struct wsov_cell *cell) {
	if (!cell)
		return;
	if (cell->live_lower_tree)
		wlr_scene_node_destroy(&cell->live_lower_tree->node);
	if (cell->live_upper_tree)
		wlr_scene_node_destroy(&cell->live_upper_tree->node);
	cell->live_lower_tree = NULL;
	cell->live_upper_tree = NULL;
	memset(cell->live_layer_trees, 0, sizeof(cell->live_layer_trees));
}

bool wsov_create_cell_live_trees(struct wsov_cell *cell) {
	if (!cell || !wsov.root)
		return false;
	if (cell->live_lower_tree && cell->live_upper_tree)
		return true;

	cell->live_lower_tree = wlr_scene_tree_create(wsov.root);
	cell->live_upper_tree = wlr_scene_tree_create(wsov.root);
	if (!cell->live_lower_tree || !cell->live_upper_tree) {
		wsov_destroy_cell_live_trees(cell);
		return false;
	}
	for (int layer = 0; layer < NUM_LAYERS; layer++) {
		struct wlr_scene_tree *parent =
			layer < LyrTop ? cell->live_lower_tree : cell->live_upper_tree;
		cell->live_layer_trees[layer] = wlr_scene_tree_create(parent);
		if (!cell->live_layer_trees[layer]) {
			wsov_destroy_cell_live_trees(cell);
			return false;
		}
	}
	wsov_order_live_projection(cell);
	return true;
}

void wsov_order_live_projection(struct wsov_cell *cell) {
	if (!cell)
		return;
	/*
	 * Keep the live scene split around layer-shell TOP surfaces. Lower client
	 * layers (tile/float/...) stay above the offscreen base buffer, while TOP
	 * and higher client layers stay above the TOP layer-shell previews.
	 */
	if (cell->live_lower_tree)
		wlr_scene_node_place_above(&cell->live_lower_tree->node,
								   &cell->contents->node);
	if (cell->live_upper_tree)
		wlr_scene_node_place_below(&cell->live_upper_tree->node,
								   &cell->foreground->node);
}

void wsov_order_live_projections(void) {
	for (int i = 0; i < wsov.cell_count; i++)
		wsov_order_live_projection(&wsov.cells[i]);
}

void wsov_order_live_clients(struct wsov_cell *cell) {
	if (!cell)
		return;

	/*
	 * Items are sorted bottom-to-top. Raising each live tree in that order
	 * restores the same stack after a client changes scene layers.
	 */
	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot || !item->client || item->layer >= NUM_LAYERS)
			continue;
		struct wsov_live_client *live =
			wsov_cell_live_client_for(cell, item->client);
		if (live && live->tree &&
			live->tree->node.parent == cell->live_layer_trees[item->layer]) {
			wlr_scene_node_raise_to_top(&live->tree->node);
		}
	}
}

bool wsov_sync_live_client_layers(struct wsov_cell *cell) {
	if (!cell)
		return false;

	bool changed = false;
	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot || !item->client || item->layer >= NUM_LAYERS ||
			wsov_live_owner_for_client(item->client) != cell)
			continue;

		struct wsov_live_client *live =
			wsov_cell_live_client_for(cell, item->client);
		if (!live || !live->tree || !cell->live_layer_trees[item->layer])
			continue;

		struct wlr_scene_tree *target = cell->live_layer_trees[item->layer];
		if (live->tree->node.parent != target) {
			wlr_scene_node_reparent(&live->tree->node, target);
			changed = true;
		}
	}
	if (changed)
		wsov_order_live_clients(cell);
	return changed;
}

bool wsov_cell_live_projection_matches(const struct wsov_cell *cell) {
	if (cell->live_enabled != cell->live_desired)
		return false;
	if (!cell->live_desired)
		return cell->live_surface_count == 0;

	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot || !item->client ||
			!wsov_client_eligible(item->client))
			continue;
		bool expected = wsov_live_owner_for_client(item->client) == cell;
		if (expected != wsov_item_is_live(cell, item))
			return false;
		if (expected) {
			struct wsov_live_client *live =
				wsov_cell_live_client_for(cell, item->client);
			/*
			 * Floating and maximize transitions change the target layer. The
			 * live client tree is reparented in place before this cache check,
			 * so this only falls back to a full rebuild if reconciliation could
			 * not preserve the projected tree.
			 */
			if (!live || !live->tree ||
				live->tree->node.parent != cell->live_layer_trees[item->layer])
				return false;
		}
	}
	for (int i = 0; i < cell->live_surface_count; i++) {
		Client *client = cell->live_surfaces[i]->client;
		if (!client || wsov_live_owner_for_client(client) != cell)
			return false;
	}
	return true;
}

void wsov_build_cell_live_projection(struct wsov_cell *cell) {
	if (!cell->live_enabled || cell->item_count <= 0 ||
		!wsov_create_cell_live_trees(cell))
		return;

	for (int i = 0; i < cell->item_count; i++) {
		struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot || !item->client ||
			!wsov_client_eligible(item->client) ||
			wsov_live_owner_for_client(item->client) != cell)
			continue;

		bool already_added = false;
		for (int j = 0; j < i; j++) {
			if (!cell->items[j].is_snapshot &&
				cell->items[j].client == item->client) {
				already_added = true;
				break;
			}
		}
		if (already_added)
			continue;

		struct wlr_surface *surface = client_surface(item->client);
		if (!surface || !surface->mapped || item->layer >= NUM_LAYERS)
			continue;
		struct wsov_live_client *live = wsov_live_client_add(
			cell, item->client, cell->live_layer_trees[item->layer]);
		if (!live)
			continue;
		struct wsov_live_surface_data data = {
			.client = item->client,
			.cell = cell,
			.tree = live->tree,
			.effects = live->effects,
		};
		wlr_surface_for_each_surface(surface, wsov_live_surface_add, &data);
	}

	if (cell->live_surface_count > 0) {
		cell->contents_dirty = true;
		cell->foreground_dirty = true;
	}
	wsov_order_live_projection(cell);
	/*
	 * Rebuild the off-screen cell immediately after mounting live surfaces.
	 * Otherwise the previous cell buffer can retain its own copy of the same
	 * client and composite it underneath the live copy for one or more frames,
	 * producing a vertically shifted duplicate of the window content.
	 */
	if (cell->live_surface_count > 0)
		wsov_render_cell(cell, wsov_output_scale());
}

void wsov_update_live_desires(const struct wsov_camera_view *view) {
	struct wsov_cell *target = wsov_cell_for_tag(wsov.target_tag);
	int target_index = -1;
	for (int i = 0; i < wsov.cell_count; i++) {
		if (&wsov.cells[i] == target) {
			target_index = i;
			break;
		}
	}

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		uint8_t priority = 0;
		if (cell->item_count > 0 &&
			wsov_cell_live_content_visible_in_view(cell, view))
			priority = 2;
		if (cell == target)
			priority = MANGO_MAX(priority, 1);
		if (wsov.layout == WORKSPACE_OVERVIEW_SCROLL && target_index >= 0 &&
			cell->item_count > 0 && abs(i - target_index) <= 1)
			priority = MANGO_MAX(priority, 1);
		cell->live_priority = priority;
		cell->live_desired = priority > 0;
	}
}

bool wsov_sync_live_projection(const struct wsov_camera_view *view) {
	if (wsov.closing)
		return false;
	struct wsov_cell *target = wsov_cell_for_tag(wsov.target_tag);
	if (!target || !wsov.root)
		return false;
	wsov.live_cell = target;
	wsov_update_live_desires(view);

	bool changed = false;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (wsov_sync_live_client_layers(cell))
			changed = true;
		if (wsov_cell_live_projection_matches(cell))
			continue;
		/*
		 * Remove every obsolete owner before creating new owners. Otherwise a
		 * multi-tag client could briefly have two live scene copies when its
		 * owner moves to a cell earlier in the strip.
		 */
		wsov_clear_cell_live_surfaces(cell, true);
		cell->live_enabled = cell->live_desired;
		cell->live_rebuild_pending = true;
		changed = true;
	}
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!cell->live_rebuild_pending)
			continue;
		cell->live_rebuild_pending = false;
		wsov_build_cell_live_projection(cell);
	}
	if (changed)
		wsov_order_live_projections();
	return changed;
}

void wsov_rebuild_live_projection(void) {
	if (wsov.closing || !wsov.root)
		return;
	wsov_layout_live_projection();
	wsov_render_dirty_cells(false);
}

void wsov_live_client_clip(Client *c, struct wlr_box *clip) {
	client_get_clip(c, clip);
#ifdef XWAYLAND
	if (client_is_x11(c))
		return;
#endif

	/*
	 * client_get_clip uses c->geom, which already contains the final target
	 * size when a maximize animation starts. Live previews follow
	 * animation.current and can still be displaying the pre-configure surface,
	 * so use the current XDG geometry for the content extent instead.
	 */
	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	if (geometry.width <= 0 || geometry.height <= 0)
		return;

	clip->x = geometry.x;
	clip->y = geometry.y;
	clip->width = geometry.width;
	clip->height = geometry.height;
}

struct wsov_live_clip_data {
	struct wlr_box clip;
};

static void wsov_clip_live_card_buffer(struct wlr_scene_buffer *buffer, int sx,
									   int sy, void *data) {
	struct wsov_live_clip_data *clip = data;
	if (!buffer || !buffer->buffer || !buffer->node.enabled || !wsov.mon)
		return;

	int width = buffer->dst_width;
	int height = buffer->dst_height;
	if (width <= 0 || height <= 0) {
		width = buffer->buffer->width;
		height = buffer->buffer->height;
		if (buffer->transform == WL_OUTPUT_TRANSFORM_90 ||
			buffer->transform == WL_OUTPUT_TRANSFORM_270 ||
			buffer->transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
			buffer->transform == WL_OUTPUT_TRANSFORM_FLIPPED_270) {
			int tmp = width;
			width = height;
			height = tmp;
		}
	}

	struct wlr_box clip_box = clip->clip;
	struct wlr_box box = {
		.x = sx,
		.y = sy,
		.width = width,
		.height = height,
	};
	struct wlr_box visible;
	if (!wlr_box_intersection(&visible, &box, &clip_box)) {
		wlr_scene_node_set_enabled(&buffer->node, false);
		return;
	}
	if (wlr_box_equal(&visible, &box))
		return;

	struct wlr_fbox src = buffer->src_box;
	if (wlr_fbox_empty(&src)) {
		src = (struct wlr_fbox){0, 0, buffer->buffer->width,
								buffer->buffer->height};
	}
	if (buffer->transform == WL_OUTPUT_TRANSFORM_NORMAL) {
		src.x += src.width * (visible.x - box.x) / box.width;
		src.y += src.height * (visible.y - box.y) / box.height;
		src.width *= (double)visible.width / box.width;
		src.height *= (double)visible.height / box.height;
	} else {
		int buffer_w = buffer->buffer->width;
		int buffer_h = buffer->buffer->height;
		struct wlr_fbox oriented;
		wlr_fbox_transform(&oriented, &src, buffer->transform, buffer_w,
						   buffer_h);
		oriented.x += oriented.width * (visible.x - box.x) / box.width;
		oriented.y += oriented.height * (visible.y - box.y) / box.height;
		oriented.width *= (double)visible.width / box.width;
		oriented.height *= (double)visible.height / box.height;
		bool swapped = buffer->transform == WL_OUTPUT_TRANSFORM_90 ||
					   buffer->transform == WL_OUTPUT_TRANSFORM_270 ||
					   buffer->transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
					   buffer->transform == WL_OUTPUT_TRANSFORM_FLIPPED_270;
		int oriented_w = swapped ? buffer_h : buffer_w;
		int oriented_h = swapped ? buffer_w : buffer_h;
		wlr_fbox_transform(&src, &oriented,
						   wlr_output_transform_invert(buffer->transform),
						   oriented_w, oriented_h);
	}

	wlr_scene_node_set_enabled(&buffer->node, true);
	wlr_scene_node_set_position(&buffer->node,
								buffer->node.x + visible.x - box.x,
								buffer->node.y + visible.y - box.y);
	wlr_scene_buffer_set_dest_size(buffer, visible.width, visible.height);
	wlr_scene_buffer_set_source_box(buffer, &src);
}

/*
 * The live card is a scene tree instead of one texture. Apply the card's
 * output-space clip to every buffer in that tree, including buffers for
 * sub-surfaces and transient surface animations, so no child buffer can paint
 * past the workspace preview or the monitor it belongs to.
 */
static void wsov_clip_live_card(struct wsov_cell *cell,
								const struct wlr_box *clip) {
	if (!cell || !clip)
		return;
	struct wsov_live_clip_data data = {.clip = *clip};
	if (cell->live_lower_tree)
		wlr_scene_node_for_each_buffer(&cell->live_lower_tree->node,
									   wsov_clip_live_card_buffer, &data);
	if (cell->live_upper_tree)
		wlr_scene_node_for_each_buffer(&cell->live_upper_tree->node,
									   wsov_clip_live_card_buffer, &data);
}

void wsov_layout_live_projection_impl(void) {
	if (!wsov.mon || wsov.cell_count <= 0)
		return;

	struct wsov_camera_view view;
	if (!wsov_get_camera_view(wsov.progress, &view))
		return;
	if (!wsov.closing)
		wsov_sync_live_projection(&view);
	int border = wsov_border_width();

	struct wlr_box monitor_clip = {
		.x = 0,
		.y = 0,
		.width = wsov.mon->m.width,
		.height = wsov.mon->m.height,
	};
	for (int cell_index = 0; cell_index < wsov.cell_count; cell_index++) {
		struct wsov_cell *cell = &wsov.cells[cell_index];
		if (!cell->live_lower_tree || !cell->live_upper_tree ||
			cell->live_surface_count <= 0)
			continue;
		if (!wsov_cell_live_content_visible_in_view(cell, &view)) {
			for (int i = 0; i < cell->live_surface_count; i++) {
				wlr_scene_node_set_enabled(
					&cell->live_surfaces[i]->scene_surface->buffer->node,
					false);
				wsov_live_effects_update(cell->live_surfaces[i]->effects, NULL,
										 NULL, 1.0);
			}
			continue;
		}

		struct wlr_box cell_box = wsov_cell_layout_box(cell);
		struct wlr_box cell_content_preview = {
			.x = cell_box.x + border,
			.y = cell_box.y + border,
			.width = MANGO_MAX(1, cell_box.width - 2 * border),
			.height = MANGO_MAX(1, cell_box.height - 2 * border),
		};
		struct wlr_box workspace_clip = {
			.x = (int)lround((cell_content_preview.x - view.x) * view.scale_x),
			.y = (int)lround((cell_content_preview.y - view.y) * view.scale_y),
			.width = MANGO_MAX(
				1, (int)lround(cell_content_preview.width * view.scale_x)),
			.height = MANGO_MAX(
				1, (int)lround(cell_content_preview.height * view.scale_y)),
		};
		struct wlr_box overflow_clip =
			wsov_live_overflow_clip(&workspace_clip, &monitor_clip);

		for (int i = 0; i < cell->live_surface_count; i++) {
			struct wsov_live_surface *entry = cell->live_surfaces[i];
			struct wlr_scene_buffer *buffer = entry->scene_surface->buffer;
			struct wsov_item *item =
				wsov_cell_item_for_client(cell, entry->client);
			struct wlr_surface *surface = entry->surface;
			if (!buffer)
				continue;
			double effect_scale = fmin(view.scale_x, view.scale_y);
			if (!item || wsov_drag_hides_item(cell, entry->client) ||
				!surface || surface->current.width <= 0 ||
				surface->current.height <= 0) {
				wsov_live_effects_update(entry->effects, NULL, NULL,
										 effect_scale);
				wlr_scene_node_set_enabled(&buffer->node, false);
				continue;
			}

			double item_scale_x =
				(double)MANGO_MAX(1, cell_box.width - 2 * border) /
				MANGO_MAX(1, cell->buffer_w);
			double item_scale_y =
				(double)MANGO_MAX(1, cell_box.height - 2 * border) /
				MANGO_MAX(1, cell->buffer_h);
			effect_scale =
				fmin(item->render_scale_x * item_scale_x * view.scale_x,
					 item->render_scale_y * item_scale_y * view.scale_y);
			if (!wsov.mon ||
				entry->effect_frame_serial != wsov.mon->frame_serial) {
				double opacity = CLAMP_FLOAT(
					entry->client->opacity_animation.current_opacity, 0.0, 1.0);
				wlr_scene_buffer_set_opacity(buffer, opacity);
				wsov_live_effects_apply_surface(entry->effects, buffer,
												effect_scale);
				entry->effect_frame_serial =
					wsov.mon ? wsov.mon->frame_serial : 0;
			}

			struct wlr_box geo = item->workspace_geom;
			if (geo.width <= 0 || geo.height <= 0)
				geo = entry->client->geom;
			if (geo.width <= 2 * item->border_width ||
				geo.height <= 2 * item->border_width) {
				wsov_live_effects_update(entry->effects, NULL, NULL,
										 effect_scale);
				wlr_scene_node_set_enabled(&buffer->node, false);
				continue;
			}

			struct wlr_box clip;
			wsov_live_client_clip(entry->client, &clip);
			bool use_clip_source = false;
			struct wlr_fbox clip_src = {0};

			float content_w = 0.0f;
			float content_h = 0.0f;
#ifdef XWAYLAND
			if (client_is_x11(entry->client)) {
				content_w = surface->current.width;
				content_h = surface->current.height;
			} else
#endif
			{
				if (!entry->client->surface.xdg) {
					wsov_live_effects_update(entry->effects, NULL, NULL,
											 effect_scale);
					wlr_scene_node_set_enabled(&buffer->node, false);
					continue;
				}
				content_w = clip.width;
				content_h = clip.height;
			}
			if (content_w <= 0 || content_h <= 0) {
				wsov_live_effects_update(entry->effects, NULL, NULL,
										 effect_scale);
				wlr_scene_node_set_enabled(&buffer->node, false);
				continue;
			}

			double item_offset_x = (double)item->box.x - cell->box.x - border;
			double item_offset_y = (double)item->box.y - cell->box.y - border;
			double window_preview_x =
				cell_box.x + border + item_offset_x * item_scale_x;
			double window_preview_y =
				cell_box.y + border + item_offset_y * item_scale_y;
			double window_preview_w = item->box.width * item_scale_x;
			double window_preview_h = item->box.height * item_scale_y;
			double border_inset_x =
				item->border_width * item->render_scale_x * item_scale_x;
			double border_inset_y =
				item->border_width * item->render_scale_y * item_scale_y;
			double content_preview_x = window_preview_x + border_inset_x;
			double content_preview_y = window_preview_y + border_inset_y;
			double content_preview_w =
				MANGO_MAX(1.0, window_preview_w - 2 * border_inset_x);
			double content_preview_h =
				MANGO_MAX(1.0, window_preview_h - 2 * border_inset_y);
			double surface_scale_x = content_preview_w / content_w;
			double surface_scale_y = content_preview_h / content_h;

			struct wlr_box window_box = {
				.x = (int)lround((window_preview_x - view.x) * view.scale_x),
				.y = (int)lround((window_preview_y - view.y) * view.scale_y),
				.width =
					MANGO_MAX(1, (int)lround(window_preview_w * view.scale_x)),
				.height =
					MANGO_MAX(1, (int)lround(window_preview_h * view.scale_y)),
			};
			wsov_live_effects_update(entry->effects, &window_box,
									 &overflow_clip, effect_scale);

			double preview_x;
			double preview_y;
			double preview_w;
			double preview_h;
#ifdef XWAYLAND
			if (client_is_x11(entry->client)) {
				preview_x =
					content_preview_x + (entry->sx - clip.x) * surface_scale_x;
				preview_y =
					content_preview_y + (entry->sy - clip.y) * surface_scale_y;
				preview_w = surface->current.width * surface_scale_x;
				preview_h = surface->current.height * surface_scale_y;
			} else
#endif
			{
				struct wlr_box surface_bounds = {
					.x = entry->sx,
					.y = entry->sy,
					.width = surface->current.width,
					.height = surface->current.height,
				};
				struct wlr_box surface_visible;
				if (!wlr_box_intersection(&surface_visible, &surface_bounds,
										  &clip)) {
					wsov_live_effects_update(entry->effects, NULL, NULL,
											 effect_scale);
					wlr_scene_node_set_enabled(&buffer->node, false);
					continue;
				}
				struct wlr_fbox surface_box = {
					.x = surface_visible.x - entry->sx,
					.y = surface_visible.y - entry->sy,
					.width = surface_visible.width,
					.height = surface_visible.height,
				};
				client_surface_get_source_box(surface, &surface_box, &clip_src);
				use_clip_source = true;
				preview_x = content_preview_x +
							(surface_visible.x - clip.x) * surface_scale_x;
				preview_y = content_preview_y +
							(surface_visible.y - clip.y) * surface_scale_y;
				preview_w = surface_visible.width * surface_scale_x;
				preview_h = surface_visible.height * surface_scale_y;
			}

			struct wlr_box dst = {
				.x = (int)lround((preview_x - view.x) * view.scale_x),
				.y = (int)lround((preview_y - view.y) * view.scale_y),
				.width = MANGO_MAX(1, (int)lround(preview_w * view.scale_x)),
				.height = MANGO_MAX(1, (int)lround(preview_h * view.scale_y)),
			};
			struct wlr_box content_clip = {
				.x = (int)lround((content_preview_x - view.x) * view.scale_x),
				.y = (int)lround((content_preview_y - view.y) * view.scale_y),
				.width =
					MANGO_MAX(1, (int)lround(content_preview_w * view.scale_x)),
				.height =
					MANGO_MAX(1, (int)lround(content_preview_h * view.scale_y)),
			};
			struct wlr_box visible_in_cell;
			struct wlr_box visible;
			if (!wlr_box_intersection(&visible_in_cell, &dst, &content_clip)) {
				wsov_live_effects_update(entry->effects, NULL, NULL,
										 effect_scale);
				wlr_scene_node_set_enabled(&buffer->node, false);
				continue;
			}
			if (!wlr_box_intersection(&visible, &visible_in_cell,
									  &overflow_clip) ||
				visible.width <= 0 || visible.height <= 0) {
				wsov_live_effects_update(entry->effects, NULL, NULL,
										 effect_scale);
				wlr_scene_node_set_enabled(&buffer->node, false);
				continue;
			}
			struct wlr_fbox src;
			if (use_clip_source) {
				src = clip_src;
			} else {
				struct wlr_fbox surface_box = {
					.x = 0,
					.y = 0,
					.width = surface->current.width,
					.height = surface->current.height,
				};
				client_surface_get_source_box(surface, &surface_box, &src);
			}
			src.x += src.width * (visible.x - dst.x) / dst.width;
			src.y += src.height * (visible.y - dst.y) / dst.height;
			src.width *= (double)visible.width / dst.width;
			src.height *= (double)visible.height / dst.height;

			wlr_scene_node_set_enabled(&buffer->node, true);
			wlr_scene_node_set_position(&buffer->node, visible.x, visible.y);
			wlr_scene_buffer_set_dest_size(buffer, visible.width,
										   visible.height);
			wlr_scene_buffer_set_source_box(buffer, &src);
		}
		wsov_clip_live_card(cell, &overflow_clip);
	}
}

void wsov_layout_live_projection(void) {
	if (wsov.live_layout_flushing)
		return;
	wsov.live_layout_dirty = false;
	wsov.live_layout_flushing = true;
	wsov_layout_live_projection_impl();
	wsov.live_layout_flushing = false;
}

void wsov_mark_live_layout_dirty(void) {
	if (!workspace_overview_is_active())
		return;
	wsov.live_layout_dirty = true;
	if (wsov.mon)
		wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void wsov_flush_live_layout(void) {
	if (!wsov.live_layout_dirty || wsov.live_layout_flushing ||
		!workspace_overview_is_active())
		return;
	wsov_layout_live_projection();
}
