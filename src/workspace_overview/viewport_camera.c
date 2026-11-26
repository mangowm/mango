#include "mango/workspace_overview/workspace_overview_internal.h"

struct wlr_box wsov_cell_layout_box(const struct wsov_cell *cell) {
	if (!cell)
		return (struct wlr_box){0};
	return wsov.layout_animating ? cell->layout_current : cell->box;
}

bool wsov_live_clips_to_workspace(const struct wsov_cell *cell) {
	if (!cell || wsov.layout != WORKSPACE_OVERVIEW_SCROLL || !wsov.mon ||
		!wsov.mon->pertag)
		return true;

	const Layout *layout = wsov.mon->pertag->ltidxs[cell->tag];
	if (!layout)
		return true;
	if (wsov_scrolls_vertically())
		return layout->id == VERTICAL_SCROLLER;
	if (wsov_scrolls_horizontally())
		return layout->id == SCROLLER;
	return true;
}

struct wlr_box wsov_live_overflow_clip(const struct wlr_box *workspace,
									   const struct wlr_box *monitor) {
	if (!workspace || !monitor)
		return (struct wlr_box){0};

	struct wlr_box clip = *monitor;
	if (wsov_scrolls_vertically()) {
		clip.y = workspace->y;
		clip.height = workspace->height;
	} else if (wsov_scrolls_horizontally()) {
		clip.x = workspace->x;
		clip.width = workspace->width;
	} else {
		/* Grid previews are clipped to their frame and the output it belongs
		 * to. The frame itself can be partially outside the output while the
		 * camera zooms toward a card near an edge. */
		clip = *workspace;
	}

	struct wlr_box visible;
	if (!wlr_box_intersection(&visible, &clip, monitor))
		return (struct wlr_box){0};
	return visible;
}

struct wlr_box wsov_item_display_box(const struct wsov_cell *cell,
									 const struct wsov_item *item) {
	if (!cell || !item)
		return (struct wlr_box){0};

	struct wlr_box cell_box = wsov_cell_layout_box(cell);
	double scale_x =
		(double)MANGO_MAX(1, cell_box.width - 2 * wsov_border_width()) /
		MANGO_MAX(1, cell->buffer_w);
	double scale_y =
		(double)MANGO_MAX(1, cell_box.height - 2 * wsov_border_width()) /
		MANGO_MAX(1, cell->buffer_h);
	double local_x = item->box.x - cell->box.x - wsov_border_width();
	double local_y = item->box.y - cell->box.y - wsov_border_width();
	int scroll_x =
		wsov_scrolls_horizontally() ? (int)lround(wsov.scroll_offset) : 0;
	int scroll_y =
		wsov_scrolls_vertically() ? (int)lround(wsov.scroll_offset) : 0;
	return (struct wlr_box){
		.x = cell_box.x - scroll_x + wsov_border_width() +
			 (int)lround(local_x * scale_x),
		.y = cell_box.y - scroll_y + wsov_border_width() +
			 (int)lround(local_y * scale_y),
		.width = MANGO_MAX(1, (int)lround(item->box.width * scale_x)),
		.height = MANGO_MAX(1, (int)lround(item->box.height * scale_y)),
	};
}

struct wlr_box wsov_item_final_display_box(const struct wsov_cell *cell,
										   const struct wsov_item *item) {
	if (!cell || !item || item->is_snapshot || !item->client)
		return (struct wlr_box){0};

	struct wlr_box cell_box = cell->box;
	int content_w = MANGO_MAX(1, cell_box.width - 2 * wsov_border_width());
	int content_h = MANGO_MAX(1, cell_box.height - 2 * wsov_border_width());
	double scale_x = (double)content_w / MANGO_MAX(1, wsov.mon->m.width);
	double scale_y = (double)content_h / MANGO_MAX(1, wsov.mon->m.height);
	double local_x = item->client->geom.x - wsov.mon->m.x;
	double local_y = item->client->geom.y - wsov.mon->m.y;
	int scroll_x =
		wsov_scrolls_horizontally() ? (int)lround(wsov.scroll_offset) : 0;
	int scroll_y =
		wsov_scrolls_vertically() ? (int)lround(wsov.scroll_offset) : 0;
	return (struct wlr_box){
		.x = cell_box.x - scroll_x + wsov_border_width() +
			 (int)lround(local_x * scale_x),
		.y = cell_box.y - scroll_y + wsov_border_width() +
			 (int)lround(local_y * scale_y),
		.width = MANGO_MAX(1, (int)lround(item->client->geom.width * scale_x)),
		.height =
			MANGO_MAX(1, (int)lround(item->client->geom.height * scale_y)),
	};
}

