/*
 * Client 管理：窗口生命周期、规则、聚焦、平铺/浮动/全屏状态切换，
 * 以及 XWayland 客户端相关处理。
 */

static void client_update_geometry(Client *c) {
	if (client_is_x11(c)) {
#ifdef XWAYLAND
		/* 先确定 xwayland_scale 再取几何，否则拿到的是物理尺寸 */
		xwayland_apply_scale(c);
		client_get_geometry(c, &c->geom);
		if (c->isfloating) {
			fix_xwayland_coordinate(&c->geom);
			c->float_geom = c->geom;
		}
#endif
	}
}

static void client_init_xwayland(Client *c) {
	if (client_is_x11(c)) {
#ifdef XWAYLAND
		/* 记录 XWayland 根 buffer 节点 */
		struct wlr_scene_node *child;
		wl_list_for_each(child, &c->scene_surface->children, link) {
			if (child->type != WLR_SCENE_NODE_BUFFER)
				continue;
			struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(child);
			if (wlr_scene_surface_try_from_buffer(buffer)) {
				c->xwl_root_buffer = buffer;
				/* scene 命中检测需要把逻辑坐标换算成 X11 物理坐标 */
				c->xwl_root_buffer->point_accepts_input =
					xwayland_scene_buffer_point_accepts_input;
				break;
			}
		}
		/* scene 处理后强制根 surface 显示逻辑尺寸 */
		LISTEN(&client_surface(c)->events.commit, &c->commmitx11, commitx11);
#endif
	}
}

static bool client_init_unmanaged(Client *c) {
	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		/* Unmanaged clients always are floating */
		xwayland_apply_scale(c);
		/* 应用 scale 后重新计算 c->geom（逻辑尺寸）*/
		client_get_geometry(c, &c->geom);
		struct wlr_box geo = c->geom;
		fix_xwayland_coordinate(&geo);
		struct wlr_box xgeo = geo;
		xwayland_logical_to_x11(&xgeo, c->xwayland_scale);
		wlr_scene_node_set_position(&c->scene->node, geo.x, geo.y);
		wlr_xwayland_surface_configure(c->surface.xwayland, xgeo.x, xgeo.y,
									   xgeo.width, xgeo.height);
		/* 立即按 buffer 实际尺寸设置 dest_size（逻辑尺寸 = buffer/scale），
		 * 避免弹出第一帧以物理尺寸显示导致内容被缩放，再等 commit 才纠正 */
		client_update_xwayland_dest_size(c);
		LISTEN(&c->surface.xwayland->events.set_geometry, &c->set_geometry,
			   setgeometrynotify);
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		return true;
#endif
	}
	return false;
}

static void client_apply_xwayland(Client *c) {
	if (client_is_x11(c)) {
#ifdef XWAYLAND
		/* applyrules/setmon 之后 c->mon 才确定，此时应用 XWayland 缩放 */
		xwayland_apply_scale(c);
		c->overview_backup_geom = c->geom;
#endif
	}
}

#ifdef XWAYLAND
static bool
xwayland_scene_buffer_point_accepts_input(struct wlr_scene_buffer *buffer,
										  double *sx, double *sy) {
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface)
		return false;

	double tx = *sx, ty = *sy;
	struct wlr_scene_node *node = &buffer->node;
	while (node && !node->data)
		node = node->parent ? &node->parent->node : NULL;
	Client *c = node ? node->data : NULL;
	if (c && client_is_x11(c)) {
#ifdef XWAYLAND
		if (config.xwayland_ignore_scale && c->xwayland_scale > 0.f) {
			tx *= c->xwayland_scale;
			ty *= c->xwayland_scale;
		}
#endif
	}
	return wlr_surface_point_accepts_input(scene_surface->surface, tx, ty);
}
#endif

static void view_insert_shift_tags(Monitor *m, uint32_t target) {
	Client *c;
	uint32_t map[tag_num_MAX + 1] = {0};
	uint32_t i;

	if (!m || target < 1 || target >= (uint32_t)config.tag_num)
		return;

	if (get_tag_status((uint32_t)config.tag_num, m))
		return;

	for (i = 1; i <= (uint32_t)config.tag_num; i++) {
		if (i < target)
			map[i] = i;
		else if (i < (uint32_t)config.tag_num)
			map[i] = i + 1;
		else
			map[i] = i;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon != m || c->iskilling || c->is_logic_hide)
			continue;
		c->tags = tag_remap_mask(c->tags, map);
	}

	m->tagset[m->seltags] = tag_remap_mask(m->tagset[m->seltags], map);
	m->tagset[m->seltags ^ 1] = tag_remap_mask(m->tagset[m->seltags ^ 1], map);
	m->ovbk_current_tagset = tag_remap_mask(m->ovbk_current_tagset, map);
	m->ovbk_prev_tagset = tag_remap_mask(m->ovbk_prev_tagset, map);

	if (m->pertag->curtag <= (uint32_t)config.tag_num && map[m->pertag->curtag])
		m->pertag->curtag = map[m->pertag->curtag];
	if (m->pertag->prevtag <= (uint32_t)config.tag_num &&
		map[m->pertag->prevtag])
		m->pertag->prevtag = map[m->pertag->prevtag];

	for (i = (uint32_t)config.tag_num - 1; i >= target; i--) {
		tag_gather_move_pertag(m, i + 1, i);
	}
}

void client_change_mon(Client *c, Monitor *m) {
	setmon(c, m, c->tags, true);
	if (c->isfloating) {
		c->float_geom = c->geom =
			setclient_coordinate_center(c, c->mon, c->geom, 0, 0);
	}
}

void applybounds(Client *c, struct wlr_box *bbox) {
	/* set minimum possible */
	c->geom.width = MANGO_MAX(1 + 2 * (int32_t)c->bw, c->geom.width);
	c->geom.height = MANGO_MAX(1 + 2 * (int32_t)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

void clear_fullscreen_and_maximized_state(Monitor *m) {
	Client *fc = NULL;
	wl_list_for_each(fc, &clients, link) {
		if (fc && VISIBLEON(fc, m) && ISFULLSCREEN(fc)) {
			clear_fullscreen_flag(fc);
		}
	}
}

/*清除全屏标志,还原全屏时清0的border*/
void clear_fullscreen_flag(Client *c) {

	if ((c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id ==
			 SCROLLER ||
		 c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id ==
			 VERTICAL_SCROLLER) &&
		!c->isfloating) {
		return;
	}

	if (c->isfullscreen) {
		setfullscreen(c, false, true);
	}

	if (c->ismaximizescreen) {
		setmaximizescreen(c, 0, true);
	}
}

void client_pending_fullscreen_state(Client *c, int32_t isfullscreen) {
	c->isfullscreen = isfullscreen;

	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_fullscreen(c->foreign_toplevel,
													  isfullscreen);
}

void client_pending_maximized_state(Client *c, int32_t ismaximized) {
	c->ismaximizescreen = ismaximized;
	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_maximized(c->foreign_toplevel,
													 ismaximized);
}

void client_pending_minimized_state(Client *c, int32_t isminimized) {
	c->isminimized = isminimized;
	if (c->foreign_toplevel && !c->iskilling)
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,
													 isminimized);
}

void show_scratchpad(Client *c) {
	c->is_scratchpad_show = 1;
	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(c, 0);
		client_pending_maximized_state(c, 0);
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	/* return if fullscreen */
	if (!c->isfloating) {
		setfloating(c, 1);
		c->geom.width = c->iscustomsize
							? c->float_geom.width
							: c->mon->w.width * config.scratchpad_width_ratio;
		c->geom.height =
			c->iscustomsize ? c->float_geom.height
							: c->mon->w.height * config.scratchpad_height_ratio;
		// 重新计算居中的坐标
		c->float_geom = c->geom = c->animainit_geom = c->animation.current =
			setclient_coordinate_center(c, c->mon, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->geom, 0);
	}

	c->oldtags = c->mon->tagset[c->mon->seltags];
	wl_list_safe_reinsert_next(&clients, &c->link);
	show_hide_client(c);
	setborder_color(c);
}

void client_update_oldmonname_record(Client *c, Monitor *m) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	strncpy(c->oldmonname, m->wlr_output->name, sizeof(c->oldmonname) - 1);
	c->oldmonname[sizeof(c->oldmonname) - 1] = '\0';
}

