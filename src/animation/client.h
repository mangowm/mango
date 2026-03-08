void client_actual_size(Client *c, int32_t *width, int32_t *height) {
	*width = c->animation.current.width - 2 * (int32_t)c->bw;

	*height = c->animation.current.height - 2 * (int32_t)c->bw;
}

void set_rect_size(struct wlr_scene_rect *rect, int32_t width, int32_t height) {
	wlr_scene_rect_set_size(rect, GEZERO(width), GEZERO(height));
}

enum corner_location set_client_corner_location(Client *c) {
	enum corner_location current_corner_location = CORNER_LOCATION_ALL;
	struct wlr_box target_geom = animations ? c->animation.current : c->geom;
	if (target_geom.x + border_radius <= c->mon->m.x) {
		current_corner_location &= ~CORNER_LOCATION_LEFT; // 清除左标志位
	}
	if (target_geom.x + target_geom.width - border_radius >=
		c->mon->m.x + c->mon->m.width) {
		current_corner_location &= ~CORNER_LOCATION_RIGHT; // 清除右标志位
	}
	if (target_geom.y + border_radius <= c->mon->m.y) {
		current_corner_location &= ~CORNER_LOCATION_TOP; // 清除上标志位
	}
	if (target_geom.y + target_geom.height - border_radius >=
		c->mon->m.y + c->mon->m.height) {
		current_corner_location &= ~CORNER_LOCATION_BOTTOM; // 清除下标志位
	}
	return current_corner_location;
}

bool is_horizontal_stack_layout(Monitor *m) {

	if (m->pertag->curtag &&
		(m->pertag->ltidxs[m->pertag->curtag]->id == TILE ||
		 m->pertag->ltidxs[m->pertag->curtag]->id == DECK))
		return true;

	return false;
}

bool is_horizontal_right_stack_layout(Monitor *m) {

	if (m->pertag->curtag &&
		(m->pertag->ltidxs[m->pertag->curtag]->id == RIGHT_TILE))
		return true;

	return false;
}

int32_t is_special_animation_rule(Client *c) {

	if (is_scroller_layout(c->mon) && !c->isfloating) {
		return DOWN;
	} else if (c->mon->visible_tiling_clients == 1 && !c->isfloating) {
		return DOWN;
	} else if (c->mon->visible_tiling_clients == 2 && !c->isfloating &&
			   !new_is_master && is_horizontal_stack_layout(c->mon)) {
		return RIGHT;
	} else if (!c->isfloating && new_is_master &&
			   is_horizontal_stack_layout(c->mon)) {
		return LEFT;
	} else if (c->mon->visible_tiling_clients == 2 && !c->isfloating &&
			   !new_is_master && is_horizontal_right_stack_layout(c->mon)) {
		return LEFT;
	} else if (!c->isfloating && new_is_master &&
			   is_horizontal_right_stack_layout(c->mon)) {
		return RIGHT;
	} else {
		return UNDIR;
	}
}

void set_client_open_animation(Client *c, struct wlr_box geo) {
	int32_t slide_direction;
	int32_t horizontal, horizontal_value;
	int32_t vertical, vertical_value;
	int32_t special_direction;
	int32_t center_x, center_y;

	if ((!c->animation_type_open && strcmp(animation_type_open, "fade") == 0) ||
		(c->animation_type_open &&
		 strcmp(c->animation_type_open, "fade") == 0)) {
		c->animainit_geom.width = geo.width;
		c->animainit_geom.height = geo.height;
		c->animainit_geom.x = geo.x;
		c->animainit_geom.y = geo.y;
		return;
	} else if ((!c->animation_type_open &&
				strcmp(animation_type_open, "zoom") == 0) ||
			   (c->animation_type_open &&
				strcmp(c->animation_type_open, "zoom") == 0)) {
		c->animainit_geom.width = geo.width * zoom_initial_ratio;
		c->animainit_geom.height = geo.height * zoom_initial_ratio;
		c->animainit_geom.x = geo.x + (geo.width - c->animainit_geom.width) / 2;
		c->animainit_geom.y =
			geo.y + (geo.height - c->animainit_geom.height) / 2;
		return;
	} else {
		special_direction = is_special_animation_rule(c);
		center_x = c->geom.x + c->geom.width / 2;
		center_y = c->geom.y + c->geom.height / 2;
		if (special_direction == UNDIR) {
			horizontal = c->mon->w.x + c->mon->w.width - center_x <
								 center_x - c->mon->w.x
							 ? RIGHT
							 : LEFT;
			horizontal_value = horizontal == LEFT
								   ? center_x - c->mon->w.x
								   : c->mon->w.x + c->mon->w.width - center_x;
			vertical = c->mon->w.y + c->mon->w.height - center_y <
							   center_y - c->mon->w.y
						   ? DOWN
						   : UP;
			vertical_value = vertical == UP
								 ? center_y - c->mon->w.y
								 : c->mon->w.y + c->mon->w.height - center_y;
			slide_direction =
				horizontal_value < vertical_value ? horizontal : vertical;
		} else {
			slide_direction = special_direction;
		}
		c->animainit_geom.width = c->geom.width;
		c->animainit_geom.height = c->geom.height;
		switch (slide_direction) {
		case UP:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y = c->mon->m.y - c->geom.height;
			break;
		case DOWN:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y =
				c->geom.y + c->mon->m.height - (c->geom.y - c->mon->m.y);
			break;
		case LEFT:
			c->animainit_geom.x = c->mon->m.x - c->geom.width;
			c->animainit_geom.y = c->geom.y;
			break;
		case RIGHT:
			c->animainit_geom.x =
				c->geom.x + c->mon->m.width - (c->geom.x - c->mon->m.x);
			c->animainit_geom.y = c->geom.y;
			break;
		default:
			c->animainit_geom.x = c->geom.x;
			c->animainit_geom.y = 0 - c->geom.height;
		}
	}
}

