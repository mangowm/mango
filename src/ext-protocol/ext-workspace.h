#include "wlr_ext_workspace_v1.h"

#define EXT_WORKSPACE_ENABLE_CAPS                                              \
	EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE |                  \
		EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_DEACTIVATE

typedef struct Monitor Monitor;

struct workspace {
	struct wl_list link; // Link in global workspaces list
	uint32_t tag;		 // Numeric identifier (1-9, 0=overview)
	Monitor *m;			 // Associated monitor
	struct wlr_ext_workspace_handle_v1 *ext_workspace; // Protocol object
	/* Event listeners */
	struct wl_listener activate;
	struct wl_listener deactivate;
	struct wl_listener assign;
	struct wl_listener remove;
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

static void handle_ext_workspace_activate(struct wl_listener *listener,
										  void *data) {
	struct workspace *workspace =
		wl_container_of(listener, workspace, activate);

	if (workspace->m->isoverview) {
		return;
	}

	goto_workspace(workspace);
	wlr_log(WLR_INFO, "ext activating workspace %d", workspace->tag);
}

static void handle_ext_workspace_deactivate(struct wl_listener *listener,
											void *data) {
	struct workspace *workspace =
		wl_container_of(listener, workspace, deactivate);

	if (workspace->m->isoverview) {
		return;
	}

	toggle_workspace(workspace);
	wlr_log(WLR_INFO, "ext deactivating workspace %d", workspace->tag);
}

static const char *get_name_from_tag(uint32_t tag) {
	static const char *names[] = {"overview", "1", "2", "3", "4",
								  "5",		  "6", "7", "8", "9"};
	return (tag < sizeof(names) / sizeof(names[0])) ? names[tag] : NULL;
}

void destroy_workspace(struct workspace *workspace) {
	wl_list_remove(&workspace->activate.link);
	wl_list_remove(&workspace->deactivate.link);
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
	wlr_ext_workspace_handle_v1_set_group(workspace->ext_workspace,
										  m->ext_group);
	wlr_ext_workspace_handle_v1_set_name(workspace->ext_workspace, name);

	workspace->activate.notify = handle_ext_workspace_activate;
	wl_signal_add(&workspace->ext_workspace->events.activate,
				  &workspace->activate);

	workspace->deactivate.notify = handle_ext_workspace_deactivate;
	wl_signal_add(&workspace->ext_workspace->events.deactivate,
				  &workspace->deactivate);
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
	/* Create the global workspace manager with activation capability */
	ext_manager = wlr_ext_workspace_manager_v1_create(dpy, 1);
	/* Initialize the global workspaces list */
	wl_list_init(&workspaces);
}