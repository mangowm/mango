void overview(Monitor *m) {
	int32_t target_gappo =
		enablegaps ? (m->isoverview ? config.overviewgappo : config.gappoh) : 0;
	int32_t target_gappi =
		enablegaps ? (m->isoverview ? config.overviewgappi : config.gappih) : 0;

	int n = m->isoverview ? m->visible_clients : m->visible_tiling_clients;
	if (n == 0)
		return;

	Client *c_arr[n];
	float aspects[n];
	int actual_n = 0;
	Client *c = NULL;

	wl_list_for_each(c, &clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal &&
			((m->isoverview && !client_is_x11_popup(c)) || ISTILED(c))) {

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
	n = actual_n;
	if (n == 0)
		return;

	float suffix_sums[n + 1];
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
	int best_items_per_row[n];
	best_items_per_row[0] = n;

	for (int R = 1; R <= n; R++) {
		int temp_items_per_row[R];
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
			float A_sum = suffix_sums[start_idx] -
						  suffix_sums[start_idx + temp_items_per_row[r]];
			start_idx += temp_items_per_row[r];

			float gap_x_total = (temp_items_per_row[r] - 1) * target_gappi;
			float w_avail = max_avail_w - gap_x_total;
			if (w_avail < 1)
				w_avail = 1;

			float h_limit = w_avail / A_sum;
			if (h_limit < min_h_max_w) {
				min_h_max_w = h_limit;
			}
		}

		float gap_y_total = (R - 1) * target_gappi;
		float h_avail = max_avail_h - gap_y_total;
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
	int items_per_row[rows];
	float A_sum[rows];

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
}