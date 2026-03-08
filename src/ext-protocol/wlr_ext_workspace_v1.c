// bash on: https://gitlab.freedesktop.org/tokyo4j/wlroots/-/tree/ext-workspace
// TODO: remove this file
// refer: https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/5115

#include "wlr_ext_workspace_v1.h"
#include "ext-workspace-v1-protocol.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>

#define EXT_WORKSPACE_V1_VERSION 1

enum wlr_ext_workspace_v1_request_type {
	WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE,
	WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE,
	WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE,
	WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN,
	WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE,
};

struct wlr_ext_workspace_v1_request {
	enum wlr_ext_workspace_v1_request_type type;

	// CREATE_WORKSPACE
	char *name;
	// CREATE_WORKSPACE / ASSIGN
	struct wlr_ext_workspace_group_handle_v1 *group;
	// ACTIVATE / DEACTIVATE / ASSIGN / REMOVE
	struct wlr_ext_workspace_handle_v1 *workspace;

	struct wl_list link; // wlr_ext_workspace_manager_v1_resource.requests
};

struct wlr_ext_workspace_v1_group_output {
	struct wlr_output *output;
	struct wlr_ext_workspace_group_handle_v1 *group;
	struct wl_listener output_bind;
	struct wl_listener output_destroy;
	struct wl_list link;
};

// These structs wrap wl_resource of each interface to access the request queue
// (wlr_ext_workspace_manager_v1_resource.requests) assigned per manager
// resource

struct wlr_ext_workspace_manager_v1_resource {
	struct wl_resource *resource;
	struct wlr_ext_workspace_manager_v1 *manager;
	struct wl_list requests;			// wlr_ext_workspace_v1_request.link
	struct wl_list workspace_resources; // wlr_ext_workspace_v1_resource.link
	struct wl_list group_resources; // wlr_ext_workspace_group_v1_resource.link
	struct wl_list link;			// wlr_ext_workspace_manager_v1.resources
};

struct wlr_ext_workspace_group_v1_resource {
	struct wl_resource *resource;
	struct wlr_ext_workspace_group_handle_v1 *group;
	struct wlr_ext_workspace_manager_v1_resource *manager;
	struct wl_list link; // wlr_ext_workspace_group_v1.resources
	struct wl_list
		manager_resource_link; // wlr_ext_workspace_manager_v1_resource.group_resources
};

struct wlr_ext_workspace_v1_resource {
	struct wl_resource *resource;
	struct wlr_ext_workspace_handle_v1 *workspace;
	struct wlr_ext_workspace_manager_v1_resource *manager;
	struct wl_list link; // wlr_ext_workspace_v1.resources
	struct wl_list
		manager_resource_link; // wlr_ext_workspace_manager_v1_resource.workspace_resources
};

static const struct ext_workspace_group_handle_v1_interface group_impl;

static struct wlr_ext_workspace_group_v1_resource *
group_resource_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(
		resource, &ext_workspace_group_handle_v1_interface, &group_impl));
	return wl_resource_get_user_data(resource);
}

static const struct ext_workspace_handle_v1_interface workspace_impl;

static struct wlr_ext_workspace_v1_resource *
workspace_resource_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &ext_workspace_handle_v1_interface,
								   &workspace_impl));
	return wl_resource_get_user_data(resource);
}

static const struct ext_workspace_manager_v1_interface manager_impl;

