#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

enum ipc_watch_type {
	IPC_WATCH_NONE,
	IPC_WATCH_MONITOR,
	IPC_WATCH_CLIENT,
	IPC_WATCH_TAGS,
	IPC_WATCH_ALL_MONITORS,
	IPC_WATCH_ALL_TAGS,
	IPC_WATCH_ALL_CLIENTS,
	IPC_WATCH_KEYMODE,
	IPC_WATCH_KB_LAYOUT,
	IPC_WATCH_LAST_OPEN_SURFACE,
};

struct ipc_watch_client {
	struct wl_list link;
	int fd;
	struct wl_event_source *source;
	enum ipc_watch_type type;
	union {
		struct {
			char name[64];
		} monitor;
		struct {
			uint32_t id;
		} client;
		struct {
			char mon_name[64];
		} tags;
	} target;
};

static struct wl_list watch_clients;

struct ipc_client_state {
	int fd;
	struct wl_event_source *source;
	struct wl_event_loop *loop;
	char *buf;
	size_t buf_len;
	size_t buf_cap;
};

static void ipc_remove_watch_client(struct ipc_watch_client *wc);
static void ipc_notify_json_to_fd(int fd, cJSON *json);

/* ---------- 工具函数 ---------- */

static Monitor *monitor_by_name(const char *name) {
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
		if (strcmp(m->wlr_output->name, name) == 0)
			return m;
	}
	return NULL;
}

static Client *client_by_id(uint32_t id) {
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->id == id)
			return c;
	}
	return NULL;
}

static const char *ipc_get_layout_str(void) {
	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	static char layout[32];
	get_layout_abbr(layout,
					xkb_keymap_layout_get_name(keyboard->keymap, current));
	return layout;
}

static cJSON *tags_mask_to_array(uint32_t tagmask) {
	cJSON *arr = cJSON_CreateArray();
	for (int i = 0; i < LENGTH(tags); i++)
		if (tagmask & (1 << i))
			cJSON_AddItemToArray(arr, cJSON_CreateNumber(i + 1));
	return arr;
}

static cJSON *build_tags_json(Monitor *m) {
	cJSON *tags_array = cJSON_CreateArray();
	Client *c = NULL;
	for (int tag = 1; tag <= LENGTH(tags); tag++) {
		int numclients = 0;
		bool is_active = false, is_urgent = false;
		uint32_t tagmask = 1 << (tag - 1);
		if (tagmask & m->tagset[m->seltags])
			is_active = true;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			if (!(c->tags & tagmask & TAGMASK))
				continue;
			if (c->isurgent)
				is_urgent = true;
			numclients++;
		}
		cJSON *tag_obj = cJSON_CreateObject();
		cJSON_AddNumberToObject(tag_obj, "index", tag);
		cJSON_AddBoolToObject(tag_obj, "is_active", is_active);
		cJSON_AddBoolToObject(tag_obj, "is_urgent", is_urgent);
		cJSON_AddStringToObject(tag_obj, "layout",
								m->pertag->ltidxs[tag]->symbol);
		cJSON_AddNumberToObject(tag_obj, "client_count", numclients);
		cJSON_AddItemToArray(tags_array, tag_obj);
	}
	return tags_array;
}

static cJSON *monitor_active_client(Monitor *m) {
	cJSON *obj = cJSON_CreateObject();
	if (!m->sel) {
		cJSON_AddNullToObject(obj, "id");
		cJSON_AddNullToObject(obj, "title");
		cJSON_AddNullToObject(obj, "appid");
		return obj;
	}
	Client *c = m->sel;
	cJSON_AddNumberToObject(obj, "id", c->id);
	cJSON_AddStringToObject(obj, "title", client_get_title(c));
	cJSON_AddStringToObject(obj, "appid", client_get_appid(c));
	return obj;
}

static cJSON *monitor_active_tags(Monitor *m) {
	cJSON *arr = cJSON_CreateArray();
	uint32_t tagset;
	if (m->isoverview) {
		cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
		return arr;
	}
	tagset = m->tagset[m->seltags];
	for (int i = 0; i < LENGTH(tags); i++)
		if (tagset & (1 << i))
			cJSON_AddItemToArray(arr, cJSON_CreateNumber(i + 1));
	return arr;
}

