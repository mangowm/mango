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
}

void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact) {
	if (!ISFAKETILED(c))
		return;

	if (!c->isfullscreen && !c->ismaximizescreen) {
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