void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
									int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;
	wlr_scene_buffer_set_dest_size(buffer, buffer_data->width,
								   buffer_data->height);
}

void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int32_t sx,
							   int32_t sy, void *data) {
	BufferData *buffer_data = (BufferData *)data;

	if (buffer_data->should_scale && buffer_data->height_scale < 1 &&
		buffer_data->width_scale < 1) {
		buffer_data->should_scale = false;
	}

	if (buffer_data->should_scale && buffer_data->height_scale == 1 &&
		buffer_data->width_scale < 1) {
		buffer_data->should_scale = false;
	}

	if (buffer_data->should_scale && buffer_data->height_scale < 1 &&
		buffer_data->width_scale == 1) {
		buffer_data->should_scale = false;
	}

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);

	if (scene_surface == NULL)
		return;

	struct wlr_surface *surface = scene_surface->surface;

	if (buffer_data->should_scale) {

		int32_t surface_width = surface->current.width;
		int32_t surface_height = surface->current.height;

		surface_width = buffer_data->width_scale < 1
							? surface_width
							: buffer_data->width_scale * surface_width;
		surface_height = buffer_data->height_scale < 1
							 ? surface_height
							 : buffer_data->height_scale * surface_height;

		if (surface_width > buffer_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL) {
			surface_width = buffer_data->width;
		}

		if (surface_height > buffer_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) == NULL) {
			surface_height = buffer_data->height;
		}

		if (surface_width > buffer_data->width &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL) {
			return;
		}

		if (surface_height > buffer_data->height &&
			wlr_subsurface_try_from_wlr_surface(surface) != NULL) {
			return;
		}

		if (surface_height > 0 && surface_width > 0) {
			wlr_scene_buffer_set_dest_size(buffer, surface_width,
										   surface_height);
		}
	}
	// TODO: blur set, opacity set

	if (wlr_xdg_popup_try_from_wlr_surface(surface) != NULL)
		return;

	wlr_scene_buffer_set_corner_radius(buffer, border_radius,
									   buffer_data->corner_location);
}

void buffer_set_effect(Client *c, BufferData data) {

	if (!c || c->iskilling)
		return;

	if (c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining) {
		data.should_scale = false;
	}

	if (c == grabc)
		data.should_scale = false;

	if (c->isnoradius || c->isfullscreen ||
		(no_radius_when_single && c->mon &&
		 c->mon->visible_tiling_clients == 1)) {
		data.corner_location = CORNER_LOCATION_NONE;
	}

	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_effect, &data);
}

