// bash on: https://gitlab.freedesktop.org/tokyo4j/wlroots/-/tree/ext-workspace
// TODO: remove this file
// refer: https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/5115

#include <wayland-protocols/ext-workspace-v1-enum.h>
#include <wayland-server-core.h>

struct wlr_output;

struct wlr_ext_workspace_manager_v1 {
	struct wl_global *global;
	struct wl_list groups;	   // wlr_ext_workspace_group_handle_v1.link
	struct wl_list workspaces; // wlr_ext_workspace_handle_v1.link

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_list resources; // wlr_ext_workspace_manager_v1_resource.link
		struct wl_event_source *idle_source;
		struct wl_event_loop *event_loop;
		struct wl_listener display_destroy;
	};
};

struct wlr_ext_workspace_group_handle_v1_create_workspace_event {
	const char *name;
};

struct wlr_ext_workspace_group_handle_v1 {
	struct wlr_ext_workspace_manager_v1 *manager;
	uint32_t caps; // ext_workspace_group_handle_v1_group_capabilities
	struct {
		struct wl_signal
			create_workspace; // wlr_ext_workspace_group_handle_v1_create_workspace_event
		struct wl_signal destroy;
	} events;

	struct wl_list link; // wlr_ext_workspace_manager_v1.groups

	struct {
		struct wl_list outputs;	  // wlr_ext_workspace_v1_group_output.link
		struct wl_list resources; // wlr_ext_workspace_manager_v1_resource.link
	};
};

struct wlr_ext_workspace_handle_v1 {
	struct wlr_ext_workspace_manager_v1 *manager;
	struct wlr_ext_workspace_group_handle_v1 *group; // May be NULL
	char *id;
	char *name;
	struct wl_array coordinates;
	uint32_t caps;	// ext_workspace_handle_v1_workspace_capabilities
	uint32_t state; // ext_workspace_handle_v1_state

	struct {
		struct wl_signal activate;
		struct wl_signal deactivate;
		struct wl_signal remove;
		struct wl_signal assign; // wlr_ext_workspace_group_handle_v1
		struct wl_signal destroy;
	} events;

	struct wl_list link; // wlr_ext_workspace_manager_v1.workspaces

	struct {
		struct wl_list resources; // wlr_ext_workspace_v1_resource.link
	};
};

struct wlr_ext_workspace_manager_v1 *
wlr_ext_workspace_manager_v1_create(struct wl_display *display,
									uint32_t version);

struct wlr_ext_workspace_group_handle_v1 *
wlr_ext_workspace_group_handle_v1_create(
	struct wlr_ext_workspace_manager_v1 *manager, uint32_t caps);
void wlr_ext_workspace_group_handle_v1_destroy(
	struct wlr_ext_workspace_group_handle_v1 *group);

void wlr_ext_workspace_group_handle_v1_output_enter(
	struct wlr_ext_workspace_group_handle_v1 *group, struct wlr_output *output);
void wlr_ext_workspace_group_handle_v1_output_leave(
	struct wlr_ext_workspace_group_handle_v1 *group, struct wlr_output *output);

struct wlr_ext_workspace_handle_v1 *
wlr_ext_workspace_handle_v1_create(struct wlr_ext_workspace_manager_v1 *manager,
								   const char *id, uint32_t caps);
void wlr_ext_workspace_handle_v1_destroy(
	struct wlr_ext_workspace_handle_v1 *workspace);

void wlr_ext_workspace_handle_v1_set_group(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wlr_ext_workspace_group_handle_v1 *group);
void wlr_ext_workspace_handle_v1_set_name(
	struct wlr_ext_workspace_handle_v1 *workspace, const char *name);
void wlr_ext_workspace_handle_v1_set_coordinates(
	struct wlr_ext_workspace_handle_v1 *workspace,
	struct wl_array *coordinates);
void wlr_ext_workspace_handle_v1_set_active(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled);
void wlr_ext_workspace_handle_v1_set_urgent(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled);
void wlr_ext_workspace_handle_v1_set_hidden(
	struct wlr_ext_workspace_handle_v1 *workspace, bool enabled);