static struct wlr_ext_workspace_manager_v1_resource *
manager_resource_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(
		resource, &ext_workspace_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void workspace_handle_destroy(struct wl_client *client,
									 struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void workspace_handle_activate(struct wl_client *client,
									  struct wl_resource *workspace_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	if (!workspace_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE;
	req->workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

static void
workspace_handle_deactivate(struct wl_client *client,
							struct wl_resource *workspace_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	if (!workspace_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE;
	req->workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

static void workspace_handle_assign(struct wl_client *client,
									struct wl_resource *workspace_resource,
									struct wl_resource *group_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	struct wlr_ext_workspace_group_v1_resource *group_res =
		group_resource_from_resource(group_resource);
	if (!workspace_res || !group_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN;
	req->group = group_res->group;
	req->workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

static void workspace_handle_remove(struct wl_client *client,
									struct wl_resource *workspace_resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(workspace_resource);
	if (!workspace_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(workspace_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE;
	req->workspace = workspace_res->workspace;
	wl_list_insert(workspace_res->manager->requests.prev, &req->link);
}

static const struct ext_workspace_handle_v1_interface workspace_impl = {
	.destroy = workspace_handle_destroy,
	.activate = workspace_handle_activate,
	.deactivate = workspace_handle_deactivate,
	.assign = workspace_handle_assign,
	.remove = workspace_handle_remove,
};

static void group_handle_create_workspace(struct wl_client *client,
										  struct wl_resource *group_resource,
										  const char *name) {
	struct wlr_ext_workspace_group_v1_resource *group_res =
		group_resource_from_resource(group_resource);
	if (!group_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req = calloc(1, sizeof(*req));
	if (!req) {
		wl_resource_post_no_memory(group_resource);
		return;
	}
	req->name = strdup(name);
	if (!req->name) {
		free(req);
		wl_resource_post_no_memory(group_resource);
		return;
	}
	req->type = WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE;
	req->group = group_res->group;
	wl_list_insert(group_res->manager->requests.prev, &req->link);
}

static void group_handle_destroy(struct wl_client *client,
								 struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct ext_workspace_group_handle_v1_interface group_impl = {
	.create_workspace = group_handle_create_workspace,
	.destroy = group_handle_destroy,
};

static void destroy_workspace_resource(
	struct wlr_ext_workspace_v1_resource *workspace_res) {
	wl_list_remove(&workspace_res->link);
	wl_list_remove(&workspace_res->manager_resource_link);
	wl_resource_set_user_data(workspace_res->resource, NULL);
	free(workspace_res);
}

static void workspace_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		workspace_resource_from_resource(resource);
	if (workspace_res) {
		destroy_workspace_resource(workspace_res);
	}
}

static struct wlr_ext_workspace_v1_resource *create_workspace_resource(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	struct wlr_ext_workspace_v1_resource *workspace_res =
		calloc(1, sizeof(*workspace_res));
	if (!workspace_res) {
		return NULL;
	}

	struct wl_client *client = wl_resource_get_client(manager_res->resource);
	workspace_res->resource =
		wl_resource_create(client, &ext_workspace_handle_v1_interface,
						   wl_resource_get_version(manager_res->resource), 0);
	if (!workspace_res->resource) {
		free(workspace_res);
		return NULL;
	}
	wl_resource_set_implementation(workspace_res->resource, &workspace_impl,
								   workspace_res, workspace_resource_destroy);

	workspace_res->workspace = workspace;
	workspace_res->manager = manager_res;
	wl_list_insert(&workspace->resources, &workspace_res->link);
	wl_list_insert(&manager_res->workspace_resources,
				   &workspace_res->manager_resource_link);

	return workspace_res;
}

static void
destroy_group_resource(struct wlr_ext_workspace_group_v1_resource *group_res) {
	wl_list_remove(&group_res->link);
	wl_list_remove(&group_res->manager_resource_link);
	wl_resource_set_user_data(group_res->resource, NULL);
	free(group_res);
}

static void group_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_workspace_group_v1_resource *group_res =
		group_resource_from_resource(resource);
	if (group_res) {
		destroy_group_resource(group_res);
	}
}

static struct wlr_ext_workspace_group_v1_resource *create_group_resource(
	struct wlr_ext_workspace_group_handle_v1 *group,
	struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	struct wlr_ext_workspace_group_v1_resource *group_res =
		calloc(1, sizeof(*group_res));
	if (!group_res) {
		return NULL;
	}

	struct wl_client *client = wl_resource_get_client(manager_res->resource);
	uint32_t version = wl_resource_get_version(manager_res->resource);
	group_res->resource = wl_resource_create(
		client, &ext_workspace_group_handle_v1_interface, version, 0);
	if (group_res->resource == NULL) {
		free(group_res);
		return NULL;
	}
	wl_resource_set_implementation(group_res->resource, &group_impl, group_res,
								   group_handle_resource_destroy);

	group_res->group = group;
	group_res->manager = manager_res;
	wl_list_insert(&group->resources, &group_res->link);
	wl_list_insert(&manager_res->group_resources,
				   &group_res->manager_resource_link);

	return group_res;
}

static void destroy_request(struct wlr_ext_workspace_v1_request *req) {
	wl_list_remove(&req->link);
	free(req->name);
	free(req);
}

static void manager_handle_commit(struct wl_client *client,
								  struct wl_resource *resource) {
	struct wlr_ext_workspace_manager_v1_resource *manager_res =
		manager_resource_from_resource(resource);
	if (!manager_res) {
		return;
	}

	struct wlr_ext_workspace_v1_request *req, *tmp;
	wl_list_for_each_safe(req, tmp, &manager_res->requests, link) {
		switch (req->type) {
		case WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE:;
			struct wlr_ext_workspace_group_handle_v1_create_workspace_event
				event = {
					.name = req->name,
				};
			wl_signal_emit_mutable(&req->group->events.create_workspace,
								   &event);
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE:
			wl_signal_emit_mutable(&req->workspace->events.activate, NULL);
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE:
			wl_signal_emit_mutable(&req->workspace->events.deactivate, NULL);
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN:
			wl_signal_emit_mutable(&req->workspace->events.assign, req->group);
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE:
			wl_signal_emit_mutable(&req->workspace->events.remove, NULL);
			break;
		}
		destroy_request(req);
	}
}

static void handle_idle(void *data) {
	struct wlr_ext_workspace_manager_v1 *manager = data;

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &manager->resources, link) {
		ext_workspace_manager_v1_send_done(manager_res->resource);
	}
	manager->idle_source = NULL;
}

static void
manager_schedule_done(struct wlr_ext_workspace_manager_v1 *manager) {
	if (!manager->idle_source) {
		manager->idle_source =
			wl_event_loop_add_idle(manager->event_loop, handle_idle, manager);
	}
}

static void
workspace_send_details(struct wlr_ext_workspace_v1_resource *workspace_res) {
	struct wlr_ext_workspace_handle_v1 *workspace = workspace_res->workspace;
	struct wl_resource *resource = workspace_res->resource;

	ext_workspace_handle_v1_send_capabilities(resource, workspace->caps);
	if (workspace->coordinates.size > 0) {
		ext_workspace_handle_v1_send_coordinates(resource,
												 &workspace->coordinates);
	}
	if (workspace->name) {
		ext_workspace_handle_v1_send_name(resource, workspace->name);
	}
	if (workspace->id) {
		ext_workspace_handle_v1_send_id(resource, workspace->id);
	}
	ext_workspace_handle_v1_send_state(resource, workspace->state);
	manager_schedule_done(workspace->manager);
}

static void manager_handle_stop(struct wl_client *client,
								struct wl_resource *resource) {
	ext_workspace_manager_v1_send_finished(resource);
	wl_resource_destroy(resource);
}

static const struct ext_workspace_manager_v1_interface manager_impl = {
	.commit = manager_handle_commit,
	.stop = manager_handle_stop,
};

static void destroy_manager_resource(
	struct wlr_ext_workspace_manager_v1_resource *manager_res) {
	struct wlr_ext_workspace_v1_request *req, *tmp;
	wl_list_for_each_safe(req, tmp, &manager_res->requests, link) {
		destroy_request(req);
	}
	struct wlr_ext_workspace_v1_resource *workspace_res, *tmp2;
	wl_list_for_each_safe(workspace_res, tmp2,
						  &manager_res->workspace_resources,
						  manager_resource_link) {
		destroy_workspace_resource(workspace_res);
	}
	struct wlr_ext_workspace_group_v1_resource *group_res, *tmp3;
	wl_list_for_each_safe(group_res, tmp3, &manager_res->group_resources,
						  manager_resource_link) {
		destroy_group_resource(group_res);
	}

	wl_list_remove(&manager_res->link);
	wl_resource_set_user_data(manager_res->resource, NULL);
	free(manager_res);
}

static void manager_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_workspace_manager_v1_resource *manager_res =
		manager_resource_from_resource(resource);
	if (manager_res) {
		destroy_manager_resource(manager_res);
	}
}

static void
group_send_details(struct wlr_ext_workspace_group_v1_resource *group_res) {
	struct wlr_ext_workspace_group_handle_v1 *group = group_res->group;
	struct wl_resource *resource = group_res->resource;
	struct wl_client *client = wl_resource_get_client(resource);

	ext_workspace_group_handle_v1_send_capabilities(resource, group->caps);

	struct wlr_ext_workspace_v1_group_output *group_output;
	wl_list_for_each(group_output, &group->outputs, link) {
		struct wl_resource *output_resource;
		wl_resource_for_each(output_resource,
							 &group_output->output->resources) {
			if (wl_resource_get_client(output_resource) == client) {
				ext_workspace_group_handle_v1_send_output_enter(
					resource, output_resource);
			}
		}
	}

	manager_schedule_done(group->manager);
}

static void manager_bind(struct wl_client *client, void *data, uint32_t version,
						 uint32_t id) {
	struct wlr_ext_workspace_manager_v1 *manager = data;

	struct wlr_ext_workspace_manager_v1_resource *manager_res =
		calloc(1, sizeof(*manager_res));
	if (!manager_res) {
		wl_client_post_no_memory(client);
		return;
	}

	manager_res->manager = manager;
	wl_list_init(&manager_res->requests);
	wl_list_init(&manager_res->workspace_resources);
	wl_list_init(&manager_res->group_resources);

	manager_res->resource = wl_resource_create(
		client, &ext_workspace_manager_v1_interface, version, id);
	if (!manager_res->resource) {
		free(manager_res);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(manager_res->resource, &manager_impl,
								   manager_res, manager_resource_destroy);
	wl_list_insert(&manager->resources, &manager_res->link);

	struct wlr_ext_workspace_group_handle_v1 *group;
	wl_list_for_each(group, &manager->groups, link) {
		struct wlr_ext_workspace_group_v1_resource *group_res =
			create_group_resource(group, manager_res);
		if (!group_res) {
			wl_resource_post_no_memory(manager_res->resource);
			continue;
		}
		ext_workspace_manager_v1_send_workspace_group(manager_res->resource,
													  group_res->resource);
		group_send_details(group_res);
	}

	struct wlr_ext_workspace_handle_v1 *workspace;
	wl_list_for_each(workspace, &manager->workspaces, link) {
		struct wlr_ext_workspace_v1_resource *workspace_res =
			create_workspace_resource(workspace, manager_res);
		if (!workspace_res) {
			wl_resource_post_no_memory(manager_res->resource);
			continue;
		}
		ext_workspace_manager_v1_send_workspace(manager_res->resource,
												workspace_res->resource);
		workspace_send_details(workspace_res);

		if (!workspace->group) {
			continue;
		}
		struct wlr_ext_workspace_group_v1_resource *group_res;
		wl_list_for_each(group_res, &workspace->group->resources, link) {
			if (group_res->manager == manager_res) {
				ext_workspace_group_handle_v1_send_workspace_enter(
					group_res->resource, workspace_res->resource);
			}
		}
	}

	ext_workspace_manager_v1_send_done(manager_res->resource);
}

static void manager_handle_display_destroy(struct wl_listener *listener,
										   void *data) {
	struct wlr_ext_workspace_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);
	assert(wl_list_empty(&manager->events.destroy.listener_list));

	struct wlr_ext_workspace_group_handle_v1 *group, *tmp;
	wl_list_for_each_safe(group, tmp, &manager->groups, link) {
		wlr_ext_workspace_group_handle_v1_destroy(group);
	}

	struct wlr_ext_workspace_handle_v1 *workspace, *tmp2;
	wl_list_for_each_safe(workspace, tmp2, &manager->workspaces, link) {
		wlr_ext_workspace_handle_v1_destroy(workspace);
	}

	struct wlr_ext_workspace_manager_v1_resource *manager_res, *tmp3;
	wl_list_for_each_safe(manager_res, tmp3, &manager->resources, link) {
		destroy_manager_resource(manager_res);
	}

	if (manager->idle_source) {
		wl_event_source_remove(manager->idle_source);
	}

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_ext_workspace_manager_v1 *
wlr_ext_workspace_manager_v1_create(struct wl_display *display,
									uint32_t version) {
	assert(version <= EXT_WORKSPACE_V1_VERSION);

	struct wlr_ext_workspace_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->global =
		wl_global_create(display, &ext_workspace_manager_v1_interface, version,
						 manager, manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->event_loop = wl_display_get_event_loop(display);

	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	wl_list_init(&manager->groups);
	wl_list_init(&manager->workspaces);
	wl_list_init(&manager->resources);
	wl_signal_init(&manager->events.destroy);

	return manager;
}

struct wlr_ext_workspace_group_handle_v1 *
wlr_ext_workspace_group_handle_v1_create(
	struct wlr_ext_workspace_manager_v1 *manager, uint32_t caps) {
	struct wlr_ext_workspace_group_handle_v1 *group = calloc(1, sizeof(*group));
	if (!group) {
		return NULL;
	}

	group->manager = manager;
	group->caps = caps;

	wl_list_init(&group->outputs);
	wl_list_init(&group->resources);
	wl_signal_init(&group->events.create_workspace);
	wl_signal_init(&group->events.destroy);

	wl_list_insert(manager->groups.prev, &group->link);

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &manager->resources, link) {
		struct wlr_ext_workspace_group_v1_resource *group_res =
			create_group_resource(group, manager_res);
		if (!group_res) {
			continue;
		}
		ext_workspace_manager_v1_send_workspace_group(manager_res->resource,
													  group_res->resource);
		group_send_details(group_res);
	}

	manager_schedule_done(manager);

	return group;
}

static void
workspace_send_group(struct wlr_ext_workspace_handle_v1 *workspace,
					 struct wlr_ext_workspace_group_handle_v1 *group,
					 bool enter) {

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		struct wlr_ext_workspace_group_v1_resource *group_res;
		wl_list_for_each(group_res, &group->resources, link) {
			if (group_res->manager != workspace_res->manager) {
				continue;
			}
			if (enter) {
				ext_workspace_group_handle_v1_send_workspace_enter(
					group_res->resource, workspace_res->resource);
			} else {
				ext_workspace_group_handle_v1_send_workspace_leave(
					group_res->resource, workspace_res->resource);
			}
		}
	}

	manager_schedule_done(workspace->manager);
}

static void
destroy_group_output(struct wlr_ext_workspace_v1_group_output *group_output) {
	wl_list_remove(&group_output->output_bind.link);
	wl_list_remove(&group_output->output_destroy.link);
	wl_list_remove(&group_output->link);
	free(group_output);
}

static void group_send_output(struct wlr_ext_workspace_group_handle_v1 *group,
							  struct wlr_output *output, bool enter) {

	struct wlr_ext_workspace_group_v1_resource *group_res;
	wl_list_for_each(group_res, &group->resources, link) {
		struct wl_client *client = wl_resource_get_client(group_res->resource);

		struct wl_resource *output_resource;
		wl_resource_for_each(output_resource, &output->resources) {
			if (wl_resource_get_client(output_resource) != client) {
				continue;
			}
			if (enter) {
				ext_workspace_group_handle_v1_send_output_enter(
					group_res->resource, output_resource);
			} else {
				ext_workspace_group_handle_v1_send_output_leave(
					group_res->resource, output_resource);
			}
		}
	}

	manager_schedule_done(group->manager);
}

void wlr_ext_workspace_group_handle_v1_destroy(
	struct wlr_ext_workspace_group_handle_v1 *group) {
	if (!group) {
		return;
	}

	wl_signal_emit_mutable(&group->events.destroy, NULL);

	assert(wl_list_empty(&group->events.create_workspace.listener_list));
	assert(wl_list_empty(&group->events.destroy.listener_list));

	struct wlr_ext_workspace_handle_v1 *workspace;
	wl_list_for_each(workspace, &group->manager->workspaces, link) {
		if (workspace->group == group) {
			workspace_send_group(workspace, group, false);
			workspace->group = NULL;
		}
	}

	struct wlr_ext_workspace_group_v1_resource *group_res, *tmp;
	wl_list_for_each_safe(group_res, tmp, &group->resources, link) {
		ext_workspace_group_handle_v1_send_removed(group_res->resource);
		destroy_group_resource(group_res);
	}

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &group->manager->resources, link) {
		struct wlr_ext_workspace_v1_request *req, *tmp2;
		wl_list_for_each_safe(req, tmp2, &manager_res->requests, link) {
			if (req->group == group) {
				destroy_request(req);
			}
		}
	}

	struct wlr_ext_workspace_v1_group_output *group_output, *tmp3;
	wl_list_for_each_safe(group_output, tmp3, &group->outputs, link) {
		group_send_output(group, group_output->output, false);
		destroy_group_output(group_output);
	}

	manager_schedule_done(group->manager);

	wl_list_remove(&group->link);
	free(group);
}

static void handle_output_bind(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_v1_group_output *group_output =
		wl_container_of(listener, group_output, output_bind);
	struct wlr_output_event_bind *event = data;
	struct wl_client *client = wl_resource_get_client(event->resource);

	struct wlr_ext_workspace_group_v1_resource *group_res;
	wl_list_for_each(group_res, &group_output->group->resources, link) {
		if (wl_resource_get_client(group_res->resource) == client) {
			ext_workspace_group_handle_v1_send_output_enter(group_res->resource,
															event->resource);
		}
	}

	manager_schedule_done(group_output->group->manager);
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_v1_group_output *group_output =
		wl_container_of(listener, group_output, output_destroy);
	group_send_output(group_output->group, group_output->output, false);
	destroy_group_output(group_output);
}

static struct wlr_ext_workspace_v1_group_output *
get_group_output(struct wlr_ext_workspace_group_handle_v1 *group,
				 struct wlr_output *output) {
	struct wlr_ext_workspace_v1_group_output *group_output;
	wl_list_for_each(group_output, &group->outputs, link) {
		if (group_output->output == output) {
			return group_output;
		}
	}
	return NULL;
}

void wlr_ext_workspace_group_handle_v1_output_enter(
	struct wlr_ext_workspace_group_handle_v1 *group,
	struct wlr_output *output) {
	if (get_group_output(group, output)) {
		return;
	}
	struct wlr_ext_workspace_v1_group_output *group_output =
		calloc(1, sizeof(*group_output));
	if (!group_output) {
		return;
	}
	group_output->output = output;
	group_output->group = group;
	wl_list_insert(&group->outputs, &group_output->link);

	group_output->output_bind.notify = handle_output_bind;
	wl_signal_add(&output->events.bind, &group_output->output_bind);
	group_output->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&output->events.destroy, &group_output->output_destroy);

	group_send_output(group, output, true);
}

void wlr_ext_workspace_group_handle_v1_output_leave(
	struct wlr_ext_workspace_group_handle_v1 *group,
	struct wlr_output *output) {
	struct wlr_ext_workspace_v1_group_output *group_output =
		get_group_output(group, output);
	if (!group_output) {
		return;
	}

	group_send_output(group, output, false);
	destroy_group_output(group_output);
}

struct wlr_ext_workspace_handle_v1 *
wlr_ext_workspace_handle_v1_create(struct wlr_ext_workspace_manager_v1 *manager,
								   const char *id, uint32_t caps) {
	struct wlr_ext_workspace_handle_v1 *workspace =
		calloc(1, sizeof(*workspace));
	if (!workspace) {
		return NULL;
	}

	workspace->manager = manager;
	workspace->caps = caps;

	if (id) {
		workspace->id = strdup(id);
		if (!workspace->id) {
			free(workspace);
			return NULL;
		}
	}

	wl_list_init(&workspace->resources);
	wl_array_init(&workspace->coordinates);
	wl_signal_init(&workspace->events.activate);
	wl_signal_init(&workspace->events.deactivate);
	wl_signal_init(&workspace->events.remove);
	wl_signal_init(&workspace->events.assign);
	wl_signal_init(&workspace->events.destroy);

	wl_list_insert(&manager->workspaces, &workspace->link);

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &manager->resources, link) {
		struct wlr_ext_workspace_v1_resource *workspace_res =
			create_workspace_resource(workspace, manager_res);
		if (!workspace_res) {
			continue;
		}
		ext_workspace_manager_v1_send_workspace(manager_res->resource,
												workspace_res->resource);
		workspace_send_details(workspace_res);
	}

	manager_schedule_done(manager);

	return workspace;
}

void wlr_ext_workspace_handle_v1_destroy(
	struct wlr_ext_workspace_handle_v1 *workspace) {
	if (!workspace) {
		return;
	}

	wl_signal_emit_mutable(&workspace->events.destroy, NULL);

	assert(wl_list_empty(&workspace->events.activate.listener_list));
	assert(wl_list_empty(&workspace->events.deactivate.listener_list));
	assert(wl_list_empty(&workspace->events.remove.listener_list));
	assert(wl_list_empty(&workspace->events.assign.listener_list));
	assert(wl_list_empty(&workspace->events.destroy.listener_list));

	if (workspace->group) {
		workspace_send_group(workspace, workspace->group, false);
	}

	struct wlr_ext_workspace_v1_resource *workspace_res, *tmp;
	wl_list_for_each_safe(workspace_res, tmp, &workspace->resources, link) {
		ext_workspace_handle_v1_send_removed(workspace_res->resource);
		destroy_workspace_resource(workspace_res);
	}

	struct wlr_ext_workspace_manager_v1_resource *manager_res;
	wl_list_for_each(manager_res, &workspace->manager->resources, link) {
		struct wlr_ext_workspace_v1_request *req, *tmp2;
		wl_list_for_each_safe(req, tmp2, &manager_res->requests, link) {
			if (req->workspace == workspace) {
				destroy_request(req);
			}
		}
	}

	manager_schedule_done(workspace->manager);

	wl_list_remove(&workspace->link);
	wl_array_release(&workspace->coordinates);
	free(workspace->id);
	free(workspace->name);
	free(workspace);
}

void wlr_ext_workspace_handle_v1_set_group(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wlr_ext_workspace_group_handle_v1 *group) {
	if (workspace->group == group) {
		return;
	}

	if (workspace->group) {
		workspace_send_group(workspace, workspace->group, false);
	}
	workspace->group = group;
	if (group) {
		workspace_send_group(workspace, group, true);
	}
}

void wlr_ext_workspace_handle_v1_set_name(
	struct wlr_ext_workspace_handle_v1 *workspace, const char *name) {
	assert(name);

	if (workspace->name && strcmp(workspace->name, name) == 0) {
		return;
	}

	free(workspace->name);
	workspace->name = strdup(name);
	if (workspace->name == NULL) {
		return;
	}

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		ext_workspace_handle_v1_send_name(workspace_res->resource,
										  workspace->name);
	}

	manager_schedule_done(workspace->manager);
}

static bool array_equal(struct wl_array *a, struct wl_array *b) {
	return (a->size == b->size) &&
		   (a->size == 0 || memcmp(a->data, b->data, a->size) == 0);
}

void wlr_ext_workspace_handle_v1_set_coordinates(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wl_array *coordinates) {
	assert(coordinates);

	if (array_equal(&workspace->coordinates, coordinates)) {
		return;
	}

	wl_array_release(&workspace->coordinates);
	wl_array_init(&workspace->coordinates);
	wl_array_copy(&workspace->coordinates, coordinates);

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		ext_workspace_handle_v1_send_coordinates(workspace_res->resource,
												 &workspace->coordinates);
	}

	manager_schedule_done(workspace->manager);
}

static void workspace_set_state(struct wlr_ext_workspace_handle_v1 *workspace,
								enum ext_workspace_handle_v1_state state,
								bool enabled) {
	uint32_t old_state = workspace->state;
	if (enabled) {
		workspace->state |= state;
	} else {
		workspace->state &= ~state;
	}
	if (old_state == workspace->state) {
		return;
	}

	struct wlr_ext_workspace_v1_resource *workspace_res;
	wl_list_for_each(workspace_res, &workspace->resources, link) {
		ext_workspace_handle_v1_send_state(workspace_res->resource,
										   workspace->state);
	}

	manager_schedule_done(workspace->manager);
}

void wlr_ext_workspace_handle_v1_set_active(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled) {
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE,
						enabled);
}

void wlr_ext_workspace_handle_v1_set_urgent(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled) {
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_URGENT,
						enabled);
}

void wlr_ext_workspace_handle_v1_set_hidden(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled) {
	workspace_set_state(workspace, EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN,
						enabled);
}
