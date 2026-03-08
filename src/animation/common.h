struct dvec2 calculate_animation_curve_at(double t, int32_t type) {
	struct dvec2 point;
	double *animation_curve;
	if (type == MOVE) {
		animation_curve = animation_curve_move;
	} else if (type == OPEN) {
		animation_curve = animation_curve_open;
	} else if (type == TAG) {
		animation_curve = animation_curve_tag;
	} else if (type == CLOSE) {
		animation_curve = animation_curve_close;
	} else if (type == FOCUS) {
		animation_curve = animation_curve_focus;
	} else if (type == OPAFADEIN) {
		animation_curve = animation_curve_opafadein;
	} else if (type == OPAFADEOUT) {
		animation_curve = animation_curve_opafadeout;
	} else {
		animation_curve = animation_curve_move;
	}

	point.x = 3 * t * (1 - t) * (1 - t) * animation_curve[0] +
			  3 * t * t * (1 - t) * animation_curve[2] + t * t * t;

	point.y = 3 * t * (1 - t) * (1 - t) * animation_curve[1] +
			  3 * t * t * (1 - t) * animation_curve[3] + t * t * t;

	return point;
}

void init_baked_points(void) {
	baked_points_move = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_move));
	baked_points_open = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_open));
	baked_points_tag = calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_tag));
	baked_points_close =
		calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_close));
	baked_points_focus =
		calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_focus));
	baked_points_opafadein =
		calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_opafadein));
	baked_points_opafadeout =
		calloc(BAKED_POINTS_COUNT, sizeof(*baked_points_opafadeout));

	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_move[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), MOVE);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_open[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), OPEN);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_tag[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), TAG);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_close[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), CLOSE);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_focus[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), FOCUS);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_opafadein[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), OPAFADEIN);
	}
	for (int32_t i = 0; i < BAKED_POINTS_COUNT; i++) {
		baked_points_opafadeout[i] = calculate_animation_curve_at(
			(double)i / (BAKED_POINTS_COUNT - 1), OPAFADEOUT);
	}
}

double find_animation_curve_at(double t, int32_t type) {
	int32_t down = 0;
	int32_t up = BAKED_POINTS_COUNT - 1;

	int32_t middle = (up + down) / 2;
	struct dvec2 *baked_points;
	if (type == MOVE) {
		baked_points = baked_points_move;
	} else if (type == OPEN) {
		baked_points = baked_points_open;
	} else if (type == TAG) {
		baked_points = baked_points_tag;
	} else if (type == CLOSE) {
		baked_points = baked_points_close;
	} else if (type == FOCUS) {
		baked_points = baked_points_focus;
	} else if (type == OPAFADEIN) {
		baked_points = baked_points_opafadein;
	} else if (type == OPAFADEOUT) {
		baked_points = baked_points_opafadeout;
	} else {
		baked_points = baked_points_move;
	}

	while (up - down != 1) {
		if (baked_points[middle].x <= t) {
			down = middle;
		} else {
			up = middle;
		}
		middle = (up + down) / 2;
	}
	return baked_points[up].y;
}

