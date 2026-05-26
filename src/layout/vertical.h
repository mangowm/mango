void vertical_tile(Monitor *m) {
	int32_t i, n = 0, w, r, ie = enablegaps, mh, mx, tx;
	Client *c = NULL;
	Client *fc = NULL;
	double mfact = 0;
	int32_t master_num = 0;
	int32_t stack_num = 0;

	n = m->visible_fake_tiling_clients;
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
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapih;
	cur_gapiv =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapiv;
	cur_gapoh =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapoh;
	cur_gapov =
		config.smartgaps && m->visible_fake_tiling_clients == 1 ? 0 : cur_gapov;

	wl_list_for_each(fc, &clients, link) {
		if (VISIBLEON(fc, m) && ISFAKETILED(fc))
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
		if (!VISIBLEON(c, m) || !ISFAKETILED(c))
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
			client_tile_resize(c,
							   (struct wlr_box){.x = m->w.x + mx,
												.y = m->w.y + cur_gapov,
												.width = w,
												.height = mh - cur_gapiv * ie},
							   0);
			mx += w + cur_gapih * ie; // 使用理论宽度累加
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

			client_tile_resize(
				c,
				(struct wlr_box){.x = m->w.x + tx,
								 .y = m->w.y + mh + cur_gapov,
								 .width = w,
								 .height = m->w.height - mh - 2 * cur_gapov},
				0);
			tx += w + cur_gapih * ie; // 使用理论宽度累加
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

	cur_gappiv = config.smartgaps && m->visible_fake_tiling_clients == 1
					 ? 0
					 : cur_gappiv;
	cur_gappoh = config.smartgaps && m->visible_fake_tiling_clients == 1
					 ? 0
					 : cur_gappoh;
	cur_gappov = config.smartgaps && m->visible_fake_tiling_clients == 1
					 ? 0
					 : cur_gappov;

	n = m->visible_fake_tiling_clients;

	if (n == 0)
		return;

	wl_list_for_each(fc, &clients, link) {

		if (VISIBLEON(fc, m) && ISFAKETILED(fc))
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
		if (!VISIBLEON(c, m) || !ISFAKETILED(c))
			continue;
		if (i < nmasters) {
			client_tile_resize(
				c,
				(struct wlr_box){.x = m->w.x + cur_gappoh + mx,
								 .y = m->w.y + cur_gappov,
								 .width = (m->w.width - 2 * cur_gappoh - mx) /
										  (MIN(n, nmasters) - i),
								 .height = mh},
				0);
			mx += c->geom.width;
		} else {
			client_tile_resize(
				c,
				(struct wlr_box){.x = m->w.x + cur_gappoh,
								 .y = m->w.y + mh + cur_gappov + cur_gappiv,
								 .width = m->w.width - 2 * cur_gappoh,
								 .height = m->w.height - mh - 2 * cur_gappov -
										   cur_gappiv},
				0);
			if (c == focustop(m))
				wlr_scene_node_raise_to_top(&c->scene->node);
		}
		i++;
	}
}

void vertical_grid(Monitor *m) {
	int32_t i, n;
	int32_t cw, ch;
	int32_t rows, cols, overrows;
	Client *c = NULL;
	int32_t target_gappo =
		enablegaps ? m->isoverview ? config.overviewgappo : config.gappov : 0;
	int32_t target_gappi =
		enablegaps ? m->isoverview ? config.overviewgappi : config.gappiv : 0;
	float single_width_ratio = m->isoverview ? 0.7 : 0.9;
	float single_height_ratio = m->isoverview ? 0.8 : 0.9;

	n = m->isoverview ? m->visible_clients : m->visible_fake_tiling_clients;
	if (n == 0)
		return;

	if (n == 1) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) ||
				 ISFAKETILED(c))) {
				ch = (m->w.height - 2 * target_gappo) * single_height_ratio;
				cw = (m->w.width - 2 * target_gappo) * single_width_ratio;
				c->geom.x = m->w.x + (m->w.width - cw) / 2;
				c->geom.y = m->w.y + (m->w.height - ch) / 2;
				c->geom.width = cw;
				c->geom.height = ch;
				client_tile_resize(c, c->geom, 0);
				return;
			}
		}
	}

	if (n == 2) {
		float row_pers[2] = {1.0f, 1.0f};
		// 先提取这两个窗口现有的行比例
		i = 0;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) ||
				 ISFAKETILED(c))) {
				if (i < 2)
					row_pers[i] =
						(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
				i++;
			}
		}

		float sum_row = row_pers[0] + row_pers[1];
		float avail_h = m->w.height - 2 * target_gappo - target_gappi;
		cw = (m->w.width - 2 * target_gappo) * 0.65; // 依然保持 0.65 的美观宽度

		i = 0;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) ||
				 ISFAKETILED(c))) {
				c->grid_col_idx = 0;
				c->grid_row_idx = i;
				c->grid_col_per = 1.0f;
				c->grid_row_per = row_pers[i];

				// 根据分配的权重动态计算当前窗口的高度
				ch = avail_h * (row_pers[i] / sum_row);

				c->geom.x = m->w.x + (m->w.width - cw) / 2 + target_gappo;
				if (i == 0) {
					c->geom.y = m->w.y + target_gappo;
				} else if (i == 1) {
					// 第二个窗口的 Y 坐标紧跟第一个窗口下面
					float ch0 = avail_h * (row_pers[0] / sum_row);
					c->geom.y = m->w.y + target_gappo + ch0 + target_gappi;
				}
				c->geom.width = cw;
				c->geom.height = ch;
				client_tile_resize(c, c->geom, 0);
				i++;
			}
		}
		return;
	}
	for (rows = 0; rows <= n / 2; rows++) {
		if (rows * rows >= n)
			break;
	}
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;
	overrows = n % rows;

	float col_pers[cols];
	float row_pers[rows];
	for (i = 0; i < cols; i++)
		col_pers[i] = 1.0f;
	for (i = 0; i < rows; i++)
		row_pers[i] = 1.0f;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal &&
			((m->isoverview && !client_is_x11_popup(c)) || ISFAKETILED(c))) {
			int32_t c_idx = i / rows;
			int32_t r_idx = i % rows;
			if (r_idx == 0)
				col_pers[c_idx] =
					(c->grid_col_per > 0.0f) ? c->grid_col_per : 1.0f;
			if (c_idx == 0)
				row_pers[r_idx] =
					(c->grid_row_per > 0.0f) ? c->grid_row_per : 1.0f;
			i++;
		}
	}

	float sum_col = 0.0f, sum_row = 0.0f;
	for (i = 0; i < cols; i++)
		sum_col += col_pers[i];
	for (i = 0; i < rows; i++)
		sum_row += row_pers[i];

	float avail_w = m->w.width - 2 * target_gappo - (cols - 1) * target_gappi;
	float avail_h = m->w.height - 2 * target_gappo - (rows - 1) * target_gappi;

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal &&
			((m->isoverview && !client_is_x11_popup(c)) || ISFAKETILED(c))) {
			int32_t c_idx = i / rows;
			int32_t r_idx = i % rows;

			c->grid_col_per = col_pers[c_idx];
			c->grid_row_per = row_pers[r_idx];
			c->grid_col_idx = c_idx;
			c->grid_row_idx = r_idx;

			float fl_cy = m->w.y + target_gappo;
			float fl_ch = 0.0f;

			if (overrows && i >= n - overrows) {
				float over_h = 0.0f;
				for (int j = 0; j < overrows; j++)
					over_h += avail_h * (row_pers[j] / sum_row);
				over_h += (overrows - 1) * target_gappi;
				float dy = (m->w.height - over_h) / 2.0f - target_gappo;

				fl_cy += dy;
				for (int j = 0; j < r_idx; j++)
					fl_cy += avail_h * (row_pers[j] / sum_row) + target_gappi;
				fl_ch = avail_h * (row_pers[r_idx] / sum_row);
			} else {
				for (int j = 0; j < r_idx; j++)
					fl_cy += avail_h * (row_pers[j] / sum_row) + target_gappi;
				fl_ch = (r_idx == rows - 1)
							? (m->w.y + m->w.height - target_gappo - fl_cy)
							: avail_h * (row_pers[r_idx] / sum_row);
			}

			float fl_cx = m->w.x + target_gappo;
			for (int j = 0; j < c_idx; j++)
				fl_cx += avail_w * (col_pers[j] / sum_col) + target_gappi;
			float fl_cw = (c_idx == cols - 1)
							  ? (m->w.x + m->w.width - target_gappo - fl_cx)
							  : avail_w * (col_pers[c_idx] / sum_col);

			c->geom.x = (int32_t)fl_cx;
			c->geom.y = (int32_t)fl_cy;
			c->geom.width = (int32_t)fl_cw;
			c->geom.height = (int32_t)fl_ch;
			client_tile_resize(c, c->geom, 0);
			i++;
		}
	}
}

