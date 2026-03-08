void vertical_tile(Monitor *m) {
	int32_t i, n = 0, w, r, ie = enablegaps, mh, mx, tx;
	Client *c = NULL;
	Client *fc = NULL;
	double mfact = 0;
	int32_t master_num = 0;
	int32_t stack_num = 0;

	n = m->visible_tiling_clients;
	master_num = m->pertag->nmasters[m->pertag->curtag];
	master_num = n > master_num ? master_num : n;
	stack_num = n - master_num;

	if (n == 0)
		return;

	int32_t cur_gapih = enablegaps ? m->gappih : 0;
	int32_t cur_gapiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gapoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gapov = enablegaps ? m->gappov : 0;

	cur_gapih = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapih;
	cur_gapiv = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapiv;
	cur_gapoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapoh;
	cur_gapov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapov;

	wl_list_for_each(fc, &clients, link) {
		if (VISIBLEON(fc, m) && ISTILED(fc))
			break;
	}

	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[m->pertag->curtag];

	if (n > m->pertag->nmasters[m->pertag->curtag])
		mh = m->pertag->nmasters[m->pertag->curtag]
				 ? (m->w.height + cur_gapiv * ie) * mfact
				 : 0;
	else
		mh = m->w.height - 2 * cur_gapov + cur_gapiv * ie;

	i = 0;
	mx = tx = cur_gapoh;

	int32_t master_surplus_width =
		(m->w.width - 2 * cur_gapoh - cur_gapih * ie * (master_num - 1));
	float master_surplus_ratio = 1.0;

	int32_t slave_surplus_width =
		(m->w.width - 2 * cur_gapoh - cur_gapih * ie * (stack_num - 1));
	float slave_surplus_ratio = 1.0;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		if (i < m->pertag->nmasters[m->pertag->curtag]) {
			r = MIN(n, m->pertag->nmasters[m->pertag->curtag]) - i;
			if (c->master_inner_per > 0.0f) {
				w = master_surplus_width * c->master_inner_per /
					master_surplus_ratio;
				master_surplus_width = master_surplus_width - w;
				master_surplus_ratio =
					master_surplus_ratio - c->master_inner_per;
				c->master_mfact_per = mfact;
			} else {
				w = (m->w.width - mx - cur_gapih - cur_gapih * ie * (r - 1)) /
					r;
				c->master_inner_per = w / (m->w.width - mx - cur_gapih -
										   cur_gapih * ie * (r - 1));
				c->master_mfact_per = mfact;
			}
			resize(c,
				   (struct wlr_box){.x = m->w.x + mx,
									.y = m->w.y + cur_gapov,
									.width = w,
									.height = mh - cur_gapiv * ie},
				   0);
			mx += c->geom.width + cur_gapih * ie;
		} else {
			r = n - i;
			if (c->stack_inner_per > 0.0f) {
				w = slave_surplus_width * c->stack_inner_per /
					slave_surplus_ratio;
				slave_surplus_width = slave_surplus_width - w;
				slave_surplus_ratio = slave_surplus_ratio - c->stack_inner_per;
				c->master_mfact_per = mfact;
			} else {
				w = (m->w.width - tx - cur_gapih - cur_gapih * ie * (r - 1)) /
					r;
				c->stack_inner_per = w / (m->w.width - tx - cur_gapih -
										  cur_gapih * ie * (r - 1));
				c->master_mfact_per = mfact;
			}

			resize(c,
				   (struct wlr_box){.x = m->w.x + tx,
									.y = m->w.y + mh + cur_gapov,
									.width = w,
									.height = m->w.height - mh - 2 * cur_gapov},
				   0);
			tx += c->geom.width + cur_gapih * ie;
		}
		i++;
	}
}

void vertical_deck(Monitor *m) {
	int32_t mh, mx;
	int32_t i, n = 0;
	Client *c = NULL;
	Client *fc = NULL;
	float mfact;
	uint32_t nmasters = m->pertag->nmasters[m->pertag->curtag];

	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;

	cur_gappiv = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;
	cur_gappov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;

	n = m->visible_tiling_clients;

	if (n == 0)
		return;

	wl_list_for_each(fc, &clients, link) {

		if (VISIBLEON(fc, m) && ISTILED(fc))
			break;
	}

	// Calculate master width using mfact from pertag
	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[m->pertag->curtag];

	if (n > nmasters)
		mh = nmasters ? round((m->w.height - 2 * cur_gappov) * mfact) : 0;
	else
		mh = m->w.height - 2 * cur_gappov;

	i = mx = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		if (i < nmasters) {
			resize(
				c,
				(struct wlr_box){.x = m->w.x + cur_gappoh + mx,
								 .y = m->w.y + cur_gappov,
								 .width = (m->w.width - 2 * cur_gappoh - mx) /
										  (MIN(n, nmasters) - i),
								 .height = mh},
				0);
			mx += c->geom.width;
		} else {
			resize(c,
				   (struct wlr_box){.x = m->w.x + cur_gappoh,
									.y = m->w.y + mh + cur_gappov + cur_gappiv,
									.width = m->w.width - 2 * cur_gappoh,
									.height = m->w.height - mh -
											  2 * cur_gappov - cur_gappiv},
				   0);
			if (c == focustop(m))
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
		i++;
	}
}

