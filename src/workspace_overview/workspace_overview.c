#include "mango/workspace_overview/workspace_overview_internal.h"

struct wsov_state wsov;

#ifdef MANGO_WORKSPACE_OVERVIEW_SCENEFX

struct wsov_background_effects {
	struct wlr_scene_blur *blur;
};

struct wsov_background_effects *
wsov_background_effects_create(struct wlr_scene_tree *parent, int width,
							   int height) {
	if (!parent || !config.blur)
		return NULL;

	struct wsov_background_effects *effects = calloc(1, sizeof(*effects));
	if (!effects)
		return NULL;
	effects->blur = wlr_scene_blur_create(parent, width, height);
	if (!effects->blur) {
		free(effects);
		return NULL;
	}

	/*
	 * The wsov background is always directly above the wallpaper backdrop, so
	 * it can always use the optimized blur cache.
	 */
	wlr_scene_blur_set_should_only_blur_bottom_layer(effects->blur, true);
	wlr_scene_blur_set_strength(effects->blur, 1.0f);
	wlr_scene_blur_set_alpha(effects->blur, 1.0f);
	return effects;
}

void wsov_background_effects_update(struct wsov_background_effects *effects,
									int width, int height) {
	if (effects && effects->blur)
		wlr_scene_blur_set_size(effects->blur, width, height);
}

void wsov_background_effects_destroy(struct wsov_background_effects *effects) {
	if (!effects)
		return;
	if (effects->blur)
		wlr_scene_node_destroy(&effects->blur->node);
	free(effects);
}

#else

struct wsov_background_effects *
wsov_background_effects_create(struct wlr_scene_tree *parent, int width,
							   int height) {
	(void)parent;
	(void)width;
	(void)height;
	return NULL;
}

void wsov_background_effects_update(struct wsov_background_effects *effects,
									int width, int height) {
	(void)effects;
	(void)width;
	(void)height;
}

void wsov_background_effects_destroy(struct wsov_background_effects *effects) {
	(void)effects;
}

#endif

void wsov_flush_rebuild(void *data) {
	(void)data;
	wsov.rebuild_idle = NULL;
	wsov_rebuild();
}

void wsov_schedule_rebuild(void) {
	if (wsov.rebuild_idle)
		return;
	wsov.rebuild_idle =
		wl_event_loop_add_idle(server.event_loop, wsov_flush_rebuild, NULL);
	if (!wsov.rebuild_idle)
		wsov_rebuild();
}

void wsov_cancel_scheduled_rebuild(void) {
	if (!wsov.rebuild_idle)
		return;
	wl_event_source_remove(wsov.rebuild_idle);
	wsov.rebuild_idle = NULL;
}

void wsov_hide_clients(bool hidden) {
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != wsov.mon || client_is_unmanaged(c))
			continue;
		c->wsov_hidden = hidden;
		client_update_visibility(c);
	}
}

