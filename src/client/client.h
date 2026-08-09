/*
 * Attempt to consolidate unavoidable suck into one file, away from dwl.c.  This
 * file is not meant to be pretty.  We use a .h file with static inline
 * functions instead of a separate .c module, or function pointers like sway, so
 * that they will simply compile out if the chosen #defines leave them unused.
 */

/* Leave these functions first; they're used in the others */
static inline int32_t client_is_x11(Client *c) {
#ifdef XWAYLAND
	return c->type == X11;
#endif
	return 0;
}

static inline struct wlr_surface *client_surface(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->surface;
#endif
	return c->surface.xdg->surface;
}

static inline int32_t toplevel_from_wlr_surface(struct wlr_surface *s,
												Client **pc,
												LayerSurface **pl) {
	struct wlr_xdg_surface *xdg_surface, *tmp_xdg_surface;
	struct wlr_surface *root_surface;
	struct wlr_layer_surface_v1 *layer_surface;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int32_t type = -1;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif

	if (!s)
		return -1;
	root_surface = wlr_surface_get_root_surface(s);

#ifdef XWAYLAND
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(root_surface))) {
		c = xsurface->data;
		type = c->type;
		goto end;
	}
#endif

	if ((layer_surface =
			 wlr_layer_surface_v1_try_from_wlr_surface(root_surface))) {
		l = layer_surface->data;
		type = LayerShell;
		goto end;
	}

	xdg_surface = wlr_xdg_surface_try_from_wlr_surface(root_surface);
	while (xdg_surface) {
		tmp_xdg_surface = NULL;
		switch (xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_POPUP:
			if (!xdg_surface->popup || !xdg_surface->popup->parent)
				return -1;

			tmp_xdg_surface = wlr_xdg_surface_try_from_wlr_surface(
				xdg_surface->popup->parent);

			if (!tmp_xdg_surface)
				return toplevel_from_wlr_surface(xdg_surface->popup->parent, pc,
												 pl);

			xdg_surface = tmp_xdg_surface;
			break;
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			c = xdg_surface->data;
			type = c->type;
			goto end;
		case WLR_XDG_SURFACE_ROLE_NONE:
			return -1;
		}
	}

end:
	if (pl)
		*pl = l;
	if (pc)
		*pc = c;
	return type;
}

/* The others */
static inline void client_activate_surface(struct wlr_surface *s,
										   int32_t activated) {
	struct wlr_xdg_toplevel *toplevel;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(s))) {
		if (activated && xsurface->minimized)
			wlr_xwayland_surface_set_minimized(xsurface, false);
		wlr_xwayland_surface_activate(xsurface, activated);
		return;
	}
#endif
	if ((toplevel = wlr_xdg_toplevel_try_from_wlr_surface(s)))
		wlr_xdg_toplevel_set_activated(toplevel, activated);
}

static inline const char *client_get_appid(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->class ? c->surface.xwayland->class
										  : "broken";
#endif
	return c->surface.xdg->toplevel->app_id ? c->surface.xdg->toplevel->app_id
											: "broken";
}

static inline int32_t client_get_pid(Client *c) {
	pid_t pid;
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->pid;
#endif
	wl_client_get_credentials(c->surface.xdg->client->client, &pid, NULL, NULL);
	return pid;
}

static inline void client_get_clip(Client *c, struct wlr_box *clip) {
	*clip = (struct wlr_box){
		.x = 0,
		.y = 0,
		.width = c->geom.width - 2 * c->bw,
		.height = c->geom.height - 2 * c->bw,
	};

#ifdef XWAYLAND
	if (client_is_x11(c))
		return;
#endif

	clip->x = c->surface.xdg->geometry.x;
	clip->y = c->surface.xdg->geometry.y;
}

static inline void client_get_geometry(Client *c, struct wlr_box *geom) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		/* X11 物理尺寸转回逻辑尺寸 */
		float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
		geom->x = (int32_t)roundf(c->surface.xwayland->x / scale);
		geom->y = (int32_t)roundf(c->surface.xwayland->y / scale);
		geom->width = (int32_t)roundf(c->surface.xwayland->width / scale);
		geom->height = (int32_t)roundf(c->surface.xwayland->height / scale);
		return;
	}
