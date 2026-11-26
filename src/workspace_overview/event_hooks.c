#include "mango/workspace_overview/workspace_overview_internal.h"

double wsov_elapsed_ms(const struct timespec *start) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - start->tv_sec) * 1000.0 +
		   (now.tv_nsec - start->tv_nsec) / 1000000.0;
}

void wsov_process_frame(void) {
	if (!workspace_overview_is_active())
		return;

	if (wsov.layers_dirty) {
		wsov.layers_dirty = false;
		wlr_scene_node_raise_to_top(&wsov.root->node);
		if (wsov.direct_layer_tree)
			wlr_scene_node_raise_to_top(&wsov.direct_layer_tree->node);
		wsov_refresh_layer_previews();
	}

	bool client_animating = wsov_refresh_animated_cells();
	bool client_animation_finished = wsov.client_animating && !client_animating;
	wsov.client_animating = client_animating;
	wsov_flush_live_layout();
	if (client_animation_finished) {
		for (int i = 0; i < wsov.cell_count; i++) {
			struct wsov_cell *cell = &wsov.cells[i];
			if (!wsov_cell_live_content_visible(cell, wsov.progress)) {
				cell->capture_pending = true;
				cell->contents_dirty = true;
				cell->foreground_dirty = true;
				continue;
			}
			wsov_capture_cell_items(cell);
			wsov_position_cell_items(cell);
			cell->capture_pending = false;
			cell->contents_dirty = true;
			cell->foreground_dirty = true;
		}
	}

	if (!config.animations) {
		wsov.layout_animating = false;
		if (wsov.scroll_animating)
			wsov.scroll_offset = wsov.scroll_animation_to;
		wsov.scroll_animating = false;
		wsov.progress = wsov.animation_to;
		wsov.animating = false;
		if (wsov.closing) {
			wsov_apply_camera(wsov.progress);
			wsov_finalize_close(wsov.close_tag);
			return;
		}
		wsov_schedule_dirty_render();
	}

	bool drag_scrolling = wsov.pressed && wsov.moved && wsov.has_drag_client &&
						  wsov.drag_scroll_direction != 0;
	if (!wsov.layout_animating && !wsov.scroll_animating && !wsov.animating &&
		!drag_scrolling && !client_animating && !client_animation_finished) {
		wsov_schedule_dirty_render();
		wsov_detach_frame_listener();
		return;
	}

	if (wsov.layout_animating)
		wsov_update_layout_animation();
	if (wsov.scroll_animating)
		wsov_update_scroll_animation();
	if (drag_scrolling)
		wsov_advance_drag_scroll();

	if (wsov.animating) {
		const AnimationCurve *curve = wsov.closing
										  ? &config.animation_curve_wsov_close
										  : &config.animation_curve_wsov_open;
		uint32_t duration = animation_curve_duration_ms(
			curve, wsov.closing ? config.animation_duration_wsov_close
								: config.animation_duration_wsov_open);
		double t = wsov_elapsed_ms(&wsov.animation_start) /
				   MANGO_MAX(1.0, (double)duration);
		t = fmax(0.0, fmin(1.0, t));
		double eased = animation_curve_ease(curve, t);
		wsov.progress = wsov.animation_from +
						(wsov.animation_to - wsov.animation_from) * eased;
		if (t >= 1.0) {
			wsov.progress = wsov.animation_to;
			wsov.animating = false;
		}
	}

	wsov_render_dirty_cells(false);
	wsov_apply_camera(wsov.progress);
	if (!wsov.animating && wsov.closing) {
		wsov_finalize_close(wsov.close_tag);
	} else if (!wsov.layout_animating && !wsov.scroll_animating &&
			   !wsov.animating && !drag_scrolling && !client_animating) {
		wsov_schedule_dirty_render();
		wsov_detach_frame_listener();
	}
}

void wsov_handle_frame(struct wl_listener *listener, void *data) {
	if (wsov.closing)
		return;
	wsov_process_frame();
}