void client_draw_shadow(Client *c) {

	if (c->iskilling || !client_surface(c)->mapped || c->isnoshadow)
		return;

	if (!shadows || (!c->isfloating && shadow_only_floating)) {
		if (c->shadow->node.enabled)
			wlr_scene_node_set_enabled(&c->shadow->node, false);
		return;
	} else {
		if (c->scene_surface->node.enabled && !c->shadow->node.enabled)
			wlr_scene_node_set_enabled(&c->shadow->node, true);
	}

	bool hit_no_border = check_hit_no_border(c);
	enum corner_location current_corner_location =
		c->isfullscreen || (no_radius_when_single && c->mon &&
							c->mon->visible_tiling_clients == 1)
			? CORNER_LOCATION_NONE
			: CORNER_LOCATION_ALL;

	int32_t bwoffset = c->bw != 0 && hit_no_border ? (int32_t)c->bw : 0;

	int32_t width, height;
	client_actual_size(c, &width, &height);

	int32_t delta = shadows_size + (int32_t)c->bw - bwoffset;

	/* we calculate where to clip the shadow */
	struct wlr_box client_box = {
		.x = bwoffset,
		.y = bwoffset,
		.width = width + (int32_t)c->bw - bwoffset,
		.height = height + (int32_t)c->bw - bwoffset,
	};

	struct wlr_box shadow_box = {
		.x = shadows_position_x + bwoffset,
		.y = shadows_position_y + bwoffset,
		.width = width + 2 * delta,
		.height = height + 2 * delta,
	};

	struct wlr_box intersection_box;
	wlr_box_intersection(&intersection_box, &client_box, &shadow_box);
	/* clipped region takes shadow relative coords, so we translate everything
	 * by its position */
	intersection_box.x -= shadows_position_x + bwoffset;
	intersection_box.y -= shadows_position_y + bwoffset;

	struct clipped_region clipped_region = {
		.area = intersection_box,
		.corner_radius = border_radius,
		.corners = current_corner_location,
	};

	struct wlr_box absolute_shadow_box = {
		.x = shadow_box.x + c->animation.current.x,
		.y = shadow_box.y + c->animation.current.y,
		.width = shadow_box.width,
		.height = shadow_box.height,
	};

	int32_t right_offset, bottom_offset, left_offset, top_offset;

	if (c == grabc) {
		right_offset = 0;
		bottom_offset = 0;
		left_offset = 0;
		top_offset = 0;
	} else {
		right_offset =
			GEZERO(absolute_shadow_box.x + absolute_shadow_box.width -
				   c->mon->m.x - c->mon->m.width);
		bottom_offset =
			GEZERO(absolute_shadow_box.y + absolute_shadow_box.height -
				   c->mon->m.y - c->mon->m.height);

		left_offset = GEZERO(c->mon->m.x - absolute_shadow_box.x);
		top_offset = GEZERO(c->mon->m.y - absolute_shadow_box.y);
	}

	left_offset = MIN(left_offset, shadow_box.width);
	right_offset = MIN(right_offset, shadow_box.width);
	top_offset = MIN(top_offset, shadow_box.height);
	bottom_offset = MIN(bottom_offset, shadow_box.height);

	wlr_scene_node_set_position(&c->shadow->node, shadow_box.x + left_offset,
								shadow_box.y + top_offset);

	wlr_scene_shadow_set_size(
		c->shadow, GEZERO(shadow_box.width - left_offset - right_offset),
		GEZERO(shadow_box.height - top_offset - bottom_offset));

	clipped_region.area.x = clipped_region.area.x - left_offset;
	clipped_region.area.y = clipped_region.area.y - top_offset;

	wlr_scene_shadow_set_clipped_region(c->shadow, clipped_region);
}

void apply_shield(Client *c, struct wlr_box clip_box) {
	if (active_capture_count > 0 && c->shield_when_capture) {
		wlr_scene_node_raise_to_top(&c->shield->node);
		wlr_scene_node_set_position(&c->shield->node, clip_box.x, clip_box.y);
		wlr_scene_rect_set_size(c->shield, clip_box.width, clip_box.height);
		wlr_scene_node_set_enabled(&c->shield->node, true);
	} else {
		if (c->shield->node.enabled) {
			wlr_scene_node_lower_to_bottom(&c->shield->node);
			wlr_scene_rect_set_size(c->shield, c->animation.current.width,
									c->animation.current.height);
			wlr_scene_node_set_enabled(&c->shield->node, false);
		}
	}
}

void apply_border(Client *c) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;

	bool hit_no_border = check_hit_no_border(c);
	enum corner_location current_corner_location;
	if (c->isfullscreen || (no_radius_when_single && c->mon &&
							c->mon->visible_tiling_clients == 1)) {
		current_corner_location = CORNER_LOCATION_NONE;
	} else {
		current_corner_location = set_client_corner_location(c);
	}

	if (hit_no_border && smartgaps) {
		c->bw = 0;
		c->fake_no_border = true;
	} else if (hit_no_border && !smartgaps) {
		wlr_scene_rect_set_size(c->border, 0, 0);
		wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
		c->fake_no_border = true;
		return;
	} else if (!c->isfullscreen && VISIBLEON(c, c->mon)) {
		c->bw = c->isnoborder ? 0 : borderpx;
		c->fake_no_border = false;
	}

	struct wlr_box clip_box = c->animation.current;
	// 一但在GEZERO如果使用无符号，那么其他数据也会转换为无符号导致没有负数出错
	int32_t bw = (int32_t)c->bw;

	int32_t right_offset, bottom_offset, left_offset, top_offset;

	if (c == grabc) {
		right_offset = 0;
		bottom_offset = 0;
		left_offset = 0;
		top_offset = 0;
	} else {
		right_offset =
			GEZERO(c->animation.current.x + c->animation.current.width -
				   c->mon->m.x - c->mon->m.width);
		bottom_offset =
			GEZERO(c->animation.current.y + c->animation.current.height -
				   c->mon->m.y - c->mon->m.height);

		left_offset = GEZERO(c->mon->m.x - c->animation.current.x);
		top_offset = GEZERO(c->mon->m.y - c->animation.current.y);
	}

	int32_t inner_surface_width = GEZERO(clip_box.width - 2 * bw);
	int32_t inner_surface_height = GEZERO(clip_box.height - 2 * bw);

	int32_t inner_surface_x = GEZERO(bw - left_offset);
	int32_t inner_surface_y = GEZERO(bw - top_offset);

	int32_t rect_x = left_offset;
	int32_t rect_y = top_offset;

	int32_t rect_width =
		GEZERO(c->animation.current.width - left_offset - right_offset);
	int32_t rect_height =
		GEZERO(c->animation.current.height - top_offset - bottom_offset);

	if (left_offset > c->bw)
		inner_surface_width =
			inner_surface_width - left_offset + (int32_t)c->bw;

	if (top_offset > c->bw)
		inner_surface_height =
			inner_surface_height - top_offset + (int32_t)c->bw;

	if (right_offset > 0) {
		inner_surface_width =
			MIN(clip_box.width, inner_surface_width + right_offset);
	}

	if (bottom_offset > 0) {
		inner_surface_height =
			MIN(clip_box.height, inner_surface_height + bottom_offset);
	}

	struct clipped_region clipped_region = {
		.area = {inner_surface_x, inner_surface_y, inner_surface_width,
				 inner_surface_height},
		.corner_radius = border_radius,
		.corners = current_corner_location,
	};

	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	wlr_scene_rect_set_size(c->border, rect_width, rect_height);
	wlr_scene_node_set_position(&c->border->node, rect_x, rect_y);
	wlr_scene_rect_set_corner_radius(c->border, border_radius,
									 current_corner_location);
	wlr_scene_rect_set_clipped_region(c->border, clipped_region);
}

