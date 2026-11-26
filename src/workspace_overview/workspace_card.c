#include "mango/workspace_overview/workspace_overview_internal.h"

bool wsov_scrolls_horizontally(void) {
	return wsov.layout == WORKSPACE_OVERVIEW_SCROLL &&
		   config.workspace_overview_scroll_direction ==
			   WORKSPACE_OVERVIEW_SCROLL_HORIZONTAL;
}

bool wsov_scrolls_vertically(void) {
	return wsov.layout == WORKSPACE_OVERVIEW_SCROLL &&
		   config.workspace_overview_scroll_direction ==
			   WORKSPACE_OVERVIEW_SCROLL_VERTICAL;
}

int wsov_border_width(void) {
	return wsov.layout == WORKSPACE_OVERVIEW_SCROLL
			   ? 0
			   : config.workspace_overview_border_width;
}

void wsov_update_scroll_bounds(void) {
	if (!wsov.mon || wsov.cell_count <= 0 ||
		wsov.layout != WORKSPACE_OVERVIEW_SCROLL) {
		wsov.scroll_offset = 0.0;
		wsov.scroll_min = 0.0;
		wsov.scroll_max = 0.0;
		return;
	}

	int gap = config.workspace_overview_gap;
	double content_extent = 0.0;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wlr_box box = wsov_cell_layout_box(&wsov.cells[i]);
		double extent = wsov_scrolls_horizontally() ? box.x + box.width
													: box.y + box.height;
		content_extent = fmax(content_extent, extent);
	}

	double viewport_extent =
		wsov_scrolls_horizontally() ? wsov.mon->m.width : wsov.mon->m.height;
	double item_extent = wsov_scrolls_horizontally() ? wsov.cells[0].box.width
													 : wsov.cells[0].box.height;
	double pad = fmax(0.0, (viewport_extent - item_extent) / 2.0) + gap;
	wsov.scroll_min = -pad;
	wsov.scroll_max =
		fmax(wsov.scroll_min, content_extent - viewport_extent + pad);
	wsov.scroll_offset =
		fmax(wsov.scroll_min, fmin(wsov.scroll_max, wsov.scroll_offset));
}

double wsov_clamp_scroll(double offset) {
	return fmax(wsov.scroll_min, fmin(wsov.scroll_max, offset));
}

bool wsov_scroll_to(double offset) {
	double clamped = wsov_clamp_scroll(offset);
	if (fabs(clamped - wsov.scroll_offset) < 0.5)
		return false;
	wsov.scroll_offset = clamped;
	return true;
}

bool wsov_animate_scroll_to(double offset) {
	double clamped = wsov_clamp_scroll(offset);
	if (fabs(clamped - wsov.scroll_offset) < 0.5)
		return false;
	if (!config.animations) {
		wsov.scroll_animating = false;
		wsov.scroll_offset = clamped;
		return true;
	}

	wsov.scroll_animation_from = wsov.scroll_offset;
	wsov.scroll_animation_to = clamped;
	wsov.scroll_animation_duration_ms = animation_curve_duration_ms(
		&config.animation_curve_wsov_move, config.animation_duration_wsov_move);
	wsov.scroll_animating = true;
	clock_gettime(CLOCK_MONOTONIC, &wsov.scroll_animation_start);
	wsov_attach_frame_listener();
	wlr_output_schedule_frame(wsov.mon->wlr_output);
	return true;
}

void wsov_update_scroll_animation(void) {
	if (!wsov.scroll_animating)
		return;
	double elapsed_ms = wsov_elapsed_ms(&wsov.scroll_animation_start);
	const AnimationCurve *curve = &config.animation_curve_wsov_move;
	double t =
		elapsed_ms / MANGO_MAX(1.0, (double)wsov.scroll_animation_duration_ms);
	t = fmax(0.0, fmin(1.0, t));
	double eased = find_animation_curve_for(t, curve);
	wsov.scroll_offset =
		wsov.scroll_animation_from +
		(wsov.scroll_animation_to - wsov.scroll_animation_from) * eased;
	if (elapsed_ms >= wsov.scroll_animation_duration_ms) {
		wsov.scroll_offset = wsov.scroll_animation_to;
		wsov.scroll_animating = false;
	}
}