void wsov_start_animation(bool closing, double target) {
	if (!config.animations) {
		wsov.closing = closing;
		wsov.layout_animating = false;
		wsov.animating = false;
		wsov.animation_to = target;
		wsov.progress = target;
		wsov_apply_camera(target);
		if (closing)
			wsov_finalize_close(wsov.close_tag);
		else
			wsov_detach_frame_listener();
		return;
	}

	wsov.animation_from = fmax(0.0, fmin(1.0, wsov.progress));
	wsov.animation_to = fmax(0.0, fmin(1.0, target));
	wsov.closing = closing;
	wsov.animating = true;
	clock_gettime(CLOCK_MONOTONIC, &wsov.animation_start);
	wsov_attach_frame_listener();
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void wsov_resume_open_animation(void) {
	if (!workspace_overview_is_active() || !wsov.closing)
		return;

	wsov.target_tag = wsov.camera_tag;
	wsov.close_tag = 0;
	wsov.close_client = NULL;
	wsov.close_focus_explicit = false;
	wsov.layers_dirty = true;
	wsov_set_borders_enabled(true);
	wsov_update_styles();
	wsov_rebuild_live_projection();
	wsov_render_tags(wsov_tag_bit(wsov.target_tag));
	wsov_start_animation(false, 1.0);
}

void wsov_close(uint32_t focus_tag, Client *focus_client) {
	if (!workspace_overview_is_active() || wsov.closing)
		return;
	bool pointer_close = wsov.pressed;
	uint32_t previous_target = wsov.target_tag;
	wsov_cancel_scheduled_render();
	wsov_cancel_drag();
	wsov.scroll_animating = false;
	wsov.close_client = focus_client;
	wsov.close_focus_explicit = pointer_close;
	if (!config.animations) {
		wsov_finalize_close(focus_tag);
		return;
	}

	wsov_set_dim_enabled(false);
	wsov_set_borders_enabled(false);
	wsov.closing = true;
	wsov.layers_dirty = false;
	/* Keep the live projection through the close animation so surface effects
	 * continue to scale with the workspace. wsov_destroy_cells() removes them
	 * only after the animation reaches its final frame. */
	wsov.close_tag = focus_tag;
	uint32_t camera_tag = focus_tag ? focus_tag : wsov_active_camera_tag();
	if (!camera_tag || !wsov_cell_for_tag(camera_tag))
		camera_tag = previous_target;
	wsov.camera_tag = camera_tag;
	wsov_apply_camera(wsov.progress);
	wsov_start_animation(true, 0.0);
}

void wsov_finish_client_geometry(Client *c) {
	if (!c || !c->scene)
		return;

	if (c->animation.running && c->animation.action == MOVE) {
		wlr_scene_node_set_position(&c->scene->node, c->animation.current.x,
									c->animation.current.y);
		c->need_output_flush = true;
		return;
	}
	client_set_finish_geometry(c);
}

void wsov_finalize_close(uint32_t focus_tag) {
	if (!workspace_overview_is_active())
		return;

	Monitor *m = wsov.mon;
	Client *focus_client = wsov.close_client;
	bool focus_explicit = wsov.close_focus_explicit;
	wsov_cancel_scheduled_rebuild();
	wsov_cancel_drag();
	wsov.closing = true;
	m->is_ws_overview = false;

	wsov_detach_frame_listener();
	wlr_seat_pointer_clear_focus(server.seat);
	if (!server.exclusive_focus)
		wlr_seat_keyboard_notify_clear_focus(server.seat);
	wsov_destroy_cells();
	wsov_restore_direct_layers();
	wsov_destroy_layer_fadeouts();
	if (wsov.direct_layer_tree)
		wlr_scene_node_destroy(&wsov.direct_layer_tree->node);
	wsov_background_effects_destroy(wsov.background_effects);
	if (wsov.root)
		wlr_scene_node_destroy(&wsov.root->node);
	wsov.root = NULL;
	wsov.background = NULL;
	wsov.background_effects = NULL;
	wsov.direct_layer_tree = NULL;
	wsov.direct_top_tree = NULL;
	wsov.direct_overlay_tree = NULL;
	wsov.direct_fadeout_tree = NULL;

	if (focus_tag) {
		client_view_on_monitor(&(Arg){.ui = wsov_tag_bit(focus_tag)}, false, m,
							   false);
	} else {
		arrange(m, false, false);
	}

	/* Arrange the destination view while windows are still hidden. Only then
	 * reveal the scene, avoiding a one-frame flash of every workspace. */
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !client_is_unmanaged(c))
			wsov_finish_client_geometry(c);
	}
	wsov_hide_clients(false);
	Client *fadeout;
	wl_list_for_each(fadeout, &server.fadeout_clients, fadeout_link) {
		if (fadeout->mon == m && fadeout->scene) {
			wlr_scene_node_set_enabled(&fadeout->scene->node, true);
		}
	}
	memset(&wsov, 0, sizeof(wsov));
	pointer_cursor_activity();

	if (!focus_explicit && !focus_client)
		focus_client = client_focus_top(m);
	client_focus(focus_client, 1);
	/* Opening the overview cleared the keyboard focus, so client_focus() can no
	 * longer see that an exclusive layer owned it. Restore the layer after the
	 * client focus state has been updated to keep exclusive focus
	 * authoritative.
	 */
	reset_exclusive_layers_focus(m);
	pointer_process_motion(0, NULL, 0, 0, 0, 0);
	wlr_output_schedule_frame(m->wlr_output);
}

