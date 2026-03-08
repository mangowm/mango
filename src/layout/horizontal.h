// 网格布局窗口大小和位置计算
void grid(Monitor *m) {
	int32_t i, n;
	int32_t cx, cy, cw, ch;
	int32_t dx;
	int32_t cols, rows, overcols;
	Client *c = NULL;
	n = 0;
	int32_t target_gappo =
		enablegaps ? m->isoverview ? overviewgappo : gappoh : 0;
	int32_t target_gappi =
		enablegaps ? m->isoverview ? overviewgappi : gappih : 0;
	float single_width_ratio = m->isoverview ? 0.7 : 0.9;
	float single_height_ratio = m->isoverview ? 0.8 : 0.9;

	n = m->isoverview ? m->visible_clients : m->visible_tiling_clients;

	if (n == 0) {
		return; // 没有需要处理的客户端，直接返回
	}

	if (n == 1) {
		wl_list_for_each(c, &clients, link) {

			if (c->mon != m)
				continue;

			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
				cw = (m->w.width - 2 * target_gappo) * single_width_ratio;
				ch = (m->w.height - 2 * target_gappo) * single_height_ratio;
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
		cw = (m->w.width - 2 * target_gappo - target_gappi) / 2;
		ch = (m->w.height - 2 * target_gappo) * 0.65;
		i = 0;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;

			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
				if (i == 0) {
					c->geom.x = m->w.x + target_gappo;
					c->geom.y = m->w.y + (m->w.height - ch) / 2 + target_gappo;
					c->geom.width = cw;
					c->geom.height = ch;
					resize(c, c->geom, 0);
				} else if (i == 1) {
					c->geom.x = m->w.x + cw + target_gappo + target_gappi;
					c->geom.y = m->w.y + (m->w.height - ch) / 2 + target_gappo;
					c->geom.width = cw;
					c->geom.height = ch;
					resize(c, c->geom, 0);
				}
				i++;
			}
		}
		return;
	}

	// 计算列数和行数
	for (cols = 0; cols <= n / 2; cols++) {
		if (cols * cols >= n) {
			break;
		}
	}
	rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;

	// 计算每个客户端的高度和宽度
	ch = (m->w.height - 2 * target_gappo - (rows - 1) * target_gappi) / rows;
	cw = (m->w.width - 2 * target_gappo - (cols - 1) * target_gappi) / cols;

	// 处理多余的列
	overcols = n % cols;
	if (overcols) {
		dx = (m->w.width - overcols * cw - (overcols - 1) * target_gappi) / 2 -
			 target_gappo;
	}

	// 调整每个客户端的位置和大小
	i = 0;
	wl_list_for_each(c, &clients, link) {

		if (c->mon != m)
			continue;

		if (VISIBLEON(c, m) && !c->isunglobal &&
			((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
			cx = m->w.x + (i % cols) * (cw + target_gappi);
			cy = m->w.y + (i / cols) * (ch + target_gappi);
			if (overcols && i >= n - overcols) {
				cx += dx;
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

void deck(Monitor *m) {
	int32_t mw, my;
	int32_t i, n = 0;
	Client *c = NULL;
	Client *fc = NULL;
	float mfact;
	uint32_t nmasters = m->pertag->nmasters[m->pertag->curtag];

	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;

	cur_gappih = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappih;
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

	// Calculate master width including outer gaps
	if (n > nmasters)
		mw = nmasters ? round((m->w.width - 2 * cur_gappoh) * mfact) : 0;
	else
		mw = m->w.width - 2 * cur_gappoh;

	i = my = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		if (i < nmasters) {
			c->master_mfact_per = mfact;
			// Master area clients
			resize(
				c,
				(struct wlr_box){.x = m->w.x + cur_gappoh,
								 .y = m->w.y + cur_gappov + my,
								 .width = mw,
								 .height = (m->w.height - 2 * cur_gappov - my) /
										   (MIN(n, nmasters) - i)},
				0);
			my += c->geom.height;
		} else {
			// Stack area clients
			c->master_mfact_per = mfact;
			resize(c,
				   (struct wlr_box){.x = m->w.x + mw + cur_gappoh + cur_gappih,
									.y = m->w.y + cur_gappov,
									.width = m->w.width - mw - 2 * cur_gappoh -
											 cur_gappih,
									.height = m->w.height - 2 * cur_gappov},
				   0);
			if (c == focustop(m))
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
		i++;
	}
}

void horizontal_scroll_adjust_fullandmax(Client *c,
										 struct wlr_box *target_geom) {
	Monitor *m = c->mon;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;

	cur_gappih =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappoh =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappoh;
	cur_gappov =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappov;

	if (c->isfullscreen) {
		target_geom->height = m->m.height;
		target_geom->width = m->m.width;
		target_geom->y = m->m.y;
		return;
	}

	if (c->ismaximizescreen) {
		target_geom->height = m->w.height - 2 * cur_gappov;
		target_geom->width = m->w.width - 2 * cur_gappoh;
		target_geom->y = m->w.y + cur_gappov;
		return;
	}

	target_geom->height = m->w.height - 2 * cur_gappov;
	target_geom->y = m->w.y + (m->w.height - target_geom->height) / 2;
}

void arrange_stack(Client *scroller_stack_head, struct wlr_box geometry,
				   int32_t gappiv) {
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

	int32_t client_height;
	int32_t current_y = geometry.y;
	int32_t remain_client_height = geometry.height - (stack_size - 1) * gappiv;
	float remain_proportion = 1.0f;

	iter = scroller_stack_head;
	while (iter) {

		client_height =
			remain_client_height * (iter->stack_proportion / remain_proportion);

		struct wlr_box client_geom = {.x = geometry.x,
									  .y = current_y,
									  .width = geometry.width,
									  .height = client_height};
		resize(iter, client_geom, 0);
		remain_proportion -= iter->stack_proportion;
		remain_client_height -= client_height;
		current_y += client_height + gappiv;
		iter = iter->next_in_stack;
	}
}

void horizontal_check_scroller_root_inside_mon(Client *c,
											   struct wlr_box *geometry) {
	if (!GEOMINSIDEMON(geometry, c->mon)) {
		geometry->x = c->mon->w.x + (c->mon->w.width - geometry->width) / 2;
	}
}

// 滚动布局
void scroller(Monitor *m) {
	int32_t i, n, j;
	float single_proportion = 1.0;

	Client *c = NULL, *root_client = NULL;
	Client **tempClients = NULL; // 初始化为 NULL
	struct wlr_box target_geom;
	int32_t focus_client_index = 0;
	bool need_scroller = false;
	bool over_overspread_to_left = false;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;

	cur_gappih =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappoh =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappoh;
	cur_gappov =
		smartgaps && m->visible_scroll_tiling_clients == 1 ? 0 : cur_gappov;

	int32_t max_client_width = m->w.width - 2 * scroller_structs - cur_gappih;

	n = m->visible_scroll_tiling_clients;

	if (n == 0) {
		return; // 没有需要处理的客户端，直接返回
	}

	// 动态分配内存
	tempClients = malloc(n * sizeof(Client *));
	if (!tempClients) {
		// 处理内存分配失败的情况
		return;
	}

	// 第二次遍历，填充 tempClients
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

		target_geom.height = m->w.height - 2 * cur_gappov;
		target_geom.width = (m->w.width - 2 * cur_gappoh) * single_proportion;
		target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		horizontal_check_scroller_root_inside_mon(c, &target_geom);
		arrange_stack(c, target_geom, cur_gappiv);
		free(tempClients); // 释放内存
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
		free(tempClients); // 释放内存
		return;
	}

	for (i = 0; i < n; i++) {
		c = tempClients[i];
		if (root_client == c) {
			if (c->geom.x >= m->w.x + scroller_structs &&
				c->geom.x + c->geom.width <=
					m->w.x + m->w.width - scroller_structs) {
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
			over_overspread_to_left = true;
		} else {
			over_overspread_to_left = false;
		}

		if (over_overspread_to_left &&
			(!INSIDEMON(tempClients[1]) ||
			 (tempClients[1]->scroller_proportion +
				  tempClients[focus_client_index]->scroller_proportion >=
			  1.0f))) {
			need_scroller = true;
		} else if (!over_overspread_to_left &&
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
		   (m->prevsel->scroller_proportion * max_client_width) +
				   (tempClients[focus_client_index]->scroller_proportion *
					max_client_width) >
			   m->w.width - 2 * scroller_structs - cur_gappih)));

	if (n == 1 && scroller_ignore_proportion_single) {
		need_scroller = true;
	}

	if (start_drag_window)
		need_scroller = false;

	target_geom.height = m->w.height - 2 * cur_gappov;
	target_geom.width = max_client_width * c->scroller_proportion;
	target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
	horizontal_scroll_adjust_fullandmax(tempClients[focus_client_index],
										&target_geom);
	if (tempClients[focus_client_index]->isfullscreen) {
		target_geom.x = m->m.x;
		horizontal_check_scroller_root_inside_mon(
			tempClients[focus_client_index], &target_geom);
		arrange_stack(tempClients[focus_client_index], target_geom, cur_gappiv);
	} else if (tempClients[focus_client_index]->ismaximizescreen) {
		target_geom.x = m->w.x + cur_gappoh;
		horizontal_check_scroller_root_inside_mon(
			tempClients[focus_client_index], &target_geom);
		arrange_stack(tempClients[focus_client_index], target_geom, cur_gappiv);
	} else if (need_scroller) {
		if (need_apply_center) {
			target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		} else if (need_apply_overspread) {
			if (over_overspread_to_left) {
				target_geom.x = m->w.x + scroller_structs;
			} else {
				target_geom.x =
					m->w.x +
					(m->w.width -
					 tempClients[focus_client_index]->scroller_proportion *
						 max_client_width -
					 scroller_structs);
			}

		} else {
			target_geom.x = tempClients[focus_client_index]->geom.x >
									m->w.x + (m->w.width) / 2
								? m->w.x + (m->w.width -
											tempClients[focus_client_index]
													->scroller_proportion *
												max_client_width -
											scroller_structs)
								: m->w.x + scroller_structs;
		}
		horizontal_check_scroller_root_inside_mon(
			tempClients[focus_client_index], &target_geom);
		arrange_stack(tempClients[focus_client_index], target_geom, cur_gappiv);
	} else {
		target_geom.x = c->geom.x;
		horizontal_check_scroller_root_inside_mon(
			tempClients[focus_client_index], &target_geom);
		arrange_stack(tempClients[focus_client_index], target_geom, cur_gappiv);
	}

	for (i = 1; i <= focus_client_index; i++) {
		c = tempClients[focus_client_index - i];
		target_geom.width = max_client_width * c->scroller_proportion;
		horizontal_scroll_adjust_fullandmax(c, &target_geom);
		target_geom.x = tempClients[focus_client_index - i + 1]->geom.x -
						cur_gappih - target_geom.width;

		arrange_stack(c, target_geom, cur_gappiv);
	}

	for (i = 1; i < n - focus_client_index; i++) {
		c = tempClients[focus_client_index + i];
		target_geom.width = max_client_width * c->scroller_proportion;
		horizontal_scroll_adjust_fullandmax(c, &target_geom);
		target_geom.x = tempClients[focus_client_index + i - 1]->geom.x +
						cur_gappih +
						tempClients[focus_client_index + i - 1]->geom.width;
		arrange_stack(c, target_geom, cur_gappiv);
	}

	free(tempClients); // 最后释放内存
}

void center_tile(Monitor *m) {
	int32_t i, n = 0, h, r, ie = enablegaps, mw, mx, my, oty, ety, tw;
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

	// 获取第一个可见的平铺客户端用于主区域宽度百分比
	wl_list_for_each(fc, &clients, link) {
		if (VISIBLEON(fc, m) && ISTILED(fc))
			break;
	}

	// 间隙参数处理
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0; // 内部垂直间隙
	int32_t cur_gappih = enablegaps ? m->gappih : 0; // 内部水平间隙
	int32_t cur_gappov = enablegaps ? m->gappov : 0; // 外部垂直间隙
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0; // 外部水平间隙

	// 智能间隙处理
	cur_gappiv = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappih = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;

	int32_t nmasters = m->pertag->nmasters[m->pertag->curtag];
	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[m->pertag->curtag];

	// 初始化区域
	mw = m->w.width;
	mx = cur_gappoh;
	my = cur_gappov;
	tw = mw;

	// 判断是否需要主区域铺满
	int32_t should_overspread = center_master_overspread && (n <= nmasters);

	int32_t master_surplus_height =
		(m->w.height - 2 * cur_gappov - cur_gappiv * ie * (master_num - 1));
	float master_surplus_ratio = 1.0;

	int32_t slave_left_surplus_height =
		(m->w.height - 2 * cur_gappov - cur_gappiv * ie * (stack_num / 2 - 1));
	float slave_left_surplus_ratio = 1.0;

	int32_t slave_right_surplus_height =
		(m->w.height - 2 * cur_gappov -
		 cur_gappiv * ie * ((stack_num + 1) / 2 - 1));
	float slave_right_surplus_ratio = 1.0;

	if (n > nmasters || !should_overspread) {
		// 计算主区域宽度（居中模式）
		mw = nmasters ? (m->w.width - 2 * cur_gappoh - cur_gappih * ie) * mfact
					  : 0;

		if (n - nmasters > 1) {
			// 多个堆叠窗口：主区域居中，左右两侧各有一个堆叠区域
			tw = (m->w.width - mw) / 2 - cur_gappoh - cur_gappih * ie;
			mx = cur_gappoh + tw + cur_gappih * ie;
		} else if (n - nmasters == 1) {
			// 单个堆叠窗口的处理
			if (center_when_single_stack) {
				// stack在右边，master居中，左边空着
				tw = (m->w.width - mw) / 2 - cur_gappoh - cur_gappih * ie;
				mx = cur_gappoh + tw + cur_gappih * ie; // master居中
			} else {
				// stack在右边，master在左边
				tw = m->w.width - mw - 2 * cur_gappoh - cur_gappih * ie;
				mx = cur_gappoh; // master在左边
			}
		} else {
			// 只有主区域窗口：居中显示
			tw = (m->w.width - mw) / 2 - cur_gappoh - cur_gappih * ie;
			mx = cur_gappoh + tw + cur_gappih * ie;
		}
	} else {
		// 主区域铺满模式（只有主区域窗口时）
		mw = m->w.width - 2 * cur_gappoh;
		mx = cur_gappoh;
		tw = 0; // 堆叠区域宽度为0
	}

	oty = cur_gappov;
	ety = cur_gappov;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;

		if (i < nmasters) {
			// 主区域窗口
			r = MIN(n, nmasters) - i;
			if (c->master_inner_per > 0.0f) {
				h = master_surplus_height * c->master_inner_per /
					master_surplus_ratio;
				master_surplus_height = master_surplus_height - h;
				master_surplus_ratio =
					master_surplus_ratio - c->master_inner_per;
				c->master_mfact_per = mfact;
			} else {
				h = (m->w.height - my - cur_gappov -
					 cur_gappiv * ie * (r - 1)) /
					r;
				c->master_inner_per = h / (m->w.height - my - cur_gappov -
										   cur_gappiv * ie * (r - 1));
				c->master_mfact_per = mfact;
			}

			resize(c,
				   (struct wlr_box){.x = m->w.x + mx,
									.y = m->w.y + my,
									.width = mw,
									.height = h},
				   0);
			my += c->geom.height + cur_gappiv * ie;
		} else {
			// 堆叠区域窗口
			int32_t stack_index = i - nmasters;

			if (n - nmasters == 1) {
				// 单个堆叠窗口
				r = n - i;
				if (c->stack_inner_per > 0.0f) {
					h = (m->w.height - 2 * cur_gappov -
						 cur_gappiv * ie * (stack_num - 1)) *
						c->stack_inner_per;
					c->master_mfact_per = mfact;
				} else {
					h = (m->w.height - ety - cur_gappov -
						 cur_gappiv * ie * (r - 1)) /
						r;
					c->stack_inner_per = h / (m->w.height - ety - cur_gappov -
											  cur_gappiv * ie * (r - 1));
					c->master_mfact_per = mfact;
				}

				int32_t stack_x;
				if (center_when_single_stack) {
					// 放在右侧（master居中时，stack在右边）
					stack_x = m->w.x + mx + mw + cur_gappih * ie;
				} else {
					// 放在右侧（master在左边时，stack在右边）
					stack_x = m->w.x + mx + mw + cur_gappih * ie;
				}

				resize(c,
					   (struct wlr_box){.x = stack_x,
										.y = m->w.y + ety,
										.width = tw,
										.height = h},
					   0);
				ety += c->geom.height + cur_gappiv * ie;
			} else {
				// 多个堆叠窗口：交替放在左右两侧
				r = (n - i + 1) / 2;

				if ((stack_index % 2) ^ (n % 2 == 0)) {
					// 右侧堆叠窗口
					if (c->stack_inner_per > 0.0f) {
						h = slave_right_surplus_height * c->stack_inner_per /
							slave_right_surplus_ratio;
						slave_right_surplus_height =
							slave_right_surplus_height - h;
						slave_right_surplus_ratio =
							slave_right_surplus_ratio - c->stack_inner_per;
						c->master_mfact_per = mfact;
					} else {
						h = (m->w.height - ety - cur_gappov -
							 cur_gappiv * ie * (r - 1)) /
							r;
						c->stack_inner_per =
							h / (m->w.height - ety - cur_gappov -
								 cur_gappiv * ie * (r - 1));
						c->master_mfact_per = mfact;
					}

					int32_t stack_x = m->w.x + mx + mw + cur_gappih * ie;

					resize(c,
						   (struct wlr_box){.x = stack_x,
											.y = m->w.y + ety,
											.width = tw,
											.height = h},
						   0);
					ety += c->geom.height + cur_gappiv * ie;
				} else {
					// 左侧堆叠窗口
					if (c->stack_inner_per > 0.0f) {
						h = slave_left_surplus_height * c->stack_inner_per /
							slave_left_surplus_ratio;
						slave_left_surplus_height =
							slave_left_surplus_height - h;
						slave_left_surplus_ratio =
							slave_left_surplus_ratio - c->stack_inner_per;
						c->master_mfact_per = mfact;
					} else {
						h = (m->w.height - oty - cur_gappov -
							 cur_gappiv * ie * (r - 1)) /
							r;
						c->stack_inner_per =
							h / (m->w.height - oty - cur_gappov -
								 cur_gappiv * ie * (r - 1));
						c->master_mfact_per = mfact;
					}

					int32_t stack_x = m->w.x + cur_gappoh;
					resize(c,
						   (struct wlr_box){.x = stack_x,
											.y = m->w.y + oty,
											.width = tw,
											.height = h},
						   0);
					oty += c->geom.height + cur_gappiv * ie;
				}
			}
		}
		i++;
	}
}

void tile(Monitor *m) {
	int32_t i, n = 0, h, r, ie = enablegaps, mw, my, ty;
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

	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	cur_gappiv = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappih = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;

	wl_list_for_each(fc, &clients, link) {

		if (VISIBLEON(fc, m) && ISTILED(fc))
			break;
	}

	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[m->pertag->curtag];

	if (n > m->pertag->nmasters[m->pertag->curtag])
		mw = m->pertag->nmasters[m->pertag->curtag]
				 ? (m->w.width + cur_gappih * ie) * mfact
				 : 0;
	else
		mw = m->w.width - 2 * cur_gappoh + cur_gappih * ie;
	i = 0;
	my = ty = cur_gappov;

	int32_t master_surplus_height =
		(m->w.height - 2 * cur_gappov - cur_gappiv * ie * (master_num - 1));
	float master_surplus_ratio = 1.0;

	int32_t slave_surplus_height =
		(m->w.height - 2 * cur_gappov - cur_gappiv * ie * (stack_num - 1));
	float slave_surplus_ratio = 1.0;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		if (i < m->pertag->nmasters[m->pertag->curtag]) {
			r = MIN(n, m->pertag->nmasters[m->pertag->curtag]) - i;
			if (c->master_inner_per > 0.0f) {
				h = master_surplus_height * c->master_inner_per /
					master_surplus_ratio;
				master_surplus_height = master_surplus_height - h;
				master_surplus_ratio =
					master_surplus_ratio - c->master_inner_per;
				c->master_mfact_per = mfact;
			} else {
				h = (m->w.height - my - cur_gappov -
					 cur_gappiv * ie * (r - 1)) /
					r;
				c->master_inner_per = h / (m->w.height - my - cur_gappov -
										   cur_gappiv * ie * (r - 1));
				c->master_mfact_per = mfact;
			}
			resize(c,
				   (struct wlr_box){.x = m->w.x + cur_gappoh,
									.y = m->w.y + my,
									.width = mw - cur_gappih * ie,
									.height = h},
				   0);
			my += c->geom.height + cur_gappiv * ie;
		} else {
			r = n - i;
			if (c->stack_inner_per > 0.0f) {
				h = slave_surplus_height * c->stack_inner_per /
					slave_surplus_ratio;
				slave_surplus_height = slave_surplus_height - h;
				slave_surplus_ratio = slave_surplus_ratio - c->stack_inner_per;
				c->master_mfact_per = mfact;
			} else {
				h = (m->w.height - ty - cur_gappov -
					 cur_gappiv * ie * (r - 1)) /
					r;
				c->stack_inner_per = h / (m->w.height - ty - cur_gappov -
										  cur_gappiv * ie * (r - 1));
				c->master_mfact_per = mfact;
			}

			// wlr_log(WLR_ERROR, "stack_inner_per: %f", c->stack_inner_per);

			resize(c,
				   (struct wlr_box){.x = m->w.x + mw + cur_gappoh,
									.y = m->w.y + ty,
									.width = m->w.width - mw - 2 * cur_gappoh,
									.height = h},
				   0);
			ty += c->geom.height + cur_gappiv * ie;
		}
		i++;
	}
}

void right_tile(Monitor *m) {
	int32_t i, n = 0, h, r, ie = enablegaps, mw, my, ty;
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

	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	cur_gappiv = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappih = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappih;
	cur_gappov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;
	cur_gappoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;

	wl_list_for_each(fc, &clients, link) {

		if (VISIBLEON(fc, m) && ISTILED(fc))
			break;
	}

	mfact = fc->master_mfact_per > 0.0f ? fc->master_mfact_per
										: m->pertag->mfacts[m->pertag->curtag];

	if (n > m->pertag->nmasters[m->pertag->curtag])
		mw = m->pertag->nmasters[m->pertag->curtag]
				 ? (m->w.width + cur_gappih * ie) * mfact
				 : 0;
	else
		mw = m->w.width - 2 * cur_gappoh + cur_gappih * ie;
	i = 0;
	my = ty = cur_gappov;

	int32_t master_surplus_height =
		(m->w.height - 2 * cur_gappov - cur_gappiv * ie * (master_num - 1));
	float master_surplus_ratio = 1.0;

	int32_t slave_surplus_height =
		(m->w.height - 2 * cur_gappov - cur_gappiv * ie * (stack_num - 1));
	float slave_surplus_ratio = 1.0;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		if (i < m->pertag->nmasters[m->pertag->curtag]) {
			r = MIN(n, m->pertag->nmasters[m->pertag->curtag]) - i;
			if (c->master_inner_per > 0.0f) {
				h = master_surplus_height * c->master_inner_per /
					master_surplus_ratio;
				master_surplus_height = master_surplus_height - h;
				master_surplus_ratio =
					master_surplus_ratio - c->master_inner_per;
				c->master_mfact_per = mfact;
			} else {
				h = (m->w.height - my - cur_gappov -
					 cur_gappiv * ie * (r - 1)) /
					r;
				c->master_inner_per = h / (m->w.height - my - cur_gappov -
										   cur_gappiv * ie * (r - 1));
				c->master_mfact_per = mfact;
			}
			resize(c,
				   (struct wlr_box){.x = m->w.x + m->w.width - mw - cur_gappoh +
										 cur_gappih * ie,
									.y = m->w.y + my,
									.width = mw - cur_gappih * ie,
									.height = h},
				   0);
			my += c->geom.height + cur_gappiv * ie;
		} else {
			r = n - i;
			if (c->stack_inner_per > 0.0f) {
				h = slave_surplus_height * c->stack_inner_per /
					slave_surplus_ratio;
				slave_surplus_height = slave_surplus_height - h;
				slave_surplus_ratio = slave_surplus_ratio - c->stack_inner_per;
				c->master_mfact_per = mfact;
			} else {
				h = (m->w.height - ty - cur_gappov -
					 cur_gappiv * ie * (r - 1)) /
					r;
				c->stack_inner_per = h / (m->w.height - ty - cur_gappov -
										  cur_gappiv * ie * (r - 1));
				c->master_mfact_per = mfact;
			}

			// wlr_log(WLR_ERROR, "stack_inner_per: %f", c->stack_inner_per);

			resize(c,
				   (struct wlr_box){.x = m->w.x + cur_gappoh,
									.y = m->w.y + ty,
									.width = m->w.width - mw - 2 * cur_gappoh,
									.height = h},
				   0);
			ty += c->geom.height + cur_gappiv * ie;
		}
		i++;
	}
}

void // 17
monocle(Monitor *m) {
	Client *c = NULL;
	struct wlr_box geom;

	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	cur_gappoh = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;
	cur_gappov = smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;
		geom.x = m->w.x + cur_gappoh;
		geom.y = m->w.y + cur_gappov;
		geom.width = m->w.width - 2 * cur_gappoh;
		geom.height = m->w.height - 2 * cur_gappov;
		resize(c, geom, 0);
	}
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

void tgmix(Monitor *m) {
	int32_t n = m->visible_tiling_clients;
	if (n <= 3) {
		tile(m);
		return;
	} else {
		grid(m);
		return;
	}
}