double wsov_scroll_stride(void) {
	if (wsov.cell_count <= 0)
		return 1.0;
	if (wsov.cell_count > 1) {
		return wsov_scrolls_horizontally()
				   ? wsov.cells[1].box.x - wsov.cells[0].box.x
				   : wsov.cells[1].box.y - wsov.cells[0].box.y;
	}
	double extent = wsov_scrolls_horizontally() ? wsov.cells[0].box.width
												: wsov.cells[0].box.height;
	return extent + config.workspace_overview_gap;
}

bool wsov_step_scroll_workspaces(int32_t direction, uint32_t count) {
	if (direction == 0 || count == 0 ||
		wsov.layout != WORKSPACE_OVERVIEW_SCROLL)
		return false;
	int current = -1;
	uint32_t center_tag = wsov_cell_for_tag(wsov.scroll_center_tag)
							  ? wsov.scroll_center_tag
							  : wsov.target_tag;
	for (int i = 0; i < wsov.cell_count; i++) {
		if (wsov.cells[i].tag == center_tag) {
			current = i;
			break;
		}
	}
	if (current < 0)
		return false;
	uint32_t offset = MANGO_MIN(count, (uint32_t)wsov.cell_count);
	int next = direction > 0
				   ? MANGO_MIN(wsov.cell_count - 1, current + (int)offset)
				   : MANGO_MAX(0, current - (int)offset);
	if (next == current)
		return false;

	struct wsov_cell *target = &wsov.cells[next];
	wsov.target_tag = target->tag;
	wsov.scroll_center_tag = target->tag;
	if (!wsov.animating)
		wsov.camera_tag = target->tag;
	wsov_animate_scroll_to_target();
	wsov_refresh_target_selection();
	return true;
}

void wsov_apply_scroll_wheel(double notches, uint32_t time_msec) {
	int32_t direction = notches > 0 ? 1 : -1;
	bool continuing =
		wsov.scroll_wheel.tracking && direction == wsov.scroll_wheel.direction;
	if (!continuing) {
		wsov.scroll_wheel.accum = 0.0;
		wsov.scroll_wheel.direction = direction;
		wsov.scroll_wheel.tracking = true;
	}

	double velocity = 0.0;
	if (continuing) {
		double dt = (double)(time_msec - wsov.scroll_wheel.last_time_msec);
		velocity = fabs(notches) * 1000.0 / fmax(1.0, dt);
	}
	wsov.scroll_wheel.last_time_msec = time_msec;

	double factor = 1.0;
	double threshold_interval =
		fmax(1.0, config.workspace_overview_scroll_decel_start_interval);
	double threshold_velocity = 1000.0 / threshold_interval;
	double gain =
		fmax(0.0, fmin(1.0, config.workspace_overview_scroll_decel_gain));
	if (velocity > threshold_velocity) {
		double output_velocity =
			threshold_velocity + (velocity - threshold_velocity) * gain;
		factor = output_velocity / velocity;
	}

	double credit = fmin(1024.0, fabs(notches) * factor);
	wsov.scroll_wheel.accum += direction * credit;
	double whole = trunc(wsov.scroll_wheel.accum);
	if (whole == 0.0)
		return;
	wsov.scroll_wheel.accum -= whole;

	direction = whole > 0.0 ? 1 : -1;
	uint32_t count = (uint32_t)fmin(fabs(whole), (double)UINT32_MAX);
	wsov_step_scroll_workspaces(direction, count);
}

