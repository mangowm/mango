// 实时 overview 预览：客户端提交新帧后限速重拍快照的间隔（毫秒）
#define OVERVIEW_SNAP_INTERVAL_MS 33

// 目标窗口有其他窗口和它同个tag就返回0
uint32_t want_restore_fullscreen(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c && c != target_client && c->tags == target_client->tags &&
			c == selmon->sel &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				SCROLLER &&
			c->mon->pertag->ltidxs[get_tags_first_tag_num(c->tags)]->id !=
				VERTICAL_SCROLLER) {
			return 0;
		}
	}

	return 1;
}

void overview_backup_surface(Client *c) {

	if (c->overview_scene_surface) {
		return;
	}

	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	struct wlr_box clip_box = (struct wlr_box){
		.x = geometry.x,
		.y = geometry.y,
		.width = c->overview_backup_geom.width - 2 * config.borderpx,
		.height = c->overview_backup_geom.height - 2 * config.borderpx,
	};

	if (client_is_x11(c)) {
		clip_box.x = 0;
		clip_box.y = 0;
	}

	c->overview_scene_surface = c->scene_surface;
	wlr_scene_node_set_enabled(&c->scene_surface->node, true);
	wlr_scene_node_set_position(&c->scene_surface->node, 0, 0);
	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip_box);
	c->scene_surface =
		wlr_scene_tree_snapshot(&c->scene_surface->node, c->scene);
	wlr_scene_node_set_enabled(&c->overview_scene_surface->node, false);
	wlr_scene_node_set_enabled(&c->scene_surface->node, true);

	// 开启实时预览：进入 overview 时初始化 live 状态
	c->ov_live_enabled = true;
	c->ov_last_snap_ms = 0;
	c->ov_serial_last_snap = c->ov_surface_commit_serial;
}

// 重新计算裁剪区域并重拍快照，刷新 overview 预览画面。
void overview_resnap(Client *c) {
	if (!c->overview_scene_surface)
		return;

	struct wlr_box geometry;
	client_get_geometry(c, &geometry);
	struct wlr_box clip_box = (struct wlr_box){
		.x = geometry.x,
		.y = geometry.y,
		.width = c->overview_backup_geom.width - 2 * config.borderpx,
		.height = c->overview_backup_geom.height - 2 * config.borderpx,
	};

	if (client_is_x11(c)) {
		clip_box.x = 0;
		clip_box.y = 0;
	}

	// 销毁旧快照
	wlr_scene_node_destroy(&c->scene_surface->node);

	// 重新启用原节点、设置位置与裁剪，然后重拍快照
	wlr_scene_node_set_enabled(&c->overview_scene_surface->node, true);
	wlr_scene_node_set_position(&c->overview_scene_surface->node, 0, 0);
	wlr_scene_subsurface_tree_set_clip(&c->overview_scene_surface->node,
									   &clip_box);
	c->scene_surface =
		wlr_scene_tree_snapshot(&c->overview_scene_surface->node, c->scene);
	wlr_scene_node_set_enabled(&c->overview_scene_surface->node, false);
	if (c->scene_surface)
		wlr_scene_node_set_enabled(&c->scene_surface->node, true);

	if (c->mon->is_jump_mode && c->jump_label_node) {
		wlr_scene_node_raise_to_top(&c->jump_label_node->scene_buffer->node);
	}

	// 重拍后重新应用 overview 布局效果：快照树的 buffer 默认以原始尺寸渲染
	// （scene_node_snapshot 用 dst_width/height 初始化 dest_size），必须重新
	// 设置 dest_size 缩放，否则重拍的帧会脱离布局、以原始大小显示。
	if (c->scene_surface)
		client_apply_clip(c, 1.0);
}

// 每帧调用:喂帧 + 检测新帧 + 限速重拍快照。
// 返回 true 表示需要继续渲染（overview 期间保持实时预览）。
bool overview_live_pass(Client *c) {
	if (!c->ov_live_enabled || !c->overview_scene_surface)
		return false;
	if (!client_surface(c) || !client_surface(c)->mapped)
		return false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	// 给隐藏 surface 喂 frame callback，让客户端持续渲染
	client_send_frame_done(c, &now);

	// 客户端提交了新帧且距上次重拍超过限速间隔时，重拍快照
	uint32_t now_ms = get_now_in_ms();
	if (c->ov_surface_commit_serial != c->ov_serial_last_snap &&
		now_ms - c->ov_last_snap_ms >= OVERVIEW_SNAP_INTERVAL_MS) {
		overview_resnap(c);
		c->ov_serial_last_snap = c->ov_surface_commit_serial;
		c->ov_last_snap_ms = now_ms;
	}

	return true;
}

// 普通视图切换到overview时保存窗口的旧状态
void overview_backup(Client *c) {
	c->overview_isfloatingbak = c->isfloating;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->overview_ismaximizescreenbak = c->ismaximizescreen;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->animation.tagining = false;
	c->animation.tagouted = false;
	c->animation.tagouting = false;
	c->overview_backup_geom = c->geom;
	c->overview_backup_bw = c->bw;
	if (c->isfloating) {
		c->isfloating = 0;
	}

	overview_backup_surface(c);

	if (c->isfullscreen || c->ismaximizescreen) {
		client_pending_fullscreen_state(c, 0); // 清除窗口全屏标志
		client_pending_maximized_state(c, 0);
	}
	c->bw = c->isnoborder ? 0 : config.borderpx;

	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
							WLR_EDGE_RIGHT);
}

// overview切回到普通视图还原窗口的状态
void overview_restore(Client *c, const Arg *arg) {
	c->isfloating = c->overview_isfloatingbak;
	c->isfullscreen = c->overview_isfullscreenbak;
	c->ismaximizescreen = c->overview_ismaximizescreenbak;
	c->overview_isfloatingbak = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaximizescreenbak = 0;
	c->geom = c->overview_backup_geom;
	c->bw = c->overview_backup_bw;
	c->animation.tagining = false;
	c->is_restoring_from_ov = (arg->ui & c->tags & TAGMASK) == 0 ? true : false;

	/* 关闭实时预览状态 */
	c->ov_live_enabled = false;
	c->ov_last_snap_ms = 0;
	c->ov_serial_last_snap = 0;

	if (c->overview_scene_surface) {
		wlr_scene_node_destroy(&c->scene_surface->node);
		c->scene_surface = c->overview_scene_surface;
		c->overview_scene_surface = NULL;
	}

	if (c->isfloating) {
		// XRaiseWindow(dpy, c->win); // 提升悬浮窗口到顶层
		resize(c, c->overview_backup_geom, 0);
	} else if (c->isfullscreen || c->ismaximizescreen) {
		if (want_restore_fullscreen(c) && c->ismaximizescreen) {
			setmaximizescreen(c, 1, false);
		} else if (want_restore_fullscreen(c) && c->isfullscreen) {
			setfullscreen(c, 1, false);
		} else {
			client_pending_fullscreen_state(c, 0);
			client_pending_maximized_state(c, 0);
			setfullscreen(c, false, false);
		}
	} else {
		if (c->is_restoring_from_ov) {
			c->is_restoring_from_ov = false;
			resize(c, c->overview_backup_geom, 0);
		}
	}

	if (c->bw == 0 &&
		!c->isfullscreen) { // 如果是在ov模式中创建的窗口,没有bw记录
		c->bw = c->isnoborder ? 0 : config.borderpx;
	}

	if (c->isfloating && !c->force_tiled_state) {
		client_set_tiled(c, WLR_EDGE_NONE);
	}
}