#ifndef __DRAW_TITLE_MESSAGE_BAR_H__
#define __DRAW_TITLE_MESSAGE_BAR_H__ 1

#include "mango/common/types.h"

/* One title_message_bar per monitor; it follows the focused client in
 * overcircle mode and the highlighted client below the switcher panel. */
void title_message_bar_finish(Monitor *m);
void title_message_bar_apply_config(Monitor *m);
void title_message_bar_hide(Monitor *m);
void title_message_bar_update(Monitor *m);
void title_message_bar_update_geometry(Monitor *m);
void title_message_bar_update_title(Client *c);

#endif
