static DwindleNode *dwindle_locked_h_node = NULL;
static DwindleNode *dwindle_locked_v_node = NULL;

static DwindleNode *dwindle_new_leaf(Client *c) {
	DwindleNode *n = calloc(1, sizeof(DwindleNode));
	n->client = c;
	return n;
}

// 统计同方向上的节点总和 (N_old)
static int count_block_items(DwindleNode *node, bool split_h) {
	if (!node)
		return 0;
	if (!node->is_split || node->split_h != split_h)
		return 1;
	return count_block_items(node->first, split_h) +
		   count_block_items(node->second, split_h);
}

// 向上查找方向块路径，并计算每个祖先节点的绝对占比
static int get_block_path_and_ratios(DwindleNode *target, bool split_h,
									 DwindleNode **path, float *p) {
	int path_len = 0;
	path[path_len++] = target;
	DwindleNode *curr = target->parent;
	while (curr && curr->split_h == split_h) {
		path[path_len++] = curr;
		curr = curr->parent;
	}

	p[path_len - 1] = 1.0f; // 方向块根节点占比为 100%
	for (int i = path_len - 1; i > 0; i--) {
		DwindleNode *S = path[i];
		DwindleNode *child = path[i - 1];
		if (S->first == child)
			p[i - 1] = p[i] * S->ratio;
		else
			p[i - 1] = p[i] * (1.0f - S->ratio);
	}
	return path_len;
}

static DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c) {
	if (!node)
		return NULL;
	if (!node->is_split)
		return node->client == c ? node : NULL;
	DwindleNode *r = dwindle_find_leaf(node->first, c);
	return r ? r : dwindle_find_leaf(node->second, c);
}

static DwindleNode *dwindle_first_leaf(DwindleNode *node) {
	if (!node)
		return NULL;
	while (node->is_split)
		node = node->first;
	return node;
}

static void dwindle_free_tree(DwindleNode *node) {
	if (!node)
		return;
	dwindle_free_tree(node->first);
	dwindle_free_tree(node->second);
	free(node);
}

static void dwindle_insert(DwindleNode **root, Client *new_c, Client *focused,
						   float ratio, bool as_first, bool split_h,
						   bool lock) {
	DwindleNode *new_leaf = dwindle_new_leaf(new_c);

	if (!*root) {
		new_leaf->custom_leaf_split_h = true;
		*root = new_leaf;
		return;
	}

	DwindleNode *target = focused ? dwindle_find_leaf(*root, focused) : NULL;
	if (!target)
		target = dwindle_first_leaf(*root);

	// ================= 保持其他窗口比例缩减逻辑 =================
	DwindleNode *path[128];
	float p[128];
	int path_len = get_block_path_and_ratios(target, split_h, path, p);

	int n_old = 1;
	if (path_len > 1) {
		n_old = count_block_items(path[path_len - 1], split_h);
	}
	float N = (float)(n_old + 1);

	for (int i = path_len - 1; i > 0; i--) {
		DwindleNode *S = path[i];
		DwindleNode *child = path[i - 1];
		float p_S = p[i];
		float p_first = p_S * S->ratio;

		if (S->first == child) {
			float p_first_new = p_first * (N - 1.0f) / N + 1.0f / N;
			float p_S_new = p_S * (N - 1.0f) / N + 1.0f / N;
			S->ratio = p_first_new / p_S_new;
		} else {
			float p_first_new = p_first * (N - 1.0f) / N;
			float p_S_new = p_S * (N - 1.0f) / N + 1.0f / N;
			S->ratio = p_first_new / p_S_new;
		}
		if (S->ratio < 0.001f)
			S->ratio = 0.001f;
		if (S->ratio > 0.999f)
			S->ratio = 0.999f;
	}
	// ============================================================

	DwindleNode *split = calloc(1, sizeof(DwindleNode));
	split->is_split = true;
	split->split_h = split_h;
	split->split_locked = lock;
	split->custom_leaf_split_h = target->custom_leaf_split_h;
	new_leaf->custom_leaf_split_h = target->custom_leaf_split_h;

	if (as_first) {
		split->first = new_leaf;
		split->second = target;
	} else {
		split->first = target;
		split->second = new_leaf;
	}

	// 通用逻辑
	split->ratio = ratio;

	split->parent = target->parent;
	target->parent = split;
	new_leaf->parent = split;

	if (!split->parent) {
		*root = split;
	} else {
		if (split->parent->first == target)
			split->parent->first = split;
		else
			split->parent->second = split;
	}
}

