#include "mango/manage/xwayland_primary.h"

#include "mango/common/server.h"
#include "mango/manage/monitor.h"

#ifdef XWAYLAND
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/xwayland.h>
#include <xcb/randr.h>
#include <xcb/xcb.h>

#define XWL_NAME_MAX 64
#define XWL_CACHE_MAX 16

static xcb_connection_t *conn = NULL;
static struct wl_event_source *conn_source = NULL;
static xcb_window_t root = XCB_NONE;
static char display_name[XWL_NAME_MAX];
static char target_name[XWL_NAME_MAX];
static char applied_name[XWL_NAME_MAX];

static char cache_name[XWL_CACHE_MAX][XWL_NAME_MAX];
static xcb_randr_output_t cache_output[XWL_CACHE_MAX];
static int32_t cache_len = 0;
static bool cache_valid = false;

static bool resources_pending = false;
static xcb_randr_get_screen_resources_cookie_t resources_cookie;
static bool info_pending = false;
static xcb_randr_get_output_info_cookie_t info_cookie;
static xcb_randr_output_t *outputs = NULL;
static int32_t outputs_len = 0, outputs_idx = 0;
static struct wl_event_source *ready_timer = NULL;
static bool ready_pending = false;
static bool cache_refresh_attempted = false;

static bool xwayland_server_running(void) {
	return server.xwayland && server.xwayland->server &&
		   server.xwayland->server->ready;
}

static int32_t xwayland_primary_ready(int32_t fd, uint32_t mask, void *data);

static void xwayland_primary_unwatch(void) {
	if (conn_source) {
		wl_event_source_remove(conn_source);
		conn_source = NULL;
	}
}

static void xwayland_primary_watch(void) {
	if (!conn || conn_source) {
		return;
	}
	conn_source =
		wl_event_loop_add_fd(wl_display_get_event_loop(server.display),
							 xcb_get_file_descriptor(conn), WL_EVENT_READABLE,
							 xwayland_primary_ready, NULL);
}

static void xwayland_primary_close(void) {
	xwayland_primary_unwatch();
	if (conn) {
		xcb_disconnect(conn);
		conn = NULL;
	}
	root = XCB_NONE;
	resources_pending = false;
	info_pending = false;
	free(outputs);
	outputs = NULL;
	outputs_len = outputs_idx = 0;
	cache_len = 0;
	cache_valid = false;
}

static void xwayland_primary_apply(void);
static void xwayland_primary_start(void);
static int32_t xwayland_primary_ready(int32_t fd, uint32_t mask, void *data);

static bool xwayland_primary_connect(void) {
	if (conn) {
		return true;
	}

	int32_t screen_num = 0;
	conn = xcb_connect(display_name, &screen_num);
	if (!conn || xcb_connection_has_error(conn) || xcb_get_setup(conn) == NULL) {
		xwayland_primary_close();
		return false;
	}

	xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(conn));
	for (int32_t i = 0; i < screen_num && it.rem; i++) {
		xcb_screen_next(&it);
	}
	if (!it.rem) {
		xwayland_primary_close();
		return false;
	}
	root = it.data->root;
	cache_valid = false;
	return true;
}

static void xwayland_primary_build_cache(void) {
	free(outputs);
	outputs = NULL;
	outputs_len = outputs_idx = 0;
	cache_len = 0;
	cache_valid = false;
	resources_pending = true;
	resources_cookie = xcb_randr_get_screen_resources(conn, root);
	xcb_flush(conn);
}

static int32_t xwayland_primary_ready_timer(void *data) {
	struct wl_event_source *source = ready_timer;
	ready_timer = NULL;
	if (source) {
		wl_event_source_remove(source);
	}

	ready_pending = false;

	xwayland_primary_start();
	return 0;
}

static void xwayland_primary_apply(void) {
	if (!conn) {
		return;
	}
	if (!target_name[0]) {
		xwayland_primary_close();
		return;
	}
	if (resources_pending || info_pending) {
		return;
	}
	if (!cache_valid) {
		xwayland_primary_build_cache();
		return;
	}
	for (int32_t i = 0; i < cache_len; i++) {
		if (strncmp(cache_name[i], target_name, XWL_NAME_MAX) != 0) {
			continue;
		}
		xcb_randr_set_output_primary(conn, root, cache_output[i]);
		xcb_flush(conn);
		strncpy(applied_name, target_name, XWL_NAME_MAX - 1);
		applied_name[XWL_NAME_MAX - 1] = '\0';
		cache_refresh_attempted = false;
		free(xcb_randr_get_output_primary_reply(
			conn, xcb_randr_get_output_primary(conn, root), NULL));
		xwayland_primary_close();
		return;
	}

	if (!cache_refresh_attempted) {
		cache_refresh_attempted = true;
		cache_valid = false;
		xwayland_primary_build_cache();
		return;
	}
	applied_name[0] = '\0';
	xwayland_primary_close();
}

