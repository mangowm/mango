static void client_swap_layout_properties(Client *c1, Client *c2) {
	// grid property swap
	double grid_col_per = c1->grid_col_per;
	double grid_row_per = c1->grid_row_per;
	int32_t grid_col_idx = c1->grid_col_idx;
	int32_t grid_row_idx = c1->grid_row_idx;

	c1->grid_col_per = c2->grid_col_per;
	c1->grid_row_per = c2->grid_row_per;
	c1->grid_col_idx = c2->grid_col_idx;
	c1->grid_row_idx = c2->grid_row_idx;

	c2->grid_col_per = grid_col_per;
	c2->grid_row_per = grid_row_per;
	c2->grid_col_idx = grid_col_idx;
	c2->grid_row_idx = grid_row_idx;

	// master / stack property swap
	double master_inner_per = c1->master_inner_per;
	double master_mfact_per = c1->master_mfact_per;
	double stack_inner_per = c1->stack_inner_per;

	c1->master_inner_per = c2->master_inner_per;
	c1->master_mfact_per = c2->master_mfact_per;
	c1->stack_inner_per = c2->stack_inner_per;

	c2->master_inner_per = master_inner_per;
	c2->master_mfact_per = master_mfact_per;
	c2->stack_inner_per = stack_inner_per;
}

static void client_swap_monitors_and_tags(Client *c1, Client *c2) {
	Monitor *tmp_mon = c2->mon;
	uint32_t tmp_tags = c2->tags;
	c2->mon = c1->mon;
	c1->mon = tmp_mon;
	c2->tags = c1->tags;
	c1->tags = tmp_tags;
}

static void finish_exchange_arrange_and_focus(Client *c1, Client *c2,
											  Monitor *m1, Monitor *m2) {
	if (m1 != m2) {
		arrange(c1->mon, false, false);
		arrange(c2->mon, false, false);
	} else {
		arrange(c1->mon, false, false);
	}

	wl_list_safe_reinsert_next(&c1->flink, &c2->flink);

	if (config.warpcursor)
		warp_cursor(c1);
}

void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact) {
	if (!ISFAKETILED(c) || !c->mon)
		return;

	if (!c->mon->isoverview && !c->isfullscreen &&
		(c->group_next || c->group_prev)) {
		geo.y = geo.y + config.group_bar_height;
		geo.height -= config.group_bar_height;
	}

	if ((!c->isfullscreen && !c->ismaximizescreen) ||
		is_scroller_layout(c->mon)) {
		resize(c, geo, interact);
	}
}

static uint32_t next_client_id = 0;
uint32_t generate_client_id(void) { return ++next_client_id; }

void client_active(Client *c) {
	uint32_t target;

	if (client_is_unmanaged(c)) {
		focusclient(c, 1);
		return;
	}

	if (c->swallowdby || !c->mon)
		return;

	if (c->isminimized) {
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		c->is_scratchpad_show = 0;
		setborder_color(c);
		show_hide_client(c);
		arrange(c->mon, true, false);
		return;
	}

	target = get_tags_first_tag(c->tags);
	view_in_mon(&(Arg){.ui = target}, true, c->mon, true);
	focusclient(c, 1);
}

void client_pending_force_kill(Client *c) {
	if (!c)
		return;
	kill(c->pid, SIGKILL);
}

void client_add_jump_label_node(Client *c) {
	c->jump_label_node =
		mango_jump_label_node_create(c->scene, config.jumplabeldata);
	if (!c->jump_label_node)
		return;
	/* overview 里 label 要显示在卡片树之上 */
	if (c->ov_card_tree)
		wlr_scene_node_raise_to_top(&c->jump_label_node->scene_buffer->node);
	else
		wlr_scene_node_lower_to_bottom(&c->jump_label_node->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->jump_label_node->scene_buffer->node, false);
}

void client_add_group_bar(Client *c) {

	if (config.group_bar_height <= 0) {
		return;
	}

	uint32_t layer = c->isoverlay						? LyrOverlay
					 : c->isfloating || c->isfullscreen ? LyrTop
					 : c->ismaximizescreen				? LyrMaximize
														: LyrTile;

	c->group_bar = mango_group_bar_create(c, GroupBar, layers[layer],
										  config.groupbardata, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->group_bar->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->group_bar->scene_buffer->node, false);
	mango_group_bar_update(c->group_bar, client_get_title(c),
						   c->mon	? c->mon->wlr_output->scale
						   : selmon ? selmon->wlr_output->scale
									: 1.0f);
}