static cJSON *build_client_json(Client *c) {
	cJSON *obj = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj, "id", c->id);
	cJSON_AddNumberToObject(obj, "pid", c->pid);
	cJSON_AddStringToObject(obj, "title", client_get_title(c));
	cJSON_AddStringToObject(obj, "appid", client_get_appid(c));
	cJSON_AddStringToObject(obj, "monitor", c->mon->wlr_output->name);
	cJSON_AddItemToObject(obj, "tags", tags_mask_to_array(c->tags));
	cJSON_AddBoolToObject(obj, "is_focused", c == focustop(c->mon));
	cJSON_AddBoolToObject(obj, "is_fullscreen", c->isfullscreen);
	cJSON_AddBoolToObject(obj, "is_floating", c->isfloating);
	cJSON_AddBoolToObject(obj, "is_maximized", c->ismaximizescreen);
	cJSON_AddBoolToObject(obj, "is_global", c->isglobal);
	cJSON_AddBoolToObject(obj, "is_unglobal", c->isunglobal);
	cJSON_AddBoolToObject(obj, "is_overlay", c->isoverlay);
	cJSON_AddBoolToObject(obj, "is_fakefullscreen", c->isfakefullscreen);
	cJSON_AddBoolToObject(obj, "is_minimized", c->isminimized);
	cJSON_AddBoolToObject(obj, "is_urgent", c->isurgent);
	cJSON_AddBoolToObject(obj, "is_scratchpad", c->is_in_scratchpad);
	cJSON_AddBoolToObject(obj, "is_namedscratchpad", c->isnamedscratchpad);
	cJSON_AddNumberToObject(obj, "x", c->geom.x);
	cJSON_AddNumberToObject(obj, "y", c->geom.y);
	cJSON_AddNumberToObject(obj, "width", c->geom.width);
	cJSON_AddNumberToObject(obj, "height", c->geom.height);
	return obj;
}

static cJSON *build_monitor_json(Monitor *m) {
	cJSON *resp = cJSON_CreateObject();
	cJSON_AddStringToObject(resp, "name", m->wlr_output->name);
	cJSON_AddBoolToObject(resp, "active", m == selmon);
	cJSON_AddNumberToObject(resp, "x", m->m.x);
	cJSON_AddNumberToObject(resp, "y", m->m.y);
	cJSON_AddNumberToObject(resp, "width", m->m.width);
	cJSON_AddNumberToObject(resp, "height", m->m.height);
	cJSON_AddNumberToObject(resp, "scale", m->wlr_output->scale);
	cJSON_AddNumberToObject(resp, "layout_index",
							m->pertag->ltidxs[m->pertag->curtag] - layouts);
	cJSON_AddStringToObject(resp, "layout_symbol",
							m->pertag->ltidxs[m->pertag->curtag]->symbol);
	cJSON_AddStringToObject(resp, "last_open_surface", m->last_open_surface);
	cJSON_AddItemToObject(resp, "tags", build_tags_json(m));
	cJSON_AddItemToObject(resp, "active_tags", monitor_active_tags(m));
	cJSON_AddItemToObject(resp, "active_client", monitor_active_client(m));
	cJSON_AddItemToObject(resp, "keymode", cJSON_CreateString(keymode.mode));
	cJSON_AddItemToObject(resp, "keyboardlayout",
						  cJSON_CreateString(ipc_get_layout_str()));
	return resp;
}

static cJSON *build_all_tags_entry(Monitor *m) {
	cJSON *entry = cJSON_CreateObject();
	cJSON_AddStringToObject(entry, "monitor", m->wlr_output->name);
	cJSON_AddItemToObject(entry, "tags", build_tags_json(m));
	return entry;
}

static cJSON *build_all_tags_response(void) {
	cJSON *arr = cJSON_CreateArray();
	Monitor *m;
	wl_list_for_each(m, &mons, link)
		cJSON_AddItemToArray(arr, build_all_tags_entry(m));
	cJSON *resp = cJSON_CreateObject();
	cJSON_AddItemToObject(resp, "all_tags", arr);
	return resp;
}