struct wlr_box wsov_monitor_clip(void) {
	return wsov.mon
			   ? (struct wlr_box){0, 0, wsov.mon->m.width, wsov.mon->m.height}
			   : (struct wlr_box){0};
}

bool wsov_intersect_monitor(int x, int y, int width, int height,
							struct wlr_box *visible) {
	struct wlr_box box = {x, y, width, height};
	struct wlr_box clip = wsov_monitor_clip();
	*visible = (struct wlr_box){0};
	if (box.width <= 0 || box.height <= 0 || clip.width <= 0 ||
		clip.height <= 0 || !wlr_box_intersection(visible, &box, &clip))
		return false;
	return !wlr_box_empty(visible);
}

bool wsov_get_camera_view(double progress, struct wsov_camera_view *view) {
	if (!wsov.mon || wsov.cell_count <= 0 || !view)
		return false;

	struct wsov_cell *target = wsov_cell_for_tag(wsov.camera_tag);
	if (!target)
		target = &wsov.cells[0];
	struct wlr_box target_box = wsov_cell_layout_box(target);
	int border = wsov_border_width();
	double target_x = target_box.x + border;
	double target_y = target_box.y + border;
	double target_w = MANGO_MAX(1, target_box.width - 2 * border);
	double target_h = MANGO_MAX(1, target_box.height - 2 * border);
	double final_viewport_x =
		wsov_scrolls_horizontally() ? wsov.scroll_offset : 0.0;
	double final_viewport_y =
		wsov_scrolls_vertically() ? wsov.scroll_offset : 0.0;
	progress = fmax(0.0, fmin(1.0, progress));
	double viewport_w = target_w + (wsov.mon->m.width - target_w) * progress;
	double viewport_h = target_h + (wsov.mon->m.height - target_h) * progress;
	view->x = target_x + (final_viewport_x - target_x) * progress;
	view->y = target_y + (final_viewport_y - target_y) * progress;
	view->scale_x = (double)wsov.mon->m.width / MANGO_MAX(1.0, viewport_w);
	view->scale_y = (double)wsov.mon->m.height / MANGO_MAX(1.0, viewport_h);
	return true;
}

struct wlr_box wsov_cell_screen_box(const struct wsov_cell *cell,
									const struct wsov_camera_view *view) {
	if (!cell || !view)
		return (struct wlr_box){0};
	struct wlr_box box = wsov_cell_layout_box(cell);
	return (struct wlr_box){
		.x = (int)lround((box.x - view->x) * view->scale_x),
		.y = (int)lround((box.y - view->y) * view->scale_y),
		.width = MANGO_MAX(1, (int)lround(box.width * view->scale_x)),
		.height = MANGO_MAX(1, (int)lround(box.height * view->scale_y)),
	};
}

bool wsov_cell_box_visible(const struct wlr_box *box) {
	struct wlr_box visible;
	return box && wsov_intersect_monitor(box->x, box->y, box->width,
										 box->height, &visible);
}

bool wsov_cell_preview_visible(const struct wsov_cell *cell, double progress) {
	struct wsov_camera_view view;
	if (!wsov_get_camera_view(progress, &view))
		return false;
	struct wlr_box box = wsov_cell_screen_box(cell, &view);
	return wsov_cell_box_visible(&box);
}

/*
 * A workspace frame can be off-screen while a live scroller child overflows
 * back into the output. Treat that child as visible work; same-axis clipping
 * cannot escape the frame, so it can be culled using the frame alone.
 */