void vertical_fair(Monitor *m) {
	int32_t i, n = 0;
	Client *c = NULL;

	n = m->visible_fake_tiling_clients;
	if (n == 0)
		return;

	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	if (config.smartgaps && n == 1) {
		cur_gappiv = cur_gappih = cur_gappov = cur_gappoh = 0;
	}

	int32_t rows;
	for (rows = 0; rows <= n; rows++) {
		if (rows * rows >= n)
			break;
	}

	int32_t base_cols = n / rows;
	int32_t remainder = n % rows;
	int32_t first_group_rows = rows - remainder;
	int32_t first_group_count = first_group_rows * base_cols;
	int32_t max_cols = base_cols + (remainder > 0 ? 1 : 0);

	Client *arr[n];
	int32_t arr_idx = 0;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISFAKETILED(c)) {
			arr[arr_idx++] = c;
			if (arr_idx >= n)
				break;
		}
	}

	float row_pers[rows];
	float col_pers[max_cols];
	for (i = 0; i < rows; i++)
		row_pers[i] = 0.0f;
	for (i = 0; i < max_cols; i++)
		col_pers[i] = 0.0f;

	for (i = 0; i < n; i++) {
		c = arr[i];
		int32_t row_idx =
			(i < first_group_count)
				? (i / base_cols)
				: (first_group_rows + (i - first_group_count) / max_cols);
		int32_t col_idx = (i < first_group_count)
							  ? (i % base_cols)
							  : ((i - first_group_count) % max_cols);

		if (c->grid_row_idx == row_idx && c->grid_row_per > 0.0f)
			row_pers[row_idx] = c->grid_row_per;
		if (c->grid_col_idx == col_idx && c->grid_col_per > 0.0f)
			col_pers[col_idx] = c->grid_col_per;
	}
	for (i = 0; i < n; i++) {
		c = arr[i];
		int32_t row_idx =
			(i < first_group_count)
				? (i / base_cols)
				: (first_group_rows + (i - first_group_count) / max_cols);
		int32_t col_idx = (i < first_group_count)
							  ? (i % base_cols)
							  : ((i - first_group_count) % max_cols);

		if (row_pers[row_idx] == 0.0f && c->grid_row_per > 0.0f)
			row_pers[row_idx] = c->grid_row_per;
		if (col_pers[col_idx] == 0.0f && c->grid_col_per > 0.0f)
			col_pers[col_idx] = c->grid_col_per;
	}

	float sum_row = 0.0f;
	for (i = 0; i < rows; i++) {
		if (row_pers[i] == 0.0f)
			row_pers[i] = 1.0f;
		sum_row += row_pers[i];
	}
	for (i = 0; i < max_cols; i++) {
		if (col_pers[i] == 0.0f)
			col_pers[i] = 1.0f;
	}

	float row_y[rows], row_h[rows];
	float avail_h = m->w.height - 2 * cur_gappov - (rows - 1) * cur_gappiv;
	float next_y = m->w.y + cur_gappov;
	for (i = 0; i < rows; i++) {
		row_y[i] = next_y;
		row_h[i] = (i == rows - 1)
					   ? (m->w.y + m->w.height - cur_gappov - next_y)
					   : (avail_h * (row_pers[i] / sum_row));
		next_y += row_h[i] + cur_gappiv;
	}

	float col_x_base[base_cols], col_w_base[base_cols];
	float sum_col_base = 0.0f;
	for (i = 0; i < base_cols; i++)
		sum_col_base += col_pers[i];
	float avail_w_base =
		m->w.width - 2 * cur_gappoh - (base_cols - 1) * cur_gappih;
	float next_x = m->w.x + cur_gappoh;
	for (i = 0; i < base_cols; i++) {
		col_x_base[i] = next_x;
		col_w_base[i] = (i == base_cols - 1)
							? (m->w.x + m->w.width - cur_gappoh - next_x)
							: (avail_w_base * (col_pers[i] / sum_col_base));
		next_x += col_w_base[i] + cur_gappih;
	}

	float col_x_max[max_cols], col_w_max[max_cols];
	if (remainder > 0) {
		float sum_col_max = 0.0f;
		for (i = 0; i < max_cols; i++)
			sum_col_max += col_pers[i];
		float avail_w_max =
			m->w.width - 2 * cur_gappoh - (max_cols - 1) * cur_gappih;
		next_x = m->w.x + cur_gappoh;
		for (i = 0; i < max_cols; i++) {
			col_x_max[i] = next_x;
			col_w_max[i] = (i == max_cols - 1)
							   ? (m->w.x + m->w.width - cur_gappoh - next_x)
							   : (avail_w_max * (col_pers[i] / sum_col_max));
			next_x += col_w_max[i] + cur_gappih;
		}
	}

	for (i = 0; i < n; i++) {
		c = arr[i];
		int32_t row_idx, col_idx;
		float fl_cx, fl_cy, fl_cw, fl_ch;

		if (i < first_group_count) {
			row_idx = i / base_cols;
			col_idx = i % base_cols;
			fl_cx = col_x_base[col_idx];
			fl_cw = col_w_base[col_idx];
		} else {
			int32_t offset = i - first_group_count;
			row_idx = first_group_rows + (offset / max_cols);
			col_idx = offset % max_cols;
			fl_cx = col_x_max[col_idx];
			fl_cw = col_w_max[col_idx];
		}

		c->grid_row_per = row_pers[row_idx];
		c->grid_col_per = col_pers[col_idx];
		c->grid_row_idx = row_idx;
		c->grid_col_idx = col_idx;

		fl_cy = row_y[row_idx];
		fl_ch = row_h[row_idx];

		client_tile_resize(c,
						   (struct wlr_box){.x = (int32_t)fl_cx,
											.y = (int32_t)fl_cy,
											.width = (int32_t)fl_cw,
											.height = (int32_t)fl_ch},
						   0);
	}
}