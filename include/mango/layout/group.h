#ifndef __LAYOUT_GROUP_H__
#define __LAYOUT_GROUP_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>

Client *group_capture_get_parent(void);
bool group_capture_spawn(Client *c, Client *group_parent);

void client_add_group_bar(Client *c);
void client_focus_group_member(Client *c);
void client_check_tab_node_visible(Client *c);
void client_raise_group(Client *c);
void client_reparent_group(Client *c);
void client_handle_decorate_click(MangoGroupBar *gb);
void client_set_group_mon(Client *c, Monitor *m);
void client_set_group_config(Client *c);
void client_group_detach(Client *c);
void client_group_replace(Client *old, Client *new);

#endif
