void overview_scale(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	int orig_n = m->visible_clients;

	if (orig_n == 0)
		return;

	size_t sz_c_arr = orig_n * sizeof(Client *);
	size_t sz_aspects = orig_n * sizeof(float);
	size_t sz_suffix_sums = (orig_n + 1) * sizeof(float);
	size_t sz_best_items = orig_n * sizeof(int);
	size_t sz_temp_items = orig_n * sizeof(int);
	size_t sz_items = orig_n * sizeof(int);
	size_t sz_A_sum = orig_n * sizeof(float);

	size_t total_size = sz_c_arr + sz_aspects + sz_suffix_sums + sz_best_items +
						sz_temp_items + sz_items + sz_A_sum;

	void *buffer = malloc(total_size);
	if (!buffer) {
		return;
	}

	Client **c_arr = (Client **)buffer;
	float *aspects = (float *)((char *)buffer + sz_c_arr);
	float *suffix_sums = (float *)((char *)aspects + sz_aspects);
	int *best_items_per_row = (int *)((char *)suffix_sums + sz_suffix_sums);
	int *temp_items_per_row =
		(int *)((char *)best_items_per_row + sz_best_items);
	int *items_per_row = (int *)((char *)temp_items_per_row + sz_temp_items);
	float *A_sum = (float *)((char *)items_per_row + sz_items);

	int actual_n = 0;
	Client *c = NULL;

	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {

			c_arr[actual_n] = c;
			float aspect = 1.0f;
			if (c->overview_backup_geom.height > 0 &&
				c->overview_backup_geom.width > 0) {
				aspect = (float)c->overview_backup_geom.width /
						 c->overview_backup_geom.height;
			}
			if (aspect < 0.2f)
				aspect = 0.2f;
			if (aspect > 5.0f)
				aspect = 5.0f;

			aspects[actual_n] = aspect;
			actual_n++;
		}
	}

	int n = actual_n;
	if (n == 0) {
		free(buffer);
		return;
	}

	suffix_sums[n] = 0.0f;
	for (int i = n - 1; i >= 0; i--) {
		suffix_sums[i] = suffix_sums[i + 1] + aspects[i];
	}

	float max_avail_w = m->w.width - 2 * target_gappo;
	float max_avail_h = m->w.height - 2 * target_gappo;
	if (max_avail_w < 10)
		max_avail_w = 10;
	if (max_avail_h < 10)
		max_avail_h = 10;

	int best_rows = 1;
	float best_row_height = 0.0f;
	best_items_per_row[0] = n;

	for (int R = 1; R <= n; R++) {
		int start_idx = 0;

		for (int r = 0; r < R; r++) {
			int rows_left = R - r;

			float S_rem = suffix_sums[start_idx];
			float target_sum = S_rem / rows_left;

			float current_sum = 0;
			int count = 0;

			while (start_idx + count < n - (rows_left - 1)) {
				float next_val = aspects[start_idx + count];
				if (rows_left == 1) {
					current_sum += next_val;
					count++;
					continue;
				}

				if (count > 0) {
					float diff_without = fabs(current_sum - target_sum);
					float diff_with = fabs(current_sum + next_val - target_sum);
					if (diff_with > diff_without) {
						break;
					}
				}
				current_sum += next_val;
				count++;
			}
			temp_items_per_row[r] = count;
			start_idx += count;
		}

		float min_h_max_w = 999999.0f;
		start_idx = 0;
		for (int r = 0; r < R; r++) {
			float row_A_sum = suffix_sums[start_idx] -
							  suffix_sums[start_idx + temp_items_per_row[r]];
			start_idx += temp_items_per_row[r];

			float gap_x_total = (temp_items_per_row[r] - 1) * target_gappi;
			float w_avail = max_avail_w - gap_x_total;
			if (w_avail < 1)
				w_avail = 1;

			float h_limit = w_avail / row_A_sum;
			if (h_limit < min_h_max_w) {
				min_h_max_w = h_limit;
			}
		}

		float gap_y_total_temp = (R - 1) * target_gappi;
		float h_avail = max_avail_h - gap_y_total_temp;
		if (h_avail < 1)
			h_avail = 1;

		float h_max_h = h_avail / R;
		float final_h = min_h_max_w < h_max_h ? min_h_max_w : h_max_h;

		if (final_h > best_row_height) {
			best_row_height = final_h;
			best_rows = R;
			for (int r = 0; r < R; r++) {
				best_items_per_row[r] = temp_items_per_row[r];
			}
		}
	}

	int rows = best_rows;
	float row_height = best_row_height;

	int current_render_idx = 0;
	for (int r = 0; r < rows; r++) {
		items_per_row[r] = best_items_per_row[r];
		A_sum[r] = suffix_sums[current_render_idx] -
				   suffix_sums[current_render_idx + items_per_row[r]];
		current_render_idx += items_per_row[r];
	}

	float gap_y_total = (rows - 1) * target_gappi;
	float total_layout_height = rows * row_height + gap_y_total;
	float start_y = m->w.y + (m->w.height - total_layout_height) / 2.0f;

	int current_idx = 0;
	float current_y = start_y;

	for (int r = 0; r < rows; r++) {
		float row_width =
			row_height * A_sum[r] + (items_per_row[r] - 1) * target_gappi;
		float current_x = m->w.x + (m->w.width - row_width) / 2.0f;

		for (int i = 0; i < items_per_row[r]; i++) {
			Client *client = c_arr[current_idx];
			float aspect = aspects[current_idx];
			float client_width = row_height * aspect;

			struct wlr_box client_geom;
			client_geom.x = (int)(current_x + 0.5f);
			client_geom.y = (int)(current_y + 0.5f);

			float next_x = current_x + client_width;
			client_geom.width = (int)(next_x + 0.5f) - client_geom.x;

			float next_y = current_y + row_height;
			client_geom.height = (int)(next_y + 0.5f) - client_geom.y;

			resize(client, client_geom, 0);

			current_x = next_x + target_gappi;
			current_idx++;
		}
		current_y += row_height + target_gappi;
	}

	free(buffer);
}

