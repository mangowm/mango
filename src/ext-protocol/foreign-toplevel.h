#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

static struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

void handle_foreign_activate_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_activate_request);
	uint32_t target;

	if (c->swallowing)
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

void handle_foreign_maximize_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_maximize_request);
	struct wlr_foreign_toplevel_handle_v1_maximized_event *event = data;

	if (c->swallowing)
		return;

	if (c->ismaximizescreen && !event->maximized) {
		setmaximizescreen(c, 0);
		return;
	}

	if (!c->ismaximizescreen && event->maximized) {
		setmaximizescreen(c, 1);
		return;
	}
}

void handle_foreign_minimize_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_minimize_request);
	struct wlr_foreign_toplevel_handle_v1_minimized_event *event = data;

	if (c->swallowing)
		return;

	if (!c->isminimized && event->minimized) {
		set_minimized(c);
		return;
	}

	if (c->isminimized && !event->minimized) {
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		c->is_scratchpad_show = 0;
		setborder_color(c);
		show_hide_client(c);
		arrange(c->mon, true, false);
		return;
	}
}

void handle_foreign_fullscreen_request(struct wl_listener *listener,
									   void *data) {

	Client *c = wl_container_of(listener, c, foreign_fullscreen_request);
	struct wlr_foreign_toplevel_handle_v1_fullscreen_event *event = data;

	if (c->swallowing)
		return;

	if (c->isfullscreen && !event->fullscreen) {
		setfullscreen(c, 0);
		return;
	}

	if (!c->isfullscreen && event->fullscreen) {
		setfullscreen(c, 1);
		return;
	}
}

void handle_foreign_close_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_close_request);
	pending_kill_client(c);
}

void handle_foreign_destroy(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_destroy);
	wl_list_remove(&c->foreign_activate_request.link);
	wl_list_remove(&c->foreign_minimize_request.link);
	wl_list_remove(&c->foreign_maximize_request.link);
	wl_list_remove(&c->foreign_fullscreen_request.link);
	wl_list_remove(&c->foreign_close_request.link);
	wl_list_remove(&c->foreign_destroy.link);
}

void remove_foreign_topleve(Client *c) {
	wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
	c->foreign_toplevel = NULL;
}

void add_foreign_toplevel(Client *c) {
	if (!c || !c->mon || !c->mon->wlr_output || !c->mon->wlr_output->enabled)
		return;

	c->foreign_toplevel =
		wlr_foreign_toplevel_handle_v1_create(foreign_toplevel_manager);
	// 监听来自外部对于窗口的事件请求
	if (c->foreign_toplevel) {
		LISTEN(&(c->foreign_toplevel->events.request_activate),
			   &c->foreign_activate_request, handle_foreign_activate_request);
		LISTEN(&(c->foreign_toplevel->events.request_minimize),
			   &c->foreign_minimize_request, handle_foreign_minimize_request);
		LISTEN(&(c->foreign_toplevel->events.request_maximize),
			   &c->foreign_maximize_request, handle_foreign_maximize_request);
		LISTEN(&(c->foreign_toplevel->events.request_fullscreen),
			   &c->foreign_fullscreen_request,
			   handle_foreign_fullscreen_request);
		LISTEN(&(c->foreign_toplevel->events.request_close),
			   &c->foreign_close_request, handle_foreign_close_request);
		LISTEN(&(c->foreign_toplevel->events.destroy), &c->foreign_destroy,
			   handle_foreign_destroy);
		// 设置外部顶层句柄的id为应用的id
		const char *appid;
		appid = client_get_appid(c);
		if (appid)
			wlr_foreign_toplevel_handle_v1_set_app_id(c->foreign_toplevel,
													  appid);
		// 设置外部顶层句柄的title为应用的title
		const char *title;
		title = client_get_title(c);
		if (title)
			wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel,
													 title);
		// 设置外部顶层句柄的显示监视器为当前监视器
		wlr_foreign_toplevel_handle_v1_output_enter(c->foreign_toplevel,
													c->mon->wlr_output);
	}
}

void reset_foreign_tolevel(Client *c) {
	remove_foreign_topleve(c);
	add_foreign_toplevel(c);
}
