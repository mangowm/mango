#include <wlr/types/wlr_ext_workspace_v1.h>

#define EXT_WORKSPACE_ENABLE_CAPS                                              \
	EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE |                  \
		EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE

typedef struct Monitor Monitor;

struct workspace {
	struct wl_list link;
	uint32_t tag;
	Monitor *m;
	struct wlr_ext_workspace_handle_v1 *ext_workspace;
	struct wl_listener commit;
};

struct wlr_ext_workspace_manager_v1 *ext_manager;
struct wl_list workspaces;

void goto_workspace(struct workspace *target) {
	uint32_t tag;
	tag = 1 << (target->tag - 1);
	if (target->tag == 0) {
		toggleoverview(&(Arg){.i = -1});
		return;
	} else {
		view(&(Arg){.ui = tag}, true);
	}
}

void toggle_workspace(struct workspace *target) {
	uint32_t tag;
	tag = 1 << (target->tag - 1);
	if (target->tag == 0) {
		toggleview(&(Arg){.i = -1});
		return;
	} else {
		toggleview(&(Arg){.ui = tag});
	}
}

static void handle_ext_commit(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_v1_commit_event *event = data;
	struct wlr_ext_workspace_v1_request *request;

	wl_list_for_each(request, event->requests, link) {
		switch (request->type) {
		case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE: {
			if (!request->activate.workspace) {
				break;
			}

			struct workspace *workspace = NULL;
			struct workspace *w;
			wl_list_for_each(w, &workspaces, link) {
				if (w->ext_workspace == request->activate.workspace) {
					workspace = w;
					break;
				}
			}

			if (!workspace || workspace->m->isoverview) {
				break;
			}

			goto_workspace(workspace);
			wlr_log(WLR_INFO, "ext activating workspace %d", workspace->tag);
			break;
		}
		case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE: {
			if (!request->deactivate.workspace) {
				break;
			}

			struct workspace *workspace = NULL;
			struct workspace *w;
			wl_list_for_each(w, &workspaces, link) {
				if (w->ext_workspace == request->deactivate.workspace) {
					workspace = w;
					break;
				}
			}

			if (!workspace || workspace->m->isoverview) {
				break;
			}

			toggle_workspace(workspace);
			wlr_log(WLR_INFO, "ext deactivating workspace %d", workspace->tag);
			break;
		}
		default:
			break;
		}
	}
}

static const char *get_name_from_tag(uint32_t tag) {
	static const char *names[] = {"overview", "1", "2", "3", "4",
								  "5",		  "6", "7", "8", "9"};
	return (tag < sizeof(names) / sizeof(names[0])) ? names[tag] : NULL;
}

void destroy_workspace(struct workspace *workspace) {
	wlr_ext_workspace_handle_v1_destroy(workspace->ext_workspace);
	wl_list_remove(&workspace->link);
	free(workspace);
}

void cleanup_workspaces_by_monitor(Monitor *m) {
	struct workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &workspaces, link) {
		if (workspace->m == m) {
			destroy_workspace(workspace);
		}
	}
}

static void remove_workspace_by_tag(uint32_t tag, Monitor *m) {
	struct workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &workspaces, link) {
		if (workspace->tag == tag && workspace->m == m) {
			destroy_workspace(workspace);
			return;
		}
	}
}

static void add_workspace_by_tag(int32_t tag, Monitor *m) {
	const char *name = get_name_from_tag(tag);

	struct workspace *workspace = ecalloc(1, sizeof(*workspace));
	wl_list_append(&workspaces, &workspace->link);

	workspace->tag = tag;
	workspace->m = m;
	workspace->ext_workspace = wlr_ext_workspace_handle_v1_create(
		ext_manager, name, EXT_WORKSPACE_ENABLE_CAPS);

	workspace->ext_workspace->data = workspace;

	wlr_ext_workspace_handle_v1_set_group(workspace->ext_workspace,
										  m->ext_group);
	wlr_ext_workspace_handle_v1_set_name(workspace->ext_workspace, name);
}

void dwl_ext_workspace_printstatus(Monitor *m) {
	struct workspace *w;
	uint32_t tag_status = 0;

	wl_list_for_each(w, &workspaces, link) {
		if (w && w->m == m) {

			tag_status = get_tag_status(w->tag, m);
			if (tag_status == 2) {
				wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace, false);
				wlr_ext_workspace_handle_v1_set_urgent(w->ext_workspace, true);
			} else if (tag_status == 1) {
				wlr_ext_workspace_handle_v1_set_urgent(w->ext_workspace, false);
				wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace, false);
			} else {
				wlr_ext_workspace_handle_v1_set_urgent(w->ext_workspace, false);
				if (!w->m->pertag->no_hide[w->tag])
					wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace,
														   true);
				else {
					wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace,
														   false);
				}
			}

			if ((m->tagset[m->seltags] & (1 << (w->tag - 1)) & TAGMASK) ||
				m->isoverview) {
				wlr_ext_workspace_handle_v1_set_hidden(w->ext_workspace, false);
				wlr_ext_workspace_handle_v1_set_active(w->ext_workspace, true);
			} else {
				wlr_ext_workspace_handle_v1_set_active(w->ext_workspace, false);
			}
		}
	}
}

void refresh_monitors_workspaces_status(Monitor *m) {
	int32_t i;

	if (m->isoverview) {
		for (i = 1; i <= LENGTH(tags); i++) {
			remove_workspace_by_tag(i, m);
		}
		add_workspace_by_tag(0, m);
	} else {
		remove_workspace_by_tag(0, m);
		for (i = 1; i <= LENGTH(tags); i++) {
			add_workspace_by_tag(i, m);
		}
	}

	dwl_ext_workspace_printstatus(m);
}

void workspaces_init() {
	ext_manager = wlr_ext_workspace_manager_v1_create(dpy, 1);

	wl_list_init(&workspaces);

	static struct wl_listener commit_listener;
	commit_listener.notify = handle_ext_commit;
	wl_signal_add(&ext_manager->events.commit, &commit_listener);
}