static bool scene_node_snapshot(struct wlr_scene_node *node, int32_t lx,
								int32_t ly,
								struct wlr_scene_tree *snapshot_tree) {
	if (!node->enabled && node->type != WLR_SCENE_NODE_TREE) {
		return true;
	}

	lx += node->x;
	ly += node->y;

	struct wlr_scene_node *snapshot_node = NULL;
	switch (node->type) {
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);

		struct wlr_scene_node *child;
		wl_list_for_each(child, &scene_tree->children, link) {
			scene_node_snapshot(child, lx, ly, snapshot_tree);
		}
		break;
	}
	case WLR_SCENE_NODE_RECT: {
		// struct wlr_scene_rect *scene_rect = wlr_scene_rect_from_node(node);

		// struct wlr_scene_rect *snapshot_rect =
		// 	wlr_scene_rect_create(snapshot_tree, scene_rect->width,
		// 						  scene_rect->height, scene_rect->color);
		// snapshot_rect->node.data = scene_rect->node.data;
		// if (snapshot_rect == NULL) {
		// 	return false;
		// }

		// wlr_scene_rect_set_clipped_region(scene_rect,
		// 								  snapshot_rect->clipped_region);
		// wlr_scene_rect_set_backdrop_blur(scene_rect, false);
		// wlr_scene_rect_set_backdrop_blur_optimized(
		// 	scene_rect, snapshot_rect->backdrop_blur_optimized);
		// wlr_scene_rect_set_corner_radius(
		// 	scene_rect, snapshot_rect->corner_radius, snapshot_rect->corners);
		// wlr_scene_rect_set_color(scene_rect, snapshot_rect->color);

		// snapshot_node = &snapshot_rect->node;
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *scene_buffer =
			wlr_scene_buffer_from_node(node);

		struct wlr_scene_buffer *snapshot_buffer =
			wlr_scene_buffer_create(snapshot_tree, NULL);
		if (snapshot_buffer == NULL) {
			return false;
		}
		snapshot_node = &snapshot_buffer->node;
		snapshot_buffer->node.data = scene_buffer->node.data;

		wlr_scene_buffer_set_dest_size(snapshot_buffer, scene_buffer->dst_width,
									   scene_buffer->dst_height);
		wlr_scene_buffer_set_opaque_region(snapshot_buffer,
										   &scene_buffer->opaque_region);
		wlr_scene_buffer_set_source_box(snapshot_buffer,
										&scene_buffer->src_box);
		wlr_scene_buffer_set_transform(snapshot_buffer,
									   scene_buffer->transform);
		wlr_scene_buffer_set_filter_mode(snapshot_buffer,
										 scene_buffer->filter_mode);

		// Effects
		wlr_scene_buffer_set_opacity(snapshot_buffer, scene_buffer->opacity);
		wlr_scene_buffer_set_corner_radius(snapshot_buffer,
										   scene_buffer->corner_radius,
										   scene_buffer->corners);

		// wlr_scene_buffer_set_backdrop_blur_optimized(
		// 	snapshot_buffer, scene_buffer->backdrop_blur_optimized);
		// wlr_scene_buffer_set_backdrop_blur_ignore_transparent(
		// 	snapshot_buffer, scene_buffer->backdrop_blur_ignore_transparent);
		wlr_scene_buffer_set_backdrop_blur(snapshot_buffer, false);

		snapshot_buffer->node.data = scene_buffer->node.data;

		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(scene_buffer);
		if (scene_surface != NULL && scene_surface->surface->buffer != NULL) {
			wlr_scene_buffer_set_buffer(snapshot_buffer,
										&scene_surface->surface->buffer->base);
		} else {
			wlr_scene_buffer_set_buffer(snapshot_buffer, scene_buffer->buffer);
		}
		break;
	}
	case WLR_SCENE_NODE_SHADOW: {
		struct wlr_scene_shadow *scene_shadow =
			wlr_scene_shadow_from_node(node);

		struct wlr_scene_shadow *snapshot_shadow = wlr_scene_shadow_create(
			snapshot_tree, scene_shadow->width, scene_shadow->height,
			scene_shadow->corner_radius, scene_shadow->blur_sigma,
			scene_shadow->color);
		if (snapshot_shadow == NULL) {
			return false;
		}
		snapshot_node = &snapshot_shadow->node;

		wlr_scene_shadow_set_clipped_region(snapshot_shadow,
											scene_shadow->clipped_region);

		snapshot_shadow->node.data = scene_shadow->node.data;

		wlr_scene_node_set_enabled(&snapshot_shadow->node, false);

		break;
	}
	case WLR_SCENE_NODE_OPTIMIZED_BLUR:
		return true;
	}

	if (snapshot_node != NULL) {
		wlr_scene_node_set_position(snapshot_node, lx, ly);
	}

	return true;
}

struct wlr_scene_tree *wlr_scene_tree_snapshot(struct wlr_scene_node *node,
											   struct wlr_scene_tree *parent) {
	struct wlr_scene_tree *snapshot = wlr_scene_tree_create(parent);
	if (snapshot == NULL) {
		return NULL;
	}

	// Disable and enable the snapshot tree like so to atomically update
	// the scene-graph. This will prevent over-damaging or other weirdness.
	wlr_scene_node_set_enabled(&snapshot->node, false);

	if (!scene_node_snapshot(node, 0, 0, snapshot)) {
		wlr_scene_node_destroy(&snapshot->node);
		return NULL;
	}

	wlr_scene_node_set_enabled(&snapshot->node, true);

	return snapshot;
}

void request_fresh_all_monitors(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		wlr_output_schedule_frame(m->wlr_output);
	}
}