bool workspace_overview_is_active(void) {
	return wsov.mon && wsov.mon->is_ws_overview && wsov.root;
}

bool workspace_overview_is_active_on(const Monitor *m) {
	return workspace_overview_is_active() && wsov.mon == m;
}

bool workspace_overview_is_closing_on(const Monitor *m) {
	return m && wsov.mon == m && wsov.closing;
}

bool workspace_overview_pointer_is_pressed(void) {
	return workspace_overview_is_active() && wsov.pressed;
}

void wsov_update_root_geometry(Monitor *m) {
	if (!wsov.root || !m)
		return;
	wlr_scene_node_set_position(&wsov.root->node, m->m.x, m->m.y);
	wsov_background_effects_update(wsov.background_effects, m->m.width,
								   m->m.height);
	if (wsov.background) {
		wlr_scene_rect_set_size(wsov.background, m->m.width, m->m.height);
		const float *color = wsov.background_effects
								 ? (float[4]){0.0f, 0.0f, 0.0f, 0.0f}
								 : config.rootcolor;
		wlr_scene_rect_set_color(wsov.background, color);
	}
}

bool wsov_open(Monitor *m, WorkspaceOverviewLayout layout) {
	if (!m || workspace_overview_is_active() || m->isoverview)
		return false;

	memset(&wsov, 0, sizeof(wsov));
	wsov.mon = m;
	wsov.layout = layout;
	if (m->workspace_overview_scope == OCCUPIED_WORKSPACE &&
		config.workspace_overview_occupied_keep_seen_tags) {
		for (int tag = 1; tag <= config.tag_num; tag++) {
			if (wsov_tag_has_normal_client((uint32_t)tag))
				wsov.retained_tags |= wsov_tag_bit((uint32_t)tag);
		}
		wsov.retained_tags |= wsov_tag_bit(get_mon_curtag(m));
	}
	int count = 0;
	wsov_collect_workspace_tags(m, &count);
	if (count <= 0) {
		memset(&wsov, 0, sizeof(wsov));
		return false;
	}

	m->is_ws_overview = true;

	wsov.root = wlr_scene_tree_create(server.layers[LyrSpecialOverlay]);
	if (!wsov.root) {
		m->is_ws_overview = false;
		memset(&wsov, 0, sizeof(wsov));
		return false;
	}
	wsov.direct_layer_tree =
		wlr_scene_tree_create(server.layers[LyrSpecialOverlay]);
	if (wsov.direct_layer_tree) {
		wsov.direct_top_tree = wlr_scene_tree_create(wsov.direct_layer_tree);
		wsov.direct_overlay_tree =
			wlr_scene_tree_create(wsov.direct_layer_tree);
		wsov.direct_fadeout_tree =
			wlr_scene_tree_create(wsov.direct_layer_tree);
	}
	if (!wsov.direct_layer_tree || !wsov.direct_top_tree ||
		!wsov.direct_overlay_tree || !wsov.direct_fadeout_tree) {
		wsov_finalize_close(0);
		return false;
	}
	wsov_sync_direct_layers();
	wsov.background_effects =
		wsov_background_effects_create(wsov.root, m->m.width, m->m.height);
	wsov.background = wlr_scene_rect_create(wsov.root, m->m.width, m->m.height,
											config.rootcolor);
	wsov_update_root_geometry(m);
	if (!wsov_create_drop_indicator()) {
		wsov_finalize_close(0);
		return false;
	}
	wsov.animating = config.animations;
	wsov.progress = config.animations ? 0.0 : 1.0;
	wsov.animation_from = 0.0;
	wsov.animation_to = 1.0;
	clock_gettime(CLOCK_MONOTONIC, &wsov.animation_start);

	wsov_hide_clients(true);
	wlr_seat_pointer_clear_focus(server.seat);
	if (!server.exclusive_focus)
		wlr_seat_keyboard_notify_clear_focus(server.seat);
	pointer_cursor_activity();
	if (!server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");

	if (!wsov_create_cells()) {
		wsov_finalize_close(0);
		return false;
	}
	if (wsov.animating || wsov.layout_animating)
		wsov_attach_frame_listener();
	wlr_output_schedule_frame(m->wlr_output);
	return true;
}

void wsov_rebuild(void) {
	if (!workspace_overview_is_active())
		return;
	if (wsov.closing)
		return;
	wsov_cancel_scheduled_rebuild();

	wsov_cancel_drag();
	wsov.scroll_animating = false;
	if (!wsov_create_cells()) {
		wsov_finalize_close(0);
		return;
	}
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

bool wsov_refresh_workspace_tags(uint32_t tags) {
	if (!workspace_overview_is_active() || wsov.closing || !wsov.mon ||
		wsov.cell_count <= 0)
		return false;

	uint32_t current = 0;
	for (int i = 0; i < wsov.cell_count; i++)
		current |= wsov_tag_bit(wsov.cells[i].tag);

	/* Empty workspaces keep their cells in ALL_WORKSPACE mode. Only occupied
	 * mode can remove a cell and thereby change every preview's grid position.
	 */
	if (wsov.mon->workspace_overview_scope == OCCUPIED_WORKSPACE) {
		int expected_count = 0;
		uint32_t expected =
			wsov_collect_workspace_tags(wsov.mon, &expected_count);
		if (expected_count != wsov.cell_count || expected != current)
			return false;
	}

	tags &= current;
	if (!tags)
		return true;

	wsov_sync_monitor_selection();
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!(tags & wsov_tag_bit(cell->tag)))
			continue;
		wsov_capture_cell_items(cell);
		/*
		 * Keep cached strip snapshots for inactive workspaces. They were
		 * captured under their own tag by the layout-change path; replacing
		 * them here would apply the active tag's projection to every cell.
		 */
		if (cell->tag == get_mon_curtag(wsov.mon))
			wsov_capture_cell_bars(cell);
		wsov_position_cell_items(cell);
	}

	wsov_layout_live_projection();
	wsov_render_tags(tags);
	wsov_update_styles_tags(tags);
	if (wsov_refresh_animated_cells()) {
		wsov.client_animating = true;
		wsov_attach_frame_listener();
	}
	wlr_output_schedule_frame(wsov.mon->wlr_output);
	return true;
}

