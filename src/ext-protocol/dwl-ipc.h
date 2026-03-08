#include "dwl-ipc-unstable-v2-protocol.h"

static void dwl_ipc_manager_bind(struct wl_client *client, void *data,
								 uint32_t version, uint32_t id);
static void dwl_ipc_manager_destroy(struct wl_resource *resource);
static void dwl_ipc_manager_get_output(struct wl_client *client,
									   struct wl_resource *resource,
									   uint32_t id, struct wl_resource *output);
static void dwl_ipc_manager_release(struct wl_client *client,
									struct wl_resource *resource);
static void dwl_ipc_output_destroy(struct wl_resource *resource);
static void dwl_ipc_output_printstatus(Monitor *monitor);
static void dwl_ipc_output_printstatus_to(DwlIpcOutput *ipc_output);
static void dwl_ipc_output_set_client_tags(struct wl_client *client,
										   struct wl_resource *resource,
										   uint32_t and_tags,
										   uint32_t xor_tags);
static void dwl_ipc_output_set_layout(struct wl_client *client,
									  struct wl_resource *resource,
									  uint32_t index);
static void dwl_ipc_output_set_tags(struct wl_client *client,
									struct wl_resource *resource,
									uint32_t tagmask, uint32_t toggle_tagset);
static void dwl_ipc_output_quit(struct wl_client *client,
								struct wl_resource *resource);
static void dwl_ipc_output_dispatch(struct wl_client *client,
									struct wl_resource *resource,
									const char *dispatch, const char *arg1,
									const char *arg2, const char *arg3,
									const char *arg4, const char *arg5);
static void dwl_ipc_output_release(struct wl_client *client,
								   struct wl_resource *resource);

/* global event handlers */
static struct zdwl_ipc_manager_v2_interface dwl_manager_implementation = {
	.release = dwl_ipc_manager_release,
	.get_output = dwl_ipc_manager_get_output};
static struct zdwl_ipc_output_v2_interface dwl_output_implementation = {
	.release = dwl_ipc_output_release,
	.set_tags = dwl_ipc_output_set_tags,
	.quit = dwl_ipc_output_quit,
	.dispatch = dwl_ipc_output_dispatch,
	.set_layout = dwl_ipc_output_set_layout,
	.set_client_tags = dwl_ipc_output_set_client_tags};

