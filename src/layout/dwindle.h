typedef struct DwindleNode DwindleNode;
struct DwindleNode {
	bool is_split;
	bool split_h;
	bool split_locked;
	float ratio;
	float drag_init_ratio;
	int32_t container_x;
	int32_t container_y;
	int32_t container_w;
	int32_t container_h;
	DwindleNode *parent;
	DwindleNode *first;
	DwindleNode *second;
	Client *client;
};

static DwindleNode *dwindle_locked_h_node = NULL;
static DwindleNode *dwindle_locked_v_node = NULL;

static DwindleNode *dwindle_new_leaf(Client *c) {
	DwindleNode *n = calloc(1, sizeof(DwindleNode));
	n->client = c;
	return n;
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
						   float ratio, bool as_first) {
	DwindleNode *new_leaf = dwindle_new_leaf(new_c);

	if (!*root) {
		*root = new_leaf;
		return;
	}

	DwindleNode *target = focused ? dwindle_find_leaf(*root, focused) : NULL;
	if (!target)
		target = dwindle_first_leaf(*root);

	DwindleNode *split = calloc(1, sizeof(DwindleNode));
	split->is_split = true;
	split->ratio = ratio;
	if (as_first) {
		split->first = new_leaf;
		split->second = target;
	} else {
		split->first = target;
		split->second = new_leaf;
	}
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

	DwindleNode *sibling =
		(parent->first == leaf) ? parent->second : parent->first;
	DwindleNode *grandparent = parent->parent;
	sibling->parent = grandparent;

	/* Preserve split direction on sibling split-nodes when requested. */
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
	dwindle_insert(root, c, target, ratio, as_first);
}

static void dwindle_swap_clients(DwindleNode **root, Client *a, Client *b) {
	DwindleNode *la = dwindle_find_leaf(*root, a);
	DwindleNode *lb = dwindle_find_leaf(*root, b);
	if (!la || !lb || la == lb)
		return;
	la->client = b;
	lb->client = a;
}

static void dwindle_resize_client(Monitor *m, Client *c, int32_t dx,
								  int32_t dy) {
	uint32_t tag = m->pertag->curtag;
	DwindleNode *leaf = dwindle_find_leaf(m->pertag->dwindle_root[tag], c);
	if (!leaf)
		return;

	if (!start_drag_window) {
		start_drag_window = true;
		dwindle_locked_h_node = NULL;
		dwindle_locked_v_node = NULL;
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
	drag_begin_cursorx = cursor->x;
	drag_begin_cursory = cursor->y;

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
	bool as_first = false;

	if (focused) {
		struct wlr_box *fg = &focused->geom;
		double fcx = fg->x + fg->width * 0.5;
		double fcy = fg->y + fg->height * 0.5;

		if (config.dwindle_smart_split) {
			/* Divide the focused window into 4 triangles using its diagonals.
			 * The triangle the cursor falls in determines both the split axis
			 * and which side the new window appears on. */
			double nx = (cursor->x - fcx) / (fg->width * 0.5);
			double ny = (cursor->y - fcy) / (fg->height * 0.5);
			bool do_split_h;
			if (fabs(ny) > fabs(nx)) {
				do_split_h = false;
				as_first = (ny < 0); /* top triangle → new window on top */
			} else {
				do_split_h = true;
				as_first = (nx < 0); /* left triangle → new window on left */
			}
			dwindle_insert(root, new_c, focused, ratio, as_first);
			DwindleNode *leaf = dwindle_find_leaf(*root, new_c);
			if (leaf && leaf->parent) {
				leaf->parent->split_h = do_split_h;
				leaf->parent->split_locked = true;
			}
			return;
		}

		/* Predict likely split direction from the focused window's shape. */
		bool likely_h = (fg->width >= fg->height);
		if (likely_h) {
			if (config.dwindle_hsplit == 0)
				as_first = (cursor->x < fcx); /* follow mouse */
			else
				as_first = (config.dwindle_hsplit == 2); /* 2=left, 1=right */
		} else {
			if (config.dwindle_vsplit == 0)
				as_first = (cursor->y < fcy); /* follow mouse */
			else
				as_first = (config.dwindle_vsplit == 2); /* 2=top, 1=bottom */
		}
	}

	dwindle_insert(root, new_c, focused, ratio, as_first);
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