static cJSON *build_monitor_tags_response(Monitor *m) {
	cJSON *resp = cJSON_CreateObject();
	cJSON_AddStringToObject(resp, "monitor", m->wlr_output->name);
	cJSON_AddItemToObject(resp, "tags", build_tags_json(m));
	cJSON_AddItemToObject(resp, "active_tags", monitor_active_tags(m));
	return resp;
}

static void send_static_json(int fd, const char *json_str) {
	size_t len = strlen(json_str);
	send(fd, json_str, len, MSG_NOSIGNAL);
}

/* ---------- 一次性命令处理 ---------- */
static void handle_command(int client_fd, const char *cmd_raw) {
	cJSON *resp = NULL;
	char *json_str = NULL;
	char cmd[1024];

	strncpy(cmd, cmd_raw, sizeof(cmd) - 1);
	cmd[sizeof(cmd) - 1] = '\0';
	for (char *p = cmd; *p; p++)
		if (*p == ',')
			*p = ' ';

	if (strcmp(cmd, "get version") == 0) {
		resp = cJSON_CreateObject();
		cJSON_AddStringToObject(resp, "version", VERSION);
	} else if (strcmp(cmd, "get keymode") == 0) {
		resp = cJSON_CreateObject();
		cJSON_AddStringToObject(resp, "keymode", keymode.mode);
	} else if (strcmp(cmd, "get keyboardlayout") == 0) {
		resp = cJSON_CreateObject();
		cJSON_AddStringToObject(resp, "layout", ipc_get_layout_str());
	} else if (strncmp(cmd, "get last_open_surface ", 25) == 0) {
		const char *name = cmd + 25;
		Monitor *m = monitor_by_name(name);
		if (!m) {
			send_static_json(client_fd, "{\"error\":\"monitor not found\"}\n");
			return;
		}
		resp = cJSON_CreateObject();
		cJSON_AddStringToObject(resp, "monitor", m->wlr_output->name);
		cJSON_AddStringToObject(resp, "last_open_surface",
								m->last_open_surface);
	} else if (strncmp(cmd, "get monitor ", 12) == 0) {
		Monitor *m = monitor_by_name(cmd + 12);
		if (!m) {
			send_static_json(client_fd, "{\"error\":\"monitor not found\"}\n");
			return;
		}
		resp = build_monitor_json(m);
	} else if (strncmp(cmd, "get client ", 11) == 0) {
		Client *c = client_by_id((uint32_t)atoi(cmd + 11));
		if (!c) {
			send_static_json(client_fd, "{\"error\":\"client not found\"}\n");
			return;
		}
		resp = build_client_json(c);
	} else if (strncmp(cmd, "get tag ", 8) == 0) {
		char mon_name[64];
		int ext_tag_idx;
		if (sscanf(cmd + 8, "%63s %d", mon_name, &ext_tag_idx) != 2) {
			send_static_json(
				client_fd,
				"{\"error\":\"usage: get tag <monitor> <index>\"}\n");
			return;
		}
		int tag_idx = ext_tag_idx - 1;
		Monitor *m = monitor_by_name(mon_name);
		if (!m || tag_idx < 0 || tag_idx >= LENGTH(tags)) {
			send_static_json(client_fd,
							 "{\"error\":\"invalid monitor or tag index\"}\n");
			return;
		}
		uint32_t tagmask = 1 << tag_idx;
		int numclients = 0, focused_client = 0;
		bool is_active = false, is_urgent = false;
		if (tagmask & m->tagset[m->seltags])
			is_active = true;

		Client *c, *focused = focustop(m);
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m || !(c->tags & tagmask))
				continue;
			if (c == focused)
				focused_client = 1;
			if (c->isurgent)
				is_urgent = true;
			numclients++;
		}
		resp = cJSON_CreateObject();
		cJSON_AddStringToObject(resp, "monitor", m->wlr_output->name);
		cJSON_AddNumberToObject(resp, "tag_index", ext_tag_idx);
		cJSON_AddBoolToObject(resp, "is_active", is_active);
		cJSON_AddBoolToObject(resp, "is_urgent", is_urgent);
		cJSON_AddNumberToObject(resp, "client_count", numclients);
		cJSON_AddBoolToObject(resp, "focused_client", focused_client);
	} else if (strcmp(cmd, "get all-clients") == 0) {
		cJSON *arr = cJSON_CreateArray();
		Client *c;
		wl_list_for_each(c, &clients, link)
			cJSON_AddItemToArray(arr, build_client_json(c));
		resp = cJSON_CreateObject();
		cJSON_AddItemToObject(resp, "clients", arr);
	} else if (strcmp(cmd, "get all-monitors") == 0) {
		cJSON *arr = cJSON_CreateArray();
		Monitor *m;
		wl_list_for_each(m, &mons, link)
			cJSON_AddItemToArray(arr, build_monitor_json(m));
		resp = cJSON_CreateObject();
		cJSON_AddItemToObject(resp, "monitors", arr);
	} else if (strcmp(cmd, "get all-tags") == 0) {
		resp = build_all_tags_response();
	} else if (strncmp(cmd, "get tags ", 9) == 0) {
		Monitor *m = monitor_by_name(cmd + 9);
		if (!m) {
			send_static_json(client_fd, "{\"error\":\"monitor not found\"}\n");
			return;
		}
		resp = build_monitor_tags_response(m);
	} else if (strncmp(cmd, "dispatch ", 9) == 0) {
		char *dispatch_copy = strdup(cmd_raw + 9);
		char *out = dispatch_copy, *ptr = dispatch_copy;
		int client_id = -1;
		while (*ptr) {
			while (*ptr == ' ' || *ptr == '\t')
				*out++ = *ptr++;
			if (strncmp(ptr, "client,", 7) == 0) {
				char *end;
				long id = strtol(ptr + 7, &end, 10);
				if (id > 0 && end > ptr + 7 && (*end == '\0' || *end == ',')) {
					client_id = (int)id;
					ptr = end;
					if (*ptr == ',')
						ptr++;
					continue;
				}
			}
			*out++ = *ptr++;
		}
		*out = '\0';

		char *tokens[6] = {NULL};
		int token_count = 0;
		char *saveptr;
		char *token = strtok_r(dispatch_copy, ",", &saveptr);
		while (token && token_count < 6) {
			while (*token == ' ' || *token == '\t')
				token++;
			char *end = token + strlen(token) - 1;
			while (end >= token && (*end == ' ' || *end == '\t'))
				*end-- = '\0';
			tokens[token_count++] = token;
			token = strtok_r(NULL, ",", &saveptr);
		}

		Arg arg = {0};
		int32_t (*func)(const Arg *) =
			parse_func_name(token_count > 0 ? tokens[0] : "", &arg,
							token_count > 1 ? tokens[1] : NULL,
							token_count > 2 ? tokens[2] : NULL,
							token_count > 3 ? tokens[3] : NULL,
							token_count > 4 ? tokens[4] : NULL,
							token_count > 5 ? tokens[5] : NULL);

		if (func && client_id > 0)
			arg.tc = client_by_id((uint32_t)client_id);

		if (func) {
			func(&arg);
			send_static_json(client_fd, "{\"success\":true}\n");
		} else {
			send_static_json(client_fd, "{\"error\":\"unknown function\"}\n");
		}

		if (arg.v)
			free(arg.v);
		if (arg.v2)
			free(arg.v2);
		if (arg.v3)
			free(arg.v3);
		free(dispatch_copy);
		return; // Fast path exit
	} else {
		send_static_json(client_fd, "{\"error\":\"unknown command\"}\n");
		return;
	}

	if (resp) {
		json_str = cJSON_PrintUnformatted(resp);
		if (json_str) {
			size_t len = strlen(json_str);
			char *msg = malloc(len + 2);
			if (msg) {
				snprintf(msg, len + 2, "%s\n", json_str);
				send(client_fd, msg, len + 1, MSG_NOSIGNAL);
				free(msg);
			}
			free(json_str);
		}
		cJSON_Delete(resp);
	}
}

