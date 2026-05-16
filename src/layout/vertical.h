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
	int32_t cw, ch;
	int32_t rows, cols, overrows;
	Client *c = NULL;
	int32_t target_gappo =
		enablegaps ? m->isoverview ? config.overviewgappo : config.gappov : 0;
	int32_t target_gappi =
		enablegaps ? m->isoverview ? config.overviewgappi : config.gappiv : 0;
	float single_width_ratio = m->isoverview ? 0.7 : 0.9;
	float single_height_ratio = m->isoverview ? 0.8 : 0.9;

	n = m->isoverview ? m->visible_clients : m->visible_tiling_clients;
	if (n == 0)
		return;

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
		float row_pers[2] = {1.0f, 1.0f};
		// 先提取这两个窗口现有的行比例
		i = 0;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal &&
				((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
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
				((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
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
				resize(c, c->geom, 0);
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
			((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
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
			((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {
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
			resize(c, c->geom, 0);
			i++;
		}
	}
}

void vertical_fair(Monitor *m) {
    int32_t n = m->visible_tiling_clients;
    if (n == 0) return;

    int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
    int32_t cur_gappih = enablegaps ? m->gappih : 0;
    int32_t cur_gappov = enablegaps ? m->gappov : 0;
    int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
    cur_gappiv = config.smartgaps && n == 1 ? 0 : cur_gappiv;
    cur_gappih = config.smartgaps && n == 1 ? 0 : cur_gappih;
    cur_gappov = config.smartgaps && n == 1 ? 0 : cur_gappov;
    cur_gappoh = config.smartgaps && n == 1 ? 0 : cur_gappoh;

    int32_t rows;
    for (rows = 0; rows <= n; rows++)
        if (rows * rows >= n) break;

    int32_t base_cols         = n / rows;
    int32_t remainder         = n % rows;
    int32_t first_group_rows  = rows - remainder;
    int32_t first_group_count = first_group_rows * base_cols;

    uint32_t tag = m->pertag->curtag;
    struct FairState *fs = ensure_fair_state(m, tag);
    // adapt with transposed semantics: rows are top-level
    if (fs->n_cols != rows) {
        for (int r = fs->n_cols; r < rows; r++) {
            if (fs->cols[r].ratio <= 0.0f) fs->cols[r].ratio = 1.0f;
            int cols_in_row = (r < first_group_rows) ? base_cols : base_cols + 1;
            for (int c = 0; c < cols_in_row; c++)
                if (fs->cols[r].row_ratios[c] <= 0.0f)
                    fs->cols[r].row_ratios[c] = 1.0f;
        }
        fs->n_cols = rows;
    }

    float avail_h = m->w.height - 2*cur_gappov - (rows-1)*cur_gappiv;
    float sum_row = 0.0f;
    for (int r = 0; r < rows; r++) sum_row += fs->cols[r].ratio;

    int     i        = 0;
    int     prev_row = -1;
    float   row_y    = 0, col_x = 0, avail_w = 0, sum_col = 0;
    int     cols_in_row = 0;
    Client *c;

    wl_list_for_each(c, &clients, link) {
        if (!VISIBLEON(c, m) || !ISTILED(c)) continue;

        int row, col;
        if (i < first_group_count) {
            row = i / base_cols;
            col = i % base_cols;
            cols_in_row = base_cols;
        } else {
            int off = i - first_group_count;
            row = first_group_rows + off / (base_cols + 1);
            col = off % (base_cols + 1);
            cols_in_row = base_cols + 1;
        }

        if (row != prev_row) {
            row_y = m->w.y + cur_gappov;
            for (int pr = 0; pr < row; pr++)
                row_y += avail_h * (fs->cols[pr].ratio / sum_row) + cur_gappiv;

            struct FairColNode *rn = &fs->cols[row];
            sum_col = 0.0f;
            avail_w = m->w.width - 2*cur_gappoh - (cols_in_row-1)*cur_gappih;
            for (int ci = 0; ci < cols_in_row; ci++) {
                if (rn->row_ratios[ci] <= 0.0f) rn->row_ratios[ci] = 1.0f;
                sum_col += rn->row_ratios[ci];
            }
            col_x    = m->w.x + cur_gappoh;
            prev_row = row;
        }

        struct FairColNode *rn = &fs->cols[row];

        float row_h = (row == rows - 1)
            ? (m->w.y + m->w.height - cur_gappov - row_y)
            : avail_h * (rn->ratio / sum_row);

        float col_w = (col == cols_in_row - 1)
            ? (m->w.x + m->w.width - cur_gappoh - col_x)
            : avail_w * (rn->row_ratios[col] / sum_col);

        c->grid_row_idx = row;
        c->grid_col_idx = col;
        c->grid_row_per = rn->ratio;
        c->grid_col_per = rn->row_ratios[col];

        resize(c, (struct wlr_box){
            .x      = (int32_t)col_x,
            .y      = (int32_t)row_y,
            .width  = (int32_t)col_w,
            .height = (int32_t)row_h,
        }, 0);

        col_x += col_w + cur_gappih;
        i++;
    }
}