struct ivec2 clip_to_hide(Client *c, struct wlr_box *clip_box) {
	int32_t offsetx = 0, offsety = 0, offsetw = 0, offseth = 0;
	struct ivec2 offset = {0, 0, 0, 0};

	if (!ISSCROLLTILED(c) && !c->animation.tagining && !c->animation.tagouted &&
		!c->animation.tagouting)
		return offset;

	int32_t bottom_out_offset =
		GEZERO(c->animation.current.y + c->animation.current.height -
			   c->mon->m.y - c->mon->m.height);
	int32_t right_out_offset =
		GEZERO(c->animation.current.x + c->animation.current.width -
			   c->mon->m.x - c->mon->m.width);
	int32_t left_out_offset = GEZERO(c->mon->m.x - c->animation.current.x);
	int32_t top_out_offset = GEZERO(c->mon->m.y - c->animation.current.y);

	// 必须转换为int，否计算会没有负数导致判断错误
	int32_t bw = (int32_t)c->bw;

	/*
	  计算窗口表面超出屏幕四个方向的偏差，避免窗口超出屏幕
	  需要主要border超出屏幕的时候不计算如偏差之内而是
	  要等窗口表面超出才开始计算偏差
	*/
	if (ISSCROLLTILED(c) || c->animation.tagining || c->animation.tagouted ||
		c->animation.tagouting) {
		if (left_out_offset > 0) {
			offsetx = GEZERO(left_out_offset - bw);
			clip_box->x = clip_box->x + offsetx;
			clip_box->width = clip_box->width - offsetx;
		} else if (right_out_offset > 0) {
			offsetw = GEZERO(right_out_offset - bw);
			clip_box->width = clip_box->width - offsetw;
		}

		if (top_out_offset > 0) {
			offsety = GEZERO(top_out_offset - bw);
			clip_box->y = clip_box->y + offsety;
			clip_box->height = clip_box->height - offsety;
		} else if (bottom_out_offset > 0) {
			offseth = GEZERO(bottom_out_offset - bw);
			clip_box->height = clip_box->height - offseth;
		}
	}

	// 窗口表面超出屏幕四个方向的偏差
	offset.x = offsetx;
	offset.y = offsety;
	offset.width = offsetw;
	offset.height = offseth;

	if ((clip_box->width + bw <= 0 || clip_box->height + bw <= 0) &&
		(ISSCROLLTILED(c) || c->animation.tagouting || c->animation.tagining)) {
		c->is_clip_to_hide = true;
		wlr_scene_node_set_enabled(&c->scene->node, false);
	} else if (c->is_clip_to_hide && VISIBLEON(c, c->mon)) {
		c->is_clip_to_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, true);
	}

	return offset;
}

