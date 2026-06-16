
typedef struct {
	float x, y, w, h;
} OvPlacedRect;

typedef struct {
	float x, y;
} OvPoint;

typedef struct {
	Client *c;
	float orig_w;
	float orig_h;
	float area;
} OvLayoutItem;

static int compare_layout_items(const void *a, const void *b) {
	float area_a = ((const OvLayoutItem *)a)->area;
	float area_b = ((const OvLayoutItem *)b)->area;
	if (area_a < area_b)
		return 1;
	if (area_a > area_b)
		return -1;
	return 0;
}

static bool try_place(OvPlacedRect *placed, int placed_cnt, float w, float h,
					  float gap, float avail_w, float avail_h,
					  OvPlacedRect *out, OvPoint *cands, OvPoint *feas) {
	int cand_cnt = 0;
	cands[cand_cnt++] = (OvPoint){0.0f, 0.0f};

	for (int i = 0; i < placed_cnt; i++) {
		OvPlacedRect p = placed[i];
		cands[cand_cnt++] = (OvPoint){p.x + p.w + gap, p.y};
		cands[cand_cnt++] = (OvPoint){p.x, p.y + p.h + gap};
		cands[cand_cnt++] = (OvPoint){p.x + p.w + gap, p.y + p.h + gap};
	}

	int unique_cnt = 0;
	for (int i = 0; i < cand_cnt; i++) {
		bool dup = false;
		for (int j = 0; j < unique_cnt; j++) {
			if (fabs(cands[i].x - cands[j].x) < 0.5f &&
				fabs(cands[i].y - cands[j].y) < 0.5f) {
				dup = true;
				break;
			}
		}
		if (!dup)
			cands[unique_cnt++] = cands[i];
	}
	cand_cnt = unique_cnt;

	int feas_cnt = 0;
	for (int i = 0; i < cand_cnt; i++) {
		float cx = cands[i].x;
		float cy = cands[i].y;

		if (cx < 0 || cy < 0 || cx + w > avail_w || cy + h > avail_h)
			continue;

		bool overlap = false;
		for (int j = 0; j < placed_cnt; j++) {
			OvPlacedRect p = placed[j];
			if (!(cx + w + gap <= p.x || cx >= p.x + p.w + gap ||
				  cy + h + gap <= p.y || cy >= p.y + p.h + gap)) {
				overlap = true;
				break;
			}
		}
		if (!overlap) {
			feas[feas_cnt++] = (OvPoint){cx, cy};
		}
	}

	if (feas_cnt == 0)
		return false;

	int best = 0;
	for (int i = 1; i < feas_cnt; i++) {
		if (feas[i].y < feas[best].y ||
			(fabs(feas[i].y - feas[best].y) < 0.5f &&
			 feas[i].x < feas[best].x)) {
			best = i;
		}
	}

	out->x = feas[best].x;
	out->y = feas[best].y;
	out->w = w;
	out->h = h;
	return true;
}