void client_replace(Client *c, Client *w, bool is_group_change_member,
					bool is_swallow) {
	c->bw = w->bw;
	c->isfloating = w->isfloating;
	c->isurgent = w->isurgent;
	c->is_in_scratchpad = w->is_in_scratchpad;
	c->is_scratchpad_show = w->is_scratchpad_show;
	c->tags = w->tags;
	c->geom = w->geom;
	c->float_geom = w->float_geom;
	c->stack_inner_per = w->stack_inner_per;
	c->master_inner_per = w->master_inner_per;
	c->master_mfact_per = w->master_mfact_per;
	c->scroller_proportion = w->scroller_proportion;
	c->isglobal = w->isglobal;
	c->overview_backup_geom = w->overview_backup_geom;
	c->animation.current = w->animation.current;
	c->stack_proportion = w->stack_proportion;
	c->is_logic_hide = w->is_logic_hide;

	if (is_swallow || !is_group_change_member) {
		client_group_replace(w, c);
	}

	w->is_logic_hide = true;
	mango_group_bar_set_focus(c->group_bar, c->isgroupfocusing);

	/* 若旧窗口处于 overview：销毁其卡片树 */
	overview_destroy_card(w);
	if (w->overview_scene_surface) {
		w->overview_scene_surface = NULL;
	}

	if (c->mon && c->mon->isoverview) {
		overview_backup_surface(c);
	}

	if (w->group_bar && !is_group_change_member) {
		wlr_scene_node_set_enabled(&w->group_bar->scene_buffer->node, false);
	}

	if (w->jump_label_node) {
		wlr_scene_node_set_enabled(&w->jump_label_node->scene_buffer->node,
								   false);
	}

	wl_list_safe_reinsert_next(&w->link, &c->link);
	wl_list_safe_reinsert_prev(&w->flink, &c->flink);
	wlr_scene_node_set_enabled(&w->scene->node, false);

	if (!c->is_logic_hide) {
		wlr_scene_node_set_enabled(&c->scene->node, true);
		/* overview 中真实 surface 树由卡片树替代，保持禁用 */
		if (!c->ov_card_tree)
			wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	}

	if (w->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_output_leave(w->foreign_toplevel,
													w->mon->wlr_output);
		wlr_foreign_toplevel_handle_v1_destroy(w->foreign_toplevel);
		w->foreign_toplevel = NULL;
	}

	if (!c->foreign_toplevel && c->mon)
		add_foreign_toplevel(c);
	else if (c->foreign_toplevel && c->mon) {
		wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
													c->mon->wlr_output);
	}

	client_pending_fullscreen_state(c, w->isfullscreen);
	client_pending_maximized_state(c, w->ismaximizescreen);
	client_pending_minimized_state(c, w->isminimized);

	if (!w->mon)
		return;

	const Layout *layout = w->mon->pertag->ltidxs[w->mon->pertag->curtag];

	if (layout->id == DWINDLE || layout->id == SCROLLER ||
		layout->id == VERTICAL_SCROLLER) {

		for (uint32_t t = 0; t < (uint32_t)config.tag_num + 1; t++) {
			/* dwindle */

			if (layout->id == DWINDLE) {

				DwindleNode **root = &w->mon->pertag->dwindle_root[t];
				dwindle_remove(root, c);
				DwindleNode *dnode = dwindle_find_leaf(*root, w);
				if (dnode)
					dnode->client = c;
			}

			// scroller
			if (layout->id == SCROLLER || layout->id == VERTICAL_SCROLLER) {
				struct TagScrollerState *st = w->mon->pertag->scroller_state[t];
				if (!st)
					continue;
				struct ScrollerStackNode *cn = find_scroller_node(st, c);
				if (cn)
					scroller_node_remove(st, cn);

				struct ScrollerStackNode *wn = find_scroller_node(st, w);
				if (wn)
					wn->client = c;
			}
		}
	}

	/* 同步当前活动 tag 的全局客户端字段 */
	if (layout->id == SCROLLER || layout->id == VERTICAL_SCROLLER) {
		sync_scroller_state_to_clients(w->mon, w->mon->pertag->curtag);
	}
}

bool switch_scratchpad_client_state(Client *c) {

	if (config.scratchpad_cross_monitor && selmon && c->mon != selmon &&
		c->is_in_scratchpad) {
		// 保存原始monitor用于尺寸计算
		Monitor *oldmon = c->mon;
		c->scratchpad_switching_mon = true;
		c->mon = selmon;
		reset_foreign_tolevel(c, oldmon, c->mon);
		client_update_oldmonname_record(c, selmon);

		// 根据新monitor调整窗口尺寸
		c->float_geom.width =
			(int32_t)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
		c->float_geom.height = (int32_t)(c->float_geom.height *
										 c->mon->w.height / oldmon->w.height);

		c->float_geom =
			setclient_coordinate_center(c, c->mon, c->float_geom, 0, 0);

		// 只有显示状态的scratchpad才需要聚焦和返回true
		if (c->is_scratchpad_show) {
			c->tags = get_tags_first_tag(selmon->tagset[selmon->seltags]);
			resize(c, c->float_geom, 0);
			arrange(selmon, false, false);
			focusclient(c, 1);
			c->scratchpad_switching_mon = false;
			return true;
		} else {
			resize(c, c->float_geom, 0);
			c->scratchpad_switching_mon = false;
		}
	}

	if (c->is_in_scratchpad && c->is_scratchpad_show && c->mon &&
		(c->mon->tagset[c->mon->seltags] & c->tags) == 0) {
		c->tags = c->mon->tagset[c->mon->seltags];
		arrange(c->mon, false, false);
		focusclient(c, 1);
		return true;
	} else if (c->is_in_scratchpad && c->is_scratchpad_show &&
			   (c->mon->tagset[c->mon->seltags] & c->tags) != 0) {
		set_minimized(c);
		return true;
	} else if (c && c->is_in_scratchpad && !c->is_scratchpad_show) {
		show_scratchpad(c);
		return true;
	}

	return false;
}

void apply_named_scratchpad(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {

		if (!config.scratchpad_cross_monitor && c->mon != selmon) {
			continue;
		}

		if (config.single_scratchpad && c->is_in_scratchpad &&
			c->is_scratchpad_show && c != target_client) {
			set_minimized(c);
		}
	}

	if (!target_client->is_in_scratchpad) {
		set_minimized(target_client);
		switch_scratchpad_client_state(target_client);
	} else
		switch_scratchpad_client_state(target_client);
}

static void apply_rule_properties(Client *c, const ConfigWinRule *r) {
	APPLY_INT_PROP(c, r, isterm);
	APPLY_INT_PROP(c, r, allow_csd);
	APPLY_INT_PROP(c, r, force_fakemaximize);
	APPLY_INT_PROP(c, r, force_tiled_state);
	APPLY_INT_PROP(c, r, force_tearing);
	APPLY_INT_PROP(c, r, noswallow);
	APPLY_INT_PROP(c, r, nofocus);
	APPLY_INT_PROP(c, r, nofadein);
	APPLY_INT_PROP(c, r, nofadeout);
	APPLY_INT_PROP(c, r, no_force_center);
	APPLY_INT_PROP(c, r, isfloating);
	APPLY_INT_PROP(c, r, isfullscreen);
	APPLY_INT_PROP(c, r, isfakefullscreen);
	APPLY_INT_PROP(c, r, isnoborder);
	APPLY_INT_PROP(c, r, isnoshadow);
	APPLY_INT_PROP(c, r, isnoradius);
	APPLY_INT_PROP(c, r, isnoanimation);
	APPLY_INT_PROP(c, r, isopensilent);
	APPLY_INT_PROP(c, r, istagsilent);
	APPLY_INT_PROP(c, r, isnamedscratchpad);
	APPLY_INT_PROP(c, r, isglobal);
	APPLY_INT_PROP(c, r, isoverlay);
	APPLY_INT_PROP(c, r, shield_when_capture);
	APPLY_INT_PROP(c, r, ignore_maximize);
	APPLY_INT_PROP(c, r, ignore_minimize);
	APPLY_INT_PROP(c, r, isnosizehint);
	APPLY_INT_PROP(c, r, idleinhibit_when_focus);
	APPLY_INT_PROP(c, r, vrr_only_fullscreen);
	APPLY_INT_PROP(c, r, force_render);
	APPLY_INT_PROP(c, r, activation_bypass);
	APPLY_INT_PROP(c, r, isunglobal);
	APPLY_INT_PROP(c, r, noblur);
	APPLY_INT_PROP(c, r, allow_shortcuts_inhibit);

	APPLY_FLOAT_PROP(c, r, scroller_proportion);
	APPLY_FLOAT_PROP(c, r, scroller_proportion_single);
	APPLY_FLOAT_PROP(c, r, focused_opacity);
	APPLY_FLOAT_PROP(c, r, unfocused_opacity);

	APPLY_STRING_PROP(c, r, animation_type_open);
	APPLY_STRING_PROP(c, r, animation_type_close);
}

void set_float_malposition(Client *tc) {
	Client *c = NULL;
	int32_t x, y, offset, xreverse, yreverse;
	x = tc->geom.x;
	y = tc->geom.y;
	xreverse = 1;
	yreverse = 1;

	if (!tc || !tc->mon)
		return;

	offset = MANGO_MIN(tc->mon->w.width / 20, tc->mon->w.height / 20);

	wl_list_for_each(c, &clients, link) {
		if (c->isfloating && c != tc && VISIBLEON(c, tc->mon) &&
			abs(x - c->geom.x) < offset && abs(y - c->geom.y) < offset) {

			x = c->geom.x + offset * xreverse;
			y = c->geom.y + offset * yreverse;
			if (x < tc->mon->w.x) {
				x = x + offset;
				xreverse = 1;
			}

			if (y < tc->mon->w.y) {
				y = y + offset;
				yreverse = 1;
			}

			if (x + tc->geom.width > tc->mon->w.x + tc->mon->w.width) {
				x = x - offset;
				xreverse = -1;
			}

			if (y + tc->geom.height > tc->mon->w.y + tc->mon->w.height) {
				y = y - offset;
				yreverse = -1;
			}
		}
	}

	tc->float_geom.x = tc->geom.x = x;
	tc->float_geom.y = tc->geom.y = y;
}