static void dwindle_remove(DwindleNode **root, Client *c) {
	DwindleNode *leaf = dwindle_find_leaf(*root, c);
	if (!leaf)
		return;

	DwindleNode *parent = leaf->parent;
	if (!parent) {
		free(leaf);
		*root = NULL;
		return;
	}

	// 开始删除空间的比例回退逻辑

	// 查找连续的同方向块路径
	bool split_h = parent->split_h;
	DwindleNode *path[128];
	int path_len = 0;
	path[path_len++] = parent;
	DwindleNode *curr = parent->parent;
	while (curr && curr->split_h == split_h) {
		path[path_len++] = curr;
		curr = curr->parent;
	}

	// 计算各祖先的旧绝对占比
	float p[128];
	p[path_len - 1] = 1.0f;
	for (int i = path_len - 1; i > 0; i--) {
		DwindleNode *S = path[i];
		DwindleNode *child = path[i - 1];
		if (S->first == child)
			p[i - 1] = p[i] * S->ratio;
		else
			p[i - 1] = p[i] * (1.0f - S->ratio);
	}

	// 计算即将被删除的叶子节点，在该方向块中所占的绝对面积比例 (P_del)
	float p_del =
		p[0] * (parent->first == leaf ? parent->ratio : (1.0f - parent->ratio));
	if (p_del > 0.999f)
		p_del = 0.999f; // 兜底

	// 重算祖先比例：将 P_del 空出来的空间，按原定比例无缝分配给其他窗口
	for (int i = path_len - 1; i > 0; i--) {
		DwindleNode *S = path[i];
		DwindleNode *child = path[i - 1];
		float p_S = p[i];
		float p_first = p_S * S->ratio;

		float denom = p_S - p_del;
		if (denom < 0.0001f)
			denom = 0.0001f;

		if (S->first == child) {
			S->ratio = (p_first - p_del) / denom;
		} else {
			S->ratio = p_first / denom;
		}

		if (S->ratio < 0.001f)
			S->ratio = 0.001f;
		if (S->ratio > 0.999f)
			S->ratio = 0.999f;
	}

	// 比例重算结束

	// 基础的二叉树摘除节点逻辑
	DwindleNode *sibling =
		(parent->first == leaf) ? parent->second : parent->first;
	DwindleNode *grandparent = parent->parent;

	sibling->parent = grandparent;

	if (!sibling->is_split ||
		(!config.dwindle_preserve_split && !config.dwindle_smart_split)) {
		sibling->container_w = 0;
		sibling->container_h = 0;
	}

	if (!grandparent) {
		*root = sibling;
	} else {
		if (grandparent->first == parent)
			grandparent->first = sibling;
		else
			grandparent->second = sibling;
	}

	free(leaf);
	free(parent);
}

static void dwindle_assign(DwindleNode *node, int32_t ax, int32_t ay,
						   int32_t aw, int32_t ah, int32_t gap_h,
						   int32_t gap_v) {
	if (!node)
		return;

	if (!node->is_split) {
		if (node->client) {
			struct wlr_box box = {ax, ay, MAX(1, aw), MAX(1, ah)};
			resize(node->client, box, 0);
		}
		return;
	}

	if (!node->split_locked && node->container_w == 0 && node->container_h == 0)
		node->split_h = (aw >= ah);
	node->container_x = ax;
	node->container_y = ay;
	node->container_w = aw;
	node->container_h = ah;
	if (node->split_h) {
		int32_t w1 = MAX(1, (int32_t)(aw * node->ratio) - gap_h / 2);
		dwindle_assign(node->first, ax, ay, w1, ah, gap_h, gap_v);
		dwindle_assign(node->second, ax + w1 + gap_h, ay, aw - w1 - gap_h, ah,
					   gap_h, gap_v);
	} else {
		int32_t h1 = MAX(1, (int32_t)(ah * node->ratio) - gap_v / 2);
		dwindle_assign(node->first, ax, ay, aw, h1, gap_h, gap_v);
		dwindle_assign(node->second, ax, ay + h1 + gap_v, aw, ah - h1 - gap_v,
					   gap_h, gap_v);
	}
}

static void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
								float ratio, int32_t dir) {
	if (!c || !target || c == target)
		return;
	if (!dwindle_find_leaf(*root, c) || !dwindle_find_leaf(*root, target))
		return;
	dwindle_remove(root, c);
	bool as_first = (dir == UP || dir == LEFT);
	bool split_h = (dir == LEFT || dir == RIGHT);
	dwindle_insert(root, c, target, ratio, as_first, split_h, true);
}

static void dwindle_swap_clients(DwindleNode **root, Client *a, Client *b) {
	DwindleNode *la = dwindle_find_leaf(*root, a);
	DwindleNode *lb = dwindle_find_leaf(*root, b);
	if (!la || !lb || la == lb)
		return;
	la->client = b;
	lb->client = a;
}