bool wsov_cell_live_content_visible(const struct wsov_cell *cell,
									double progress) {
	struct wsov_camera_view view;
	if (!cell || !wsov_get_camera_view(progress, &view))
		return false;
	struct wlr_box cell_box = wsov_cell_screen_box(cell, &view);
	if (wsov_cell_box_visible(&cell_box))
		return true;
	if (wsov_live_clips_to_workspace(cell))
		return false;

	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot)
			continue;
		struct wlr_box box = {
			.x = (int)lround((item->box.x - view.x) * view.scale_x),
			.y = (int)lround((item->box.y - view.y) * view.scale_y),
			.width = MANGO_MAX(1, (int)lround(item->box.width * view.scale_x)),
			.height =
				MANGO_MAX(1, (int)lround(item->box.height * view.scale_y)),
		};
		if (wsov_cell_box_visible(&box))
			return true;
	}
	return false;
}

bool wsov_cell_live_content_visible_in_view(
	const struct wsov_cell *cell, const struct wsov_camera_view *view) {
	struct wlr_box cell_box = wsov_cell_screen_box(cell, view);
	if (wsov_cell_box_visible(&cell_box))
		return true;
	if (wsov_live_clips_to_workspace(cell))
		return false;

	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot)
			continue;
		struct wlr_box box = {
			.x = (int)lround((item->box.x - view->x) * view->scale_x),
			.y = (int)lround((item->box.y - view->y) * view->scale_y),
			.width = MANGO_MAX(1, (int)lround(item->box.width * view->scale_x)),
			.height =
				MANGO_MAX(1, (int)lround(item->box.height * view->scale_y)),
		};
		if (wsov_cell_box_visible(&box))
			return true;
	}
	return false;
}

void wsov_place_rect_clipped(struct wlr_scene_rect *rect, int x, int y,
							 int width, int height, bool enabled) {
	struct wlr_box visible;
	if (!rect)
		return;
	if (!enabled || !wsov_intersect_monitor(x, y, width, height, &visible)) {
		wlr_scene_node_set_enabled(&rect->node, false);
		return;
	}
	wlr_scene_node_set_enabled(&rect->node, true);
	wlr_scene_node_set_position(&rect->node, visible.x, visible.y);
	wlr_scene_rect_set_size(rect, visible.width, visible.height);
}

void wsov_place_buffer_clipped(struct wlr_scene_buffer *node,
							   const struct wlr_fbox *base_src, int x, int y,
							   int width, int height) {
	struct wlr_box visible;
	if (!node)
		return;
	if (!node->buffer ||
		!wsov_intersect_monitor(x, y, width, height, &visible)) {
		wlr_scene_node_set_enabled(&node->node, false);
		return;
	}

	struct wlr_fbox src = base_src
							  ? *base_src
							  : (struct wlr_fbox){0, 0, node->buffer->width,
												  node->buffer->height};
	if (src.width <= 0 || src.height <= 0)
		src =
			(struct wlr_fbox){0, 0, node->buffer->width, node->buffer->height};

	if (node->transform == WL_OUTPUT_TRANSFORM_NORMAL) {
		src.x += src.width * (visible.x - x) / width;
		src.y += src.height * (visible.y - y) / height;
		src.width *= (double)visible.width / width;
		src.height *= (double)visible.height / height;
	} else {
		int buffer_w = node->buffer->width;
		int buffer_h = node->buffer->height;
		struct wlr_fbox oriented;
		wlr_fbox_transform(&oriented, &src, node->transform, buffer_w,
						   buffer_h);
		oriented.x += oriented.width * (visible.x - x) / width;
		oriented.y += oriented.height * (visible.y - y) / height;
		oriented.width *= (double)visible.width / width;
		oriented.height *= (double)visible.height / height;
		bool swapped = node->transform == WL_OUTPUT_TRANSFORM_90 ||
					   node->transform == WL_OUTPUT_TRANSFORM_270 ||
					   node->transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
					   node->transform == WL_OUTPUT_TRANSFORM_FLIPPED_270;
		int oriented_w = swapped ? buffer_h : buffer_w;
		int oriented_h = swapped ? buffer_w : buffer_h;
		wlr_fbox_transform(&src, &oriented,
						   wlr_output_transform_invert(node->transform),
						   oriented_w, oriented_h);
	}

	wlr_scene_node_set_enabled(&node->node, true);
	wlr_scene_node_set_position(&node->node, visible.x, visible.y);
	wlr_scene_buffer_set_dest_size(node, visible.width, visible.height);
	wlr_scene_buffer_set_source_box(node, &src);
}