void client_reset_mon_tags(Client *c, Monitor *mon, uint32_t newtags) {
	if (!newtags && mon && !mon->isoverview) {
		c->tags = mon->tagset[mon->seltags];
	} else if (!newtags && mon && mon->isoverview) {
		c->tags = mon->ovbk_current_tagset;
	} else if (newtags) {
		uint32_t masked = newtags & TAGMASK;
		c->tags = masked ? masked : mon->tagset[mon->seltags];
	} else {
		c->tags = mon->tagset[mon->seltags];
	}
}

void check_match_tag_floating_rule(Client *c, Monitor *mon) {
	if (c->tags && !c->isfloating && mon && !c->swallowing &&
		mon->pertag->open_as_floating[get_tags_first_tag_num(c->tags)]) {
		c->isfloating = 1;
	}
}

void applyrules(Client *c) {
	/* rule matching */
	const char *appid, *title;
	uint32_t i, newtags = 0;
	const ConfigWinRule *r;
	Monitor *m = NULL;
	Client *fc = NULL;
	Client *parent = NULL;

	if (!c)
		return;

	parent = client_get_parent(c);

	Monitor *mon = parent && parent->mon ? parent->mon : selmon;

	c->isfloating = client_is_float_type(c) || parent;

	client_update_geometry(c);

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (i = 0; i < config.window_rules_count; i++) {

		r = &config.window_rules[i];

		// rule matching
		if (!is_window_rule_matches(r, appid, title))
			continue;

		// set general properties
		apply_rule_properties(c, r);

		// // set tags
		if (r->tags > 0) {
			newtags |= r->tags;
		} else if (parent) {
			newtags = parent->tags;
		}

		// set monitor of client
		wl_list_for_each(m, &mons, link) {
			if (match_monitor_spec(r->monitor, m)) {
				mon = m;
			}
		}

		if (c->isnamedscratchpad) {
			c->isfloating = 1;
		}

		if (r->scroller_proportion > 0.0f) {
			c->iscustom_scroller_proportion = 1;
		}

		if (r->scroller_proportion_single > 0.0f) {
			c->iscustom_scroller_proportion_single = 1;
		}

		// set geometry of floating client

		if (r->width > 1)
			c->float_geom.width = r->width;
		else if (r->width > 0 && r->width <= 1)
			c->float_geom.width = round(mon->m.width * r->width);
		if (r->height > 1)
			c->float_geom.height = r->height;
		else if (r->height > 0 && r->height <= 1)
			c->float_geom.height = round(mon->m.height * r->height);

		if (r->width > 0 || r->height > 0) {
			c->iscustomsize = 1;
		}

		if (r->offsetx || r->offsety) {
			c->iscustompos = 1;
			c->float_geom = c->geom = setclient_coordinate_center(
				c, mon, c->float_geom, r->offsetx, r->offsety);
		}
		if (c->isfloating) {
			c->geom = c->float_geom.width > 0 && c->float_geom.height > 0
						  ? c->float_geom
						  : c->geom;
			if (!c->isnosizehint)
				client_set_size_bound(c);
		}
	}

	if (mon)
		set_size_per(mon, c);

	// if no geom rule hit and is normal winodw, use the center pos and record
	// the hit size
	if (!c->iscustompos &&
		(!client_is_x11(c) || (c->geom.x == 0 && c->geom.y == 0))) {
		struct wlr_box pending_center_geom =
			c->iscustomsize ? c->float_geom : c->geom;
		c->float_geom = c->geom =
			setclient_coordinate_center(c, mon, pending_center_geom, 0, 0);
	} else if (!c->iscustomsize) {
		c->float_geom = c->geom;
	}

	/*-----------------------apply rule action-------------------------*/

	// rule action only apply after map not apply in the init commit
	struct wlr_surface *surface = client_surface(c);
	if (!surface || !surface->mapped)
		return;

	// apply swallow rule
	c->pid = client_get_pid(c);
	if (!c->noswallow && !c->isfloating && !client_is_float_type(c) &&
		!c->surface.xdg->initial_commit) {
		Client *p = termforwin(c);
		if (p && !p->isminimized) {
			c->swallowing = p;
			p->swallowdby = c;

			client_replace(c, p, false, true);

			mon = p->mon;
			newtags = p->tags;
		}
	}

	int32_t fullscreen_state_backup =
		c->isfullscreen || client_wants_fullscreen(c);

	bool should_init_get_focus =
		!c->isopensilent &&
		!(client_is_x11_popup(c) && client_should_ignore_focus(c)) && mon &&
		(!c->istagsilent || !newtags || newtags & mon->tagset[mon->seltags]);

	if (!should_init_get_focus) {
		wl_list_safe_reinsert_prev(&fstack, &c->flink);
	}

	setmon(c, mon, newtags, should_init_get_focus);

	if (!c->isfloating) {
		c->old_stack_inner_per = c->stack_inner_per;
		c->old_master_inner_per = c->master_inner_per;
	}

	if (c->mon &&
		!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]) &&
		!c->isopensilent && !c->istagsilent) {
		c->animation.tag_from_rule = true;
		view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
	}

	setfullscreen(c, fullscreen_state_backup, true);

	if (c->isfakefullscreen) {
		setfakefullscreen(c, 1);
	}

	/*
	if there is a new non-floating window in the current tag, the fullscreen
	window in the current tag will exit fullscreen and participate in tiling
	*/
	wl_list_for_each(fc, &clients,
					 link) if (fc && fc != c && c->tags & fc->tags && c->mon &&
							   VISIBLEON(fc, c->mon) && ISFULLSCREEN(fc) &&
							   !c->isfloating) {
		clear_fullscreen_flag(fc);
		arrange(c->mon, false, false);
	}

	if (c->isfloating && !c->iscustompos && !c->isnamedscratchpad) {
		wl_list_safe_reinsert_prev(&clients, &c->link);
		set_float_malposition(c);
	}

	// apply named scratchpad rule
	if (c->isnamedscratchpad) {
		apply_named_scratchpad(c);
	}

	// apply overlay rule
	if (c->isoverlay && c->scene) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&c->scene->node);
	}
}

void apply_window_snap(Client *c) {
	int32_t snap_up = 99999, snap_down = 99999, snap_left = 99999,
			snap_right = 99999;
	int32_t snap_up_temp = 0, snap_down_temp = 0, snap_left_temp = 0,
			snap_right_temp = 0;
	int32_t snap_up_screen = 0, snap_down_screen = 0, snap_left_screen = 0,
			snap_right_screen = 0;
	int32_t snap_up_mon = 0, snap_down_mon = 0, snap_left_mon = 0,
			snap_right_mon = 0;

	uint32_t cbw = !render_border || c->fake_no_border ? config.borderpx : 0;
	uint32_t tcbw;
	uint32_t cx, cy, cw, ch, tcx, tcy, tcw, tch;
	cx = c->geom.x + cbw;
	cy = c->geom.y + cbw;
	cw = c->geom.width - 2 * cbw;
	ch = c->geom.height - 2 * cbw;

	Client *tc = NULL;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (!c->isfloating || !config.enable_floating_snap)
		return;

	wl_list_for_each(tc, &clients, link) {
		if (tc && tc->isfloating && !tc->iskilling &&
			client_surface(tc)->mapped && VISIBLEON(tc, c->mon)) {

			tcbw = !render_border || tc->fake_no_border ? config.borderpx : 0;
			tcx = tc->geom.x + tcbw;
			tcy = tc->geom.y + tcbw;
			tcw = tc->geom.width - 2 * tcbw;
			tch = tc->geom.height - 2 * tcbw;

			snap_left_temp = cx - tcx - tcw;
			snap_right_temp = tcx - cx - cw;
			snap_up_temp = cy - tcy - tch;
			snap_down_temp = tcy - cy - ch;

			if (snap_left_temp < snap_left && snap_left_temp >= 0) {
				snap_left = snap_left_temp;
			}
			if (snap_right_temp < snap_right && snap_right_temp >= 0) {
				snap_right = snap_right_temp;
			}
			if (snap_up_temp < snap_up && snap_up_temp >= 0) {
				snap_up = snap_up_temp;
			}
			if (snap_down_temp < snap_down && snap_down_temp >= 0) {
				snap_down = snap_down_temp;
			}
		}
	}

	snap_left_mon = cx - c->mon->m.x;
	snap_right_mon = c->mon->m.x + c->mon->m.width - cx - cw;
	snap_up_mon = cy - c->mon->m.y;
	snap_down_mon = c->mon->m.y + c->mon->m.height - cy - ch;

	if (snap_up_mon >= 0 && snap_up_mon < snap_up)
		snap_up = snap_up_mon;
	if (snap_down_mon >= 0 && snap_down_mon < snap_down)
		snap_down = snap_down_mon;
	if (snap_left_mon >= 0 && snap_left_mon < snap_left)
		snap_left = snap_left_mon;
	if (snap_right_mon >= 0 && snap_right_mon < snap_right)
		snap_right = snap_right_mon;

	snap_left_screen = cx - c->mon->w.x;
	snap_right_screen = c->mon->w.x + c->mon->w.width - cx - cw;
	snap_up_screen = cy - c->mon->w.y;
	snap_down_screen = c->mon->w.y + c->mon->w.height - cy - ch;

	if (snap_up_screen >= 0 && snap_up_screen < snap_up)
		snap_up = snap_up_screen;
	if (snap_down_screen >= 0 && snap_down_screen < snap_down)
		snap_down = snap_down_screen;
	if (snap_left_screen >= 0 && snap_left_screen < snap_left)
		snap_left = snap_left_screen;
	if (snap_right_screen >= 0 && snap_right_screen < snap_right)
		snap_right = snap_right_screen;

	if (snap_left < snap_right && snap_left < config.snap_distance) {
		c->geom.x = c->geom.x - snap_left;
	}

	if (snap_right <= snap_left && snap_right < config.snap_distance) {
		c->geom.x = c->geom.x + snap_right;
	}

	if (snap_up < snap_down && snap_up < config.snap_distance) {
		c->geom.y = c->geom.y - snap_up;
	}

	if (snap_down <= snap_up && snap_down < config.snap_distance) {
		c->geom.y = c->geom.y + snap_down;
	}

	c->float_geom = c->geom;
	resize(c, c->geom, 0);
}

void commitnotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commit);
	struct wlr_box *new_geo;

	/* overview 卡片节点是独立的 scene_surface，提交后自动更新，无需在此处理 */

	if (c->surface.xdg->initial_commit) {
		// xdg client will first enter this before mapnotify
		init_client_properties(c);
		applyrules(c);
		if (c->mon) {
			client_set_scale(client_surface(c), c->mon->wlr_output->scale);
		}
		setmon(c, NULL, 0,
			   true); /* Make sure to reapply rules in mapnotify() */

		uint32_t serial = wlr_xdg_surface_schedule_configure(c->surface.xdg);
		if (serial > 0) {
			c->configure_serial = serial;
		}

		uint32_t wm_caps = WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;

		if (!c->ignore_minimize)
			wm_caps |= WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;

		if (!c->ignore_maximize)
			wm_caps |= WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;

		wlr_xdg_toplevel_set_wm_capabilities(c->surface.xdg->toplevel, wm_caps);

		if (c->mon) {
			wlr_xdg_toplevel_set_bounds(c->surface.xdg->toplevel,
										c->mon->w.width - 2 * c->bw,
										c->mon->w.height - 2 * c->bw);
		}

		if (c->decoration)
			requestdecorationmode(&c->set_decoration_mode, c->decoration);
		return;
	}

	if (!c || c->iskilling || c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining)
		return;

	if (c->configure_serial &&
		c->configure_serial <= c->surface.xdg->current.configure_serial)
		c->configure_serial = 0;

	if (!c->dirty) {
		new_geo = &c->surface.xdg->geometry;
		c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
				   new_geo->height != c->geom.height - 2 * c->bw ||
				   new_geo->x != 0 || new_geo->y != 0;
	}

	if (c == grabc || !c->dirty)
		return;

	resize(c, c->geom, 0);

	new_geo = &c->surface.xdg->geometry;
	c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
			   new_geo->height != c->geom.height - 2 * c->bw ||
			   new_geo->x != 0 || new_geo->y != 0;
}

void // fix for 0.5
createnotify(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;

	/* Allocate a Client for this surface */
	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = config.borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->events.destroy, &c->destroy, destroynotify);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&toplevel->events.request_minimize, &c->minimize, minimizenotify);
	LISTEN(&toplevel->events.set_title, &c->set_title, updatetitle);
}

void // 0.7 custom
destroynotify(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	/* decoration 监听器挂在 deco->events 上，toplevel 销毁时（wlroots 会
	 * 同步销毁 decoration 资源并发 destroy 信号）client 可能先被释放，
	 * 不摘掉会导致 decoration destroy 信号遍历到已释放的监听器而崩溃 */
	if (c->decoration) {
		wl_list_remove(&c->destroy_decoration.link);
		wl_list_remove(&c->set_decoration_mode.link);
	}
	free(c);
}

void focusclient(Client *c, int32_t lift) {

	Client *last_focus_client = NULL;
	Monitor *um = NULL;

	struct wlr_surface *old_keyboard_focus_surface =
		seat->keyboard_state.focused_surface;

	if (locked)
		return;

	if (c && c->iskilling)
		return;

	if (c && !client_surface(c)->mapped)
		return;

	if (c && client_should_ignore_focus(c) && client_is_x11_popup(c))
		return;

	if (c && c->nofocus)
		return;

	/* Raise client in stacking order if requested */
	if (c && lift) {
		client_raise_group(c);
	}

	if (c && client_surface(c) == old_keyboard_focus_surface && selmon &&
		selmon->sel)
		return;

	if (selmon && selmon->sel && selmon->sel != c &&
		selmon->sel->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			selmon->sel->foreign_toplevel, false);
	}

	if (c && !c->iskilling && !client_is_unmanaged(c) && c->mon) {

		last_focus_client = selmon ? selmon->sel : NULL;
		selmon = c->mon;
		selmon->prevsel = selmon->sel;
		selmon->sel = c;
		c->isfocusing = true;

		check_keep_idle_inhibit(c);
		check_vrr_enable(c);

		if (last_focus_client && !last_focus_client->iskilling &&
			last_focus_client != c) {
			last_focus_client->isfocusing = false;
			client_set_unfocused_opacity_animation(last_focus_client);
		}

		client_set_focused_opacity_animation(c);

		// decide whether need to re-arrange

		// change focus link position
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);

		if (c && selmon->prevsel &&
			(selmon->prevsel->tags & selmon->tagset[selmon->seltags]) &&
			(c->tags & selmon->tagset[selmon->seltags]) && !c->isfloating &&
			(is_scroller_layout(selmon) || is_monocle_layout(selmon))) {
			arrange(selmon, false, false);
		}

		// change border color
		c->isurgent = 0;
	}

	// update other monitor focus disappear
	wl_list_for_each(um, &mons, link) {
		if (um->wlr_output->enabled && um != selmon && um->sel &&
			!um->sel->iskilling && um->sel->isfocusing) {

			um->sel->isfocusing = false;
			client_set_unfocused_opacity_animation(um->sel);

			if (um->sel->foreign_toplevel) {
				wlr_foreign_toplevel_handle_v1_set_activated(
					um->sel->foreign_toplevel, false);
			}
		}
	}

	if (c && !c->iskilling && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);

	/* Deactivate old client if focus is changing */
	if (old_keyboard_focus_surface &&
		(!c || client_surface(c) != old_keyboard_focus_surface)) {
		/* If an exclusive_focus layer is focused, don't focus or activate
		 * the client, but only update its position in fstack to render its
		 * border with focuscolor and focus it after the exclusive_focus
		 * layer is closed. */
		Client *w = NULL;
		LayerSurface *l = NULL;
		int32_t type =
			toplevel_from_wlr_surface(old_keyboard_focus_surface, &w, &l);
		if (type == LayerShell && l->scene->node.enabled &&
			l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			l == exclusive_focus) {
			return;
		} else if (w && w == exclusive_focus && client_wants_focus(w)) {
			return;
			/* Don't deactivate old_keyboard_focus_surface client if the new
			 * one wants focus, as this causes issues with winecfg and
			 * probably other clients */
		} else if (w && !client_is_unmanaged(w) &&
				   (!c || !client_wants_focus(c))) {
			client_activate_surface(old_keyboard_focus_surface, 0);
		}
	}
	printstatus(IPC_WATCH_ARRANGGE);

	if (!c) {

		if (selmon && selmon->sel &&
			(!VISIBLEON(selmon->sel, selmon) || selmon->sel->iskilling ||
			 !client_surface(selmon->sel)->mapped))
			selmon->sel = NULL;

		// clear text input focus state
		mango_im_relay_set_focus(mango_input_method_relay, NULL);
		wlr_seat_keyboard_notify_clear_focus(seat);
		check_vrr_enable(c);
		if (active_constraint) {
			cursorconstrain(NULL);
		}
		return;
	}

	/* Change cursor surface */
	motionnotify(0, NULL, 0, 0, 0, 0);

	// set text input focus
	// must before client_notify_enter,
	// otherwise the position of text_input will be wrong.
	mango_im_relay_set_focus(mango_input_method_relay, client_surface(c));

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);

	if (active_constraint && active_constraint->surface != client_surface(c)) {
		cursorconstrain(NULL);
	}

	struct wlr_pointer_constraint_v1 *constraint;
	wl_list_for_each(constraint, &pointer_constraints->constraints, link) {
		if (constraint->surface == client_surface(c)) {
			cursorconstrain(constraint);
			break;
		}
	}
}

void // 0.6
fullscreennotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, fullscreen);

	if (!c || c->iskilling)
		return;

	setfullscreen(c, client_wants_fullscreen(c), true);
}

void pending_kill_client(Client *c) {
	if (!c || c->iskilling)
		return;
	client_send_close(c);
}