void vertical_scroll_adjust_fullandmax(Client *c, struct wlr_box *target_geom) {
	Monitor *m = c->mon;
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	cur_gappiv =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappov =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappoh =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappoh;

	if (c->isfullscreen) {
		target_geom->width = m->m.width;
		target_geom->height = m->m.height;
		target_geom->x = m->m.x;
		return;
	}

	if (c->ismaximizescreen) {
		target_geom->width = m->w.width - 2 * cur_gappoh;
		target_geom->height = m->w.height - 2 * cur_gappov;
		target_geom->x = m->w.x + cur_gappoh;
		return;
	}

	target_geom->width = m->w.width - 2 * cur_gappoh;
	target_geom->x = m->w.x + (m->w.width - target_geom->width) / 2;
}

void arrange_stack_vertical(Client *scroller_stack_head,
							struct wlr_box geometry, int32_t gappih) {
	int32_t stack_size = 0;
	Client *iter = scroller_stack_head;

	while (iter) {
		stack_size++;
		iter = iter->next_in_stack;
	}

	if (stack_size == 0)
		return;

	float total_proportion = 0.0f;
	iter = scroller_stack_head;
	while (iter) {
		if (iter->stack_proportion <= 0.0f || iter->stack_proportion >= 1.0f) {
			iter->stack_proportion =
				stack_size == 1 ? 1.0f : 1.0f / (stack_size - 1);
		}
		total_proportion += iter->stack_proportion;
		iter = iter->next_in_stack;
	}

	iter = scroller_stack_head;
	while (iter) {
		iter->stack_proportion = iter->stack_proportion / total_proportion;
		iter = iter->next_in_stack;
	}

	int32_t client_width;
	int32_t current_x = geometry.x;
	int32_t remain_client_width = geometry.width - (stack_size - 1) * gappih;
	float remain_proportion = 1.0f;

	iter = scroller_stack_head;
	while (iter) {

		client_width =
			remain_client_width * (iter->stack_proportion / remain_proportion);

		struct wlr_box client_geom = {.y = geometry.y,
									  .x = current_x,
									  .height = geometry.height,
									  .width = client_width};
		resize(iter, client_geom, 0);
		remain_proportion -= iter->stack_proportion;
		remain_client_width -= client_width;
		current_x += client_width + gappih;
		iter = iter->next_in_stack;
	}
}

void vertical_check_scroller_root_inside_mon(Client *c,
											 struct wlr_box *geometry) {
	if (!GEOMINSIDEMON(geometry, c->mon)) {
		geometry->y = c->mon->w.y + (c->mon->w.height - geometry->height) / 2;
	}
}

