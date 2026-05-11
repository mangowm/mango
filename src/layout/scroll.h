/* 获取或创建指定 monitor 某个 tag 的 scroller 状态 */
static struct TagScrollerState *ensure_scroller_state(Monitor *m,
													  uint32_t tag) {
	if (!m->pertag->scroller_state[tag]) {
		struct TagScrollerState *st =
			calloc(1, sizeof(struct TagScrollerState));
		m->pertag->scroller_state[tag] = st;
	}
	return m->pertag->scroller_state[tag];
}

/* 在 tag 状态中查找客户端对应的节点（无则返回 NULL） */
static struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
													Client *c) {
	if (!st)
		return NULL;
	for (struct ScrollerStackNode *n = st->all_first; n; n = n->all_next)
		if (n->client == c)
			return n;
	return NULL;
}

/* 创建一个新节点并插入到 tag 状态的 all 链表中 */
static struct ScrollerStackNode *
scroller_node_create(struct TagScrollerState *st, Client *c) {
	struct ScrollerStackNode *n = calloc(1, sizeof(*n));
	n->client = c;
	n->scroller_proportion = c->scroller_proportion;
	n->stack_proportion = c->stack_proportion;
	n->scroller_proportion_single = c->scroller_proportion_single;
	n->next_in_stack = NULL;
	n->prev_in_stack = NULL;
	n->all_next = st->all_first;
	st->all_first = n;
	st->count++;
	return n;
}

/* 从 tag 状态中移除一个节点并释放 */
static void scroller_node_remove(struct TagScrollerState *st,
								 struct ScrollerStackNode *target) {
	if (!st || !target)
		return;

	/* 保存邻居 */
	struct ScrollerStackNode *prev = target->prev_in_stack;
	struct ScrollerStackNode *next = target->next_in_stack;

	/* 从堆叠链表中摘除 */
	if (prev)
		prev->next_in_stack = next;
	if (next)
		next->prev_in_stack = prev;

	/* 从 all 链表摘除 */
	struct ScrollerStackNode **indirect = &st->all_first;
	while (*indirect && *indirect != target)
		indirect = &(*indirect)->all_next;
	if (*indirect == target) {
		*indirect = target->all_next;
		st->count--;
	}
	free(target);
}

/* 清空一个 tag 的全部 scroller 状态 */
static void clear_scroller_state(struct TagScrollerState *st) {
	if (!st)
		return;
	struct ScrollerStackNode *n = st->all_first;
	while (n) {
		struct ScrollerStackNode *next = n->all_next;
		free(n);
		n = next;
	}
	free(st);
}

/* 在 Monitor 销毁时清理所有 tag 的 scroller 状态 */
static void cleanup_monitor_scroller(Monitor *m) {
	for (int t = 0; t < LENGTH(tags) + 1; t++) {
		if (m->pertag->scroller_state[t]) {
			clear_scroller_state(m->pertag->scroller_state[t]);
			m->pertag->scroller_state[t] = NULL;
		}
	}
}

/* 将某个 tag 的状态同步回所有客户端的全局字段 */
static void sync_scroller_state_to_clients(Monitor *m, uint32_t tag) {
	struct TagScrollerState *st = m->pertag->scroller_state[tag];
	if (!st)
		return;
	for (struct ScrollerStackNode *n = st->all_first; n; n = n->all_next) {
		Client *c = n->client;
		c->scroller_proportion = n->scroller_proportion;
		c->stack_proportion = n->stack_proportion;
		c->scroller_proportion_single = n->scroller_proportion_single;
	}
}

