#include "mango/workspace_overview/workspace_overview_internal.h"

bool wsov_create_drop_indicator(void) {
	if (!wsov.root)
		return false;

	wsov.drop_fill = wlr_scene_rect_create(wsov.root, 1, 1, (float[4]){0});
	if (!wsov.drop_fill)
		return false;
	wsov_set_rect_color(wsov.drop_fill, config.dropcolor);
	wsov_hide_drop_indicator();
	return true;
}

bool wsov_drag_hides_item(struct wsov_cell *cell, Client *c) {
	return wsov.moved && wsov.has_drag_client && c == wsov.drag_client &&
		   cell->tag == wsov.source_tag;
}

bool wsov_drag_hides_group(struct wsov_cell *cell, Client *group_head) {
	if (!wsov.moved || !wsov.has_drag_client || !group_head ||
		cell->tag != wsov.source_tag)
		return false;
	for (Client *cur = group_head; cur; cur = cur->group_next)
		if (cur == wsov.drag_client)
			return true;
	return false;
}

void wsov_destroy_drag_buffer(void) {
	if (wsov.drag_buffer) {
		wlr_scene_node_destroy(&wsov.drag_buffer->node);
		wsov.drag_buffer = NULL;
	}
	wsov.drag_width = 0;
	wsov.drag_height = 0;
	wsov.drag_bar_height = 0;
	wsov.drag_source_width = 0;
	wsov.drag_source_height = 0;
	wsov.drag_preview_x = 0;
	wsov.drag_preview_y = 0;
}

void wsov_begin_drag_floating(Client *c) {
	if (!c || wsov.drag_floating_changed)
		return;

	wsov.drag_was_floating = c->isfloating;
	wsov.drag_was_drag_to_tile = c->drag_to_tile;
	wsov.drag_saved_float_geom = c->float_geom;
	if (c->isfloating)
		return;
	/* The drag itself must not clear fullscreen/maximized. Leave that decision
	 * to the normal arrange path after the window is dropped on its target. */
	if (c->isfullscreen || c->ismaximizescreen)
		return;

	/* Match the normal move-drag path: a tiled window leaves the tiling model
	 * as soon as the drag starts, then returns to it on drop or cancel. */
	c->drag_to_tile = true;
	exit_scroller_stack(c);
	client_set_floating(c, 1, false);
	c->old_stack_inner_per = 0.0f;
	c->old_master_inner_per = 0.0f;
	set_size_per(c->mon, c, false);
	wsov.drag_floating_changed = true;
}

void wsov_restore_drag_floating(void) {
	Client *c = wsov.drag_client;
	if (wsov.drag_floating_changed && c) {
		c->float_geom = wsov.drag_saved_float_geom;
		c->drag_to_tile = wsov.drag_was_drag_to_tile;
		if (c->mon && c->isfloating)
			client_set_floating(c, 0, false);
		else
			c->isfloating = 0;
	}
	wsov.drag_floating_changed = false;
	wsov.drag_was_floating = false;
	wsov.drag_was_drag_to_tile = false;
	wsov.drag_saved_float_geom = (struct wlr_box){0};
}

bool wsov_capture_drag_group_bars(Client *c, struct wsov_cell *bars) {
	if (!c || !bars || !client_is_group_member(c) || !c->group_bar)
		return false;

	Client *head = client_group_head(c);
	Client *anchor = client_group_focused(head);
	if (!anchor)
		anchor = c;
	wsov_draw_group_bar_for_capture(anchor);

	bool captured = false;
	for (Client *cur = head; cur; cur = cur->group_next) {
		if (!cur->group_bar || !cur->group_bar->scene->node.enabled)
			continue;
		wsov_capture_bar(bars, cur->group_bar);
		captured = true;
	}
	return captured;
}