void dwl_ipc_manager_bind(struct wl_client *client, void *data,
						  uint32_t version, uint32_t id) {
	struct wl_resource *manager_resource =
		wl_resource_create(client, &zdwl_ipc_manager_v2_interface, version, id);
	if (!manager_resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(manager_resource,
								   &dwl_manager_implementation, NULL,
								   dwl_ipc_manager_destroy);

	zdwl_ipc_manager_v2_send_tags(manager_resource, LENGTH(tags));

	for (uint32_t i = 0; i < LENGTH(layouts); i++)
		zdwl_ipc_manager_v2_send_layout(manager_resource, layouts[i].symbol);
}

void dwl_ipc_manager_destroy(struct wl_resource *resource) {
	/* No state to destroy */
}

void dwl_ipc_manager_get_output(struct wl_client *client,
								struct wl_resource *resource, uint32_t id,
								struct wl_resource *output) {
	DwlIpcOutput *ipc_output;
	struct wlr_output *op = wlr_output_from_resource(output);
	if (!op)
		return;
	Monitor *monitor = op->data;
	struct wl_resource *output_resource =
		wl_resource_create(client, &zdwl_ipc_output_v2_interface,
						   wl_resource_get_version(resource), id);
	if (!output_resource)
		return;

	ipc_output = ecalloc(1, sizeof(*ipc_output));
	ipc_output->resource = output_resource;
	ipc_output->mon = monitor;
	wl_resource_set_implementation(output_resource, &dwl_output_implementation,
								   ipc_output, dwl_ipc_output_destroy);
	wl_list_insert(&monitor->dwl_ipc_outputs, &ipc_output->link);
	dwl_ipc_output_printstatus_to(ipc_output);
}

void dwl_ipc_manager_release(struct wl_client *client,
							 struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void dwl_ipc_output_destroy(struct wl_resource *resource) {
	DwlIpcOutput *ipc_output = wl_resource_get_user_data(resource);
	wl_list_remove(&ipc_output->link);
	free(ipc_output);
}

// 修改IPC输出函数，接受掩码参数
void dwl_ipc_output_printstatus(Monitor *monitor) {
	DwlIpcOutput *ipc_output;
	wl_list_for_each(ipc_output, &monitor->dwl_ipc_outputs, link)
		dwl_ipc_output_printstatus_to(ipc_output);
}

// 修改主IPC输出函数，根据掩码发送相应事件
void dwl_ipc_output_printstatus_to(DwlIpcOutput *ipc_output) {
	Monitor *monitor = ipc_output->mon;
	Client *c = NULL, *focused = NULL;
	struct wlr_keyboard *keyboard;
	xkb_layout_index_t current;
	int32_t tagmask, state, numclients, focused_client, tag;
	const char *title, *appid, *symbol;
	char kb_layout[32];
	focused = focustop(monitor);
	zdwl_ipc_output_v2_send_active(ipc_output->resource, monitor == selmon);

	for (tag = 0; tag < LENGTH(tags); tag++) {
		numclients = state = focused_client = 0;
		tagmask = 1 << tag;
		if ((tagmask & monitor->tagset[monitor->seltags]) != 0)
			state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != monitor)
				continue;
			if (!(c->tags & tagmask))
				continue;
			if (c == focused)
				focused_client = 1;
			if (c->isurgent)
				state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;
			numclients++;
		}
		zdwl_ipc_output_v2_send_tag(ipc_output->resource, tag, state,
									numclients, focused_client);
	}

	title = focused ? client_get_title(focused) : "";
	appid = focused ? client_get_appid(focused) : "";

	if (monitor->isoverview) {
		symbol = overviewlayout.symbol;
	} else {
		symbol = monitor->pertag->ltidxs[monitor->pertag->curtag]->symbol;
	}

	keyboard = &kb_group->wlr_group->keyboard;
	current = xkb_state_serialize_layout(keyboard->xkb_state,
										 XKB_STATE_LAYOUT_EFFECTIVE);
	get_layout_abbr(kb_layout,
					xkb_keymap_layout_get_name(keyboard->keymap, current));

	zdwl_ipc_output_v2_send_layout(
		ipc_output->resource,
		monitor->pertag->ltidxs[monitor->pertag->curtag] - layouts);
	zdwl_ipc_output_v2_send_title(ipc_output->resource, title ? title : broken);
	zdwl_ipc_output_v2_send_appid(ipc_output->resource, appid ? appid : broken);
	zdwl_ipc_output_v2_send_layout_symbol(ipc_output->resource, symbol);
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_FULLSCREEN_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_fullscreen(ipc_output->resource,
										   focused ? focused->isfullscreen : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_FLOATING_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_floating(ipc_output->resource,
										 focused ? focused->isfloating : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_X_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_x(ipc_output->resource,
								  focused ? focused->geom.x : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_Y_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_y(ipc_output->resource,
								  focused ? focused->geom.y : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_WIDTH_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_width(ipc_output->resource,
									  focused ? focused->geom.width : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_HEIGHT_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_height(ipc_output->resource,
									   focused ? focused->geom.height : 0);
	}
	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_LAST_LAYER_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_last_layer(ipc_output->resource,
										   monitor->last_surface_ws_name);
	}

	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_KB_LAYOUT_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_kb_layout(ipc_output->resource, kb_layout);
	}

	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_KEYMODE_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_keymode(ipc_output->resource, keymode.mode);
	}

	if (wl_resource_get_version(ipc_output->resource) >=
		ZDWL_IPC_OUTPUT_V2_SCALEFACTOR_SINCE_VERSION) {
		zdwl_ipc_output_v2_send_scalefactor(ipc_output->resource,
											monitor->wlr_output->scale * 100);
	}

	zdwl_ipc_output_v2_send_frame(ipc_output->resource);
}

void dwl_ipc_output_set_client_tags(struct wl_client *client,
									struct wl_resource *resource,
									uint32_t and_tags, uint32_t xor_tags) {
	DwlIpcOutput *ipc_output;
	Monitor *monitor = NULL;
	Client *selected_client = NULL;
	uint32_t newtags = 0;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;

	monitor = ipc_output->mon;
	selected_client = focustop(monitor);
	if (!selected_client)
		return;

	newtags = (selected_client->tags & and_tags) ^ xor_tags;
	if (!newtags)
		return;

	selected_client->tags = newtags;
	if (selmon == monitor)
		focusclient(focustop(monitor), 1);
	arrange(selmon, false, false);
	printstatus();
}

void dwl_ipc_output_set_layout(struct wl_client *client,
							   struct wl_resource *resource, uint32_t index) {
	DwlIpcOutput *ipc_output;
	Monitor *monitor = NULL;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;

	monitor = ipc_output->mon;
	if (index >= LENGTH(layouts))
		index = 0;

	monitor->pertag->ltidxs[monitor->pertag->curtag] = &layouts[index];
	clear_fullscreen_and_maximized_state(monitor);
	arrange(monitor, false, false);
	printstatus();
}

void dwl_ipc_output_set_tags(struct wl_client *client,
							 struct wl_resource *resource, uint32_t tagmask,
							 uint32_t toggle_tagset) {
	DwlIpcOutput *ipc_output;
	Monitor *monitor = NULL;
	uint32_t newtags = tagmask & TAGMASK;

	ipc_output = wl_resource_get_user_data(resource);
	if (!ipc_output)
		return;
	monitor = ipc_output->mon;

	view_in_mon(&(Arg){.ui = newtags}, true, monitor, true);
}

void dwl_ipc_output_quit(struct wl_client *client,
						 struct wl_resource *resource) {
	quit(&(Arg){0});
}

void dwl_ipc_output_dispatch(struct wl_client *client,
							 struct wl_resource *resource, const char *dispatch,
							 const char *arg1, const char *arg2,
							 const char *arg3, const char *arg4,
							 const char *arg5) {

	int32_t (*func)(const Arg *);
	Arg arg;
	func = parse_func_name((char *)dispatch, &arg, (char *)arg1, (char *)arg2,
						   (char *)arg3, (char *)arg4, (char *)arg5);
	if (func) {
		func(&arg);
	}

	if (arg.v)
		free(arg.v);
	if (arg.v2)
		free(arg.v2);
	if (arg.v3)
		free(arg.v3);
}

void dwl_ipc_output_release(struct wl_client *client,
							struct wl_resource *resource) {
	wl_resource_destroy(resource);
}