void wsov_update_drag_scroll(double x, double y) {
	int32_t direction = 0;
	if (wsov.layout == WORKSPACE_OVERVIEW_SCROLL && wsov.pressed &&
		wsov.moved && wsov.has_drag_client && wsov.mon) {
		double lx = x - wsov.mon->m.x;
		double ly = y - wsov.mon->m.y;
		if (lx >= 0 && ly >= 0 && lx < wsov.mon->m.width &&
			ly < wsov.mon->m.height) {
			double along = wsov_scrolls_horizontally() ? lx : ly;
			double viewport = wsov_scrolls_horizontally() ? wsov.mon->m.width
														  : wsov.mon->m.height;
			double edge = fmax(24.0, fmin(96.0, viewport * 0.05));
			if (along < edge)
				direction = -1;
			else if (along > viewport - edge)
				direction = 1;
		}
	}
	if (direction == wsov.drag_scroll_direction)
		return;

	wsov.drag_scroll_direction = direction;
	wsov.drag_scroll_frame_valid = false;
	if (direction == 0)
		return;
	wsov_attach_frame_listener();
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

bool wsov_advance_drag_scroll(void) {
	if (!wsov.mon || !server.cursor || wsov.drag_scroll_direction == 0)
		return false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!wsov.drag_scroll_frame_valid) {
		wsov.drag_scroll_last_frame = now;
		wsov.drag_scroll_frame_valid = true;
		return false;
	}
	double dt = (now.tv_sec - wsov.drag_scroll_last_frame.tv_sec) +
				(now.tv_nsec - wsov.drag_scroll_last_frame.tv_nsec) / 1e9;
	wsov.drag_scroll_last_frame = now;
	dt = fmax(0.001, fmin(0.05, dt));

	double speed =
		wsov_scroll_stride() * config.workspace_overview_drag_scroll_speed;
	double offset =
		wsov.scroll_offset + wsov.drag_scroll_direction * speed * dt;
	if (!wsov_scroll_to(offset))
		return false;
	wsov_update_drop_indicator(server.cursor->x, server.cursor->y);
	return true;
}

void wsov_scroll_to_target(void) {
	if (wsov.layout != WORKSPACE_OVERVIEW_SCROLL)
		return;
	if (!wsov_cell_for_tag(wsov.scroll_center_tag))
		wsov.scroll_center_tag = wsov.target_tag;
	struct wsov_cell *target = wsov_cell_for_tag(wsov.scroll_center_tag);
	if (!target || !wsov.mon)
		return;
	struct wlr_box box = wsov_cell_layout_box(target);
	double center = wsov_scrolls_horizontally() ? box.x + box.width / 2.0
												: box.y + box.height / 2.0;
	double viewport_center = wsov_scrolls_horizontally()
								 ? wsov.mon->m.width / 2.0
								 : wsov.mon->m.height / 2.0;
	wsov_scroll_to(center - viewport_center);
}

void wsov_animate_scroll_to_final_target(void) {
	if (wsov.layout != WORKSPACE_OVERVIEW_SCROLL)
		return;
	if (!wsov_cell_for_tag(wsov.scroll_center_tag))
		wsov.scroll_center_tag = wsov.target_tag;
	struct wsov_cell *target = wsov_cell_for_tag(wsov.scroll_center_tag);
	if (!target || !wsov.mon)
		return;
	struct wlr_box box = target->box;
	double center = wsov_scrolls_horizontally() ? box.x + box.width / 2.0
												: box.y + box.height / 2.0;
	double viewport_center = wsov_scrolls_horizontally()
								 ? wsov.mon->m.width / 2.0
								 : wsov.mon->m.height / 2.0;
	wsov_animate_scroll_to(center - viewport_center);
}

void wsov_animate_scroll_to_target(void) {
	if (wsov.layout != WORKSPACE_OVERVIEW_SCROLL)
		return;
	struct wsov_cell *target = wsov_cell_for_tag(wsov.target_tag);
	if (!target || !wsov.mon)
		return;
	struct wlr_box box = wsov_cell_layout_box(target);
	double center = wsov_scrolls_horizontally() ? box.x + box.width / 2.0
												: box.y + box.height / 2.0;
	double viewport_center = wsov_scrolls_horizontally()
								 ? wsov.mon->m.width / 2.0
								 : wsov.mon->m.height / 2.0;
	if (wsov_animate_scroll_to(center - viewport_center))
		wsov_apply_camera(wsov.progress);
}