void client_apply_clip(Client *c, float factor) {

	if (c->iskilling || !client_surface(c)->mapped)
		return;

	struct wlr_box clip_box;
	bool should_render_client_surface = false;
	struct ivec2 offset;
	BufferData buffer_data;

	enum corner_location current_corner_location =
		set_client_corner_location(c);

	if (!animations) {
		c->animation.running = false;
		c->need_output_flush = false;
		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;

		client_get_clip(c, &clip_box);

		offset = clip_to_hide(c, &clip_box);

		apply_border(c);
		client_draw_shadow(c);

		if (clip_box.width <= 0 || clip_box.height <= 0) {
			return;
		}

		apply_shield(c, clip_box);
		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);
		buffer_set_effect(c, (BufferData){1.0f, 1.0f, clip_box.width,
										  clip_box.height,
										  current_corner_location, true});
		return;
	}

	// 获取窗口动画实时位置矩形
	int32_t width, height;
	client_actual_size(c, &width, &height);

	// 计算出除了边框的窗口实际剪切大小
	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	clip_box = (struct wlr_box){
		.x = geometry.x,
		.y = geometry.y,
		.width = width,
		.height = height,
	};

	if (client_is_x11(c)) {
		clip_box.x = 0;
		clip_box.y = 0;
	}

	// 检测窗口是否需要剪切超出屏幕部分，如果需要就调整实际要剪切的矩形
	offset = clip_to_hide(c, &clip_box);

	// 应用窗口装饰
	apply_border(c);
	client_draw_shadow(c);

	// 如果窗口剪切区域已经剪切到0，则不渲染窗口表面
	if (clip_box.width <= 0 || clip_box.height <= 0) {
		should_render_client_surface = false;
		wlr_scene_node_set_enabled(&c->scene_surface->node, false);
	} else {
		should_render_client_surface = true;
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	// 不用在执行下面的窗口表面剪切和缩放等效果操作
	if (!should_render_client_surface) {
		return;
	}

	// 应用窗口表面剪切
	apply_shield(c, clip_box);
	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);

	// 获取剪切后的表面的实际大小用于计算缩放
	int32_t acutal_surface_width = geometry.width - offset.x - offset.width;
	int32_t acutal_surface_height = geometry.height - offset.y - offset.height;

	if (acutal_surface_width <= 0 || acutal_surface_height <= 0)
		return;

	buffer_data.should_scale = true;
	buffer_data.width = clip_box.width;
	buffer_data.height = clip_box.height;
	buffer_data.corner_location = current_corner_location;

	if (factor == 1.0) {
		buffer_data.width_scale = 1.0;
		buffer_data.height_scale = 1.0;
	} else {
		buffer_data.width_scale =
			(float)buffer_data.width / acutal_surface_width;
		buffer_data.height_scale =
			(float)buffer_data.height / acutal_surface_height;
	}

	buffer_set_effect(c, buffer_data);
}

void fadeout_client_animation_next_tick(Client *c) {
	if (!c)
		return;

	BufferData buffer_data;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	int32_t passed_time = timespec_to_ms(&now) - c->animation.time_started;
	double animation_passed =
		c->animation.duration
			? (double)passed_time / (double)c->animation.duration
			: 1.0;

	int32_t type = c->animation.action = c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	int32_t width = c->animation.initial.width +
					(c->current.width - c->animation.initial.width) * factor;
	int32_t height = c->animation.initial.height +
					 (c->current.height - c->animation.initial.height) * factor;

	int32_t x = c->animation.initial.x +
				(c->current.x - c->animation.initial.x) * factor;
	int32_t y = c->animation.initial.y +
				(c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);

	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	double opacity_eased_progress =
		find_animation_curve_at(animation_passed, OPAFADEOUT);

	double percent = fadeout_begin_opacity -
					 (opacity_eased_progress * fadeout_begin_opacity);

	double opacity = MAX(percent, 0);

	if (animation_fade_out && !c->nofadeout)
		wlr_scene_node_for_each_buffer(&c->scene->node,
									   scene_buffer_apply_opacity, &opacity);

	if ((c->animation_type_close &&
		 strcmp(c->animation_type_close, "zoom") == 0) ||
		(!c->animation_type_close &&
		 strcmp(animation_type_close, "zoom") == 0)) {

		buffer_data.width = width;
		buffer_data.height = height;
		buffer_data.width_scale = animation_passed;
		buffer_data.height_scale = animation_passed;

		wlr_scene_node_for_each_buffer(
			&c->scene->node, snap_scene_buffer_apply_effect, &buffer_data);
	}

	if (animation_passed >= 1.0) {
		wl_list_remove(&c->fadeout_link);
		wlr_scene_node_destroy(&c->scene->node);
		free(c);
		c = NULL;
	}
}

void client_animation_next_tick(Client *c) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	int32_t passed_time = timespec_to_ms(&now) - c->animation.time_started;
	double animation_passed =
		c->animation.duration
			? (double)passed_time / (double)c->animation.duration
			: 1.0;

	int32_t type = c->animation.action == NONE ? MOVE : c->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	Client *pointer_c = NULL;
	double sx = 0, sy = 0;
	struct wlr_surface *surface = NULL;

	int32_t width = c->animation.initial.width +
					(c->current.width - c->animation.initial.width) * factor;
	int32_t height = c->animation.initial.height +
					 (c->current.height - c->animation.initial.height) * factor;

	int32_t x = c->animation.initial.x +
				(c->current.x - c->animation.initial.x) * factor;
	int32_t y = c->animation.initial.y +
				(c->current.y - c->animation.initial.y) * factor;

	wlr_scene_node_set_position(&c->scene->node, x, y);
	c->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	c->is_pending_open_animation = false;

	if (animation_passed >= 1.0) {

		// clear the open action state
		// To prevent him from being mistaken that
		// it's still in the opening animation in resize
		c->animation.action = MOVE;

		c->animation.tagining = false;
		c->animation.running = false;

		if (c->animation.tagouting) {
			c->animation.tagouting = false;
			wlr_scene_node_set_enabled(&c->scene->node, false);
			client_set_suspended(c, true);
			c->animation.tagouted = true;
			c->animation.current = c->geom;
		}

		xytonode(cursor->x, cursor->y, NULL, &pointer_c, NULL, &sx, &sy);

		surface =
			pointer_c && pointer_c == c ? client_surface(pointer_c) : NULL;
		if (surface && pointer_c == selmon->sel) {
			wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		}

		// end flush in next frame, not the current frame
		c->need_output_flush = false;
	}

	client_apply_clip(c, factor);
}