static void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
								   int32_t sy, void *user_data) {
	Client *c = user_data;

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	struct wlr_surface *surface = scene_surface->surface;
	/* we dont blur subsurfaces */
	if (wlr_subsurface_try_from_wlr_surface(surface) != NULL)
		return;

	if (config.blur && c && !c->noblur) {
		if (config.blur_optimized) {
			wlr_scene_blur_set_should_only_blur_bottom_layer(c->blur, true);
		} else {
			wlr_scene_blur_set_should_only_blur_bottom_layer(c->blur, false);
		}
	}
}

void init_client_properties(Client *c) {
#ifdef XWAYLAND
	c->xwl_req_valid = false;
	c->xwl_req_x = 0;
	c->xwl_req_y = 0;
	c->xwl_req_w = 0;
	c->xwl_req_h = 0;
#endif
	c->blur_opacity = 1.0f;
	c->is_logic_hide = false;
	c->isgroupfocusing = false;
	c->group_prev = NULL;
	c->group_next = NULL;
	c->grid_col_per = 1.0f;
	c->grid_row_per = 1.0f;
	c->jump_label_node = NULL;
	c->group_bar = NULL;
	c->overview_scene_surface = NULL;
	c->drop_direction = UNDIR;
	c->enable_drop_area_draw = false;
	c->isfocusing = false;
	c->isfloating = 0;
	c->isfakefullscreen = 0;
	c->isnoanimation = 0;
	c->isopensilent = 0;
	c->istagsilent = 0;
	c->noswallow = 0;
	c->isterm = 0;
	c->noblur = 0;
	c->tearing_hint = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->overview_isfloatingbak = 0;
	c->pid = 0;
	c->swallowdby = NULL;
	c->swallowing = NULL;
	c->ismaster = 0;
	c->old_ismaster = 0;
	c->isleftstack = 0;
	c->ismaximizescreen = 0;
	c->isfullscreen = 0;
	c->need_float_size_reduce = 0;
	c->iskilling = 0;
	c->isglobal = 0;
	c->isminimized = 0;
	c->isoverlay = 0;
	c->isunglobal = 0;
	c->is_in_scratchpad = 0;
	c->isnamedscratchpad = 0;
	c->is_scratchpad_show = 0;
	c->need_float_size_reduce = 0;
	c->is_clip_to_hide = 0;
	c->is_restoring_from_ov = 0;
	c->isurgent = 0;
	c->need_output_flush = 0;
	c->scroller_proportion = config.scroller_default_proportion;
	c->is_pending_open_animation = true;
	c->drag_to_tile = false;
	c->scratchpad_switching_mon = false;
	c->fake_no_border = false;
	c->focused_opacity = config.focused_opacity;
	c->unfocused_opacity = config.unfocused_opacity;
	c->nofocus = 0;
	c->nofadein = 0;
	c->nofadeout = 0;
	c->no_force_center = 0;
	c->isnoborder = 0;
	c->isnosizehint = 0;
	c->isnoradius = 0;
	c->isnoshadow = 0;
	c->ignore_maximize = 1;
	c->ignore_minimize = 1;
	c->iscustomsize = 0;
	c->iscustompos = 0;
	c->iscustom_scroller_proportion = 0;
	c->iscustom_scroller_proportion_single = 0;
	c->master_mfact_per = 0.0f;
	c->master_inner_per = 0.0f;
	c->stack_inner_per = 0.0f;
	c->old_stack_inner_per = 0.0f;
	c->old_master_inner_per = 0.0f;
	c->old_master_mfact_per = 0.0f;
	c->isterm = 0;
	c->allow_csd = 0;
	c->force_fakemaximize = 0;
	c->force_tiled_state = 1;
	c->force_tearing = 0;
	c->allow_shortcuts_inhibit = SHORTCUTS_INHIBIT_ENABLE;
	c->idleinhibit_when_focus = 0;
	c->vrr_only_fullscreen = 0;
	/* unmap 时若在 overview，先销毁卡片树，避免泄漏 */
	overview_destroy_card(c);
	c->ov_card_tree = NULL;
	wl_list_init(&c->ov_card_surfaces);
	c->force_render = 0;
	c->activation_bypass = 0;
	c->scroller_proportion_single = 0.0f;
	c->float_geom.width = 0;
	c->float_geom.height = 0;
	c->float_geom.x = 0;
	c->float_geom.y = 0;
	c->stack_proportion = 0.0f;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	memcpy(c->opacity_animation.initial_border_color, config.bordercolor,
		   sizeof(c->opacity_animation.initial_border_color));
	memcpy(c->opacity_animation.current_border_color, config.bordercolor,
		   sizeof(c->opacity_animation.current_border_color));
	c->opacity_animation.initial_opacity = c->unfocused_opacity;
	c->opacity_animation.current_opacity = c->unfocused_opacity;
	c->animation.tagining = false;
	c->animation.running = false;
	c->animation.overining = false;
	c->animation.overview_enter_anim_set = false;
	c->animation.tagouting = false;
	c->animation.tagouted = false;
	wl_list_init(&c->link);
	wl_list_init(&c->flink);
}

void // old fix to 0.5
mapnotify(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *at_client = NULL;
	Client *c = wl_container_of(listener, c, map);
	int32_t i = 0;

	c->id = generate_client_id();

	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_init_xwayland(c);

	if (!client_is_x11(c))
		client_get_geometry(c, &c->geom);

	if (client_is_x11(c))
		init_client_properties(c);

	// set special window properties
	if (client_is_unmanaged(c) || client_is_x11_popup(c)) {
		c->bw = 0;
		c->isnoborder = 1;
	} else {
		c->bw = config.borderpx;
	}

	if (client_should_global(c)) {
		c->isunglobal = 1;
	}

	// init client geom
	if (!client_is_x11(c)) {
		c->geom.width += 2 * c->bw;
		c->geom.height += 2 * c->bw;
		c->overview_backup_geom = c->geom;
	}

	struct wlr_ext_foreign_toplevel_handle_v1_state foreign_toplevel_state = {
		.app_id = client_get_appid(c),
		.title = client_get_title(c),
	};

	c->image_capture_scene = wlr_scene_create();
	c->ext_foreign_toplevel = wlr_ext_foreign_toplevel_handle_v1_create(
		foreign_toplevel_list, &foreign_toplevel_state);
	c->ext_foreign_toplevel->data = c;
	c->image_capture_scene_surface = wlr_scene_surface_create(
		&c->image_capture_scene->tree, client_surface(c));

	/* Handle unmanaged clients first so we can return prior create borders
	 */
	if (client_init_unmanaged(c))
		return;
	// extra node

	for (i = 0; i < 2; i++) {
		c->splitindicator[i] = wlr_scene_rect_create(
			c->scene, 0, 0,
			c->isurgent ? config.urgentcolor : config.splitcolor);
		c->splitindicator[i]->node.data = c;
		wlr_scene_node_lower_to_bottom(&c->splitindicator[i]->node);
		wlr_scene_node_set_enabled(&c->splitindicator[i]->node, false);
	}

	client_add_group_bar(c);

	c->droparea = wlr_scene_rect_create(c->scene, 0, 0, config.dropcolor);
	wlr_scene_node_lower_to_bottom(&c->droparea->node);
	wlr_scene_node_set_position(&c->droparea->node, 0, 0);
	wlr_scene_node_set_enabled(&c->droparea->node, false);

	c->border = wlr_scene_rect_create(
		c->scene, 0, 0, c->isurgent ? config.urgentcolor : config.bordercolor);
	wlr_scene_node_lower_to_bottom(&c->border->node);
	wlr_scene_node_set_position(&c->border->node, 0, 0);
	wlr_scene_rect_set_corner_radii(c->border,
									corner_radii_all(config.border_radius));
	wlr_scene_node_set_enabled(&c->border->node, true);

	c->shadow =
		wlr_scene_shadow_create(c->scene, 0, 0, config.border_radius,
								config.shadows_blur, config.shadowscolor);

	c->blur = wlr_scene_blur_create(c->scene, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->blur->node);

	wlr_scene_node_lower_to_bottom(&c->shadow->node);
	wlr_scene_node_set_enabled(&c->shadow->node, true);

	c->shield =
		wlr_scene_rect_create(c->scene, 0, 0, (float[4]){0, 0, 0, 0xff});
	c->shield->node.data = c;
	wlr_scene_node_lower_to_bottom(&c->shield->node);
	wlr_scene_node_set_enabled(&c->shield->node, false);

	if (config.new_is_master && selmon && !is_scroller_layout(selmon))
		// tile at the top
		wl_list_insert(&clients, &c->link); // 新窗口是master,头部入栈
	else if (selmon && is_scroller_layout(selmon) &&
			 selmon->visible_scroll_tiling_clients > 0) {

		if (selmon->sel && ISSCROLLTILED(selmon->sel) &&
			VISIBLEON(selmon->sel, selmon)) {
			at_client = scroll_get_stack_tail_client(selmon->sel);
		} else {
			at_client = center_tiled_select(selmon);
		}

		if (at_client) {
			wl_list_insert(&at_client->link, &c->link);
		} else {
			wl_list_insert(clients.prev, &c->link); // 尾部入栈
		}
	} else
		wl_list_insert(clients.prev, &c->link); // 尾部入栈

	wl_list_insert(&fstack, &c->flink);

	applyrules(c);

	client_apply_xwayland(c);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	}

	// apply buffer effects of client
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   iter_xdg_scene_buffers, c);
	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);

	// set border color
	setborder_color(c);

	if (c->mon && c->mon->isoverview) {
		overview_backup_surface(c);
	}

	// make sure the animation is open type
	c->is_pending_open_animation = true;
	resize(c, c->geom, 0);
	printstatus(IPC_WATCH_ARRANGGE);
}

void maximizenotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, maximize);

	if (!c || !c->mon || c->iskilling || c->ignore_maximize)
		return;

	if (!client_is_x11(c) && !c->surface.xdg->initialized) {
		return;
	}

	if (client_request_maximize(c, data)) {
		setmaximizescreen(c, 1, true);
	} else {
		setmaximizescreen(c, 0, true);
	}
}

void unminimize(Client *c) {
	if (c && c->is_in_scratchpad && c->is_scratchpad_show) {
		client_pending_minimized_state(c, 0);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		return;
	}

	if (c && c->isminimized) {
		show_hide_client(c);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		arrange(c->mon, false, false);
		return;
	}
}

void set_minimized(Client *c) {

	if (!c || !c->mon || c == grabc)
		return;

	c->isglobal = 0;
	c->oldtags = c->mon->tagset[c->mon->seltags];
	c->mini_restore_tag = c->tags;
	c->tags = 0;
	client_pending_minimized_state(c, 1);
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	focusclient(focustop(selmon), 1);
	arrange(c->mon, false, false);

	if (c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,
													 false);

	wl_list_remove(&c->link);				// 从原来位置移除
	wl_list_insert(clients.prev, &c->link); // 插入尾部
}

void minimizenotify(struct wl_listener *listener, void *data) {

	Client *c = wl_container_of(listener, c, minimize);

	if (!c || !c->mon || c->iskilling || c->isminimized)
		return;

	if (client_request_minimize(c, data) && !c->ignore_minimize) {
		if (!c->isminimized)
			set_minimized(c);
		client_set_minimized(c, true);
	} else {
		if (c->isminimized)
			unminimize(c);
		client_set_minimized(c, false);
	}
}

void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data) {
	wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}

void client_set_opacity(Client *c, double opacity) {
	opacity = CLAMP_FLOAT(opacity, 0.0f, 1.0f);
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_opacity, &opacity);
}

void setborder_color(Client *c) {
	if (!c || !c->mon)
		return;

	float *border_color = get_border_color(c);
	memcpy(c->opacity_animation.target_border_color, border_color,
		   sizeof(c->opacity_animation.target_border_color));
	client_set_border_color(c, border_color);
}

void exchange_two_client(Client *c1, Client *c2) {
	if (c1 == NULL || c2 == NULL ||
		(!config.exchange_cross_monitor && c1->mon != c2->mon)) {
		return;
	}

	Monitor *m1 = c1->mon;
	Monitor *m2 = c2->mon;
	const Layout *layout1 = m1->pertag->ltidxs[m1->pertag->curtag];
	const Layout *layout2 = m2->pertag->ltidxs[m2->pertag->curtag];

	if (layout1->id == SCROLLER || layout2->id == SCROLLER ||
		layout1->id == VERTICAL_SCROLLER || layout2->id == VERTICAL_SCROLLER) {
		exchange_two_scroller_clients(c1, c2);
		return;
	}

	if (layout1->id == DWINDLE && layout2->id == DWINDLE) {
		dwindle_swap_clients(c1, c2);
		return;
	}

	client_swap_layout_properties(c1, c2);

	wl_list_swap(&c1->link, &c2->link);

	if (m1 != m2) {
		client_swap_monitors_and_tags(c1, c2);
	}

	finish_exchange_arrange_and_focus(c1, c2, m1, m2);
}

void // 0.5
setfloating(Client *c, int32_t floating) {

	Client *fc = NULL;
	struct wlr_box target_box;
	int32_t old_floating_state = c->isfloating;
	c->isfloating = floating;
	bool window_size_outofrange = false;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	target_box = c->geom;

	if (floating == 1 && c != grabc) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		client_pending_maximized_state(c, 0);
		exit_scroller_stack(c);

		// 重新计算居中的坐标
		if (!client_is_x11(c) && !c->iscustompos)
			target_box =
				setclient_coordinate_center(c, c->mon, target_box, 0, 0);
		else
			target_box = c->geom;

		// restore to the memeroy geom
		if (c->float_geom.width > 0 && c->float_geom.height > 0) {
			if (c->mon &&
				c->float_geom.width >= c->mon->w.width - config.gappoh) {
				c->float_geom.width = c->mon->w.width * 0.9;
				window_size_outofrange = true;
			}
			if (c->mon &&
				c->float_geom.height >= c->mon->w.height - config.gappov) {
				c->float_geom.height = c->mon->w.height * 0.9;
				window_size_outofrange = true;
			}
			if (window_size_outofrange) {
				c->float_geom =
					setclient_coordinate_center(c, c->mon, c->float_geom, 0, 0);
			}
			resize(c, c->float_geom, 0);
		} else {
			resize(c, target_box, 0);
		}

		c->need_float_size_reduce = 0;
	} else if (c->isfloating && c == grabc) {
		c->need_float_size_reduce = 0;
	} else {
		c->need_float_size_reduce = 1;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		// 让当前tag中的全屏窗口退出全屏参与平铺
		wl_list_for_each(fc, &clients,
						 link) if (fc && fc != c && VISIBLEON(fc, c->mon) &&
								   c->tags & fc->tags && ISFULLSCREEN(fc) &&
								   old_floating_state) {
			clear_fullscreen_flag(fc);
		}
	}

	client_reparent_group(c);

	if (c->isfloating) {
		set_size_per(c->mon, c);
	}

	if (!c->force_fakemaximize)
		client_set_maximized(c, false);

	if (!c->isfloating || c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);
	} else {
		client_set_tiled(c, WLR_EDGE_NONE);
	}

	arrange(c->mon, false, false);

	if (!c->isfloating) {
		c->old_master_inner_per = c->master_inner_per;
		c->old_stack_inner_per = c->stack_inner_per;
	}

	setborder_color(c);
	printstatus(IPC_WATCH_ARRANGGE);
}

void reset_maximizescreen_size(Client *c) {
	struct wlr_box geom;
	geom.x = c->mon->w.x + config.gappoh;
	geom.y = c->mon->w.y + config.gappov;
	geom.width = c->mon->w.width - 2 * config.gappoh;
	geom.height = c->mon->w.height - 2 * config.gappov;

	if (c->group_next || c->group_prev) {
		geom.height -= config.group_bar_height;
		geom.y += config.group_bar_height;
	}

	resize(c, geom, 0);
}

void exit_scroller_stack(Client *c) {
	if (!c || !c->mon)
		return;

	uint32_t tag = c->mon->pertag->curtag;
	struct TagScrollerState *st = c->mon->pertag->scroller_state[tag];
	if (st) {
		struct ScrollerStackNode *n = find_scroller_node(st, c);
		if (n) {
			scroller_node_remove(st, n);
			return;
		}
	}
}

void setmaximizescreen(Client *c, int32_t maximizescreen, bool rearrange) {
	struct wlr_box maximizescreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling ||
		c == grabc)
		return;

	if (c->mon->isoverview)
		return;

	client_pending_maximized_state(c, maximizescreen);

	if (maximizescreen) {

		if (c->isfullscreen) {
			client_pending_fullscreen_state(c, 0);
			client_set_fullscreen(c, 0);
		}

		exit_scroller_stack(c);

		maximizescreen_box.x = c->mon->w.x + config.gappoh;
		maximizescreen_box.y = c->mon->w.y + config.gappov;
		maximizescreen_box.width = c->mon->w.width - 2 * config.gappoh;
		maximizescreen_box.height = c->mon->w.height - 2 * config.gappov;

		if (c->group_next || c->group_prev) {
			maximizescreen_box.height -= config.group_bar_height;
			maximizescreen_box.y += config.group_bar_height;
		}

		wlr_scene_node_raise_to_top(&c->scene->node);
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, maximizescreen_box, 0);
	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			setfloating(c, 1);
	}

	client_reparent_group(c);

	if (!c->force_fakemaximize && !c->ismaximizescreen) {
		client_set_maximized(c, false);
	} else if (!c->force_fakemaximize && c->ismaximizescreen) {
		client_set_maximized(c, true);
	}

	if (rearrange)
		arrange(c->mon, false, false);
}

void setfakefullscreen(Client *c, int32_t fakefullscreen) {
	c->isfakefullscreen = fakefullscreen;
	if (!c->mon)
		return;

	if (c->isfullscreen)
		setfullscreen(c, 0, true);

	client_set_fullscreen(c, fakefullscreen);
}

