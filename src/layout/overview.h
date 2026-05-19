// 紧凑型自适应行高概览布局 (Mission Control / GNOME 风格)
void overview(Monitor *m) {
	int32_t target_gappo =
		enablegaps ? (m->isoverview ? config.overviewgappo : config.gappoh) : 0;
	int32_t target_gappi =
		enablegaps ? (m->isoverview ? config.overviewgappi : config.gappih) : 0;

	int n = m->isoverview ? m->visible_clients : m->visible_tiling_clients;
	if (n == 0)
		return;

	// 收集有效客户端，并提取它们原始的宽高比
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
			// 限制极端宽高比，防止某个 1px 宽度的异常窗口毁掉整个布局的数学计算
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

	// 动态决定行数与列数分配 (例如 7 个窗口分发为 3, 2, 2)
	int cols = 1;
	while (cols * cols < n)
		cols++;
	int rows = (n + cols - 1) / cols;

	int items_per_row[rows];
	int remaining = n;
	for (int r = 0; r < rows; r++) {
		// 使用向上取整的除法，让首行尽可能多排布，视觉重心更稳
		int count = (remaining + rows - r - 1) / (rows - r);
		items_per_row[r] = count;
		remaining -= count;
	}

	// 计算整个布局允许的最大可用区域 (留出四周安全边距)
	float max_avail_w = m->w.width - 2 * target_gappo;
	float max_avail_h = m->w.height - 2 * target_gappo;
	if (max_avail_w < 10)
		max_avail_w = 10;
	if (max_avail_h < 10)
		max_avail_h = 10;

	// 计算能够满足所有限制条件的 "最大统一行高"
	float A_sum[rows];		   // 每行窗口宽高比之和
	float h_max_w = 999999.0f; // 受宽度限制推导出的行高上限

	for (int r = 0; r < rows; r++) {
		A_sum[r] = 0;
		int start_idx = 0;
		for (int i = 0; i < r; i++)
			start_idx += items_per_row[i];

		for (int i = 0; i < items_per_row[r]; i++) {
			A_sum[r] += aspects[start_idx + i];
		}

		// 这行所有的内部间隙总和
		float gap_x_total = (items_per_row[r] - 1) * target_gappi;
		// 保证最宽的一行也不会超出 max_avail_w
		float h_limit = (max_avail_w - gap_x_total) / A_sum[r];
		if (h_limit < h_max_w)
			h_max_w = h_limit;
	}

	float gap_y_total = (rows - 1) * target_gappi;
	// 保证总行高不会超出 max_avail_h
	float h_max_h = (max_avail_h - gap_y_total) / rows;

	// 最终采用的行高是水平和垂直双向限制中最严苛的一个
	float row_height = h_max_w < h_max_h ? h_max_w : h_max_h;

	// 应用坐标并进行防撕裂渲染
	float total_layout_height = rows * row_height + gap_y_total;
	// 计算全局起点 Y，确保整个概览在屏幕垂直居中
	float start_y = m->w.y + (m->w.height - total_layout_height) / 2.0f;

	int current_idx = 0;
	float current_y = start_y;

	for (int r = 0; r < rows; r++) {
		// 根据当前确定的行高，反推这行真实的像素宽度
		float row_width =
			row_height * A_sum[r] + (items_per_row[r] - 1) * target_gappi;
		// 让当前这一排窗口在屏幕水平居中 (不满列数的行会自动居中对齐)
		float current_x = m->w.x + (m->w.width - row_width) / 2.0f;

		for (int i = 0; i < items_per_row[r]; i++) {
			Client *client = c_arr[current_idx];
			float aspect = aspects[current_idx];
			float client_width = row_height * aspect;

			// 【关键防错位】累加 float 坐标，最后写入 client
			// 时再强转+0.5四舍五入。
			// 避免每次计算内部小间隙都丢弃浮点精度，导致最后多出或少出 1px
			// 缝隙。
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