static void dwindle_resize_client(Monitor *m, Client *c) {
	uint32_t tag = m->pertag->curtag;
	DwindleNode *leaf = dwindle_find_leaf(m->pertag->dwindle_root[tag], c);
	if (!leaf)
		return;

	if (!start_drag_window) {
		start_drag_window = true;
		dwindle_locked_h_node = NULL;
		dwindle_locked_v_node = NULL;
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		DwindleNode *node = leaf->parent;
		while (node) {
			if (node->split_h && !dwindle_locked_h_node) {
				dwindle_locked_h_node = node;
				node->drag_init_ratio = node->ratio;
			}
			if (!node->split_h && !dwindle_locked_v_node) {
				dwindle_locked_v_node = node;
				node->drag_init_ratio = node->ratio;
			}
			if (dwindle_locked_h_node && dwindle_locked_v_node)
				break;
			node = node->parent;
		}
	}

	if (!dwindle_locked_h_node && !dwindle_locked_v_node)
		return;

	if (dwindle_locked_h_node) {
		float cw = (float)MAX(1, dwindle_locked_h_node->container_w);
		float ox = (float)(cursor->x - drag_begin_cursorx);
		if (config.dwindle_smart_resize) {
			/* Move the boundary toward the cursor: invert direction when
			 * the drag started on the right side of the split line. */
			float split_x = dwindle_locked_h_node->container_x +
							cw * dwindle_locked_h_node->drag_init_ratio;
			if (drag_begin_cursorx >= split_x)
				ox = -ox;
		}
		dwindle_locked_h_node->ratio =
			dwindle_locked_h_node->drag_init_ratio + ox / cw;
		dwindle_locked_h_node->ratio =
			CLAMP_FLOAT(dwindle_locked_h_node->ratio, 0.05f, 0.95f);
	}

	if (dwindle_locked_v_node) {
		float ch = (float)MAX(1, dwindle_locked_v_node->container_h);
		float oy = (float)(cursor->y - drag_begin_cursory);
		if (config.dwindle_smart_resize) {
			/* Same logic for the vertical split line. */
			float split_y = dwindle_locked_v_node->container_y +
							ch * dwindle_locked_v_node->drag_init_ratio;
			if (drag_begin_cursory >= split_y)
				oy = -oy;
		}
		dwindle_locked_v_node->ratio =
			dwindle_locked_v_node->drag_init_ratio + oy / ch;
		dwindle_locked_v_node->ratio =
			CLAMP_FLOAT(dwindle_locked_v_node->ratio, 0.05f, 0.95f);
	}

	int32_t n = m->visible_tiling_clients;
	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(m->pertag->dwindle_root[tag], m->w.x + gap_oh,
				   m->w.y + gap_ov, m->w.width - 2 * gap_oh,
				   m->w.height - 2 * gap_ov, gap_ih, gap_iv);
}

static void dwindle_resize_client_step(Monitor *m, Client *c, int32_t dx,
									   int32_t dy) {
	uint32_t tag = m->pertag->curtag;
	DwindleNode *leaf = dwindle_find_leaf(m->pertag->dwindle_root[tag], c);
	if (!leaf)
		return;

	DwindleNode *h_node = NULL;
	DwindleNode *v_node = NULL;
	DwindleNode *node = leaf->parent;

	while (node) {
		if (node->split_h && !h_node)
			h_node = node;
		if (!node->split_h && !v_node)
			v_node = node;
		if (h_node && v_node)
			break;
		node = node->parent;
	}

	if (!h_node && !v_node)
		return;

	if (h_node && dx) {
		float cw = (float)MAX(1, h_node->container_w);
		float delta = (float)dx / cw;
		h_node->ratio = CLAMP_FLOAT(h_node->ratio + delta, 0.05f, 0.95f);
	}

	if (v_node && dy) {
		float ch = (float)MAX(1, v_node->container_h);
		float delta = (float)dy / ch;
		v_node->ratio = CLAMP_FLOAT(v_node->ratio + delta, 0.05f, 0.95f);
	}

	int32_t n_clients = m->visible_tiling_clients;
	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n_clients == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(m->pertag->dwindle_root[tag], m->w.x + gap_oh,
				   m->w.y + gap_ov, m->w.width - 2 * gap_oh,
				   m->w.height - 2 * gap_ov, gap_ih, gap_iv);
}

static void dwindle_remove_client(Client *c) {
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		for (uint32_t t = 0; t < LENGTH(tags) + 1; t++)
			dwindle_remove(&m->pertag->dwindle_root[t], c);
	}
}

/* Insert a new client respecting dwindle_vsplit, dwindle_hsplit, and
 * dwindle_smart_split config options. */
