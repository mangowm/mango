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

	cur_gapih =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapih;
	cur_gapiv =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapiv;
	cur_gapoh =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapoh;
	cur_gapov =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gapov;

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

	cur_gappiv =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappiv;
	cur_gappoh =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappoh;
	cur_gappov =
		config.smartgaps && m->visible_tiling_clients == 1 ? 0 : cur_gappov;

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

void vertical_grid(Monitor *m) {
	int32_t i, n;
	int32_t cx, cy, cw, ch;
	int32_t dy;
	int32_t rows, cols, overrows;
	Client *c = NULL;
	int32_t target_gappo =
		enablegaps ? m->isoverview ? config.overviewgappo : config.gappov : 0;
	int32_t target_gappi =
		enablegaps ? m->isoverview ? config.overviewgappi : config.gappiv : 0;
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

void vertical_fair(Monitor *m) {
	int32_t i, n = 0;
	Client *c = NULL;

	n = m->visible_tiling_clients;
	if (n == 0)
		return;

	// 间隙参数处理
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	// 智能间隙
	cur_gappiv = config.smartgaps && n == 1 ? 0 : cur_gappiv;
	cur_gappih = config.smartgaps && n == 1 ? 0 : cur_gappih;
	cur_gappov = config.smartgaps && n == 1 ? 0 : cur_gappov;
	cur_gappoh = config.smartgaps && n == 1 ? 0 : cur_gappoh;

	// 计算最佳行数 rows = ceil(sqrt(n))
	int32_t rows;
	for (rows = 0; rows <= n; rows++) {
		if (rows * rows >= n)
			break;
	}

	int32_t base_cols = n / rows; // 每行的基础列数
	int32_t remainder = n % rows; // 多出来的窗口

	// 计算上半部分（大窗口）的行数和总窗口数
	int32_t first_group_rows = rows - remainder;
	int32_t first_group_count = first_group_rows * base_cols;

	// 计算标准行高
	int32_t row_height =
		(m->w.height - 2 * cur_gappov - (rows - 1) * cur_gappiv) / rows;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || !ISTILED(c))
			continue;

		int32_t row_idx, col_idx, cols_in_this_row;

		// 判断当前窗口属于哪一行、哪一列
		// 上半部分行拥有较少的列数（窗口更宽），下半部分行承担余数（窗口更窄）
		if (i < first_group_count) {
			row_idx = i / base_cols;
			col_idx = i % base_cols;
			cols_in_this_row = base_cols;
		} else {
			int32_t offset = i - first_group_count;
			row_idx = first_group_rows + (offset / (base_cols + 1));
			col_idx = offset % (base_cols + 1);
			cols_in_this_row = base_cols + 1;
		}

		// 计算 Y 坐标和高度 (最后一行吃掉剩余像素)
		int32_t cy = m->w.y + cur_gappov + row_idx * (row_height + cur_gappiv);
		int32_t ch = (row_idx == rows - 1)
						 ? (m->w.height - 2 * cur_gappov -
							row_idx * (row_height + cur_gappiv))
						 : row_height;

		// 计算 X 坐标和宽度 (最后一列吃掉剩余像素，防止缝隙)
		int32_t base_cw = (m->w.width - 2 * cur_gappoh -
						   (cols_in_this_row - 1) * cur_gappih) /
						  cols_in_this_row;
		int32_t cx = m->w.x + cur_gappoh + col_idx * (base_cw + cur_gappih);
		int32_t cw = (col_idx == cols_in_this_row - 1)
						 ? (m->w.width - 2 * cur_gappoh -
							col_idx * (base_cw + cur_gappih))
						 : base_cw;

		resize(c, (struct wlr_box){.x = cx, .y = cy, .width = cw, .height = ch},
			   0);
		i++;
	}
}