void client_focus_group_member(Client *c) {
	if (!c->group_prev && !c->group_next)
		return;

	if (c->isgroupfocusing)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur_focusing = NULL;
	while (head) {
		if (head->isgroupfocusing) {
			cur_focusing = head;
			break;
		}
		head = head->group_next;
	}

	if (!cur_focusing || !cur_focusing->mon)
		return;

	if (cur_focusing && cur_focusing->mon->isoverview)
		return;

	cur_focusing->isgroupfocusing = false;
	c->mon = cur_focusing->mon;
	client_replace(c, cur_focusing, true, false);
	mango_group_bar_set_focus(cur_focusing->group_bar, false);

	c->isgroupfocusing = true;
	mango_group_bar_set_focus(c->group_bar, true);

	client_reparent_group(c);

	focusclient(c, 1);

	arrange(c->mon, false, false);
}

void client_check_tab_node_visible(Client *c) {

	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (!c->mon->isoverview && cur->group_bar &&
			(cur->group_next || cur->group_prev) && TAGMATCH(c, c->mon) &&
			ISNORMAL(c) && !c->isfullscreen) {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   true);
		} else {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   false);
		}
		cur = cur->group_next;
	}
}

void client_raise_group(Client *c) {
	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_raise_to_top(&cur->group_bar->scene_buffer->node);
		}
		wlr_scene_node_raise_to_top(&cur->scene->node);
		cur = cur->group_next;
	}
}

void client_reparent_group(Client *c) {
	if (!c || !c->mon)
		return;

	int32_t layer = c->isoverlay					   ? LyrOverlay
					: c->isfloating || c->isfullscreen ? LyrTop
					: c->ismaximizescreen			   ? LyrMaximize
													   : LyrTile;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_reparent(&cur->group_bar->scene_buffer->node,
									layers[layer]);
		}
		wlr_scene_node_reparent(&cur->scene->node, layers[layer]);
		cur = cur->group_next;
	}
}

void client_handle_decorate_click(MangoGroupBar *gb) {

	if (!gb)
		return;

	if (gb->node_data) {
		Client *c = gb->node_data;
		client_focus_group_member(c);
	}
}

void client_set_group_mon(Client *c, Monitor *m) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		client_change_mon(cur, m);
		cur = cur->group_next;
	}
}

void client_set_group_config(Client *c) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->jump_label_node)
			mango_jump_label_node_apply_config(cur->jump_label_node,
											   &config.jumplabeldata);
		wlr_scene_rect_set_color(cur->droparea, config.dropcolor);
		wlr_scene_rect_set_color(cur->splitindicator[0], config.splitcolor);
		wlr_scene_rect_set_color(cur->splitindicator[1], config.splitcolor);
		mango_group_bar_apply_config(cur->group_bar, &config.groupbardata);
		cur = cur->group_next;
	}
}

void client_group_detach(Client *c) {
	if (c->group_prev)
		c->group_prev->group_next = c->group_next;
	if (c->group_next)
		c->group_next->group_prev = c->group_prev;
	c->group_prev = NULL;
	c->group_next = NULL;
	c->isgroupfocusing = false;
}

void client_group_replace(Client *old, Client *new) {
	client_group_detach(new);

	new->group_prev = old->group_prev;
	new->group_next = old->group_next;
	if (old->group_prev)
		old->group_prev->group_next = new;
	if (old->group_next)
		old->group_next->group_prev = new;
	old->group_prev = NULL;
	old->group_next = NULL;

	if (old->is_logic_hide || (!new->group_prev && !new->group_next)) {
		new->isgroupfocusing = false;
	} else {
		new->isgroupfocusing = old->isgroupfocusing;
	}
}

void mango_surface_frame_done(struct wlr_surface *surface, int sx, int sy,
							  void *data) {
	(void)sx;
	(void)sy;
	wlr_surface_send_frame_done(surface, data);
}

// 给被隐藏窗口的所有 surface（含 subsurface）喂 frame callback，
// 让客户端在 overview 预览中继续渲染（解除帧回调节流导致的停画）。
// 不能用 wlr_scene_node_for_each_buffer 遍历原 scene_surface 树：
// 该树在拍完快照后被 disabled，scenefx 的 for_each_buffer 会直接跳过
// disabled 节点（wlr_scene.c scene_node_for_each_scene_buffer），导致
// 一个 surface 都喂不到——普通窗口就会因收不到 frame callback 而停画。
void client_send_frame_done(Client *c, const struct timespec *now) {
	struct wlr_surface *s = client_surface(c);
	if (!s)
		return;
	wlr_surface_for_each_surface(s, mango_surface_frame_done, (void *)now);
}

bool client_force_render(Client *c) {
	if (!c || !c->mon || c->iskilling || !client_surface(c)->mapped ||
		c->scene->node.enabled)
		return false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	client_send_frame_done(c, &now);
	return true;
}