void wsov_set_cell_box(struct wsov_cell *cell, int x, int y, int width,
					   int height) {
	cell->box = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};
	cell->buffer_w = cell->box.width - 2 * wsov_border_width();
	cell->buffer_h = cell->box.height - 2 * wsov_border_width();
	cell->layout_current = cell->box;

	wlr_scene_node_set_position(&cell->frame->node, cell->box.x, cell->box.y);
	wlr_scene_rect_set_size(cell->frame, cell->box.width, cell->box.height);
	wlr_scene_node_set_position(&cell->contents->node,
								cell->box.x + wsov_border_width(),
								cell->box.y + wsov_border_width());
	wlr_scene_buffer_set_dest_size(cell->contents, cell->buffer_w,
								   cell->buffer_h);
	wlr_scene_node_set_position(&cell->foreground->node,
								cell->box.x + wsov_border_width(),
								cell->box.y + wsov_border_width());
	wlr_scene_buffer_set_dest_size(cell->foreground, cell->buffer_w,
								   cell->buffer_h);
}

int wsov_grid_cols(int count) {
	return MANGO_MAX(1, (int)ceil(sqrt((double)MANGO_MAX(1, count))));
}

void wsov_layout_grid_cells(void) {
	int n = wsov.cell_count;
	int cols = wsov_grid_cols(n);
	int rows = (n + cols - 1) / cols;
	int gap = config.workspace_overview_gap;
	int avail_w = MANGO_MAX(1, wsov.mon->m.width);
	int avail_h = MANGO_MAX(1, wsov.mon->m.height);
	double aspect = (double)wsov.mon->m.width / wsov.mon->m.height;
	int cell_w = MANGO_MAX(1, (avail_w - (cols - 1) * gap) / cols);
	int cell_h = (int)lround(cell_w / aspect);
	int max_cell_h = MANGO_MAX(1, (avail_h - (rows - 1) * gap) / rows);
	if (cell_h > max_cell_h) {
		cell_h = max_cell_h;
		cell_w = (int)lround(cell_h * aspect);
	}
	if (n == 1) {
		int max_single_w =
			MANGO_MIN(avail_w, MANGO_MAX(1, (int)lround(avail_w * 0.9)));
		if (cell_w > max_single_w) {
			cell_w = max_single_w;
			cell_h = MANGO_MAX(1, (int)lround(cell_w / aspect));
		}
	}
	cell_w = MANGO_MAX(2 * wsov_border_width() + 1, cell_w);
	cell_h = MANGO_MAX(2 * wsov_border_width() + 1, cell_h);
	int grid_h = rows * cell_h + (rows - 1) * gap;
	int grid_y = MANGO_MAX(0, (avail_h - grid_h) / 2);

	for (int i = 0; i < n; i++) {
		int col = i % cols;
		int row = i / cols;
		int row_start = row * cols;
		int row_end = MANGO_MIN(row_start + cols, n);
		int row_len = row_end - row_start;
		int row_w = row_len * cell_w + (row_len - 1) * gap;
		int row_x = MANGO_MAX(0, (avail_w - row_w) / 2);
		int sx = row_x + col * (cell_w + gap);
		int sy = grid_y + row * (cell_h + gap);
		wsov_set_cell_box(&wsov.cells[i], sx, sy, cell_w, cell_h);
	}
}