#endif
	*geom = c->surface.xdg->geometry;
}

static inline Client *client_get_parent(Client *c) {
	Client *p = NULL;
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		if (c->surface.xwayland->parent)
			toplevel_from_wlr_surface(c->surface.xwayland->parent->surface, &p,
									  NULL);
		return p;
	}
#endif
	if (c->surface.xdg->toplevel->parent)
		toplevel_from_wlr_surface(
			c->surface.xdg->toplevel->parent->base->surface, &p, NULL);
	return p;
}

static inline int32_t client_has_children(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return !wl_list_empty(&c->surface.xwayland->children);
#endif
	/* surface.xdg->link is never empty because it always contains at least the
	 * surface itself. */
	return wl_list_length(&c->surface.xdg->link) > 1;
}

static inline const char *client_get_title(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->title ? c->surface.xwayland->title
										  : "broken";
#endif
	return c->surface.xdg->toplevel->title ? c->surface.xdg->toplevel->title
										   : "broken";
}

static inline int32_t client_is_float_type(Client *c) {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_xdg_toplevel_state state;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		xcb_size_hints_t *size_hints = surface->size_hints;

		if (!size_hints)
			return 0;

		if (surface->modal)
			return 1;

		if (wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG) ||
			wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH) ||
			wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLBAR) ||
			wlr_xwayland_surface_has_window_type(
				surface, WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY)) {
			return 1;
		}

		return size_hints && size_hints->min_width > 0 &&
			   size_hints->min_height > 0 &&
			   (size_hints->max_width == size_hints->min_width ||
				size_hints->max_height == size_hints->min_height);
	}
#endif

	toplevel = c->surface.xdg->toplevel;
	state = toplevel->current;
	return toplevel->parent || (state.min_width != 0 && state.min_height != 0 &&
								(state.min_width == state.max_width ||
								 state.min_height == state.max_height));
}

static inline int32_t client_is_rendered_on_mon(Client *c, Monitor *m) {
	/* This is needed for when you don't want to check formal assignment,
	 * but rather actual displaying of the pixels.
	 * Usually VISIBLEON suffices and is also faster. */
	struct wlr_surface_output *s;
	int32_t unused_lx, unused_ly;
	if (!wlr_scene_node_coords(&c->scene->node, &unused_lx, &unused_ly))
		return 0;
	wl_list_for_each(s, &client_surface(c)->current_outputs,
					 link) if (s->output == m->wlr_output) return 1;
	return 0;
}

static inline int32_t client_is_unmanaged(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->override_redirect;
#endif
	return 0;
}

static inline void client_notify_enter(struct wlr_surface *s,
									   struct wlr_keyboard *kb) {
	if (kb)
		wlr_seat_keyboard_notify_enter(seat, s, kb->keycodes, kb->num_keycodes,
									   &kb->modifiers);
	else
		wlr_seat_keyboard_notify_enter(seat, s, NULL, 0, NULL);
}

static inline void client_send_close(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_close(c->surface.xwayland);
		return;
	}
#endif
	wlr_xdg_toplevel_send_close(c->surface.xdg->toplevel);
}

static inline void client_set_border_color(Client *c,
										   const float color[static 4]) {
	wlr_scene_rect_set_color(c->border, color);
}

static inline void client_set_fullscreen(Client *c, int32_t fullscreen) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_fullscreen(c->surface.xwayland, fullscreen);
		return;
	}
#endif
	wlr_xdg_toplevel_set_fullscreen(c->surface.xdg->toplevel, fullscreen);
}

static inline void client_set_scale(struct wlr_surface *s, float scale) {
	wlr_fractional_scale_v1_notify_scale(s, scale);
	wlr_surface_set_preferred_buffer_scale(s, (int32_t)ceilf(scale));
}