void vertical_scroll_adjust_fullandmax(Client *c, struct wlr_box *target_geom) {
	Monitor *m = c->mon;
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;

	cur_gappiv = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappiv;
	cur_gappov = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappov;
	cur_gappoh = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappoh;

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

void vertical_check_scroller_root_inside_mon(Client *c,
											 struct wlr_box *geometry) {
	if (!GEOMINSIDEMON(geometry, c->mon)) {
		geometry->y = c->mon->w.y + (c->mon->w.height - geometry->height) / 2;
	}
}

void horizontal_scroll_adjust_fullandmax(Client *c,
										 struct wlr_box *target_geom) {
	Monitor *m = c->mon;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;

	cur_gappih = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappih;
	cur_gappoh = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappoh;
	cur_gappov = config.smartgaps && m->visible_scroll_tiling_clients == 1
					 ? 0
					 : cur_gappov;

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

void horizontal_check_scroller_root_inside_mon(Client *c,
											   struct wlr_box *geometry) {
	if (!GEOMINSIDEMON(geometry, c->mon)) {
		geometry->x = c->mon->w.x + (c->mon->w.width - geometry->width) / 2;
	}
}

void arrange_stack_node(struct ScrollerStackNode *head, struct wlr_box geometry,
						int32_t gappiv) {
	int32_t stack_size = 0;
	struct ScrollerStackNode *iter = head;
	while (iter) {
		stack_size++;
		iter = iter->next_in_stack;
	}
	if (stack_size == 0)
		return;

	/* 归一化比例 */
	float total_proportion = 0.0f;
	iter = head;
	while (iter) {
		if (iter->stack_proportion <= 0.0f || iter->stack_proportion >= 1.0f)
			iter->stack_proportion =
				stack_size == 1 ? 1.0f : 1.0f / (stack_size - 1);
		total_proportion += iter->stack_proportion;
		iter = iter->next_in_stack;
	}
	iter = head;
	while (iter) {
		iter->stack_proportion /= total_proportion;
		iter = iter->next_in_stack;
	}

	/* 竖向排列（水平堆叠） */
	int32_t client_height;
	int32_t current_y = geometry.y;
	int32_t remain_client_height = geometry.height - (stack_size - 1) * gappiv;
	float remain_proportion = 1.0f;

	iter = head;
	while (iter) {
		client_height =
			remain_client_height * (iter->stack_proportion / remain_proportion);
		struct wlr_box client_geom = {.x = geometry.x,
									  .y = current_y,
									  .width = geometry.width,
									  .height = client_height};
		resize(iter->client, client_geom, 0);
		remain_proportion -= iter->stack_proportion;
		remain_client_height -= client_height;
		current_y += client_height + gappiv;
		iter = iter->next_in_stack;
	}
}

void arrange_stack_vertical_node(struct ScrollerStackNode *head,
								 struct wlr_box geometry, int32_t gappih) {
	int32_t stack_size = 0;
	struct ScrollerStackNode *iter = head;
	while (iter) {
		stack_size++;
		iter = iter->next_in_stack;
	}
	if (stack_size == 0)
		return;

	/* 归一化比例 */
	float total_proportion = 0.0f;
	iter = head;
	while (iter) {
		if (iter->stack_proportion <= 0.0f || iter->stack_proportion >= 1.0f)
			iter->stack_proportion =
				stack_size == 1 ? 1.0f : 1.0f / (stack_size - 1);
		total_proportion += iter->stack_proportion;
		iter = iter->next_in_stack;
	}
	iter = head;
	while (iter) {
		iter->stack_proportion /= total_proportion;
		iter = iter->next_in_stack;
	}

	/* 横向排列（垂直堆叠） */
	int32_t client_width;
	int32_t current_x = geometry.x;
	int32_t remain_client_width = geometry.width - (stack_size - 1) * gappih;
	float remain_proportion = 1.0f;

	iter = head;
	while (iter) {
		client_width =
			remain_client_width * (iter->stack_proportion / remain_proportion);
		struct wlr_box client_geom = {.y = geometry.y,
									  .x = current_x,
									  .height = geometry.height,
									  .width = client_width};
		resize(iter->client, client_geom, 0);
		remain_proportion -= iter->stack_proportion;
		remain_client_width -= client_width;
		current_x += client_width + gappih;
		iter = iter->next_in_stack;
	}
}

void scroller(Monitor *m) {
	uint32_t tag = m->pertag->curtag;
	struct TagScrollerState *st = ensure_scroller_state(m, tag);

	/* 收集当前可见的所有 scroller 平铺窗口 */
	Client *vis[512];
	int32_t count = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c))
			vis[count++] = c;
		if (count == 512)
			break;
	}

	/* 移除不再可见的节点 */
	struct ScrollerStackNode *n = st->all_first;
	while (n) {
		bool found = false;
		for (int i = 0; i < count; i++) {
			if (vis[i] == n->client) {
				found = true;
				break;
			}
		}
		struct ScrollerStackNode *next = n->all_next;
		if (!found)
			scroller_node_remove(st, n);
		n = next;
	}

	/* 为新的可见窗口创建节点 */
	for (int i = 0; i < count; i++) {
		if (!find_scroller_node(st, vis[i])) {
			scroller_node_create(st, vis[i]);
		}
	}

	/* 按全局客户端链表顺序收集所有堆叠头，确保视觉顺序正确 */
	struct ScrollerStackNode *heads[64];
	int32_t n_heads = 0;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node && !node->prev_in_stack) {
				bool already = false;
				for (int k = 0; k < n_heads; k++) {
					if (heads[k] == node) {
						already = true;
						break;
					}
				}
				if (!already)
					heads[n_heads++] = node;
			}
		}
	}

	if (n_heads == 0) {
		sync_scroller_state_to_clients(m, tag);
		return;
	}

	m->visible_scroll_tiling_clients = n_heads;

	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	if (config.smartgaps && n_heads == 1) {
		cur_gappih = cur_gappoh = cur_gappov = 0;
	}
	int32_t max_client_width =
		m->w.width - 2 * config.scroller_structs - cur_gappih;

	/* 单客户端特例 */
	if (n_heads == 1 && !config.scroller_ignore_proportion_single &&
		!heads[0]->client->isfullscreen &&
		!heads[0]->client->ismaximizescreen) {
		struct ScrollerStackNode *head = heads[0];
		float single_proportion =
			head->scroller_proportion_single > 0.0f
				? head->scroller_proportion_single
				: config.scroller_default_proportion_single;
		struct wlr_box target_geom;
		target_geom.height = m->w.height - 2 * cur_gappov;
		target_geom.width = (m->w.width - 2 * cur_gappoh) * single_proportion;
		target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		horizontal_check_scroller_root_inside_mon(head->client, &target_geom);
		arrange_stack_node(head, target_geom, cur_gappiv);
		sync_scroller_state_to_clients(m, tag);
		return;
	}

	struct ScrollerStackNode *root_node = NULL;
	if (m->sel && ISSCROLLTILED(m->sel)) {
		root_node = find_scroller_node(st, m->sel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node && m->prevsel && ISSCROLLTILED(m->prevsel)) {
		root_node = find_scroller_node(st, m->prevsel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node)
		root_node = heads[n_heads / 2]; /* 简单回退 */

	int32_t focus_index = -1;
	for (int i = 0; i < n_heads; i++) {
		if (heads[i] == root_node) {
			focus_index = i;
			break;
		}
	}
	if (focus_index < 0)
		focus_index = n_heads / 2;

	/* 判断是否需要滚动、overspread、center */
	bool need_scroller = false;
	bool over_overspread_to_left = false;
	Client *root_client = root_node->client;

	if (root_client->geom.x >= m->w.x + config.scroller_structs &&
		root_client->geom.x + root_client->geom.width <=
			m->w.x + m->w.width - config.scroller_structs) {
		need_scroller = false;
	} else {
		need_scroller = true;
	}

	bool need_apply_overspread =
		config.scroller_prefer_overspread && n_heads > 1 &&
		(focus_index == 0 || focus_index == n_heads - 1) &&
		heads[focus_index]->scroller_proportion < 1.0f;

	if (need_apply_overspread) {
		if (focus_index == 0) {
			over_overspread_to_left = true;
		} else {
			over_overspread_to_left = false;
		}
		if (over_overspread_to_left &&
			(!INSIDEMON(heads[1]->client) ||
			 (heads[1]->scroller_proportion + heads[0]->scroller_proportion >=
			  1.0f))) {
			need_scroller = true;
		} else if (!over_overspread_to_left &&
				   (!INSIDEMON(heads[focus_index - 1]->client) ||
					(heads[focus_index - 1]->scroller_proportion +
						 heads[focus_index]->scroller_proportion >=
					 1.0f))) {
			need_scroller = true;
		} else {
			need_apply_overspread = false;
		}
	}

	bool need_apply_center =
		config.scroller_focus_center || n_heads == 1 ||
		(config.scroller_prefer_center && !need_apply_overspread &&
		 (!m->prevsel ||
		  (ISSCROLLTILED(m->prevsel) &&
		   (m->prevsel->scroller_proportion * max_client_width) +
				   (heads[focus_index]->scroller_proportion *
					max_client_width) >
			   m->w.width - 2 * config.scroller_structs - cur_gappih)));

	if (n_heads == 1 && config.scroller_ignore_proportion_single) {
		need_scroller = true;
	}
	if (start_drag_window)
		need_scroller = false;

	struct wlr_box target_geom;
	target_geom.height = m->w.height - 2 * cur_gappov;
	target_geom.width =
		max_client_width * heads[focus_index]->scroller_proportion;
	target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
	horizontal_scroll_adjust_fullandmax(heads[focus_index]->client,
										&target_geom);

	if (heads[focus_index]->client->isfullscreen) {
		target_geom.x = m->m.x;
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	} else if (heads[focus_index]->client->ismaximizescreen) {
		target_geom.x = m->w.x + cur_gappoh;
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	} else if (need_scroller) {
		if (need_apply_center) {
			target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		} else if (need_apply_overspread) {
			if (over_overspread_to_left) {
				target_geom.x = m->w.x + config.scroller_structs;
			} else {
				target_geom.x =
					m->w.x + (m->w.width -
							  heads[focus_index]->scroller_proportion *
								  max_client_width -
							  config.scroller_structs);
			}
		} else {
			target_geom.x =
				root_client->geom.x > m->w.x + (m->w.width) / 2
					? m->w.x + (m->w.width -
								heads[focus_index]->scroller_proportion *
									max_client_width -
								config.scroller_structs)
					: m->w.x + config.scroller_structs;
		}
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	} else {
		target_geom.x = root_client->geom.x;
		horizontal_check_scroller_root_inside_mon(heads[focus_index]->client,
												  &target_geom);
		arrange_stack_node(heads[focus_index], target_geom, cur_gappiv);
	}

	/* 排列左侧的堆叠 */
	for (int i = 1; i <= focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index - i];
		struct wlr_box left_geom;
		left_geom.height = m->w.height - 2 * cur_gappov;
		left_geom.width = max_client_width * cur->scroller_proportion;
		horizontal_scroll_adjust_fullandmax(cur->client, &left_geom);
		left_geom.x = heads[focus_index - i + 1]->client->geom.x - cur_gappih -
					  left_geom.width;
		arrange_stack_node(cur, left_geom, cur_gappiv);
	}

	/* 排列右侧的堆叠 */
	for (int i = 1; i < n_heads - focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index + i];
		struct wlr_box right_geom;
		right_geom.height = m->w.height - 2 * cur_gappov;
		right_geom.width = max_client_width * cur->scroller_proportion;
		horizontal_scroll_adjust_fullandmax(cur->client, &right_geom);
		right_geom.x = heads[focus_index + i - 1]->client->geom.x + cur_gappih +
					   heads[focus_index + i - 1]->client->geom.width;
		arrange_stack_node(cur, right_geom, cur_gappiv);
	}

	sync_scroller_state_to_clients(m, tag);
}