void wsov_layout_scroll_cells(void) {
	int n = wsov.cell_count;
	int gap = config.workspace_overview_gap;
	int avail_w = MANGO_MAX(1, wsov.mon->m.width);
	int avail_h = MANGO_MAX(1, wsov.mon->m.height);
	double aspect = (double)wsov.mon->m.width / wsov.mon->m.height;
	int fit_w = MANGO_MAX(
		1, (int)lround(avail_w * config.workspace_overview_scroll_ratio));
	int fit_h = MANGO_MAX(
		1, (int)lround(avail_h * config.workspace_overview_scroll_ratio));
	int cell_w = MANGO_MIN(fit_w, MANGO_MAX(1, (int)lround(fit_h * aspect)));
	int cell_h = MANGO_MAX(1, (int)lround(cell_w / aspect));
	cell_w = MANGO_MAX(2 * wsov_border_width() + 1, cell_w);
	cell_h = MANGO_MAX(2 * wsov_border_width() + 1, cell_h);
	int center_x = MANGO_MAX(0, (avail_w - cell_w) / 2);
	int center_y = MANGO_MAX(0, (avail_h - cell_h) / 2);

	for (int i = 0; i < n; i++) {
		int sx = wsov_scrolls_horizontally() ? i * (cell_w + gap) : center_x;
		int sy = wsov_scrolls_vertically() ? i * (cell_h + gap) : center_y;
		wsov_set_cell_box(&wsov.cells[i], sx, sy, cell_w, cell_h);
	}
}

void wsov_layout_cells(void) {
	if (!wsov.mon || wsov.cell_count <= 0)
		return;
	if (wsov.layout == WORKSPACE_OVERVIEW_SCROLL)
		wsov_layout_scroll_cells();
	else
		wsov_layout_grid_cells();
	wsov_update_scroll_bounds();
}

void wsov_update_layout_animation(void) {
	if (!wsov.layout_animating)
		return;

	double elapsed_ms = wsov_elapsed_ms(&wsov.layout_animation_start);
	double eased = 0.0;
	const AnimationCurve *curve = &config.animation_curve_wsov_move;
	double t = elapsed_ms / MANGO_MAX(1.0, wsov.layout_animation_duration_ms);
	t = fmax(0.0, fmin(1.0, t));
	eased = find_animation_curve_for(t, curve);
	bool finished = elapsed_ms >= wsov.layout_animation_duration_ms;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (finished) {
			cell->layout_current = cell->box;
			continue;
		}
		cell->layout_current = (struct wlr_box){
			.x = (int)lround(cell->layout_from.x +
							 (cell->box.x - cell->layout_from.x) * eased),
			.y = (int)lround(cell->layout_from.y +
							 (cell->box.y - cell->layout_from.y) * eased),
			.width = MANGO_MAX(
				1, (int)lround(cell->layout_from.width +
							   (cell->box.width - cell->layout_from.width) *
								   eased)),
			.height = MANGO_MAX(
				1, (int)lround(cell->layout_from.height +
							   (cell->box.height - cell->layout_from.height) *
								   eased)),
		};
	}
	if (finished)
		wsov.layout_animating = false;
}

void wsov_destroy_cells(void) {
	wsov_cancel_scheduled_render();
	wsov_clear_live_projection(false);
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (cell->contents)
			wlr_scene_node_destroy(&cell->contents->node);
		if (cell->foreground)
			wlr_scene_node_destroy(&cell->foreground->node);
		wsov_release_cell_buffers(cell);
		mango_dim_node_destroy(cell->dim);
		if (cell->frame)
			wlr_scene_node_destroy(&cell->frame->node);
		for (int j = 0; j < 4; j++) {
			if (cell->border[j])
				wlr_scene_node_destroy(&cell->border[j]->node);
		}
		wsov_destroy_cell_live_trees(cell);
		wsov_destroy_layer_previews(cell);
		wsov_clear_cell_bars(cell);
		free(cell->bar_items);
		free(cell->items);
	}
	free(wsov.cells);
	wsov.cells = NULL;
	wsov.cell_count = 0;
}