/* ---------- Watch 模式支持 ---------- */
static void ipc_notify_json_to_fd(int fd, cJSON *json) {
	char *str = cJSON_PrintUnformatted(json);
	if (!str)
		return;
	size_t len = strlen(str);
	char *msg = malloc(len + 2);
	if (!msg) {
		free(str);
		return;
	}
	snprintf(msg, len + 2, "%s\n", str);
	if (send(fd, msg, len + 1, MSG_NOSIGNAL) < 0) {
		struct ipc_watch_client *wc, *tmp;
		wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
			if (wc->fd == fd) {
				ipc_remove_watch_client(wc);
				break;
			}
		}
	}
	free(msg);
	free(str);
}

static void ipc_remove_watch_client(struct ipc_watch_client *wc) {
	wl_list_remove(&wc->link);
	wl_event_source_remove(wc->source);
	close(wc->fd);
	free(wc);
}

static int ipc_watch_data_handler(int fd, uint32_t mask, void *data) {
	struct ipc_watch_client *wc = data;
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		ipc_remove_watch_client(wc);
		return 0;
	}
	if (mask & WL_EVENT_READABLE) {
		char buf[64];
		ssize_t n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
		if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
			ipc_remove_watch_client(wc);
		}
	}
	return 0;
}

static bool handle_watch_command(int fd, const char *cmd,
								 struct ipc_client_state *client) {
	enum ipc_watch_type type = IPC_WATCH_NONE;
	const char *arg = NULL;
	uint32_t client_id = 0;

	if (strncmp(cmd, "watch monitor ", 14) == 0) {
		type = IPC_WATCH_MONITOR;
		arg = cmd + 14;
	} else if (strncmp(cmd, "watch client ", 13) == 0) {
		type = IPC_WATCH_CLIENT;
		client_id = (uint32_t)atoi(cmd + 13);
	} else if (strncmp(cmd, "watch tags ", 11) == 0) {
		type = IPC_WATCH_TAGS;
		arg = cmd + 11;
	} else if (strcmp(cmd, "watch all-monitors") == 0) {
		type = IPC_WATCH_ALL_MONITORS;
	} else if (strcmp(cmd, "watch all-tags") == 0) {
		type = IPC_WATCH_ALL_TAGS;
	} else if (strcmp(cmd, "watch all-clients") == 0) {
		type = IPC_WATCH_ALL_CLIENTS;
	} else if (strcmp(cmd, "watch keymode") == 0) {
		type = IPC_WATCH_KEYMODE;
	} else if (strcmp(cmd, "watch keyboardlayout") == 0) {
		type = IPC_WATCH_KB_LAYOUT;
	} else if (strncmp(cmd, "watch last_open_surface ", 27) == 0) {
		type = IPC_WATCH_LAST_OPEN_SURFACE;
		arg = cmd + 27;
	}

	if (type == IPC_WATCH_NONE)
		return false;

	struct ipc_watch_client *wc = calloc(1, sizeof(*wc));
	wc->fd = fd;
	wc->type = type;

	if ((type == IPC_WATCH_MONITOR || type == IPC_WATCH_LAST_OPEN_SURFACE) &&
		arg)
		snprintf(wc->target.monitor.name, sizeof(wc->target.monitor.name), "%s",
				 arg);
	else if (type == IPC_WATCH_TAGS && arg)
		snprintf(wc->target.tags.mon_name, sizeof(wc->target.tags.mon_name),
				 "%s", arg);
	else if (type == IPC_WATCH_CLIENT)
		wc->target.client.id = client_id;

	wl_event_source_remove(client->source);
	wc->source = wl_event_loop_add_fd(
		client->loop, fd, WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,
		ipc_watch_data_handler, wc);
	wl_list_insert(&watch_clients, &wc->link);

	/* 推送初始状态 */
	cJSON *json = NULL;
	switch (type) {
	case IPC_WATCH_MONITOR: {
		Monitor *m = monitor_by_name(arg);
		if (m)
			json = build_monitor_json(m);
		break;
	}
	case IPC_WATCH_LAST_OPEN_SURFACE: {
		Monitor *m = monitor_by_name(arg);
		if (m) {
			json = cJSON_CreateObject();
			cJSON_AddStringToObject(json, "monitor", m->wlr_output->name);
			cJSON_AddStringToObject(json, "last_open_surface",
									m->last_open_surface);
		}
		break;
	}
	case IPC_WATCH_CLIENT: {
		Client *c = client_by_id(client_id);
		if (c)
			json = build_client_json(c);
		break;
	}
	case IPC_WATCH_TAGS: {
		Monitor *m = monitor_by_name(arg);
		if (m)
			json = build_monitor_tags_response(m);
		break;
	}
	case IPC_WATCH_ALL_MONITORS: {
		cJSON *arr = cJSON_CreateArray();
		Monitor *m;
		wl_list_for_each(m, &mons, link)
			cJSON_AddItemToArray(arr, build_monitor_json(m));
		json = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "monitors", arr);
		break;
	}
	case IPC_WATCH_ALL_TAGS: {
		json = build_all_tags_response();
		break;
	}
	case IPC_WATCH_ALL_CLIENTS: {
		cJSON *arr = cJSON_CreateArray();
		Client *c;
		wl_list_for_each(c, &clients, link)
			cJSON_AddItemToArray(arr, build_client_json(c));
		json = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "clients", arr);
		break;
	}
	case IPC_WATCH_KEYMODE: {
		json = cJSON_CreateObject();
		cJSON_AddStringToObject(json, "keymode", keymode.mode);
		break;
	}
	case IPC_WATCH_KB_LAYOUT: {
		json = cJSON_CreateObject();
		cJSON_AddStringToObject(json, "layout", ipc_get_layout_str());
		break;
	}
	default:
		break;
	}

	if (json) {
		ipc_notify_json_to_fd(fd, json);
		cJSON_Delete(json);
	}

	free(client->buf);
	free(client);
	return true;
}

