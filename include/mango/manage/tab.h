#ifndef __MANGO_MANAGE_TAB_H__
#define __MANGO_MANAGE_TAB_H__ 1

#include "mango/animation/common.h"
#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>

bool tab_layout_active(const Monitor *m);
void tab_sync_monitor(Monitor *m);
void tab_detach_client(Client *c);
void tab_replace_client(Client *old, Client *new);
void tab_focus_member(Client *c);
void tab_sync_focus(Client *c);
void client_add_tab_bar(Client *c);
void client_update_tab_bar_title(Client *c);
void client_apply_tab_bar_config(Client *c);
void client_remove_tab_bar(Client *c);
void client_draw_tabbar(Client *c, struct ivec2 offsets);
void global_draw_tab_bar(Client *c, int32_t x, int32_t y, int32_t width,
						 int32_t height);
void client_reparent_tab(Client *c);
void client_raise_tab(Client *c);

#endif