void wsov_attach_frame_listener(void) {
	if (!wsov.mon)
		return;
	if (wsov.frame_active)
		return;
	wsov.frame_active = true;
	wsov.frame.notify = wsov_handle_frame;
	wl_signal_add(&wsov.mon->wlr_output->events.frame, &wsov.frame);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void wsov_detach_frame_listener(void) {
	wsov.frame_active = false;
	if (!wsov.frame.link.prev || !wsov.frame.link.next ||
		wsov.frame.link.prev == &wsov.frame.link)
		return;
	wl_list_remove(&wsov.frame.link);
	wl_list_init(&wsov.frame.link);
}

void workspace_overview_prepare_output_frame(Monitor *m) {
	if (!workspace_overview_is_active())
		return;
	/*
	 * Live card nodes are shared scene nodes. Any output can commit them, not
	 * just the monitor showing WSOV, so flush the common clip before every
	 * output frame. Otherwise an inner scroller commit can be presented on
	 * another monitor for one frame before the WSOV output refreshes it.
	 */
	if (wsov.frame_active && (wsov.closing || wsov.animating ||
							  wsov.layout_animating || wsov.scroll_animating)) {
		wsov_process_frame();
		return;
	}
	wsov_flush_live_layout();
}

void workspace_overview_client_commit(Client *c) {
	if (!workspace_overview_is_active() || c->mon != wsov.mon)
		return;
	uint32_t current_layer = client_target_layer(c);
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!wsov_client_on_tag(c, cell->tag))
			continue;

		bool found = false;
		for (int j = 0; j < cell->item_count; j++) {
			struct wsov_item *item = &cell->items[j];
			if (item->client != c)
				continue;
			found = true;
			if (wsov_item_is_live(cell, item))
				break;
			if (item->layer < LyrTop || current_layer < LyrTop)
				cell->contents_dirty = true;
			if (item->layer >= LyrTop || current_layer >= LyrTop)
				cell->foreground_dirty = true;
			break;
		}
		if (!found) {
			cell->contents_dirty = true;
			cell->foreground_dirty = true;
		}
	}
	wsov_mark_live_layout_dirty();
	wsov_schedule_dirty_render();
	if (c->is_pending_open_animation || c->need_output_flush ||
		c->animation.running || c->opacity_animation.running) {
		wsov.client_animating = true;
		wsov_attach_frame_listener();
	}
}

void workspace_overview_client_frame(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return;
	wsov_mark_live_layout_dirty();
}

void workspace_overview_layers_changed(LayerSurface *layer) {
	if (!workspace_overview_is_active_on(layer ? layer->mon : NULL))
		return;
	wsov_sync_direct_layers();
	wsov_refresh_layer_preview(layer);
}

struct wlr_scene_tree *
workspace_overview_layer_fadeout_parent(LayerSurface *layer) {
	if (!layer || !layer->layer_surface ||
		!workspace_overview_is_active_on(layer->mon) ||
		!wsov.direct_fadeout_tree)
		return NULL;

	int32_t layer_index = layer->layer_surface->current.layer;
	if (layer_index != ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
		layer_index != ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)
		return NULL;
	return wsov.direct_fadeout_tree;
}

void workspace_overview_client_added(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return;
	if (!client_is_unmanaged(c)) {
		c->wsov_hidden = true;
		client_update_visibility(c);
	}
	wlr_seat_pointer_clear_focus(server.seat);
	wlr_seat_keyboard_notify_clear_focus(server.seat);
	wsov_refresh_workspace_or_rebuild(wsov_client_tags(c));
	if (!wsov.closing && workspace_overview_is_active()) {
		wsov_rebuild_live_projection();
		wsov_render_tags(wsov_tag_bit(wsov.target_tag));
	}
	if (!wsov.closing && workspace_overview_is_active() &&
		(c->is_pending_open_animation || c->need_output_flush ||
		 c->animation.running || c->opacity_animation.running)) {
		wsov.client_animating = true;
		wsov_attach_frame_listener();
	}
}

void workspace_overview_client_removed(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return;
	if (c == wsov.close_client)
		wsov.close_client = NULL;
	if (c == wsov.drag_client)
		wsov_cancel_drag();
	c->wsov_hidden = false;
	wsov_refresh_workspace_or_rebuild(wsov_client_tags(c));
	if (!wsov.closing && workspace_overview_is_active()) {
		wsov_rebuild_live_projection();
		wsov_render_tags(wsov_tag_bit(wsov.target_tag));
	}
}

void workspace_overview_client_replaced(Client *old, Client *new) {
	if (!workspace_overview_is_active() || !old || !new || old == new)
		return;
	if (wsov.drag_client == old)
		wsov.drag_client = new;
	if (wsov.close_client == old)
		wsov.close_client = new;
}

void workspace_overview_client_tags_changed(Client *c) {
	if (!workspace_overview_is_active() || wsov.closing || !c ||
		c->mon != wsov.mon)
		return;
	uint32_t old_tags = wsov_cell_tags_for_client(c);
	uint32_t new_tags = wsov_client_tags(c);
	wsov_refresh_workspace_or_rebuild(old_tags | new_tags);
	if (!wsov.closing && workspace_overview_is_active()) {
		wsov_rebuild_live_projection();
		wsov_render_tags(wsov_tag_bit(wsov.target_tag));
	}
}