/* XWayland 根 surface 的 source_box 裁剪。
 *
 * X11 buffer 是物理尺寸（应用按 1:1 渲染），而 clip 是 mango 的逻辑可见
 * 区域。wlr_scene_subsurface_tree_set_clip 会把 clip 当作 surface 逻辑坐标
 * （XWayland 的 state->width/height 实为物理）去裁剪并重设 dest_size，导致
 * 内容被放大。这里改用 source_box + dest_size 直接裁剪 xwl_root_buffer：
 *   - source_box 用物理坐标（clip 逻辑 × xwayland_scale），保持 1:1 采样；
 *   - dest_size 用逻辑坐标（clip 逻辑尺寸），保持逻辑缩放显示；
 *   - buffer 节点平移到 (clip.x, clip.y)，让可见内容保持在原屏幕位置
 *     （否则窗口向左溢出时，裁剩的内容不会右移，仍会溢出到屏幕外）。 */
static inline void client_update_xwayland_clip(Client *c,
											   struct wlr_box *clip) {
#ifdef XWAYLAND
	if (!c->xwl_root_buffer || !c->xwl_root_buffer->buffer)
		return;
	struct wlr_buffer *buf = c->xwl_root_buffer->buffer;
	float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;

	/* 记录裁剪状态，供 surface commit 后恢复 */
	c->xwl_clip = *clip;
	c->xwl_clip_active = true;

	if (clip->width <= 0 || clip->height <= 0)
		return;

	struct wlr_fbox src = {
		.x = (float)clip->x * scale,
		.y = (float)clip->y * scale,
		.width = (float)clip->width * scale,
		.height = (float)clip->height * scale,
	};
	/* clamp 到物理 buffer 范围，防止越界采样 */
	if (src.x < 0.f)
		src.x = 0.f;
	if (src.y < 0.f)
		src.y = 0.f;
	if (src.x + src.width > buf->width)
		src.width = buf->width - src.x;
	if (src.y + src.height > buf->height)
		src.height = buf->height - src.y;
	/* 裁剪起点已超出 buffer 范围时 src.width/height 可能为负，
	 * wlr_scene_buffer_set_source_box 断言要求非负，这里兜底 clamp */
	if (src.width < 0.f)
		src.width = 0.f;
	if (src.height < 0.f)
		src.height = 0.f;

	wlr_scene_buffer_set_source_box(c->xwl_root_buffer, &src);
	wlr_scene_buffer_set_dest_size(c->xwl_root_buffer, clip->width,
								   clip->height);
	/* 平移 buffer 节点到裁剪起点，保持可见内容在屏幕上的原位置 */
	wlr_scene_node_set_position(&c->xwl_root_buffer->node, clip->x, clip->y);
#endif
}

/* 同步 XWayland 根 surface 的 dest_size（逻辑尺寸） */
static inline void client_update_xwayland_dest_size(Client *c) {
#ifdef XWAYLAND
	if (!c->xwl_root_buffer || !c->xwl_root_buffer->buffer)
		return;
	/* 处于 source_box 裁剪状态时，surface 提交后恢复裁剪，
	 * 避免窗口静止（动画结束）后被强制复原为未裁剪 */
	if (c->xwl_clip_active) {
		client_update_xwayland_clip(c, &c->xwl_clip);
		return;
	}
	struct wlr_buffer *buf = c->xwl_root_buffer->buffer;
	float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
	int32_t w, h;
	if (client_is_unmanaged(c)) {
		w = (int32_t)roundf(buf->width / scale);
		h = (int32_t)roundf(buf->height / scale);
	} else {
		struct wlr_box cur = c->animation.current;
		w = cur.width - 2 * (int32_t)c->bw;
		h = cur.height - 2 * (int32_t)c->bw;
	}
	if (w > 0 && h > 0) {
		wlr_scene_buffer_set_dest_size(c->xwl_root_buffer, w, h);
		/* 恢复整块采样与原点，清除裁剪残留 */
		struct wlr_fbox full = {
			.x = 0,
			.y = 0,
			.width = buf->width,
			.height = buf->height,
		};
		wlr_scene_buffer_set_source_box(c->xwl_root_buffer, &full);
		wlr_scene_node_set_position(&c->xwl_root_buffer->node, 0, 0);
	}
#endif
}