// 竖屏滚动布局
void vertical_scroller(Monitor *m) {
	int32_t i, n, j;
	float single_proportion = 1.0;

	Client *c = NULL, *root_client = NULL;
	Client **tempClients = NULL;
	struct wlr_box target_geom;
	int32_t focus_client_index = 0;
	bool need_scroller = false;
	bool over_overspread_to_up = false;
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;

	cur_gappiv =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappov =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappoh =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappoh;

	int32_t max_client_height = m->w.height - 2 * scroller_structs - cur_gappiv;

	n = m->visible_scroll_tiling_clients;

	if (n == 0) {
		return;
	}

	tempClients = malloc(n * sizeof(Client *));
	if (!tempClients) {
		return;
	}

	j = 0;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c) && !c->prev_in_stack) {
			tempClients[j] = c;
			j++;
		}
	}

	if (n == 1 && !scroller_ignore_proportion_single &&
		!tempClients[0]->isfullscreen && !tempClients[0]->ismaximizescreen) {
		c = tempClients[0];

		single_proportion = c->scroller_proportion_single > 0.0f
								? c->scroller_proportion_single
								: scroller_default_proportion_single;

		target_geom.width = m->w.width - 2 * cur_gappoh;
		target_geom.height = (m->w.height - 2 * cur_gappov) * single_proportion;
		target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		vertical_check_scroller_root_inside_mon(c, &target_geom);
		arrange_stack_vertical(c, target_geom, cur_gappih);
		free(tempClients);
		return;
	}

	if (m->sel && !client_is_unmanaged(m->sel) && ISSCROLLTILED(m->sel)) {
		root_client = m->sel;
	} else if (m->prevsel && ISSCROLLTILED(m->prevsel) &&
			   VISIBLEON(m->prevsel, m) && !client_is_unmanaged(m->prevsel)) {
		root_client = m->prevsel;
	} else {
		root_client = center_tiled_select(m);
	}

	// root_client might be in a stack, find the stack head
	if (root_client) {
		root_client = get_scroll_stack_head(root_client);
	}

	if (!root_client) {
		free(tempClients);
		return;
	}

	for (i = 0; i < n; i++) {
		c = tempClients[i];
		if (root_client == c) {
			if (c->geom.y >= m->w.y + scroller_structs &&
				c->geom.y + c->geom.height <=
					m->w.y + m->w.height - scroller_structs) {
				need_scroller = false;
			} else {
				need_scroller = true;
			}
			focus_client_index = i;
			break;
		}
	}

	bool need_apply_overspread =
		scroller_prefer_overspread && m->visible_scroll_tiling_clients > 1 &&
		(focus_client_index == 0 || focus_client_index == n - 1) &&
		tempClients[focus_client_index]->scroller_proportion < 1.0f;

	if (need_apply_overspread) {

		if (focus_client_index == 0) {
			over_overspread_to_up = true;
		} else {
			over_overspread_to_up = false;
		}

		if (over_overspread_to_up &&
			(!INSIDEMON(tempClients[1]) ||
			 (tempClients[1]->scroller_proportion +
				  tempClients[focus_client_index]->scroller_proportion >=
			  1.0f))) {
			need_scroller = true;
		} else if (!over_overspread_to_up &&
				   (!INSIDEMON(tempClients[focus_client_index - 1]) ||
					(tempClients[focus_client_index - 1]->scroller_proportion +
						 tempClients[focus_client_index]->scroller_proportion >=
					 1.0f))) {
			need_scroller = true;
		} else {
			need_apply_overspread = false;
		}
	}

	bool need_apply_center =
		scroller_focus_center || m->visible_scroll_tiling_clients == 1 ||
		(scroller_prefer_center && !need_apply_overspread &&
		 (!m->prevsel ||
		  (ISSCROLLTILED(m->prevsel) &&
		   (m->prevsel->scroller_proportion * max_client_height) +
				   (tempClients[focus_client_index]->scroller_proportion *
					max_client_height) >
			   m->w.height - 2 * scroller_structs - cur_gappiv)));

	if (n == 1 && scroller_ignore_proportion_single) {
		need_scroller = true;
	}

	if (start_drag_window)
		need_scroller = false;

	target_geom.width = m->w.width - 2 * cur_gappoh;
	target_geom.height = max_client_height * c->scroller_proportion;
	target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
	vertical_scroll_adjust_fullandmax(tempClients[focus_client_index],
									  &target_geom);

	if (tempClients[focus_client_index]->isfullscreen) {
		target_geom.y = m->m.y;
		vertical_check_scroller_root_inside_mon(tempClients[focus_client_index],
												&target_geom);
		arrange_stack_vertical(tempClients[focus_client_index], target_geom,
							   cur_gappih);
	} else if (tempClients[focus_client_index]->ismaximizescreen) {
		target_geom.y = m->w.y + cur_gappov;
		vertical_check_scroller_root_inside_mon(tempClients[focus_client_index],
												&target_geom);
		arrange_stack_vertical(tempClients[focus_client_index], target_geom,
							   cur_gappih);
	} else if (need_scroller) {
		if (need_apply_center) {
			target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		} else if (need_apply_overspread) {
			if (over_overspread_to_up) {
				target_geom.y = m->w.y + scroller_structs;
			} else {
				target_geom.y =
					m->w.y +
					(m->w.height -
					 tempClients[focus_client_index]->scroller_proportion *
						 max_client_height -
					 scroller_structs);
			}
		} else {
			target_geom.y = root_client->geom.y > m->w.y + (m->w.height) / 2
								? m->w.y + (m->w.height -
											tempClients[focus_client_index]
													->scroller_proportion *
												max_client_height -
											scroller_structs)
								: m->w.y + scroller_structs;
		}
		vertical_check_scroller_root_inside_mon(tempClients[focus_client_index],
												&target_geom);
		arrange_stack_vertical(tempClients[focus_client_index], target_geom,
							   cur_gappih);
	} else {
		target_geom.y = c->geom.y;
		vertical_check_scroller_root_inside_mon(tempClients[focus_client_index],
												&target_geom);
		arrange_stack_vertical(tempClients[focus_client_index], target_geom,
							   cur_gappih);
	}

	for (i = 1; i <= focus_client_index; i++) {
		c = tempClients[focus_client_index - i];
		target_geom.height = max_client_height * c->scroller_proportion;
		vertical_scroll_adjust_fullandmax(c, &target_geom);
		target_geom.y = tempClients[focus_client_index - i + 1]->geom.y -
						cur_gappiv - target_geom.height;

		arrange_stack_vertical(c, target_geom, cur_gappih);
	}

	for (i = 1; i < n - focus_client_index; i++) {
		c = tempClients[focus_client_index + i];
		target_geom.height = max_client_height * c->scroller_proportion;
		vertical_scroll_adjust_fullandmax(c, &target_geom);
		target_geom.y = tempClients[focus_client_index + i - 1]->geom.y +
						cur_gappiv +
						tempClients[focus_client_index + i - 1]->geom.height;
		arrange_stack_vertical(c, target_geom, cur_gappih);
	}

	free(tempClients);
}

