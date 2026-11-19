#ifndef __WORKSPACE_OVERVIEW_H__
#define __WORKSPACE_OVERVIEW_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>

bool workspace_overview_is_active(void);
bool workspace_overview_is_active_on(const Monitor *m);
bool workspace_overview_is_closing_on(const Monitor *m);

int32_t toggle_workspace_overview(const Arg *arg);
int32_t enter_workspace_overview(const Arg *arg);
int32_t leave_workspace_overview(const Arg *arg);

bool workspace_overview_handle_button(
	const struct wlr_pointer_button_event *event);
bool workspace_overview_handle_axis(const struct wlr_pointer_axis_event *event);
void workspace_overview_handle_motion(double x, double y);
bool workspace_overview_handle_key(uint32_t sym);

void workspace_overview_client_commit(Client *c);
void workspace_overview_layers_changed(LayerSurface *layer);
void workspace_overview_client_added(Client *c);
void workspace_overview_client_removed(Client *c);
void workspace_overview_client_replaced(Client *old, Client *new);
void workspace_overview_client_tags_changed(Client *c);
void workspace_overview_client_title_changed(Client *c);
void workspace_overview_client_stack_changed(Client *c);
void workspace_overview_layout_changed(Monitor *m);
bool workspace_overview_active_workspace_will_empty(Client *c);
void workspace_overview_client_unmapped(Client *c, bool activate_target);
void workspace_overview_monitor_destroy(Monitor *m);
void workspace_overview_monitor_changed(Monitor *m);
void workspace_overview_reload(void);

#endif