double wsov_output_scale(void) {
	return wsov.mon && wsov.mon->wlr_output
			   ? fmax(1.0, wsov.mon->wlr_output->scale)
			   : 1.0;
}

struct wsov_cell *wsov_cell_at(double x, double y) {
	if (!wsov.root || !wsov.mon)
		return NULL;

	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	if (lx < 0 || ly < 0 || lx >= wsov.mon->m.width || ly >= wsov.mon->m.height)
		return NULL;
	if (wsov_scrolls_horizontally())
		lx += wsov.scroll_offset;
	else if (wsov_scrolls_vertically())
		ly += wsov.scroll_offset;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wlr_box box = wsov_cell_layout_box(&wsov.cells[i]);
		if (wlr_box_contains_point(&box, lx, ly))
			return &wsov.cells[i];
	}
	return NULL;
}

struct wsov_cell *wsov_drop_cell_at(double x, double y) {
	struct wsov_cell *cell = wsov_cell_at(x, y);
	if (cell || !wsov.mon || x < wsov.mon->m.x ||
		x >= wsov.mon->m.x + wsov.mon->m.width || y < wsov.mon->m.y ||
		y >= wsov.mon->m.y + wsov.mon->m.height)
		return cell;

	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	if (wsov_scrolls_horizontally())
		lx += wsov.scroll_offset;
	else if (wsov_scrolls_vertically())
		ly += wsov.scroll_offset;
	struct wsov_cell *nearest = NULL;
	double best = 1e30;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wlr_box box = wsov_cell_layout_box(&wsov.cells[i]);
		double cx = box.x + box.width / 2.0;
		double cy = box.y + box.height / 2.0;
		double dx = cx - lx;
		double dy = cy - ly;
		double distance = dx * dx + dy * dy;
		if (distance < best) {
			best = distance;
			nearest = &wsov.cells[i];
		}
	}
	return nearest;
}

struct wsov_item *wsov_item_at(struct wsov_cell *cell, double x, double y) {
	if (!cell || !wsov.mon)
		return NULL;
	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	struct wsov_item *hit = NULL;
	for (int i = 0; i < cell->item_count; i++) {
		struct wsov_item *item = &cell->items[i];
		if (item->is_snapshot)
			continue;
		struct wlr_box box = wsov_item_display_box(cell, item);
		if (wlr_box_contains_point(&box, lx, ly) &&
			(!hit || item->layer >= hit->layer))
			hit = item;
	}
	return hit;
}

struct wsov_cell *wsov_hit_cell_at(double x, double y) {
	struct wsov_cell *cell = wsov_cell_at(x, y);
	if (cell)
		return cell;

	/*
	 * Live scroller windows may intentionally extend past their workspace on
	 * the orthogonal axis. Every workspace now has a live projection, so the
	 * same pixels must remain draggable instead of requiring a click inside
	 * the frame.
	 */
	struct wsov_cell *best_cell = NULL;
	struct wsov_item *best_item = NULL;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *candidate = &wsov.cells[i];
		if (wsov_live_clips_to_workspace(candidate))
			continue;
		struct wsov_item *item = wsov_item_at(candidate, x, y);
		if (!item || !wsov_item_is_live(candidate, item))
			continue;
		if (!best_item || item->layer > best_item->layer ||
			(item->layer == best_item->layer &&
			 candidate->tag == wsov.target_tag)) {
			best_cell = candidate;
			best_item = item;
		}
	}
	if (best_cell)
		return best_cell;
	return NULL;
}