bool wsov_create_drag_buffer(struct wsov_item *item) {
	wsov_destroy_drag_buffer();
	if (!item || item->box.width <= 0 || item->box.height <= 0)
		return false;

	int width = item->client && item->client->isfloating
					? MANGO_MAX(1, item->box.width)
					: MANGO_MIN(item->box.width, 900);
	int workspace_width =
		MANGO_MAX(1, item->workspace_geom.width > 0 ? item->workspace_geom.width
													: item->box.width);
	int workspace_height = MANGO_MAX(1, item->workspace_geom.height > 0
											? item->workspace_geom.height
											: item->box.height);
	double output_scale = wsov_output_scale();
	double render_scale = (double)width / workspace_width;
	int window_height =
		MANGO_MAX(1, (int)lround(workspace_height * render_scale));
	struct wsov_cell drag_bars = {0};
	int bar_height = 0;
	if (wsov_capture_drag_group_bars(item->client, &drag_bars))
		bar_height =
			MANGO_MAX(1, (int)lround(config.group_bar_height * render_scale));
	int height = bar_height + window_height;
	int buffer_width = MANGO_MAX(1, (int)lround(width * output_scale));
	int buffer_height = MANGO_MAX(1, (int)lround(height * output_scale));

	struct wlr_buffer *buffer = wsov_create_buffer(buffer_width, buffer_height);
	if (!buffer) {
		wsov_clear_cell_bars(&drag_bars);
		free(drag_bars.bar_items);
		return false;
	}
	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(server.renderer, buffer, NULL);
	if (!pass) {
		wsov_clear_cell_bars(&drag_bars);
		free(drag_bars.bar_items);
		wlr_buffer_drop(buffer);
		return false;
	}

	float background[4] = {config.rootcolor[0], config.rootcolor[1],
						   config.rootcolor[2], 1.0f};
	wsov_add_rect(pass, (struct wlr_box){0, 0, buffer_width, buffer_height},
				  background, WLR_RENDER_BLEND_MODE_NONE);

	struct wsov_cell drag_cell = {
		.box = {.x = -wsov_border_width(),
				.y = -wsov_border_width(),
				.width = width + 2 * wsov_border_width(),
				.height = window_height + 2 * wsov_border_width()},
		.buffer_w = buffer_width,
		.buffer_h = buffer_height,
	};
	struct wsov_item drag_item = {
		.client = item->client,
		.box = {.x = 0,
				.y = bar_height,
				.width = width,
				.height = window_height},
		.render_scale = render_scale,
		.render_scale_x = render_scale,
		.render_scale_y = render_scale,
	};
	struct wsov_render_resources resources = {0};
	resources.target = buffer;
	wsov_render_client(pass, &drag_cell, &drag_item, &resources, output_scale);
	if (bar_height > 0) {
		pixman_region32_t clip;
		int scaled_bar_height = (int)lround(bar_height * output_scale);
		pixman_region32_init_rect(&clip, 0, 0, buffer_width, scaled_bar_height);
		double scale_x =
			(double)buffer_width / MANGO_MAX(1, item->client->geom.width);
		double scale_y =
			(double)scaled_bar_height / MANGO_MAX(1, config.group_bar_height);
		int origin_y = item->client->geom.y - config.group_bar_height;
		for (int i = 0; i < drag_bars.bar_item_count; i++) {
			wsov_render_bar_item_at(pass, &drag_bars.bar_items[i], &resources,
									item->client->geom.x, origin_y, scale_x,
									scale_y, &clip);
		}
		pixman_region32_fini(&clip);
	}

	if (!wlr_render_pass_submit(pass)) {
		wsov_release_textures(&resources);
		wsov_clear_cell_bars(&drag_bars);
		free(drag_bars.bar_items);
		wlr_buffer_drop(buffer);
		return false;
	}
	wsov_release_textures(&resources);
	wsov_clear_cell_bars(&drag_bars);
	free(drag_bars.bar_items);

	wsov.drag_buffer = wlr_scene_buffer_create(wsov.root, buffer);
	wlr_buffer_drop(buffer);
	if (!wsov.drag_buffer)
		return false;
	wlr_scene_buffer_set_dest_size(wsov.drag_buffer, width, height);
	wlr_scene_buffer_set_filter_mode(wsov.drag_buffer,
									 WLR_SCALE_FILTER_BILINEAR);
	wlr_scene_node_raise_to_top(&wsov.drag_buffer->node);
	wsov.drag_bar_height = bar_height;
	wsov.drag_width = width;
	wsov.drag_height = height;
	wsov.drag_source_width = item->box.width;
	wsov.drag_source_height = item->box.height;
	return true;
}

void wsov_cancel_drag(void) {
	bool had_drag =
		wsov.pressed || wsov.moved || wsov.has_drag_client || wsov.drag_buffer;
	uint32_t return_tag = wsov.drag_return_tag;
	wsov_hide_drop_indicator();
	wsov_restore_drag_floating();
	wsov_destroy_drag_buffer();
	wsov.pressed = false;
	wsov.moved = false;
	wsov.has_drag_client = false;
	wsov.drag_scroll_direction = 0;
	wsov.drag_scroll_frame_valid = false;
	wsov.drag_client = NULL;
	wsov.source_tag = 0;
	wsov.drag_return_tag = 0;
	if (had_drag && return_tag) {
		wsov.target_tag = return_tag;
		wsov.camera_tag = return_tag;
	}
	if (wsov.mon && !server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");
}

bool wsov_item_is_drop_target(const struct wsov_item *item) {
	return item && !item->is_snapshot && item->client &&
		   item->client != wsov.drag_client && ISTILED(item->client);
}

int32_t wsov_cell_tiling_count(const struct wsov_cell *cell) {
	int32_t count = 0;
	if (!cell)
		return 0;
	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (!item->is_snapshot && item->client != wsov.drag_client &&
			ISTILED(item->client))
			count++;
	}
	return count;
}