void vertical_grid(Monitor *m) {
	int32_t i, n;
	int32_t cx, cy, cw, ch;
	int32_t dy;
	int32_t rows, cols, overrows;
	Client *c = NULL;
	int32_t target_gappo =
		enablegaps ? m->isoverview ? overviewgappo : gappov : 0;
	int32_t target_gappi =
		enablegaps ? m->isoverview ? overviewgappi : gappiv : 0;
	float single_width_ratio = m->isoverview ? 0.7 : 0.9;
	float single_height_ratio = m->isoverview ? 0.8 : 0.9;

	n = m->isoverview ? m->visible_clients : m->visible_tiling_clients;

	if (n == 0) {
		return;
	}

	if (n == 1) {
		wl_list_for_each(c, &clients, link) {

			if (c->mon != m)
				continue;

			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
				ch = (m->w.height - 2 * target_gappo) * single_height_ratio;
				cw = (m->w.width - 2 * target_gappo) * single_width_ratio;
				c->geom.x = m->w.x + (m->w.width - cw) / 2;
				c->geom.y = m->w.y + (m->w.height - ch) / 2;
				c->geom.width = cw;
				c->geom.height = ch;
				resize(c, c->geom, 0);
				return;
			}
		}
	}

	if (n == 2) {
		ch = (m->w.height - 2 * target_gappo - target_gappi) / 2;
		cw = (m->w.width - 2 * target_gappo) * 0.65;
		i = 0;
		wl_list_for_each(c, &clients, link) {

			if (c->mon != m)
				continue;

			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
				if (i == 0) {
					c->geom.x = m->w.x + (m->w.width - cw) / 2 + target_gappo;
					c->geom.y = m->w.y + target_gappo;
					c->geom.width = cw;
					c->geom.height = ch;
					resize(c, c->geom, 0);
				} else if (i == 1) {
					c->geom.x = m->w.x + (m->w.width - cw) / 2 + target_gappo;
					c->geom.y = m->w.y + ch + target_gappo + target_gappi;
					c->geom.width = cw;
					c->geom.height = ch;
					resize(c, c->geom, 0);
				}
				i++;
			}
		}
		return;
	}

	for (rows = 0; rows <= n / 2; rows++) {
		if (rows * rows >= n) {
			break;
		}
	}
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;

	cw = (m->w.width - 2 * target_gappo - (cols - 1) * target_gappi) / cols;
	ch = (m->w.height - 2 * target_gappo - (rows - 1) * target_gappi) / rows;

	overrows = n % rows;
	if (overrows) {
		dy = (m->w.height - overrows * ch - (overrows - 1) * target_gappi) / 2 -
			 target_gappo;
	}

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;

		if (VISIBLEON(c, m) && !c->isunglobal &&
			((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
			cx = m->w.x + (i / rows) * (cw + target_gappi);
			cy = m->w.y + (i % rows) * (ch + target_gappi);
			if (overrows && i >= n - overrows) {
				cy += dy;
			}
			c->geom.x = cx + target_gappo;
			c->geom.y = cy + target_gappo;
			c->geom.width = cw;
			c->geom.height = ch;
			resize(c, c->geom, 0);
			i++;
		}
	}
}