void workspace_overview_client_title_changed(Client *c) {
	if (!workspace_overview_is_active() || wsov.closing || !c ||
		c->mon != wsov.mon)
		return;

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!wsov_client_on_tag(c, cell->tag))
			continue;
		wsov_capture_cell_bars(cell);
		wsov_render_cell(cell, wsov_output_scale());
	}
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void workspace_overview_client_stack_changed(Client *c) {
	if (!workspace_overview_is_active() || wsov.closing || !c ||
		c->mon != wsov.mon)
		return;
	uint32_t tags = wsov_cell_tags_for_client(c) | wsov_client_tags(c);
	wsov_refresh_workspace_or_rebuild(tags);
	if (wsov.closing || !workspace_overview_is_active())
		return;
	wsov_rebuild_live_projection();
	wsov_render_tags(wsov_tag_bit(wsov.target_tag));
	wsov_update_styles_tags(tags | wsov_tag_bit(wsov.target_tag));
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void workspace_overview_layout_changed(Monitor *m) {
	if (!workspace_overview_is_active_on(m) || wsov.closing ||
		wsov.capturing_layout)
		return;
	uint32_t tags = wsov_layout_affected_tags(m);
	uint32_t previous_target = wsov.target_tag;
	uint32_t current_view_tag = get_mon_curtag(m);
	uint32_t current_view_bit = wsov_tag_bit(current_view_tag);
	bool keep_view_tag = m->workspace_overview_scope == OCCUPIED_WORKSPACE &&
						 config.workspace_overview_occupied_keep_seen_tags &&
						 current_view_bit;
	if (keep_view_tag)
		wsov.retained_tags |= current_view_bit;
	bool view_target_available =
		current_view_tag &&
		(wsov_cell_for_tag(current_view_tag) || keep_view_tag);
	bool target_changed =
		view_target_available && current_view_tag != previous_target;
	if (view_target_available) {
		wsov.target_tag = current_view_tag;
		/*
		 * Pointer hover may update the target and focus while the opening
		 * animation runs, but the camera must stay anchored to the card whose
		 * zoom originated the animation.
		 */
		if (!wsov.animating)
			wsov.camera_tag = current_view_tag;
		if (target_changed)
			wsov.scroll_center_tag = current_view_tag;
	}
	wsov_settle_tag_animations(m, tags);
	wsov_capture_workspace_tags(tags);
	wsov_refresh_workspace_or_rebuild(tags);
	if (!workspace_overview_is_active() || wsov.closing)
		return;

	if (target_changed) {
		if (wsov.layout == WORKSPACE_OVERVIEW_SCROLL)
			wsov_animate_scroll_to_target();
		wsov_apply_target_selection(false);
	}
}

void workspace_overview_client_unmapped(Client *c, bool activate_target) {
	if (!workspace_overview_is_active() || wsov.closing || !c)
		return;
	uint32_t tags = wsov_client_tags(c);
	uint32_t old_target_tag = wsov.target_tag;
	uint32_t previous_camera_tag = wsov.camera_tag;
	wsov_select_default_target();
	wsov.camera_tag = wsov.target_tag;
	Client *previous = wsov.mon->sel;
	uint32_t target_bit = wsov_tag_bit(wsov.target_tag);
	bool switch_workspace = activate_target && target_bit &&
							!(wsov.mon->tagset[wsov.mon->seltags] & target_bit);
	if (switch_workspace) {
		client_view_on_monitor(&(Arg){.ui = target_bit}, false, wsov.mon, true);
		wsov_capture_workspace_tags(target_bit);
	}
	wsov_sync_monitor_selection();
	wsov_rebuild_live_projection();
	if (switch_workspace) {
		wsov_render_tags(target_bit);
	} else {
		wsov_render_selection_clients(previous, wsov.mon->sel);
	}
	wsov_update_styles_tags(tags | wsov_tag_bit(old_target_tag) | target_bit);
	if (switch_workspace || previous_camera_tag != wsov.camera_tag)
		wsov_apply_camera(wsov.progress);
}

void workspace_overview_monitor_destroy(Monitor *m) {
	if (workspace_overview_is_active_on(m))
		wsov_finalize_close(0);
}

void workspace_overview_monitor_changed(Monitor *m) {
	if (!workspace_overview_is_active_on(m) || wsov.closing)
		return;
	wsov_update_root_geometry(m);
	wsov_schedule_rebuild();
}

void workspace_overview_reload(void) {
	if (!workspace_overview_is_active() || wsov.closing || !wsov.mon)
		return;
	wsov_update_root_geometry(wsov.mon);
	wsov_schedule_rebuild();
}