bool wsov_create_cells(void) {
	typedef struct {
		uint32_t tag;
		struct wlr_box box;
	} wsov_layout_snapshot;

	int old_cell_count = wsov.cell_count;
	double old_scroll_offset = wsov.scroll_offset;
	wsov_layout_snapshot *snapshot = NULL;
	if (old_cell_count > 0) {
		snapshot = malloc(old_cell_count * sizeof(*snapshot));
		if (snapshot) {
			for (int i = 0; i < old_cell_count; i++) {
				snapshot[i] = (wsov_layout_snapshot){
					.tag = wsov.cells[i].tag,
					.box = wsov_cell_layout_box(&wsov.cells[i]),
				};
			}
		}
	}

	wsov_destroy_cells();
	uint32_t occupied = wsov_collect_workspace_tags(wsov.mon, &wsov.cell_count);
	if (wsov.cell_count <= 0) {
		free(snapshot);
		return false;
	}

	wsov.cells = calloc(wsov.cell_count, sizeof(*wsov.cells));
	if (!wsov.cells) {
		wsov.cell_count = 0;
		free(snapshot);
		return false;
	}

	int index = 0;
	for (int tag = 1; tag <= config.tag_num; tag++) {
		if (!(occupied & wsov_tag_bit((uint32_t)tag)))
			continue;

		struct wsov_cell *cell = &wsov.cells[index++];
		cell->tag = (uint32_t)tag;
		cell->borders_enabled = true;
		cell->frame = wlr_scene_rect_create(wsov.root, 1, 1, config.rootcolor);
		for (int i = 0; i < 4; i++) {
			cell->border[i] =
				wlr_scene_rect_create(wsov.root, 1, 1, config.bordercolor);
		}
		cell->contents = wlr_scene_buffer_create(wsov.root, NULL);
		cell->foreground = wlr_scene_buffer_create(wsov.root, NULL);
		cell->dim = mango_dim_node_create(wsov.root, (float[4]){0, 0, 0, 0});
		if (!cell->frame || !cell->contents || !cell->border[0] ||
			!cell->border[1] || !cell->border[2] || !cell->border[3] ||
			!cell->foreground || !cell->dim) {
			free(snapshot);
			return false;
		}
	}

	wsov_layout_cells();
	wsov.layout_animating = false;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		cell->layout_from = cell->box;
		if (!snapshot)
			continue;
		for (int j = 0; j < old_cell_count; j++) {
			if (snapshot[j].tag != cell->tag)
				continue;
			cell->layout_from = snapshot[j].box;
			if (config.animations &&
				!wlr_box_equal(&cell->layout_from, &cell->box))
				wsov.layout_animating = true;
			break;
		}
	}
	free(snapshot);
	if (config.animations && wsov.layout_animating) {
		for (int i = 0; i < wsov.cell_count; i++)
			wsov.cells[i].layout_current = wsov.cells[i].layout_from;
		wsov.layout_animation_duration_ms =
			animation_curve_duration_ms(&config.animation_curve_wsov_move,
										config.animation_duration_wsov_move);
		clock_gettime(CLOCK_MONOTONIC, &wsov.layout_animation_start);
		wsov_attach_frame_listener();
	}
	wsov_create_layer_previews();
	wsov_select_default_target();
	/*
	 * Keep the scroll anchor independent from target_tag. Pointer hover may
	 * change the selected target without moving the viewport, while the wheel
	 * must continue from the workspace that is currently centered.
	 */
	if (!wsov_cell_for_tag(wsov.scroll_center_tag))
		wsov.scroll_center_tag = wsov.target_tag;
	wsov.camera_tag = wsov.target_tag;
	if (old_cell_count > 0 && config.animations) {
		/*
		 * Removing an occupied workspace changes the scroll bounds and can
		 * make the selected workspace jump several positions. Keep the old
		 * viewport as the animation start instead of letting layout_cells()
		 * clamp it to the new final position first.
		 */
		wsov.scroll_offset = old_scroll_offset;
		wsov_animate_scroll_to_final_target();
	} else {
		wsov_scroll_to_target();
	}
	wsov_sync_monitor_selection();
	wsov_capture_workspace_layouts();
	for (int i = 0; i < wsov.cell_count; i++) {
		wsov.cells[i].contents_dirty = true;
		wsov.cells[i].foreground_dirty = true;
	}
	wsov_rebuild_live_projection();
	wsov_render_all();
	wsov_update_styles();
	wsov_apply_camera(wsov.progress);
	if (wsov_refresh_animated_cells()) {
		wsov.client_animating = true;
		wsov_attach_frame_listener();
		wlr_output_schedule_frame(wsov.mon->wlr_output);
	}
	return true;
}