void wsov_refresh_workspace_or_rebuild(uint32_t tags) {
	if (!wsov_refresh_workspace_tags(tags))
		wsov_rebuild();
}

int32_t toggle_workspace_overview(const Arg *arg) {
	if (!server.selected_monitor || server.grab_client ||
		server.selected_monitor->isoverview)
		return 0;
	if (workspace_overview_is_active()) {
		if (wsov.closing)
			wsov_resume_open_animation();
		else
			wsov_close(wsov.target_tag, NULL);
	} else {
		Monitor *m = server.selected_monitor;
		m->workspace_overview_scope =
			arg->i == OCCUPIED_WORKSPACE ? OCCUPIED_WORKSPACE : ALL_WORKSPACE;
		WorkspaceOverviewLayout layout = arg->i2 == WORKSPACE_OVERVIEW_SCROLL
											 ? WORKSPACE_OVERVIEW_SCROLL
											 : WORKSPACE_OVERVIEW_GRID;
		wsov_open(m, layout);
	}
	return 0;
}

int32_t enter_workspace_overview(const Arg *arg) {
	if (!server.selected_monitor || server.grab_client ||
		server.selected_monitor->isoverview)
		return 0;
	if (workspace_overview_is_active()) {
		wsov_resume_open_animation();
	} else {
		Monitor *m = server.selected_monitor;
		m->workspace_overview_scope =
			arg->i == OCCUPIED_WORKSPACE ? OCCUPIED_WORKSPACE : ALL_WORKSPACE;
		WorkspaceOverviewLayout layout = arg->i2 == WORKSPACE_OVERVIEW_SCROLL
											 ? WORKSPACE_OVERVIEW_SCROLL
											 : WORKSPACE_OVERVIEW_GRID;
		wsov_open(m, layout);
	}
	return 0;
}

int32_t leave_workspace_overview(const Arg *arg) {
	if (workspace_overview_is_active())
		wsov_close(0, NULL);
	return 0;
}