bool wsov_scene_node_layout_coords(struct wlr_scene_node *node, int *x,
								   int *y) {
	if (!node)
		return false;
	int lx = 0, ly = 0;
	for (struct wlr_scene_node *cur = node; cur;
		 cur = cur->parent ? &cur->parent->node : NULL) {
		lx += cur->x;
		ly += cur->y;
	}
	*x = lx;
	*y = ly;
	return true;
}

void wsov_apply_camera(double progress) {
	if (!wsov.mon || wsov.cell_count <= 0)
		return;

	struct wsov_camera_view view;
	if (!wsov_get_camera_view(progress, &view))
		return;

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		struct wlr_box cell_box = wsov_cell_layout_box(cell);
		double width = cell_box.width * view.scale_x;
		double height = cell_box.height * view.scale_y;
		double x = (cell_box.x - view.x) * view.scale_x;
		double y = (cell_box.y - view.y) * view.scale_y;

		wsov_place_rect_clipped(cell->frame, (int)lround(x), (int)lround(y),
								MANGO_MAX(1, (int)lround(width)),
								MANGO_MAX(1, (int)lround(height)), true);
		int content_x = (int)lround(
			(cell_box.x + wsov_border_width() - view.x) * view.scale_x);
		int content_y = (int)lround(
			(cell_box.y + wsov_border_width() - view.y) * view.scale_y);
		int content_w = MANGO_MAX(
			1,
			(int)lround(MANGO_MAX(1, cell_box.width - 2 * wsov_border_width()) *
						view.scale_x));
		int content_h =
			MANGO_MAX(1, (int)lround(MANGO_MAX(1, cell_box.height -
													  2 * wsov_border_width()) *
									 view.scale_y));
		wsov_place_buffer_clipped(cell->contents, NULL, content_x, content_y,
								  content_w, content_h);
		wsov_place_buffer_clipped(cell->foreground, NULL, content_x, content_y,
								  content_w, content_h);
		struct wlr_box dim_visible;
		if (!cell->dim_enabled ||
			!wsov_intersect_monitor(content_x, content_y, content_w, content_h,
									&dim_visible)) {
			mango_dim_node_set_enabled(cell->dim, false);
		} else {
			mango_dim_node_set_enabled(cell->dim, true);
			mango_dim_node_set_box(cell->dim, dim_visible.x, dim_visible.y,
								   dim_visible.width, dim_visible.height);
		}
		int frame_w = MANGO_MAX(1, (int)lround(width));
		int frame_h = MANGO_MAX(1, (int)lround(height));
		int border_w =
			MANGO_MAX(1, (int)lround(wsov_border_width() *
									 fmin(view.scale_x, view.scale_y)));
		int frame_x = (int)lround(x);
		int frame_y = (int)lround(y);
		wsov_place_rect_clipped(cell->border[0], frame_x, frame_y, frame_w,
								border_w, cell->borders_enabled);
		wsov_place_rect_clipped(cell->border[1], frame_x,
								frame_y + frame_h - border_w, frame_w, border_w,
								cell->borders_enabled);
		wsov_place_rect_clipped(cell->border[2], frame_x, frame_y, border_w,
								frame_h, cell->borders_enabled);
		wsov_place_rect_clipped(cell->border[3], frame_x + frame_w - border_w,
								frame_y, border_w, frame_h,
								cell->borders_enabled);
		for (int j = 0; j < cell->layer_count; j++) {
			struct wsov_layer_preview *preview = &cell->layers[j];
			double local_scale_x =
				(double)content_w / MANGO_MAX(1, wsov.mon->m.width);
			double local_scale_y =
				(double)content_h / MANGO_MAX(1, wsov.mon->m.height);
			int px =
				(int)lround(content_x + preview->workspace_x * local_scale_x);
			int py =
				(int)lround(content_y + preview->workspace_y * local_scale_y);
			wsov_place_buffer_clipped(
				preview->node, &preview->src_box, px, py,
				MANGO_MAX(1, (int)lround(preview->width * local_scale_x)),
				MANGO_MAX(1, (int)lround(preview->height * local_scale_y)));
		}
	}
	wsov_layout_live_projection();
	if (!wsov.closing)
		wsov_render_dirty_cells(false);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}