static void xwayland_primary_start(void) {
	if (ready_pending) {
		return;
	}
	if (strncmp(applied_name, target_name, XWL_NAME_MAX) == 0) {
		return;
	}
	if (!xwayland_server_running()) {
		applied_name[0] = '\0';
		cache_valid = false;
		return;
	}
	if (conn && xcb_connection_has_error(conn)) {
		xwayland_primary_close();
	}
	if (!conn && !xwayland_primary_connect()) {
		return;
	}

	xwayland_primary_watch();
	xwayland_primary_apply();
}

static int32_t xwayland_primary_ready(int32_t fd, uint32_t mask, void *data) {
	if (!conn || (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR))) {
		xwayland_primary_close();
		return 0;
	}

	xcb_generic_event_t *event;
	while ((event = xcb_poll_for_event(conn)) != NULL) {
		free(event);
	}

	if (resources_pending) {
		xcb_randr_get_screen_resources_reply_t *resources =
			xcb_randr_get_screen_resources_reply(conn, resources_cookie, NULL);
		if (!resources) {
			xwayland_primary_close();
			return 0;
		}

		int32_t len = xcb_randr_get_screen_resources_outputs_length(resources);
		outputs = malloc(sizeof(*outputs) * (len > 0 ? len : 1));
		if (outputs && len > 0) {
			memcpy(outputs, xcb_randr_get_screen_resources_outputs(resources),
				   sizeof(*outputs) * len);
			outputs_len = len;
		}
		free(resources);
		resources_pending = false;
	} else if (info_pending) {
		xcb_randr_get_output_info_reply_t *info =
			xcb_randr_get_output_info_reply(conn, info_cookie, NULL);
		if (info) {
			int32_t len = xcb_randr_get_output_info_name_length(info);
			const char *name =
				(const char *)xcb_randr_get_output_info_name(info);
			if (name && len > 0 && cache_len < XWL_CACHE_MAX) {
				int32_t copy = len < XWL_NAME_MAX - 1 ? len : XWL_NAME_MAX - 1;
				memcpy(cache_name[cache_len], name, copy);
				cache_name[cache_len][copy] = '\0';
				cache_output[cache_len] = outputs[outputs_idx];
				cache_len++;
			}
			free(info);
		}
		info_pending = false;
		outputs_idx++;
	}

	if (resources_pending || info_pending) {
		return 0;
	}
	if (outputs_idx < outputs_len) {
		info_pending = true;
		info_cookie = xcb_randr_get_output_info(conn, outputs[outputs_idx],
												XCB_CURRENT_TIME);
		xcb_flush(conn);
		return 0;
	}

	free(outputs);
	outputs = NULL;
	outputs_len = outputs_idx = 0;
	cache_valid = true;
	xwayland_primary_apply();
	if (!resources_pending && !info_pending) {
		xwayland_primary_unwatch();
	}
	return 0;
}

void xwayland_primary_init(void) {
	const char *display =
		server.xwayland ? server.xwayland->display_name : NULL;
	if (!display) {
		return;
	}

	xwayland_primary_close();
	strncpy(display_name, display, XWL_NAME_MAX - 1);
	display_name[XWL_NAME_MAX - 1] = '\0';
	applied_name[0] = '\0';

	if (server.selected_monitor && server.selected_monitor->wlr_output &&
		server.selected_monitor->wlr_output->name) {
		strncpy(target_name, server.selected_monitor->wlr_output->name,
				XWL_NAME_MAX - 1);
		target_name[XWL_NAME_MAX - 1] = '\0';
	} else {
		target_name[0] = '\0';
	}

	ready_pending = false;
	cache_refresh_attempted = false;

	if (!ready_timer) {
		ready_timer =
			wl_event_loop_add_timer(wl_display_get_event_loop(server.display),
									xwayland_primary_ready_timer, NULL);
	}
	if (ready_timer) {
		wl_event_source_timer_update(ready_timer, 500);
	}

	xwayland_primary_start();
}

void xwayland_primary_set(Monitor *m) {
	const char *display =
		server.xwayland ? server.xwayland->display_name : NULL;
	if (!display || !m || !m->wlr_output || !m->wlr_output->name) {
		return;
	}

	if (strncmp(display_name, display, XWL_NAME_MAX) != 0) {
		xwayland_primary_close();
		strncpy(display_name, display, XWL_NAME_MAX - 1);
		display_name[XWL_NAME_MAX - 1] = '\0';
	}
	if (strncmp(target_name, m->wlr_output->name, XWL_NAME_MAX) != 0) {
		cache_refresh_attempted = false;
	}
	strncpy(target_name, m->wlr_output->name, XWL_NAME_MAX - 1);
	target_name[XWL_NAME_MAX - 1] = '\0';

	if (ready_pending ||
		strncmp(applied_name, target_name, XWL_NAME_MAX) == 0) {
		return;
	}

	xwayland_primary_start();
}

void xwayland_primary_invalidate(void) {
	cache_refresh_attempted = false;
	applied_name[0] = '\0';

	if (ready_pending) {
		return;
	}
	if (resources_pending || info_pending) {
		return;
	}

	cache_valid = false;
	xwayland_primary_start();
}

#endif