struct wsov_item *wsov_drop_item_at(struct wsov_cell *cell, double x,
									double y) {
	struct wsov_item *hit = wsov_item_at(cell, x, y);
	if (wsov_item_is_drop_target(hit))
		return hit;
	if (!cell || !wsov.mon)
		return NULL;

	struct wsov_item *nearest = NULL;
	double best = 1e30;
	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	for (int i = 0; i < cell->item_count; i++) {
		struct wsov_item *item = &cell->items[i];
		if (!wsov_item_is_drop_target(item))
			continue;
		struct wlr_box box = wsov_item_display_box(cell, item);
		double cx = box.x + box.width / 2.0;
		double cy = box.y + box.height / 2.0;
		double distance = (cx - lx) * (cx - lx) + (cy - ly) * (cy - ly);
		if (distance < best) {
			best = distance;
			nearest = item;
		}
	}
	return nearest;
}

bool wsov_get_drop_target(double x, double y, struct wsov_cell **cell_out,
						  struct wsov_item **item_out, struct wlr_box *zone_out,
						  int32_t *direction_out) {
	struct wsov_cell *cell = wsov_drop_cell_at(x, y);
	if (!cell || !wsov.mon || !wsov.drag_client || wsov.drag_was_floating) {
		return false;
	}

	struct wsov_item *item = wsov_drop_item_at(cell, x, y);
	if (!item)
		return false;

	const Layout *layout = wsov.mon->pertag->ltidxs[cell->tag];
	struct wlr_box item_box = wsov_item_final_display_box(cell, item);
	if (item_box.width <= 0 || item_box.height <= 0)
		return false;
	double rel_x = x - wsov.mon->m.x - item_box.x;
	double rel_y = y - wsov.mon->m.y - item_box.y;
	struct wlr_box zone;
	int32_t direction = client_drop_zone(
		layout, item->client->ismaster, wsov_cell_tiling_count(cell), rel_x,
		rel_y, item_box.width, item_box.height, &zone);
	zone.x += item_box.x;
	zone.y += item_box.y;
	item->client->drop_direction = direction;

	if (cell_out)
		*cell_out = cell;
	if (item_out)
		*item_out = item;
	if (zone_out)
		*zone_out = zone;
	if (direction_out)
		*direction_out = direction;
	return true;
}

void wsov_hide_drop_indicator(void) {
	wsov.drop_indicator_visible = false;
	if (wsov.drop_fill)
		wlr_scene_node_set_enabled(&wsov.drop_fill->node, false);
}

void wsov_update_drop_indicator(double x, double y) {
	struct wsov_cell *cell = NULL;
	struct wlr_box zone;
	if (!wsov_get_drop_target(x, y, &cell, NULL, &zone, NULL) ||
		zone.width <= 0 || zone.height <= 0) {
		wsov_hide_drop_indicator();
		return;
	}

	if (cell->tag != wsov.target_tag) {
		wsov.target_tag = cell->tag;
		wsov.camera_tag = cell->tag;
		wsov_refresh_target_selection();
	}

	int32_t width = MANGO_MAX(1, zone.width);
	int32_t height = MANGO_MAX(1, zone.height);
	wsov_place_rect_clipped(wsov.drop_fill, zone.x, zone.y, width, height,
							true);
	if (!wsov.drop_fill)
		return;

	wsov.drop_indicator_visible = true;

	wlr_scene_node_raise_to_top(&wsov.drop_fill->node);
	if (wsov.drag_buffer)
		wlr_scene_node_raise_to_top(&wsov.drag_buffer->node);
}