void overview_resize(Monitor *m) {
	int32_t i, n;
	int32_t cx, cy, cw, ch;
	int32_t dx;
	int32_t cols, rows, overcols;
	Client *c = NULL;

	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;
	float single_width_ratio = 0.7;
	float single_height_ratio = 0.8;

	n = m->visible_clients;

	if (n == 0)
		return;

	if (n == 1) {
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
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
			if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
				if (i == 0) {
					c->geom.x = m->w.x + target_gappo;
					c->geom.y = m->w.y + (m->w.height - ch) / 2 + target_gappo;
				} else if (i == 1) {
					c->geom.x = m->w.x + cw + target_gappo + target_gappi;
					c->geom.y = m->w.y + (m->w.height - ch) / 2 + target_gappo;
				}
				c->geom.width = cw;
				c->geom.height = ch;
				resize(c, c->geom, 0);
				i++;
			}
		}
		return;
	}

	for (cols = 0; cols <= n / 2; cols++) {
		if (cols * cols >= n)
			break;
	}
	rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;

	ch = (m->w.height - 2 * target_gappo - (rows - 1) * target_gappi) / rows;
	cw = (m->w.width - 2 * target_gappo - (cols - 1) * target_gappi) / cols;

	overcols = n % cols;
	if (overcols) {
		dx = (m->w.width - overcols * cw - (overcols - 1) * target_gappi) / 2 -
			 target_gappo;
	}

	i = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			cx = m->w.x + (i % cols) * (cw + target_gappi);
			cy = m->w.y + (i / cols) * (ch + target_gappi);
			if (overcols && i >= n - overcols)
				cx += dx;
			c->geom.x = cx + target_gappo;
			c->geom.y = cy + target_gappo;
			c->geom.width = cw;
			c->geom.height = ch;
			resize(c, c->geom, 0);
			i++;
		}
	}
}

void overview(Monitor *m) {
	if (config.ov_no_resize) {
		overview_scale(m);
	} else {
		overview_resize(m);
	}
}