void overview_scale(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	int orig_n = m->visible_clients;
	if (orig_n == 0)
		return;

	OvLayoutItem *items = calloc(orig_n, sizeof(OvLayoutItem));
	if (!items)
		return;

	int n = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			items[n].c = c;
			float w = c->overview_backup_geom.width;
			float h = c->overview_backup_geom.height;
			if (w <= 0 || h <= 0) {
				w = 100.0f;
				h = 100.0f;
			}
			items[n].orig_w = w;
			items[n].orig_h = h;
			items[n].area = w * h;
			n++;
		}
	}

	if (n == 0) {
		free(items);
		return;
	}

	qsort(items, n, sizeof(OvLayoutItem), compare_layout_items);

	float max_avail_w = fmaxf(1.0f, m->w.width - 2 * target_gappo);
	float max_avail_h = fmaxf(1.0f, m->w.height - 2 * target_gappo);

	int max_points = 1 + 3 * n;
	OvPlacedRect *placed = calloc(n, sizeof(OvPlacedRect));
	OvPoint *cands = calloc(max_points, sizeof(OvPoint));
	OvPoint *feas = calloc(max_points, sizeof(OvPoint));

	if (!placed || !cands || !feas) {
		free(items);
		free(placed);
		free(cands);
		free(feas);
		return;
	}

	float low = 0.0f, high = 1.0f, best_s = 0.0f;
	for (int iter = 0; iter < 50; iter++) {
		float mid = (low + high) / 2.0f;
		bool ok = true;
		int placed_cnt = 0;

		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * mid;
			float h = items[k].orig_h * mid;
			OvPlacedRect out;
			if (!try_place(placed, placed_cnt, w, h, (float)target_gappi,
						   max_avail_w, max_avail_h, &out, cands, feas)) {
				ok = false;
				break;
			}
			placed[placed_cnt++] = out;
		}

		if (ok) {
			best_s = mid;
			low = mid;
		} else {
			high = mid;
		}
	}

	if (best_s > 0.0f) {
		int placed_cnt = 0;

		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * best_s;
			float h = items[k].orig_h * best_s;
			OvPlacedRect out;
			try_place(placed, placed_cnt, w, h, (float)target_gappi,
					  max_avail_w, max_avail_h, &out, cands, feas);
			placed[placed_cnt++] = out;
		}

		if (n > 1) {
			float grid_box_w = 0;
			for (int k = 0; k < n - 1; k++) {
				float r = placed[k].x + placed[k].w;
				if (r > grid_box_w)
					grid_box_w = r;
			}

			OvPlacedRect *last = &placed[n - 1];
			float max_x = grid_box_w - last->w;

			if (max_x > last->x) {
				for (int k = 0; k < n - 1; k++) {
					OvPlacedRect p = placed[k];
					if (!(last->y + last->h + target_gappi <= p.y ||
						  last->y >= p.y + p.h + target_gappi)) {
						if (p.x > last->x) {
							float limit = p.x - target_gappi - last->w;
							if (limit < max_x) {
								max_x = limit;
							}
						}
					}
				}

				if (max_x > last->x) {
					last->x += (max_x - last->x) / 2.0f;
				}
			}
		}

		float box_w = 0, box_h = 0;
		for (int k = 0; k < n; k++) {
			float r = placed[k].x + placed[k].w;
			float b = placed[k].y + placed[k].h;
			if (r > box_w)
				box_w = r;
			if (b > box_h)
				box_h = b;
		}

		float dx = (max_avail_w - box_w) / 2.0f;
		float dy = (max_avail_h - box_h) / 2.0f;
		float base_x = m->w.x + target_gappo + dx;
		float base_y = m->w.y + target_gappo + dy;

		for (int k = 0; k < n; k++) {
			Client *cl = items[k].c;
			struct wlr_box geom;
			geom.x = (int)(base_x + placed[k].x + 0.5f);
			geom.y = (int)(base_y + placed[k].y + 0.5f);
			float w = items[k].orig_w * best_s;
			float h = items[k].orig_h * best_s;
			geom.width = (int)(geom.x + w + 0.5f) - geom.x;
			geom.height = (int)(geom.y + h + 0.5f) - geom.y;
			resize(cl, geom, 0);
		}
	}

	free(items);
	free(placed);
	free(cands);
	free(feas);
}