/* ---------- Socket 事件处理 ---------- */
static int ipc_handle_client_data(int fd, uint32_t mask, void *data) {
	struct ipc_client_state *client = data;
	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR))
		goto cleanup;

	if (mask & WL_EVENT_READABLE) {
		size_t available = client->buf_cap - client->buf_len;
		if (available < 4096) {
			size_t new_cap = client->buf_cap ? client->buf_cap * 2 : 8192;
			char *new_buf = realloc(client->buf, new_cap);
			if (!new_buf) {
				wlr_log(WLR_ERROR, "IPC: out of memory");
				goto cleanup;
			}
			client->buf = new_buf;
			client->buf_cap = new_cap;
			available = client->buf_cap - client->buf_len;
		}

		// 直接读取到 client->buf 尾部，跳过临时数组
		ssize_t n = recv(fd, client->buf + client->buf_len, available - 1,
						 MSG_DONTWAIT);
		if (n <= 0)
			goto cleanup;

		client->buf_len += n;
		client->buf[client->buf_len] = '\0';

		char *nl = memchr(client->buf, '\n', client->buf_len);
		if (!nl) {
			if (client->buf_len > 1024 * 1024)
				goto cleanup; // 防御过长命令
			return 0;
		}
		*nl = '\0';
		char *cmd = client->buf;

		bool is_watch = handle_watch_command(fd, cmd, client);
		if (is_watch)
			return 0;

		handle_command(fd, cmd);
		goto cleanup;
	}
	return 0;

