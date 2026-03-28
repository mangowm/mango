static void canvas_geom_init(Client *c, Monitor *m, uint32_t tag, float pan_x,
							 float pan_y, int *cascade_idx) {
	int w = c->geom.width > 0 ? c->geom.width : 640;
	int h = c->geom.height > 0 ? c->geom.height : 480;
	float zoom = m->pertag->canvas_zoom[tag];

	float cx = pan_x + (m->w.width / 2.0f) / zoom;
	float cy = pan_y + (m->w.height / 2.0f) / zoom;

	if (c->is_pending_open_animation) {
		c->canvas_geom[tag].x = (int32_t)(cx - w / 2.0f);
		c->canvas_geom[tag].y = (int32_t)(cy - h / 2.0f);
	} else {
		c->canvas_geom[tag].x = (int32_t)(cx + (*cascade_idx) * 30 - w / 2.0f);
		c->canvas_geom[tag].y = (int32_t)(cy + (*cascade_idx) * 30 - h / 2.0f);
		(*cascade_idx)++;
	}
	c->canvas_geom[tag].width = w;
	c->canvas_geom[tag].height = h;
}

static void canvas_reposition(Monitor *m) {
	Client *c;
	uint32_t tag = m->pertag->curtag;

	float pan_x = m->pertag->canvas_pan_x[tag];
	float pan_y = m->pertag->canvas_pan_y[tag];
	float zoom = m->pertag->canvas_zoom[tag];

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isunglobal)
			continue;
		if (c->isfullscreen || c->ismaximizescreen)
			continue;
		if (c->canvas_geom[tag].width <= 0 || c->canvas_geom[tag].height <= 0)
			continue;

		int new_x =
			m->w.x + (int32_t)roundf((c->canvas_geom[tag].x - pan_x) * zoom);
		int new_y =
			m->w.y + (int32_t)roundf((c->canvas_geom[tag].y - pan_y) * zoom);

		if (c->animation.running) {
			c->animation.running = false;
			c->need_output_flush = false;
		}

		c->geom.x = new_x;
		c->geom.y = new_y;
		c->pending.x = new_x;
		c->pending.y = new_y;
		c->current.x = new_x;
		c->current.y = new_y;
		c->animation.current.x = new_x;
		c->animation.current.y = new_y;
		c->animation.initial.x = new_x;
		c->animation.initial.y = new_y;
		c->animainit_geom.x = new_x;
		c->animainit_geom.y = new_y;

		wlr_scene_node_set_position(&c->scene->node, new_x, new_y);

		client_apply_clip(c, 1.0);
		if (!c->is_clip_to_hide)
			wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, NULL);
		if (zoom == 1.0f)
			clear_visual_zoom(c);
		else
			apply_visual_zoom(c, zoom);
	}
}

static void canvas(Monitor *m) {
	Client *c;
	uint32_t tag = m->pertag->curtag;

	float pan_x = m->pertag->canvas_pan_x[tag];
	float pan_y = m->pertag->canvas_pan_y[tag];
	float zoom = m->pertag->canvas_zoom[tag];

	int cascade_idx = 0;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isunglobal)
			continue;

		if (c->isfullscreen || c->ismaximizescreen)
			continue;

		if (c->canvas_geom[tag].width == 0 && c->canvas_geom[tag].height == 0)
			canvas_geom_init(c, m, tag, pan_x, pan_y, &cascade_idx);

		int screen_x =
			m->w.x + (int32_t)roundf((c->canvas_geom[tag].x - pan_x) * zoom);
		int screen_y =
			m->w.y + (int32_t)roundf((c->canvas_geom[tag].y - pan_y) * zoom);

		float effective_zoom = zoom;
		int32_t base_w = c->canvas_geom[tag].width;
		int32_t base_h = c->canvas_geom[tag].height;

		if (m->canvas_in_overview && c->canvas_geom_backup[tag].width > 0) {
			base_w = c->canvas_geom_backup[tag].width;
			base_h = c->canvas_geom_backup[tag].height;
			effective_zoom *= (float)c->canvas_geom[tag].width / base_w;
		}

		struct wlr_box client_geom = {
			.x = screen_x,
			.y = screen_y,
			.width = base_w,
			.height = base_h,
		};
		resize(c, client_geom, 0);

		if (effective_zoom == 1.0f)
			clear_visual_zoom(c);
		else
			apply_visual_zoom(c, effective_zoom);
	}
}

static void canvas_pan_to_client(Monitor *m, Client *c) {
	if (!m || !c || !is_canvas_layout(m))
		return;

	uint32_t tag = m->pertag->curtag;
	if (c->canvas_geom[tag].width <= 0 || c->canvas_geom[tag].height <= 0)
		return;

	float zoom = m->pertag->canvas_zoom[tag];
	float pan_x = m->pertag->canvas_pan_x[tag];
	float pan_y = m->pertag->canvas_pan_y[tag];
	float vp_w = m->w.width / zoom;
	float vp_h = m->w.height / zoom;

	float cx = c->canvas_geom[tag].x;
	float cy = c->canvas_geom[tag].y;
	float cw = c->canvas_geom[tag].width;
	float ch = c->canvas_geom[tag].height;

	if (cx >= pan_x && cy >= pan_y &&
		cx + cw <= pan_x + vp_w && cy + ch <= pan_y + vp_h)
		return;

	m->pertag->canvas_pan_x[tag] = cx + cw / 2.0f - vp_w / 2.0f;
	m->pertag->canvas_pan_y[tag] = cy + ch / 2.0f - vp_h / 2.0f;
	canvas_reposition(m);
}