void setfullscreen(Client *c, int32_t fullscreen,
				   bool rearrange) // 用自定义全屏代理自带全屏
{

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling ||
		c == grabc)
		return;

	if (c->mon->isoverview)
		return;

	c->isfullscreen = fullscreen;

	client_set_fullscreen(c, fullscreen);
	client_pending_fullscreen_state(c, fullscreen);

	if (fullscreen) {

		if (c->ismaximizescreen && !c->force_fakemaximize) {
			client_set_maximized(c, false);
		}

		client_pending_maximized_state(c, 0);

		exit_scroller_stack(c);
		c->isfakefullscreen = 0;

		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
		if (!is_scroller_layout(c->mon) || c->isfloating)
			resize(c, c->mon->m, 1);

	} else {
		c->bw = c->isnoborder ? 0 : config.borderpx;
		if (c->isfloating)
			setfloating(c, 1);
	}

	client_reparent_group(c);
	check_vrr_enable(c);

	if (rearrange)
		arrange(c->mon, false, false);
}

void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus) {
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	if (oldmon && oldmon->sel == c) {
		oldmon->sel = NULL;
	}

	if (oldmon && oldmon->prevsel == c) {
		oldmon->prevsel = NULL;
	}

	c->mon = m;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon, false, false);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		reset_foreign_tolevel(c, oldmon, m);
		resize(c, c->geom, 0);
		client_reset_mon_tags(c, m, newtags);
		check_match_tag_floating_rule(c, m);
		setfloating(c, c->isfloating);
		setfullscreen(c, c->isfullscreen,
					  true); /* This will call arrange(c->mon) */
	}

	if (focus && !client_is_x11_popup(c)) {
		focusclient(focustop(selmon), 1);
	}
}

void show_hide_client(Client *c) {
	uint32_t target = 1;

	if (c->mon)
		set_size_per(c->mon, c);
	target = get_tags_first_tag(c->oldtags);

	if (!c->is_in_scratchpad) {
		tag_client(&(Arg){.ui = target}, c);
	} else {
		c->tags = c->oldtags;
		c->isminimized = 0;
		if (c->mon)
			arrange(c->mon, false, false);
	}
	client_pending_minimized_state(c, 0);
	focusclient(c, 1);

	if (c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);
}

void tag_client(const Arg *arg, Client *target_client) {
	Client *fc = NULL;
	if (target_client && arg->ui & TAGMASK) {

		target_client->tags = arg->ui & TAGMASK;

		wl_list_for_each(fc, &clients, link) {
			if (fc && fc != target_client && target_client->tags & fc->tags &&
				ISFULLSCREEN(fc) && !target_client->isfloating) {
				clear_fullscreen_flag(fc);
			}
		}
		view(&(Arg){.ui = arg->ui, .i = arg->i}, true);

	} else {
		view(arg, true);
	}

	focusclient(target_client, 1);
	printstatus(IPC_WATCH_ARRANGGE);
}

void unmapnotify(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown.
	 */
	Client *c = wl_container_of(listener, c, unmap);
	Monitor *m = NULL;
	Client *nextfocus = NULL;
	c->iskilling = 1;
	struct ScrollerStackNode *target_node =
		c->mon ? find_scroller_node(
					 c->mon->pertag->scroller_state[c->mon->pertag->curtag], c)
			   : NULL;
	struct ScrollerStackNode *prev_node =
		target_node ? target_node->prev_in_stack : NULL;
	struct ScrollerStackNode *next_node =
		target_node ? target_node->next_in_stack : NULL;

	if (config.animations && !c->is_clip_to_hide && !c->isminimized &&
		(!c->mon || VISIBLEON(c, c->mon)))
		init_fadeout_client(c);

	// If the client is in a stack, remove it from the stack

	if (c->swallowing) {
		c->swallowing->mon = c->mon;
		client_replace(c->swallowing, c, false, true);
	} else if ((c->group_next || c->group_prev) && c->isgroupfocusing) {
		Client *group_replacement =
			c->group_next ? c->group_next : c->group_prev;
		group_replacement->mon = c->mon;
		client_replace(group_replacement, c, false, false);
	} else {
		scroller_remove_client(c);
		dwindle_remove_client(c);
	}

	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
		if (dropc) {
			dropc->enable_drop_area_draw = false;
			client_set_drop_area(dropc);
			dropc = NULL;
		}
	}

	if (c == dropc) {
		dropc = NULL;
	}

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		if (c == m->sel) {
			m->sel = NULL;
		}
		if (c == m->prevsel) {
			m->prevsel = NULL;
		}
	}

	if (c->mon && c->mon == selmon) {
		if (next_node && !c->swallowing) {
			nextfocus = next_node->client;
		} else if (prev_node && !c->swallowing) {
			nextfocus = prev_node->client;
		} else {
			nextfocus = focustop(selmon);
		}

		if (nextfocus) {
			focusclient(nextfocus, 1);
		}

		if (!nextfocus && selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}
	}

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		if (c->commmitx11.link.prev && c->commmitx11.link.next &&
			c->commmitx11.link.prev != &c->commmitx11.link) {
			wl_list_remove(&c->commmitx11.link);
			wl_list_init(&c->commmitx11.link);
		}
	}
#endif

	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			if (c->set_geometry.link.prev && c->set_geometry.link.next &&
				c->set_geometry.link.prev != &c->set_geometry.link) {
				wl_list_remove(&c->set_geometry.link);
				wl_list_init(&c->set_geometry.link);
			}
		}
#endif
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {

		client_group_detach(c);

		wl_list_remove(&c->link);
		setmon(c, NULL, 0, true);
		wl_list_remove(&c->flink);
	}

	if (c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	if (c->ext_foreign_toplevel) {
		wlr_ext_foreign_toplevel_handle_v1_destroy(c->ext_foreign_toplevel);
		c->ext_foreign_toplevel = NULL;
	}

	if (c->swallowing) {
		setmaximizescreen(c->swallowing, c->ismaximizescreen, true);
		setfullscreen(c->swallowing, c->isfullscreen, true);
		c->swallowing->swallowdby = NULL;
		c->swallowing = NULL;
	}

	if (c->swallowdby) {
		c->swallowdby->swallowing = NULL;
		c->swallowdby = NULL;
	}

	if (c->jump_label_node) {
		mango_jump_label_node_destroy(c->jump_label_node);
		c->jump_label_node = NULL;
	}

	if (c->group_bar) {
		mango_group_bar_destroy(c->group_bar);
		c->group_bar = NULL;
	}

	if (c->image_capture_tree) {
		wlr_scene_node_destroy(&c->image_capture_tree->node);
		c->image_capture_tree = NULL;
	}
	if (c->image_capture_scene) {
		wlr_scene_node_destroy(&c->image_capture_scene->tree.node);
		c->image_capture_scene = NULL;
	}

	c->image_capture_source = NULL;
	init_client_properties(c);

	wlr_scene_node_destroy(&c->scene->node);
	printstatus(IPC_WATCH_ARRANGGE);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void updatetitle(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_title);

	if (!c || c->iskilling)
		return;

	const char *title;
	title = client_get_title(c);
	mango_group_bar_update(c->group_bar, title,
						   c->mon ? c->mon->wlr_output->scale : 1.0f);
	if (title && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel, title);
	if (title && c->ext_foreign_toplevel) {
		wlr_ext_foreign_toplevel_handle_v1_update_state(
			c->ext_foreign_toplevel,
			&(struct wlr_ext_foreign_toplevel_handle_v1_state){
				.title = title,
				.app_id = c->ext_foreign_toplevel->app_id,
			});
	}
	if (c == focustop(c->mon))
		printstatus(IPC_WATCH_ARRANGGE);
}

void // 17 fix to 0.5
urgent(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	if (config.focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
		focusclient(c, 1);
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			setborder_color(c);
		printstatus(IPC_WATCH_ARRANGGE);
	}
}

void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
				 bool changefocus) {
	uint32_t i, tmptag;

	if (!m || (arg->ui != (~0 & TAGMASK) && m->isoverview)) {
		return;
	}

	if (arg->ui == 0) {
		return;
	}

	if (arg->ui == UINT32_MAX) {
		if (m->tagset[0] != m->tagset[1]) {
			m->pertag->prevtag = get_tags_first_tag_num(m->tagset[m->seltags]);
			m->seltags ^= 1; /* toggle sel tagset */
			m->pertag->curtag = get_tags_first_tag_num(m->tagset[m->seltags]);
			goto toggleseltags;
		} else {
			return;
		}
	}

	if ((m->tagset[m->seltags] & arg->ui & TAGMASK) != 0) {
		want_animation = false;
	}

	m->seltags ^= 1; /* toggle sel tagset */

	if (arg->ui & TAGMASK) {
		m->tagset[m->seltags] = arg->ui & TAGMASK;
		tmptag = m->pertag->curtag;

		if (arg->ui == (~0 & TAGMASK))
			m->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i) && i < (uint32_t)config.tag_num &&
						arg->ui != 0;
				 i++)
				;
			m->pertag->curtag = i >= (uint32_t)config.tag_num
									? (uint32_t)config.tag_num
									: i + 1;
		}

		m->pertag->prevtag =
			tmptag == m->pertag->curtag ? m->pertag->prevtag : tmptag;
	} else {
		tmptag = m->pertag->prevtag;
		m->pertag->prevtag = m->pertag->curtag;
		m->pertag->curtag = tmptag;
	}

toggleseltags:

	if (changefocus)
		focusclient(focustop(m), 1);
	arrange(m, want_animation, true);
	printstatus(IPC_WATCH_ARRANGGE);
}