void overview_resize(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;
	float single_width_ratio = 0.7f;
	float single_height_ratio = 0.8f;

	int orig_n = m->visible_clients;
	if (orig_n == 0)
		return;

	Client **c_arr = malloc(orig_n * sizeof(Client *));
	if (!c_arr)
		return;

	int n = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			c_arr[n++] = c;
		}
	}

	if (n == 0) {
		free(c_arr);
		return;
	}

	if (n == 1) {
		int32_t cw = (m->w.width - 2 * target_gappo) * single_width_ratio;
		int32_t ch = (m->w.height - 2 * target_gappo) * single_height_ratio;
		c_arr[0]->geom.x = m->w.x + (m->w.width - cw) / 2;
		c_arr[0]->geom.y = m->w.y + (m->w.height - ch) / 2;
		c_arr[0]->geom.width = cw;
		c_arr[0]->geom.height = ch;
		resize(c_arr[0], c_arr[0]->geom, 0);
		free(c_arr);
		return;
	}

	if (n == 2) {
		int32_t cw = (m->w.width - 2 * target_gappo - target_gappi) / 2;
		int32_t ch = (m->w.height - 2 * target_gappo) * 0.65f;

		c_arr[0]->geom.x = m->w.x + target_gappo;
		c_arr[0]->geom.y = m->w.y + (m->w.height - ch) / 2 + target_gappo;
		c_arr[0]->geom.width = cw;
		c_arr[0]->geom.height = ch;
		resize(c_arr[0], c_arr[0]->geom, 0);

		c_arr[1]->geom.x = m->w.x + cw + target_gappo + target_gappi;
		c_arr[1]->geom.y = m->w.y + (m->w.height - ch) / 2 + target_gappo;
		c_arr[1]->geom.width = cw;
		c_arr[1]->geom.height = ch;
		resize(c_arr[1], c_arr[1]->geom, 0);

		free(c_arr);
		return;
	}

	int32_t cols = 1;
	while (cols * cols < n) {
		cols++;
	}
	int32_t rows = (n + cols - 1) / cols;

	int32_t ch =
		(m->w.height - 2 * target_gappo - (rows - 1) * target_gappi) / rows;
	int32_t cw =
		(m->w.width - 2 * target_gappo - (cols - 1) * target_gappi) / cols;

	if (ch < 1)
		ch = 1;
	if (cw < 1)
		cw = 1;

	int32_t overcols = n % cols;
	int32_t dx = 0;
	if (overcols) {
		dx = (m->w.width - overcols * cw - (overcols - 1) * target_gappi) / 2 -
			 target_gappo;
	}

	for (int i = 0; i < n; i++) {
		int32_t cx = m->w.x + (i % cols) * (cw + target_gappi);
		int32_t cy = m->w.y + (i / cols) * (ch + target_gappi);

		if (overcols && i >= n - overcols) {
			cx += dx;
		}

		c_arr[i]->geom.x = cx + target_gappo;
		c_arr[i]->geom.y = cy + target_gappo;
		c_arr[i]->geom.width = cw;
		c_arr[i]->geom.height = ch;
		resize(c_arr[i], c_arr[i]->geom, 0);
	}

	free(c_arr);
}

void create_jump_hints(Monitor *m) {
	const char jump_labels[] = "HJKLASDFGQWERTYUIOPZXCVBNM";
	int label_idx = 0;
	Client *c;

	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) &&
			((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
			if (label_idx >= 26)
				break;
			char c_char = jump_labels[label_idx];
			c->jump_char = c_char;

			// 把字符变成字符串
			char label_text[2] = {c_char, '\0'};

			mango_text_node_update(c->text_node, label_text, 1.0f);
			wlr_scene_node_set_enabled(&c->text_node->scene_buffer->node, true);
			wlr_scene_node_raise_to_top(&c->text_node->scene_buffer->node);
			wlr_scene_node_set_position(
				&c->text_node->scene_buffer->node,
				c->geom.width / 2 - c->text_node->logical_width / 2,
				c->geom.height / 2 - c->text_node->logical_height / 2);
			label_idx++;
		}
	}
}

void begin_jump_mode(Monitor *m) { m->is_jump_mode = 1; }

void finish_jump_mode(Monitor *m) {
	Client *c = NULL;

	if (!m->is_jump_mode) {
		return;
	}

	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m)) {
			if (c->text_node->scene_buffer->node.enabled) {
				c->jump_char = '\0';
				wlr_scene_node_set_enabled(&c->text_node->scene_buffer->node,
										   false);
			}
		}
	}

	m->is_jump_mode = 0;
}

void overview(Monitor *m) {

	if (config.ov_no_resize) {
		overview_scale(m);
	} else {
		overview_resize(m);
	}

	if (m->is_jump_mode) {
		create_jump_hints(m);
	}
}