void init_fadeout_client(Client *c) {

	if (!c->mon || client_is_unmanaged(c))
		return;

	if (!c->scene) {
		return;
	}

	if (c->shield_when_capture && active_capture_count > 0) {
		return;
	}

	if ((c->animation_type_close &&
		 strcmp(c->animation_type_close, "none") == 0) ||
		(!c->animation_type_close &&
		 strcmp(animation_type_close, "none") == 0)) {
		return;
	}

	Client *fadeout_client = ecalloc(1, sizeof(*fadeout_client));

	wlr_scene_node_set_enabled(&c->scene->node, true);
	client_set_border_color(c, bordercolor);
	fadeout_client->scene =
		wlr_scene_tree_snapshot(&c->scene->node, layers[LyrFadeOut]);
	wlr_scene_node_set_enabled(&c->scene->node, false);

	if (!fadeout_client->scene) {
		free(fadeout_client);
		return;
	}

	fadeout_client->animation.duration = animation_duration_close;
	fadeout_client->geom = fadeout_client->current =
		fadeout_client->animainit_geom = fadeout_client->animation.initial =
			c->animation.current;
	fadeout_client->mon = c->mon;
	fadeout_client->animation_type_close = c->animation_type_close;
	fadeout_client->animation.action = CLOSE;
	fadeout_client->bw = c->bw;
	fadeout_client->nofadeout = c->nofadeout;

	// 这里snap节点的坐标设置是使用的相对坐标，所以不能加上原来坐标
	// 这跟普通node有区别

	fadeout_client->animation.initial.x = 0;
	fadeout_client->animation.initial.y = 0;

	if ((!c->animation_type_close &&
		 strcmp(animation_type_close, "fade") == 0) ||
		(c->animation_type_close &&
		 strcmp(c->animation_type_close, "fade") == 0)) {
		fadeout_client->current.x = 0;
		fadeout_client->current.y = 0;
		fadeout_client->current.width = 0;
		fadeout_client->current.height = 0;
	} else if ((c->animation_type_close &&
				strcmp(c->animation_type_close, "slide") == 0) ||
			   (!c->animation_type_close &&
				strcmp(animation_type_close, "slide") == 0)) {
		fadeout_client->current.y =
			c->geom.y + c->geom.height / 2 > c->mon->m.y + c->mon->m.height / 2
				? c->mon->m.height -
					  (c->animation.current.y - c->mon->m.y) // down out
				: c->mon->m.y - c->geom.height;				 // up out
		fadeout_client->current.x = 0; // x无偏差，垂直划出
	} else {
		fadeout_client->current.y =
			(fadeout_client->geom.height -
			 fadeout_client->geom.height * zoom_end_ratio) /
			2;
		fadeout_client->current.x =
			(fadeout_client->geom.width -
			 fadeout_client->geom.width * zoom_end_ratio) /
			2;
		fadeout_client->current.width =
			fadeout_client->geom.width * zoom_end_ratio;
		fadeout_client->current.height =
			fadeout_client->geom.height * zoom_end_ratio;
	}

	fadeout_client->animation.time_started = get_now_in_ms();
	wlr_scene_node_set_enabled(&fadeout_client->scene->node, true);
	wl_list_insert(&fadeout_clients, &fadeout_client->fadeout_link);

	// 请求刷新屏幕
	request_fresh_all_monitors();
}

void client_commit(Client *c) {
	c->current = c->pending; // 设置动画的结束位置

	if (c->animation.should_animate) {
		if (!c->animation.running) {
			c->animation.current = c->animainit_geom;
		}

		c->animation.initial = c->animainit_geom;
		c->animation.time_started = get_now_in_ms();

		// 标记动画开始
		c->animation.running = true;
		c->animation.should_animate = false;
	}
	// 请求刷新屏幕
	request_fresh_all_monitors();
}