static inline uint32_t client_set_size(Client *c, uint32_t width,
									   uint32_t height) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		struct wlr_surface_state *state = &surface->surface->current;

		/* configure 用物理尺寸（逻辑 × xscale），让 X11 按 1:1 渲染 */
		float xscale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
		int32_t xw =
			(int32_t)roundf((c->geom.width - 2 * (int32_t)c->bw) * xscale);
		int32_t xh =
			(int32_t)roundf((c->geom.height - 2 * (int32_t)c->bw) * xscale);
		int32_t xx = (int32_t)roundf((c->geom.x + (int32_t)c->bw) * xscale);
		int32_t xy = (int32_t)roundf((c->geom.y + (int32_t)c->bw) * xscale);

		if ((int32_t)state->width == xw && (int32_t)state->height == xh &&
			(int32_t)c->surface.xwayland->x == xx &&
			(int32_t)c->surface.xwayland->y == xy) {
			return 0;
		}

		/* 客户端尚未 ack 时 state 不更新，用已请求参数去重，
		 * 避免重复发相同 configure 导致客户端反复重渲染/上传 */
		if (c->xwl_req_valid && c->xwl_req_x == xx && c->xwl_req_y == xy &&
			c->xwl_req_w == xw && c->xwl_req_h == xh) {
			return 0;
		}
		c->xwl_req_valid = true;
		c->xwl_req_x = xx;
		c->xwl_req_y = xy;
		c->xwl_req_w = xw;
		c->xwl_req_h = xh;

		xcb_size_hints_t *size_hints = surface->size_hints;
		int32_t width = xw;
		int32_t height = xh;

		if (size_hints && xw < (int32_t)size_hints->min_width)
			width = size_hints->min_width;
		if (size_hints && xh < (int32_t)size_hints->min_height)
			height = size_hints->min_height;

		wlr_xwayland_surface_configure(c->surface.xwayland, xx, xy, width,
									   height);
		return 1;
	}
#endif
	if ((int32_t)width == c->surface.xdg->toplevel->current.width &&
		(int32_t)height == c->surface.xdg->toplevel->current.height)
		return 0;
	/* 客户端尚未 ack 时 current 不更新，用 scheduled（已请求的配置）去重，
	 * 避免重复发相同尺寸的 configure 导致客户端反复重渲染/上传 */
	if ((int32_t)width == c->surface.xdg->toplevel->scheduled.width &&
		(int32_t)height == c->surface.xdg->toplevel->scheduled.height)
		return 0;
	return wlr_xdg_toplevel_set_size(c->surface.xdg->toplevel, (int32_t)width,
									 (int32_t)height);
}

static inline void client_set_minimized(Client *c, bool minimized) {
#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_minimized(c->surface.xwayland, minimized);
		return;
	}
#endif

	return;
}

static inline void client_set_maximized(Client *c, bool maximized) {
	struct wlr_xdg_toplevel *toplevel;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		wlr_xwayland_surface_set_maximized(c->surface.xwayland, maximized,
										   maximized);
		return;
	}
#endif
	toplevel = c->surface.xdg->toplevel;
	wlr_xdg_toplevel_set_maximized(toplevel, maximized);
	return;
}

static inline void client_set_tiled(Client *c, uint32_t edges) {
	struct wlr_xdg_toplevel *toplevel;
#ifdef XWAYLAND
	if (client_is_x11(c) && c->force_fakemaximize) {
		wlr_xwayland_surface_set_maximized(c->surface.xwayland,
										   edges != WLR_EDGE_NONE,
										   edges != WLR_EDGE_NONE);
		return;
	}
#endif

	toplevel = c->surface.xdg->toplevel;

	if (wl_resource_get_version(c->surface.xdg->toplevel->resource) >=
		XDG_TOPLEVEL_STATE_TILED_RIGHT_SINCE_VERSION) {
		wlr_xdg_toplevel_set_tiled(c->surface.xdg->toplevel, edges);
	}

	if (c->force_fakemaximize) {
		wlr_xdg_toplevel_set_maximized(toplevel, edges != WLR_EDGE_NONE);
	}
}

static inline int32_t client_should_ignore_focus(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;

		if (!surface->hints)
			return 0;

		return !surface->hints->input;
	}
#endif
	return 0;
}

