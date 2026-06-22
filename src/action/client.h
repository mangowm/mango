static void client_swap_layout_properties(Client *c1, Client *c2) {
	// Grid 属性交换
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

	// Master / Stack 属性交换
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
	wl_list_remove(&c2->flink);
	wl_list_insert(&c1->flink, &c2->flink);

	if (config.warpcursor)
		warp_cursor(c1);
}

void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact) {
	if (!ISFAKETILED(c))
		return;

	if (c->isfullscreen && c->tab_bar_node) {
		wlr_scene_node_set_enabled(&c->tab_bar_node->scene_buffer->node, false);
	}

	if (!c->mon->isoverview && c->tab_bar_node && !c->isfullscreen &&
		(c->group_next || c->group_prev)) {
		geo.y = geo.y + config.tab_bar_height;
		geo.height -= config.tab_bar_height;
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

	if (c->swallowing || !c->mon)
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
	wlr_scene_node_lower_to_bottom(&c->jump_label_node->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->jump_label_node->scene_buffer->node, false);
}

void client_add_tab_bar_node(Client *c) {

	if (config.tab_bar_height <= 0) {
		return;
	}

	MangoCustomDecorate *MangoCustomDecorate =
		ecalloc(1, sizeof(MangoCustomDecorate));
	MangoCustomDecorate->node_data = c;
	MangoCustomDecorate->node_type = MANGO_TITLE_NODE;
	MangoCustomDecorate->type = CustomDecorate;
	uint32_t layer = c->isfloating || c->isfullscreen ? LyrTop
					 : c->ismaximizescreen			  ? LyrMaximize
													  : LyrTile;

	c->tab_bar_node = mango_tab_bar_node_create(
		MangoCustomDecorate, layers[layer], config.tabdata, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->tab_bar_node->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->tab_bar_node->scene_buffer->node, false);
	mango_tab_bar_node_update(c->tab_bar_node, client_get_title(c), 1.0);
}

void client_focus_group_member(Client *c) {
	if (!c->group_prev && !c->group_next)
		return;

	if (c->isgroupfocusing)
		return;

	if (c->mon->isoverview)
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

	if (cur_focusing) {
		cur_focusing->isgroupfocusing = false;
		client_replace(c, cur_focusing, true);
		mango_tab_bar_node_set_focus(cur_focusing->tab_bar_node, false);
	}

	c->isgroupfocusing = true;
	mango_tab_bar_node_set_focus(c->tab_bar_node, true);

	client_reparent_group(c);

	focusclient(c, 1);

	arrange(c->mon, false, false);
}

void client_check_tab_node_visible(Client *c) {

	if (!c || c->iskilling)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (!c->mon->isoverview && cur->tab_bar_node &&
			(cur->group_next || cur->group_prev) && VISIBLEON(c, c->mon) &&
			ISNORMAL(c) && !c->isfullscreen &&
			(!is_monocle_layout(c->mon) || c == c->mon->sel)) {
			wlr_scene_node_set_enabled(&cur->tab_bar_node->scene_buffer->node,
									   true);
		} else {
			wlr_scene_node_set_enabled(&cur->tab_bar_node->scene_buffer->node,
									   false);
		}
		cur = cur->group_next;
	}
}

void client_raise_group(Client *c) {
	if (!c || !c->mon)
		return;

	if (!c->group_prev && !c->group_next)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->tab_bar_node) {
			wlr_scene_node_raise_to_top(&cur->tab_bar_node->scene_buffer->node);
			wlr_scene_node_raise_to_top(&cur->scene->node);
		}
		cur = cur->group_next;
	}
}

void client_reparent_group(Client *c) {
	if (!c || !c->tab_bar_node)
		return;

	if (!c->group_prev && !c->group_next)
		return;

	int32_t layer = c->isfloating || c->isfullscreen ? LyrTop
					: c->ismaximizescreen			 ? LyrMaximize
													 : LyrTile;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->tab_bar_node) {
			wlr_scene_node_reparent(&cur->tab_bar_node->scene_buffer->node,
									layers[layer]);
			wlr_scene_node_reparent(&cur->scene->node, layers[layer]);
		}
		cur = cur->group_next;
	}
}

void client_handle_decorate_click(MangoCustomDecorate *md) {

	if (!md)
		return;

	if (md->node_type == MANGO_TITLE_NODE) {
		Client *c = md->node_data;
		client_focus_group_member(c);
	}
}