void client_set_pending_state(Client *c) {

	if (!c || c->iskilling)
		return;

	// 判断是否需要动画
	if (!animations) {
		c->animation.should_animate = false;
	} else if (animations && c->animation.tagining) {
		c->animation.should_animate = true;
	} else if (!animations || c == grabc ||
			   (!c->is_pending_open_animation &&
				wlr_box_equal(&c->current, &c->pending))) {
		c->animation.should_animate = false;
	} else {
		c->animation.should_animate = true;
	}

	if (((c->animation_type_open &&
		  strcmp(c->animation_type_open, "none") == 0) ||
		 (!c->animation_type_open &&
		  strcmp(animation_type_open, "none") == 0)) &&
		c->animation.action == OPEN) {
		c->animation.duration = 0;
	}

	if (c->istagswitching) {
		c->animation.duration = 0;
		c->istagswitching = 0;
	}

	if (start_drag_window) {
		c->animation.should_animate = false;
		c->animation.duration = 0;
	}

	if (c->isnoanimation) {
		c->animation.should_animate = false;
		c->animation.duration = 0;
	}

	// 开始动画
	client_commit(c);
	c->dirty = true;
}

void resize(Client *c, struct wlr_box geo, int32_t interact) {

	// 动画设置的起始函数，这里用来计算一些动画的起始值
	// 动画起始位置大小是由于c->animainit_geom确定的

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	struct wlr_box *bbox;
	struct wlr_box clip;

	if (!c->mon)
		return;

	c->need_output_flush = true;
	c->dirty = true;

	// float_geom = c->geom;
	bbox = (interact || c->isfloating || c->isfullscreen) ? &sgeom : &c->mon->w;

	if (is_scroller_layout(c->mon) && (!c->isfloating || c == grabc)) {
		c->geom = geo;
		c->geom.width = MAX(1 + 2 * (int32_t)c->bw, c->geom.width);
		c->geom.height = MAX(1 + 2 * (int32_t)c->bw, c->geom.height);
	} else { // 这里会限制不允许窗口划出屏幕
		c->geom = geo;
		applybounds(
			c,
			bbox); // 去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常
	}

	if (!c->isnosizehint && !c->ismaximizescreen && !c->isfullscreen &&
		c->isfloating) {
		client_set_size_bound(c);
	}

	if (!c->is_pending_open_animation) {
		c->animation.begin_fade_in = false;
	}

	if (c->animation.action == OPEN && !c->animation.tagining &&
		!c->animation.tagouting && wlr_box_equal(&c->geom, &c->current)) {
		c->animation.action = c->animation.action;
	} else if (c->animation.tagouting) {
		c->animation.duration = animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->animation.tagining) {
		c->animation.duration = animation_duration_tag;
		c->animation.action = TAG;
	} else if (c->is_pending_open_animation) {
		c->animation.duration = animation_duration_open;
		c->animation.action = OPEN;
	} else {
		c->animation.duration = animation_duration_move;
		c->animation.action = MOVE;
	}

	// 动画起始位置大小设置
	if (c->animation.tagouting) {
		c->animainit_geom = c->animation.current;
	} else if (c->animation.tagining) {
		c->animainit_geom.height = c->animation.current.height;
		c->animainit_geom.width = c->animation.current.width;
	} else if (c->is_pending_open_animation) {
		set_client_open_animation(c, c->geom);
	} else {
		c->animainit_geom = c->animation.current;
	}

	if (c->isnoborder || c->iskilling) {
		c->bw = 0;
	}

	bool hit_no_border = check_hit_no_border(c);
	if (hit_no_border && smartgaps) {
		c->bw = 0;
		c->fake_no_border = true;
	}

	// c->geom 是真实的窗口大小和位置，跟过度的动画无关，用于计算布局
	c->configure_serial = client_set_size(c, c->geom.width - 2 * c->bw,
										  c->geom.height - 2 * c->bw);

	if (c->configure_serial != 0) {
		c->mon->resizing_count_pending++;
	}

	if (c == grabc) {
		c->animation.running = false;
		c->need_output_flush = false;

		c->animainit_geom = c->current = c->pending = c->animation.current =
			c->geom;
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);

		client_draw_shadow(c);
		apply_border(c);
		client_get_clip(c, &clip);
		apply_shield(c, clip);
		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
		return;
	}
	// 如果不是工作区切换时划出去的窗口，就让动画的结束位置，就是上面的真实位置和大小
	// c->pending 决定动画的终点，一般在其他调用resize的函数的附近设置了
	if (!c->animation.tagouting && !c->iskilling) {
		c->pending = c->geom;
	}

	if (c->swallowedby && c->animation.action == OPEN) {
		c->animainit_geom = c->swallowedby->animation.current;
	}

	if (c->swallowing) {
		c->animainit_geom = c->geom;
	}

	if ((c->isglobal || c->isunglobal) && c->isfloating &&
		c->animation.action == TAG) {
		c->animainit_geom = c->geom;
	}

	if (c->scratchpad_switching_mon && c->isfloating) {
		c->animainit_geom = c->geom;
	}

	// 开始应用动画设置
	client_set_pending_state(c);

	setborder_color(c);
}

bool client_draw_fadeout_frame(Client *c) {
	if (!c)
		return false;

	fadeout_client_animation_next_tick(c);
	return true;
}