static void dwindle_insert_with_config(DwindleNode **root, Client *new_c,
									   Client *focused, float ratio) {

	if (!new_c || !focused)
		return;

	bool as_first = false;
	bool split_h = false;
	bool lock = false;

	struct wlr_box *fg = &focused->geom;
	double fcx = fg->x + fg->width * 0.5;
	double fcy = fg->y + fg->height * 0.5;

	if (config.dwindle_smart_split) {
		double nx = (cursor->x - fcx) / (fg->width * 0.5);
		double ny = (cursor->y - fcy) / (fg->height * 0.5);

		if (fabs(ny) > fabs(nx)) {
			split_h = false;
			as_first = (ny < 0);
		} else {
			split_h = true;
			as_first = (nx < 0);
		}
		lock = true;
	} else {
		bool likely_h = (fg->width >= fg->height);
		if (likely_h) {
			if (config.dwindle_hsplit == 0)
				as_first = (cursor->x < fcx);
			else
				as_first = (config.dwindle_hsplit == 2);
		} else {
			if (config.dwindle_vsplit == 0)
				as_first = (cursor->y < fcy);
			else
				as_first = (config.dwindle_vsplit == 2);
		}
	}

	DwindleNode *target = focused ? dwindle_find_leaf(*root, focused) : NULL;
	// 防止极端情况下找不到 target
	if (!target && *root)
		target = dwindle_first_leaf(*root);

	// 计算 1/N 比例
	if (config.dwindle_manual_split && target) {
		split_h = target->custom_leaf_split_h;
		lock = true;
		as_first = false;

		// ================= 计算新节点的 1/N 比例 =================
		DwindleNode *path[128];
		float p[128];
		int path_len = get_block_path_and_ratios(target, split_h, path, p);

		int n_old = 1;
		if (path_len > 1) {
			n_old = count_block_items(path[path_len - 1], split_h);
		}
		float N = (float)(n_old + 1);

		float p_target_old = p[0];
		// 算出 split 节点在方向块里总共能分到的绝对面积比例
		float p_split_new = p_target_old * (N - 1.0f) / N + 1.0f / N;

		// 逆推传给 dwindle_insert 的局部 ratio
		if (as_first) {
			ratio = (1.0f / N) / p_split_new;
		} else {
			ratio = (p_target_old * (N - 1.0f) / N) / p_split_new;
		}

		// 安全兜底
		if (ratio < 0.001f)
			ratio = 0.001f;
		if (ratio > 0.999f)
			ratio = 0.999f;
		// =========================================================
	}

	// 调用通用插入函数
	dwindle_insert(root, new_c, focused, ratio, as_first, split_h, lock);
}

void dwindle(Monitor *m) {
	int32_t n = m->visible_tiling_clients;
	if (n == 0)
		return;

	uint32_t tag = m->pertag->curtag;
	DwindleNode **root = &m->pertag->dwindle_root[tag];
	float ratio = config.dwindle_split_ratio;

	Client *vis[512];
	int32_t count = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISTILED(c))
			vis[count++] = c;
		if (count >= 512)
			break;
	}

	{
		DwindleNode *leaves[512];
		int32_t lc = 0;

		DwindleNode *stack[1024];
		int32_t sp = 0;
		if (*root)
			stack[sp++] = *root;
		while (sp > 0) {
			DwindleNode *nd = stack[--sp];
			if (!nd->is_split) {
				leaves[lc++] = nd;
			} else {
				if (nd->second)
					stack[sp++] = nd->second;
				if (nd->first)
					stack[sp++] = nd->first;
			}
		}

		for (int32_t i = 0; i < lc; i++) {
			bool found = false;
			for (int32_t j = 0; j < count; j++)
				if (vis[j] == leaves[i]->client) {
					found = true;
					break;
				}
			if (!found)
				dwindle_remove(root, leaves[i]->client);
		}
	}

	Client *focused = focustop(m);
	if (focused && !dwindle_find_leaf(*root, focused))
		focused = m->sel;
	for (int32_t i = 0; i < count; i++) {
		if (!dwindle_find_leaf(*root, vis[i]))
			dwindle_insert_with_config(root, vis[i], focused, ratio);
	}

	int32_t gap_ih = enablegaps ? m->gappih : 0;
	int32_t gap_iv = enablegaps ? m->gappiv : 0;
	int32_t gap_oh = enablegaps ? m->gappoh : 0;
	int32_t gap_ov = enablegaps ? m->gappov : 0;
	if (config.smartgaps && n == 1)
		gap_ih = gap_iv = gap_oh = gap_ov = 0;

	dwindle_assign(*root, m->w.x + gap_oh, m->w.y + gap_ov,
				   m->w.width - 2 * gap_oh, m->w.height - 2 * gap_ov, gap_ih,
				   gap_iv);
}