cleanup:
	close(client->fd);
	wl_event_source_remove(client->source);
	free(client->buf);
	free(client);
	return 0;
}

static int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	struct wl_event_loop *loop = data;
	int client_fd = accept(fd, NULL, NULL);
	if (client_fd < 0)
		return 0;

	int flags = fcntl(client_fd, F_GETFL, 0);
	fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

	struct ipc_client_state *client = calloc(1, sizeof(*client));
	client->fd = client_fd;
	client->loop = loop;
	client->source = wl_event_loop_add_fd(
		loop, client_fd, WL_EVENT_READABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,
		ipc_handle_client_data, client);
	return 0;
}

/* ---------- 外部通知接口 ---------- */

void ipc_notify_monitor(Monitor *m) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_MONITOR &&
			strcmp(m->wlr_output->name, wc->target.monitor.name) == 0) {
			if (!json_str) {
				cJSON *json = build_monitor_json(m);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_last_surface_ws_name(Monitor *m) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_LAST_OPEN_SURFACE &&
			strcmp(m->wlr_output->name, wc->target.monitor.name) == 0) {
			if (!json_str) {
				cJSON *json = cJSON_CreateObject();
				cJSON_AddStringToObject(json, "monitor", m->wlr_output->name);
				cJSON_AddStringToObject(json, "last_open_surface",
										m->last_open_surface);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_client(Client *c) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_CLIENT && c->id == wc->target.client.id) {
			if (!json_str) {
				cJSON *json = build_client_json(c);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_tags(Monitor *m) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_TAGS &&
			strcmp(m->wlr_output->name, wc->target.tags.mon_name) == 0) {
			if (!json_str) {
				cJSON *json = build_monitor_tags_response(m);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_all_monitors(void) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_ALL_MONITORS) {
			if (!json_str) {
				cJSON *arr = cJSON_CreateArray();
				Monitor *m;
				wl_list_for_each(m, &mons, link)
					cJSON_AddItemToArray(arr, build_monitor_json(m));
				cJSON *json = cJSON_CreateObject();
				cJSON_AddItemToObject(json, "monitors", arr);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_all_clients(void) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_ALL_CLIENTS) {
			if (!json_str) {
				cJSON *arr = cJSON_CreateArray();
				Client *c;
				wl_list_for_each(c, &clients, link)
					cJSON_AddItemToArray(arr, build_client_json(c));
				cJSON *json = cJSON_CreateObject();
				cJSON_AddItemToObject(json, "clients", arr);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_all_tags(void) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_ALL_TAGS) {
			if (!json_str) {
				cJSON *json = build_all_tags_response();
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_keymode(void) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_KEYMODE) {
			if (!json_str) {
				cJSON *json = cJSON_CreateObject();
				cJSON_AddStringToObject(json, "keymode", keymode.mode);
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

void ipc_notify_kb_layout(void) {
	char *json_str = NULL;
	size_t len = 0;
	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link) {
		if (wc->type == IPC_WATCH_KB_LAYOUT) {
			if (!json_str) {
				cJSON *json = cJSON_CreateObject();
				cJSON_AddStringToObject(json, "layout", ipc_get_layout_str());
				char *raw = cJSON_PrintUnformatted(json);
				cJSON_Delete(json);
				if (!raw)
					return;
				len = strlen(raw);
				json_str = malloc(len + 2);
				snprintf(json_str, len + 2, "%s\n", raw);
				free(raw);
			}
			if (send(wc->fd, json_str, len + 1, MSG_NOSIGNAL) < 0)
				ipc_remove_watch_client(wc);
		}
	}
	if (json_str)
		free(json_str);
}

/* ---------- 初始化与清理 ---------- */
static int ipc_sock_fd = -1;
static struct wl_event_source *ipc_event_source = NULL;
static char ipc_socket_path[256];

void ipc_init(struct wl_event_loop *event_loop) {
	wl_list_init(&watch_clients);

	const char *xdg_runtime = getenv("XDG_RUNTIME_DIR");
	if (!xdg_runtime)
		return;

	snprintf(ipc_socket_path, sizeof(ipc_socket_path), "%s/mango-%d.sock",
			 xdg_runtime, getpid());

	ipc_sock_fd =
		socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ipc_sock_fd < 0)
		return;

	struct sockaddr_un addr = {.sun_family = AF_UNIX};
	strncpy(addr.sun_path, ipc_socket_path, sizeof(addr.sun_path) - 1);

	unlink(ipc_socket_path);
	if (bind(ipc_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(ipc_sock_fd);
		return;
	}
	listen(ipc_sock_fd, 16);

	setenv("MANGO_INSTANCE_SIGNATURE", ipc_socket_path, 1);

	ipc_event_source =
		wl_event_loop_add_fd(event_loop, ipc_sock_fd, WL_EVENT_READABLE,
							 ipc_handle_connection, event_loop);
}

void ipc_cleanup(void) {
	if (ipc_event_source)
		wl_event_source_remove(ipc_event_source);
	if (ipc_sock_fd >= 0)
		close(ipc_sock_fd);
	unlink(ipc_socket_path);
	unsetenv("MANGO_INSTANCE_SIGNATURE");

	struct ipc_watch_client *wc, *tmp;
	wl_list_for_each_safe(wc, tmp, &watch_clients, link)
		ipc_remove_watch_client(wc);
}