void client_set_focused_opacity_animation(Client *c) {
	float *border_color = get_border_color(c);

	if (!animations) {
		setborder_color(c);
		return;
	}

	c->opacity_animation.duration = animation_duration_focus;
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	c->opacity_animation.target_opacity = c->focused_opacity;
	c->opacity_animation.time_started = get_now_in_ms();
	memcpy(c->opacity_animation.initial_border_color,
		   c->opacity_animation.current_border_color,
		   sizeof(c->opacity_animation.initial_border_color));
	c->opacity_animation.initial_opacity = c->opacity_animation.current_opacity;

	c->opacity_animation.running = true;
}

void client_set_unfocused_opacity_animation(Client *c) {
	// Start border color animation to unfocused
	float *border_color = get_border_color(c);

	if (!animations) {
		setborder_color(c);
		return;
	}

	c->opacity_animation.duration = animation_duration_focus;
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	// Start opacity animation to unfocused
	c->opacity_animation.target_opacity = c->unfocused_opacity;
	c->opacity_animation.time_started = get_now_in_ms();

	memcpy(c->opacity_animation.initial_border_color,
		   c->opacity_animation.current_border_color,
		   sizeof(c->opacity_animation.initial_border_color));
	c->opacity_animation.initial_opacity = c->opacity_animation.current_opacity;

	c->opacity_animation.running = true;
}

bool client_apply_focus_opacity(Client *c) {
	// Animate focus transitions (opacity + border color)
	float *border_color = get_border_color(c);
	if (c->isfullscreen) {
		c->opacity_animation.running = false;
		client_set_opacity(c, 1);
	} else if (c->animation.running && c->animation.action == OPEN) {
		c->opacity_animation.running = false;
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		int32_t passed_time = timespec_to_ms(&now) - c->animation.time_started;
		double linear_progress =
			c->animation.duration
				? (double)passed_time / (double)c->animation.duration
				: 1.0;

		double opacity_eased_progress =
			find_animation_curve_at(linear_progress, OPAFADEIN);

		float percent =
			animation_fade_in && !c->nofadein ? opacity_eased_progress : 1.0;
		float opacity =
			c == selmon->sel ? c->focused_opacity : c->unfocused_opacity;

		float target_opacity =
			percent * (1.0 - fadein_begin_opacity) + fadein_begin_opacity;
		if (target_opacity > opacity) {
			target_opacity = opacity;
		}
		memcpy(c->opacity_animation.current_border_color,
			   c->opacity_animation.target_border_color,
			   sizeof(c->opacity_animation.current_border_color));
		c->opacity_animation.current_opacity = target_opacity;
		client_set_opacity(c, target_opacity);
		client_set_border_color(c, c->opacity_animation.target_border_color);
	} else if (animations && c->opacity_animation.running) {

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		int32_t passed_time =
			timespec_to_ms(&now) - c->opacity_animation.time_started;
		double linear_progress =
			c->opacity_animation.duration
				? (double)passed_time / (double)c->opacity_animation.duration
				: 1.0;

		float eased_progress = find_animation_curve_at(linear_progress, FOCUS);

		c->opacity_animation.current_opacity =
			c->opacity_animation.initial_opacity +
			(c->opacity_animation.target_opacity -
			 c->opacity_animation.initial_opacity) *
				eased_progress;
		client_set_opacity(c, c->opacity_animation.current_opacity);

		// Animate border color
		for (int32_t i = 0; i < 4; i++) {
			c->opacity_animation.current_border_color[i] =
				c->opacity_animation.initial_border_color[i] +
				(c->opacity_animation.target_border_color[i] -
				 c->opacity_animation.initial_border_color[i]) *
					eased_progress;
		}
		client_set_border_color(c, c->opacity_animation.current_border_color);
		if (linear_progress >= 1.0f) {
			c->opacity_animation.running = false;
		} else {
			return true;
		}
	} else if (c == selmon->sel) {
		c->opacity_animation.running = false;
		c->opacity_animation.current_opacity = c->focused_opacity;
		memcpy(c->opacity_animation.current_border_color, border_color,
			   sizeof(c->opacity_animation.current_border_color));
		client_set_opacity(c, c->focused_opacity);
	} else {
		c->opacity_animation.running = false;
		c->opacity_animation.current_opacity = c->unfocused_opacity;
		memcpy(c->opacity_animation.current_border_color, border_color,
			   sizeof(c->opacity_animation.current_border_color));
		client_set_opacity(c, c->unfocused_opacity);
	}

	return false;
}

bool client_draw_frame(Client *c) {

	if (!c || !client_surface(c)->mapped)
		return false;

	if (!c->need_output_flush) {
		return client_apply_focus_opacity(c);
	}

	if (animations && c->animation.running) {
		client_animation_next_tick(c);
	} else {
		wlr_scene_node_set_position(&c->scene->node, c->pending.x,
									c->pending.y);
		c->animation.current = c->animainit_geom = c->animation.initial =
			c->pending = c->current = c->geom;
		client_apply_clip(c, 1.0);
		c->need_output_flush = false;
	}
	client_apply_focus_opacity(c);
	return true;
}