void wsov_place_floating(Client *c, struct wsov_cell *cell) {
	if (!c || !cell || !wsov.mon)
		return;
	int width = MANGO_MAX(64, c->float_geom.width > 0 ? c->float_geom.width
													  : c->geom.width);
	int height = MANGO_MAX(64, c->float_geom.height > 0 ? c->float_geom.height
														: c->geom.height);
	struct wlr_box cell_box = wsov_cell_layout_box(cell);
	double scale_x = (double)wsov.mon->m.width /
					 MANGO_MAX(1, cell_box.width - 2 * wsov_border_width());
	double scale_y = (double)wsov.mon->m.height /
					 MANGO_MAX(1, cell_box.height - 2 * wsov_border_width());
	struct wsov_cell *camera_cell = wsov_cell_for_tag(wsov.camera_tag);
	if (!camera_cell)
		camera_cell = cell;
	struct wlr_box camera_box = wsov_cell_layout_box(camera_cell);
	double progress = fmax(0.0, fmin(1.0, wsov.progress));
	double final_viewport_x =
		wsov_scrolls_horizontally() ? wsov.scroll_offset : 0.0;
	double final_viewport_y =
		wsov_scrolls_vertically() ? wsov.scroll_offset : 0.0;
	double viewport_x =
		camera_box.x + wsov_border_width() +
		(final_viewport_x - camera_box.x - wsov_border_width()) * progress;
	double viewport_y =
		camera_box.y + wsov_border_width() +
		(final_viewport_y - camera_box.y - wsov_border_width()) * progress;
	double viewport_w =
		MANGO_MAX(1, camera_box.width - 2 * wsov_border_width());
	double viewport_h =
		MANGO_MAX(1, camera_box.height - 2 * wsov_border_width());
	viewport_w += (wsov.mon->m.width - viewport_w) * progress;
	viewport_h += (wsov.mon->m.height - viewport_h) * progress;
	double camera_scale_x =
		(double)wsov.mon->m.width / MANGO_MAX(1.0, viewport_w);
	double camera_scale_y =
		(double)wsov.mon->m.height / MANGO_MAX(1.0, viewport_h);
	double preview_strip_x =
		viewport_x + (double)wsov.drag_preview_x / camera_scale_x;
	double preview_strip_y =
		viewport_y +
		(double)(wsov.drag_preview_y + wsov.drag_bar_height) / camera_scale_y;
	double content_x = preview_strip_x - cell_box.x - wsov_border_width();
	double content_y = preview_strip_y - cell_box.y - wsov_border_width();
	int nx = wsov.mon->m.x + (int)lround(content_x * scale_x);
	int ny = wsov.mon->m.y + (int)lround(content_y * scale_y);
	c->float_geom = (struct wlr_box){nx, ny, width, height};
	resize(c, c->float_geom,
		   (ResizeOpts){.interact = 0, .skip_animation = true});
}

void wsov_finish_drop(double x, double y) {
	Client *c = wsov.drag_client;
	struct wsov_cell *target = wsov_drop_cell_at(x, y);
	if (!c || !target || !wsov_client_eligible(c)) {
		uint32_t tags = wsov_client_tags(c) | wsov_tag_bit(wsov.source_tag);
		wsov_cancel_drag();
		wsov_refresh_workspace_or_rebuild(tags);
		return;
	}

	struct wsov_item *over = NULL;
	wsov_get_drop_target(x, y, &target, &over, NULL, NULL);
	bool was_floating = wsov.drag_was_floating;

	uint32_t source_bit = wsov_tag_bit(wsov.source_tag);
	uint32_t target_bit = wsov_tag_bit(target->tag);
	uint32_t old_tags = c->isglobal ? 0 : c->tags;
	uint32_t mods = keyboard_hard_modifiers();

	if (!c->isglobal && target->tag != wsov.source_tag) {
		if (mods & WLR_MODIFIER_CTRL)
			c->tags = target_bit;
		else if (mods & WLR_MODIFIER_SHIFT)
			c->tags |= target_bit;
		else
			c->tags = (c->tags & ~source_bit) | target_bit;
		c->tags &= TAGMASK;
		if (!c->tags)
			c->tags = target_bit;
		if (old_tags != c->tags)
			client_reparent_group(c);
	}
	clear_fullscreen_for_incoming_tiled_client(c);

	client_view_on_monitor(&(Arg){.ui = target_bit}, false, wsov.mon, false);
	if (over)
		pointer_place_drag_tile_on(c, over->client);
	if (was_floating)
		wsov_place_floating(c, target);

	wsov_restore_drag_floating();
	client_focus(c, 1);
	arrange(wsov.mon, false, false);

	wsov_hide_drop_indicator();
	wsov.pressed = false;
	wsov.moved = false;
	wsov.has_drag_client = false;
	wsov.drag_scroll_direction = 0;
	wsov.drag_scroll_frame_valid = false;
	wsov.drag_client = NULL;
	wsov.source_tag = 0;
	wsov.target_tag = target->tag;
	wsov.scroll_center_tag = target->tag;
	wsov.camera_tag = target->tag;
	wsov_animate_scroll_to_target();
	wsov.drag_return_tag = 0;
	wsov_destroy_drag_buffer();
	if (!server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");
	uint32_t affected_tags = source_bit | target_bit;
	if (c->isglobal)
		affected_tags |= TAGMASK;
	else
		affected_tags |= old_tags | c->tags;
	wsov_refresh_workspace_or_rebuild(affected_tags);
}