static inline int32_t client_is_x11_popup(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		// 处理不需要焦点的窗口类型
		const uint32_t no_focus_types[] = {
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_COMBO,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DND,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_MENU,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NOTIFICATION,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_POPUP_MENU,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLTIP,
			WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY};
		// 检查窗口类型是否需要禁止焦点
		for (size_t i = 0;
			 i < sizeof(no_focus_types) / sizeof(no_focus_types[0]); ++i) {
			if (wlr_xwayland_surface_has_window_type(surface,
													 no_focus_types[i])) {
				return 1;
			}
		}
	}
#endif
	return 0;
}

static inline int32_t client_should_global(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;

		if (surface->sticky)
			return 1;
	}
#endif
	return 0;
}

static inline int32_t client_should_overtop(Client *c) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		if (surface->above)
			return 1;
	}
#endif
	return 0;
}

static inline int32_t client_wants_focus(Client *c) {
#ifdef XWAYLAND
	return client_is_unmanaged(c) &&
		   wlr_xwayland_surface_override_redirect_wants_focus(
			   c->surface.xwayland) &&
		   wlr_xwayland_surface_icccm_input_model(c->surface.xwayland) !=
			   WLR_ICCCM_INPUT_MODEL_NONE;
#endif
	return 0;
}

static inline int32_t client_wants_fullscreen(Client *c) {
#ifdef XWAYLAND
	if (client_is_x11(c))
		return c->surface.xwayland->fullscreen;
#endif
	return c->surface.xdg->toplevel->requested.fullscreen;
}

static inline bool client_request_minimize(Client *c, void *data) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_minimize_event *event = data;
		return event->minimize;
	}
#endif

	return c->surface.xdg->toplevel->requested.minimized;
}

static inline bool client_request_maximize(Client *c, void *data) {

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		return surface->maximized_vert || surface->maximized_horz;
	}
#endif

	return c->surface.xdg->toplevel->requested.maximized;
}

static inline void client_set_size_bound(Client *c) {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_xdg_toplevel_state state;

#ifdef XWAYLAND
	if (client_is_x11(c)) {
		struct wlr_xwayland_surface *surface = c->surface.xwayland;
		xcb_size_hints_t *size_hints = surface->size_hints;

		if (!size_hints)
			return;

		/* size_hints 是 X11 物理尺寸，转回逻辑再比较 */
		float scale = c->xwayland_scale > 0.f ? c->xwayland_scale : 1.f;
		int32_t min_w = (int32_t)roundf(size_hints->min_width / scale);
		int32_t min_h = (int32_t)roundf(size_hints->min_height / scale);
		int32_t max_w = (int32_t)roundf(size_hints->max_width / scale);
		int32_t max_h = (int32_t)roundf(size_hints->max_height / scale);

		if ((uint32_t)c->geom.width - 2 * c->bw < (uint32_t)min_w && min_w > 0)
			c->geom.width = min_w + 2 * c->bw;
		if ((uint32_t)c->geom.height - 2 * c->bw < (uint32_t)min_h && min_h > 0)
			c->geom.height = min_h + 2 * c->bw;
		if ((uint32_t)c->geom.width - 2 * c->bw > (uint32_t)max_w && max_w > 0)
			c->geom.width = max_w + 2 * c->bw;
		if ((uint32_t)c->geom.height - 2 * c->bw > (uint32_t)max_h && max_h > 0)
			c->geom.height = max_h + 2 * c->bw;
		return;
	}
#endif

	toplevel = c->surface.xdg->toplevel;
	state = toplevel->current;
	if ((uint32_t)c->geom.width - 2 * c->bw < state.min_width &&
		state.min_width > 0) {
		c->geom.width = state.min_width + 2 * c->bw;
	}
	if ((uint32_t)c->geom.height - 2 * c->bw < state.min_height &&
		state.min_height > 0) {
		c->geom.height = state.min_height + 2 * c->bw;
	}
	if ((uint32_t)c->geom.width - 2 * c->bw > state.max_width &&
		state.max_width > 0) {
		c->geom.width = state.max_width + 2 * c->bw;
	}
	if ((uint32_t)c->geom.height - 2 * c->bw > state.max_height &&
		state.max_height > 0) {
		c->geom.height = state.max_height + 2 * c->bw;
	}
}