void vertical_scroller(Monitor *m) {
	uint32_t tag = m->pertag->curtag;
	struct TagScrollerState *st = ensure_scroller_state(m, tag);

	Client *vis[512];
	int32_t count = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c))
			vis[count++] = c;
		if (count == 512)
			break;
	}

	struct ScrollerStackNode *n = st->all_first;
	while (n) {
		bool found = false;
		for (int i = 0; i < count; i++) {
			if (vis[i] == n->client) {
				found = true;
				break;
			}
		}
		struct ScrollerStackNode *next = n->all_next;
		if (!found)
			scroller_node_remove(st, n);
		n = next;
	}

	for (int i = 0; i < count; i++) {
		if (!find_scroller_node(st, vis[i])) {
			scroller_node_create(st, vis[i]);
		}
	}

	/* 按全局顺序收集堆叠头 */
	struct ScrollerStackNode *heads[64];
	int32_t n_heads = 0;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISSCROLLTILED(c)) {
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node && !node->prev_in_stack) {
				bool already = false;
				for (int k = 0; k < n_heads; k++)
					if (heads[k] == node)
						already = true;
				if (!already)
					heads[n_heads++] = node;
			}
		}
	}

	if (n_heads == 0) {
		sync_scroller_state_to_clients(m, tag);
		return;
	}

	m->visible_scroll_tiling_clients = n_heads;

	int32_t cur_gappiv = enablegaps ? m->gappiv : 0;
	int32_t cur_gappov = enablegaps ? m->gappov : 0;
	int32_t cur_gappoh = enablegaps ? m->gappoh : 0;
	int32_t cur_gappih = enablegaps ? m->gappih : 0;
	if (config.smartgaps && n_heads == 1) {
		cur_gappiv = cur_gappov = cur_gappoh = 0;
	}
	int32_t max_client_height =
		m->w.height - 2 * config.scroller_structs - cur_gappiv;

	if (n_heads == 1 && !config.scroller_ignore_proportion_single &&
		!heads[0]->client->isfullscreen &&
		!heads[0]->client->ismaximizescreen) {
		struct ScrollerStackNode *head = heads[0];
		float single_proportion =
			head->scroller_proportion_single > 0.0f
				? head->scroller_proportion_single
				: config.scroller_default_proportion_single;
		struct wlr_box target_geom;
		target_geom.width = m->w.width - 2 * cur_gappoh;
		target_geom.height = (m->w.height - 2 * cur_gappov) * single_proportion;
		target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
		vertical_check_scroller_root_inside_mon(head->client, &target_geom);
		arrange_stack_vertical_node(head, target_geom, cur_gappih);
		sync_scroller_state_to_clients(m, tag);
		return;
	}

	struct ScrollerStackNode *root_node = NULL;
	if (m->sel && ISSCROLLTILED(m->sel)) {
		root_node = find_scroller_node(st, m->sel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node && m->prevsel && ISSCROLLTILED(m->prevsel)) {
		root_node = find_scroller_node(st, m->prevsel);
		if (root_node) {
			while (root_node->prev_in_stack)
				root_node = root_node->prev_in_stack;
		}
	}
	if (!root_node)
		root_node = heads[n_heads / 2];

	int32_t focus_index = -1;
	for (int i = 0; i < n_heads; i++) {
		if (heads[i] == root_node) {
			focus_index = i;
			break;
		}
	}
	if (focus_index < 0)
		focus_index = n_heads / 2;

	bool need_scroller = false;
	bool over_overspread_to_up = false;
	Client *root_client = root_node->client;

	if (root_client->geom.y >= m->w.y + config.scroller_structs &&
		root_client->geom.y + root_client->geom.height <=
			m->w.y + m->w.height - config.scroller_structs) {
		need_scroller = false;
	} else {
		need_scroller = true;
	}

	bool need_apply_overspread =
		config.scroller_prefer_overspread && n_heads > 1 &&
		(focus_index == 0 || focus_index == n_heads - 1) &&
		heads[focus_index]->scroller_proportion < 1.0f;

	if (need_apply_overspread) {
		if (focus_index == 0) {
			over_overspread_to_up = true;
		} else {
			over_overspread_to_up = false;
		}
		if (over_overspread_to_up &&
			(!INSIDEMON(heads[1]->client) ||
			 (heads[1]->scroller_proportion + heads[0]->scroller_proportion >=
			  1.0f))) {
			need_scroller = true;
		} else if (!over_overspread_to_up &&
				   (!INSIDEMON(heads[focus_index - 1]->client) ||
					(heads[focus_index - 1]->scroller_proportion +
						 heads[focus_index]->scroller_proportion >=
					 1.0f))) {
			need_scroller = true;
		} else {
			need_apply_overspread = false;
		}
	}

	bool need_apply_center =
		config.scroller_focus_center || n_heads == 1 ||
		(config.scroller_prefer_center && !need_apply_overspread &&
		 (!m->prevsel ||
		  (ISSCROLLTILED(m->prevsel) &&
		   (m->prevsel->scroller_proportion * max_client_height) +
				   (heads[focus_index]->scroller_proportion *
					max_client_height) >
			   m->w.height - 2 * config.scroller_structs - cur_gappiv)));

	if (n_heads == 1 && config.scroller_ignore_proportion_single) {
		need_scroller = true;
	}
	if (start_drag_window)
		need_scroller = false;

	struct wlr_box target_geom;
	target_geom.width = m->w.width - 2 * cur_gappoh;
	target_geom.height =
		max_client_height * heads[focus_index]->scroller_proportion;
	target_geom.x = m->w.x + (m->w.width - target_geom.width) / 2;
	vertical_scroll_adjust_fullandmax(heads[focus_index]->client, &target_geom);

	if (heads[focus_index]->client->isfullscreen) {
		target_geom.y = m->m.y;
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	} else if (heads[focus_index]->client->ismaximizescreen) {
		target_geom.y = m->w.y + cur_gappov;
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	} else if (need_scroller) {
		if (need_apply_center) {
			target_geom.y = m->w.y + (m->w.height - target_geom.height) / 2;
		} else if (need_apply_overspread) {
			if (over_overspread_to_up) {
				target_geom.y = m->w.y + config.scroller_structs;
			} else {
				target_geom.y =
					m->w.y + (m->w.height -
							  heads[focus_index]->scroller_proportion *
								  max_client_height -
							  config.scroller_structs);
			}
		} else {
			target_geom.y =
				root_client->geom.y > m->w.y + (m->w.height) / 2
					? m->w.y + (m->w.height -
								heads[focus_index]->scroller_proportion *
									max_client_height -
								config.scroller_structs)
					: m->w.y + config.scroller_structs;
		}
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	} else {
		target_geom.y = root_client->geom.y;
		vertical_check_scroller_root_inside_mon(heads[focus_index]->client,
												&target_geom);
		arrange_stack_vertical_node(heads[focus_index], target_geom,
									cur_gappih);
	}

	for (int i = 1; i <= focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index - i];
		struct wlr_box up_geom;
		up_geom.width = m->w.width - 2 * cur_gappoh;
		up_geom.height = max_client_height * cur->scroller_proportion;
		vertical_scroll_adjust_fullandmax(cur->client, &up_geom);
		up_geom.y = heads[focus_index - i + 1]->client->geom.y - cur_gappiv -
					up_geom.height;
		arrange_stack_vertical_node(cur, up_geom, cur_gappih);
	}

	for (int i = 1; i < n_heads - focus_index; i++) {
		struct ScrollerStackNode *cur = heads[focus_index + i];
		struct wlr_box down_geom;
		down_geom.width = m->w.width - 2 * cur_gappoh;
		down_geom.height = max_client_height * cur->scroller_proportion;
		vertical_scroll_adjust_fullandmax(cur->client, &down_geom);
		down_geom.y = heads[focus_index + i - 1]->client->geom.y + cur_gappiv +
					  heads[focus_index + i - 1]->client->geom.height;
		arrange_stack_vertical_node(cur, down_geom, cur_gappih);
	}

	sync_scroller_state_to_clients(m, tag);
}

void scroller_remove_client(Client *c) {
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		for (uint32_t t = 0; t < LENGTH(tags) + 1; t++) {
			struct TagScrollerState *st = m->pertag->scroller_state[t];
			if (!st)
				continue;
			struct ScrollerStackNode *node = find_scroller_node(st, c);
			if (node) {
				scroller_node_remove(st, node);
			}
		}
	}
}