void view(const Arg *arg, bool want_animation) {
	Monitor *m = NULL;
	if (arg->i) {
		view_in_mon(arg, want_animation, selmon, true);
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled || m == selmon)
				continue;
			// only arrange, not change monitor focus
			view_in_mon(arg, want_animation, m, false);
		}
	} else {
		view_in_mon(arg, want_animation, selmon, true);
	}
}

#ifdef XWAYLAND
/* 获取当前 XWayland 客户端的 monitor（尚未绑定 monitor 时回退到 selmon） */
static Monitor *xwayland_monitor(Client *c) {
	Monitor *m = c ? c->mon : NULL;
	if (!m)
		m = selmon;
	return m;
}
#endif

#ifdef XWAYLAND
/* X11 坐标相对逻辑坐标的缩放：fzs 时为 monitor scale，否则为 1 */
static float xwayland_client_scale(Client *c) {
	if (config.xwayland_ignore_scale) {
		Monitor *m = xwayland_monitor(c);
		/* 用浮点 scale 保证窗口 1:1 精确显示 */
		return m ? m->wlr_output->scale : 1.0f;
	}
	return 1.0f;
}
#endif

#ifdef XWAYLAND
/* 提示 X11 客户端按何分辨率渲染 */
static float xwayland_preferred_scale(Client *c) {
	if (config.xwayland_ignore_scale)
		return 1.0f;
	Monitor *m = xwayland_monitor(c);
	return m ? m->wlr_output->scale : 1.0f;
}
#endif

#ifdef XWAYLAND
/* 更新 XWayland 缩放并通知客户端 */
static void xwayland_apply_scale(Client *c) {
	if (!client_is_x11(c) || !client_surface(c))
		return;
	c->xwayland_scale = xwayland_client_scale(c);
	client_set_scale(client_surface(c), xwayland_preferred_scale(c));
}
#endif

#ifdef XWAYLAND
/* wayland 逻辑坐标 -> X11 物理尺寸（X11 = 逻辑 * scale） */
static void xwayland_logical_to_x11(struct wlr_box *box, float scale) {
	if (scale <= 0.f)
		scale = 1.f;
	box->x = (int32_t)roundf(box->x * scale);
	box->y = (int32_t)roundf(box->y * scale);
	box->width = (int32_t)roundf(box->width * scale);
	box->height = (int32_t)roundf(box->height * scale);
}
#endif

#ifdef XWAYLAND
/* X11 物理尺寸 -> wayland 逻辑坐标（逻辑 = X11 / scale） */
static void xwayland_x11_to_logical(struct wlr_box *box, float scale) {
	if (scale <= 0.f)
		scale = 1.f;
	box->x = (int32_t)roundf(box->x / scale);
	box->y = (int32_t)roundf(box->y / scale);
	box->width = (int32_t)roundf(box->width / scale);
	box->height = (int32_t)roundf(box->height / scale);
}
#endif

#ifdef XWAYLAND
void fix_xwayland_coordinate(struct wlr_box *geom) {
	if (!selmon)
		return;

	// 1. 如果窗口已经在当前活动显示器内，直接返回
	if (geom->x >= selmon->m.x && geom->x <= selmon->m.x + selmon->m.width &&
		geom->y >= selmon->m.y && geom->y <= selmon->m.y + selmon->m.height)
		return;

	geom->x = selmon->m.x + (selmon->m.width - geom->width) / 2;
	geom->y = selmon->m.y + (selmon->m.height - geom->height) / 2;
}
#endif

#ifdef XWAYLAND
void activatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, activate);
	bool need_arrange = false;

	if (!c || c->iskilling || !c->foreign_toplevel || client_is_unmanaged(c))
		return;

	if (c && c->swallowdby)
		return;

	if (c->isminimized) {
		client_pending_minimized_state(c, 0);
		c->tags = c->mini_restore_tag;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		if (VISIBLEON(c, c->mon)) {
			need_arrange = true;
		}
	}

	if (config.focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
		focusclient(c, 1);
		need_arrange = true;
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			setborder_color(c);
	}

	if (need_arrange) {
		arrange(c->mon, false, false);
	}

	printstatus(IPC_WATCH_ARRANGGE);
}
#endif

#ifdef XWAYLAND
void configurex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	struct wlr_box new_geo;
	new_geo.x = event->x;
	new_geo.y = event->y;
	new_geo.width = event->width;
	new_geo.height = event->height;
	/* event 是 X11 物理尺寸，转回 wayland 逻辑坐标 */
	xwayland_x11_to_logical(&new_geo, c->xwayland_scale);
	fix_xwayland_coordinate(&new_geo);

	if (!client_surface(c) || !client_surface(c)->mapped) {
		struct wlr_box xgeo = new_geo;
		xwayland_logical_to_x11(&xgeo, c->xwayland_scale);
		wlr_xwayland_surface_configure(c->surface.xwayland, xgeo.x, xgeo.y,
									   xgeo.width, xgeo.height);
		return;
	}

	if (client_is_unmanaged(c)) {
		struct wlr_box xgeo = new_geo;
		xwayland_logical_to_x11(&xgeo, c->xwayland_scale);
		wlr_scene_node_set_position(&c->scene->node, new_geo.x, new_geo.y);
		wlr_xwayland_surface_configure(c->surface.xwayland, xgeo.x, xgeo.y,
									   xgeo.width, xgeo.height);
		return;
	}

	if (c->isfloating && c != grabc) {
		new_geo.x = new_geo.x - c->bw;
		new_geo.y = new_geo.y - c->bw;
		new_geo.width = new_geo.width + c->bw * 2;
		new_geo.height = new_geo.height + c->bw * 2;
		fix_xwayland_coordinate(&new_geo);

		resize(c,
			   (struct wlr_box){.x = new_geo.x,
								.y = new_geo.y,
								.width = new_geo.width,
								.height = new_geo.height},
			   0);
	} else {
		arrange(c->mon, false, false);
	}
}
#endif

#ifdef XWAYLAND
void createnotifyx11(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;
	Client *c = NULL;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}
#endif

#ifdef XWAYLAND
void commitx11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commmitx11);
	struct wlr_surface_state *state = &c->surface.xwayland->surface->current;

	/* overview 卡片节点是独立的 scene_surface，提交后自动更新 */

	/* state->width/height 与 xwayland->x/y 是 X11 物理尺寸（= c->geom * scale）
	 */
	float xscale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
	int32_t xw = (int32_t)roundf((c->geom.width - 2 * (int32_t)c->bw) * xscale);
	int32_t xh =
		(int32_t)roundf((c->geom.height - 2 * (int32_t)c->bw) * xscale);
	int32_t xx = (int32_t)roundf((c->geom.x + (int32_t)c->bw) * xscale);
	int32_t xy = (int32_t)roundf((c->geom.y + (int32_t)c->bw) * xscale);

	if (xw == (int32_t)state->width && xh == (int32_t)state->height &&
		(int32_t)c->surface.xwayland->x == xx &&
		(int32_t)c->surface.xwayland->y == xy) {
		c->configure_serial = 0;
	}

	/* scene 处理后强制根 surface 显示逻辑尺寸 */
	client_update_xwayland_dest_size(c);
}
#endif

#ifdef XWAYLAND
void associatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}
#endif

#ifdef XWAYLAND
void dissociatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
	c->xwl_root_buffer = NULL;
	c->xwl_clip_active = false;
}
#endif

#ifdef XWAYLAND
void sethints(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon) || !c || !c->surface.xwayland->hints)
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus(IPC_WATCH_ARRANGGE);

	if (c->isurgent && surface && surface->mapped)
		setborder_color(c);
}
#endif

#ifdef XWAYLAND
void xwaylandready(struct wl_listener *listener, void *data) {
	struct wlr_xcursor *xcursor;

	/* assign the one and only seat */
	wlr_xwayland_set_seat(xwayland, seat);

	/* 默认光标按 monitor scale 加载，避免 HiDPI 下被放大 */
	float cursor_scale = selmon && selmon->wlr_output->scale > 0.f
							 ? selmon->wlr_output->scale
							 : 1.f;
	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default",
												   cursor_scale))) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		struct wlr_buffer *buffer = wlr_xcursor_image_get_buffer(image);
		wlr_xwayland_set_cursor(xwayland, buffer, xcursor->images[0]->hotspot_x,
								xcursor->images[0]->hotspot_y);
	}

	/* xwayland can't auto sync the keymap, so we do it manually
	  and we need to wait the xwayland completely inited
	*/
	wl_event_source_timer_update(sync_keymap, 500);
}
#endif

#ifdef XWAYLAND
static void setgeometrynotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_geometry);
	struct wlr_box geo = {
		.x = c->surface.xwayland->x,
		.y = c->surface.xwayland->y,
		.width = c->surface.xwayland->width,
		.height = c->surface.xwayland->height,
	};
	/* xwayland->x/y 是 X11 物理尺寸，转回 wayland 逻辑坐标 */
	xwayland_x11_to_logical(&geo, c->xwayland_scale);
	wlr_scene_node_set_position(&c->scene->node, geo.x, geo.y);
	motionnotify(0, NULL, 0, 0, 0, 0);
}
#endif
