#ifndef __LAYOUT_VERTICAL_H__
#define __LAYOUT_VERTICAL_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>

void vertical_tile(Monitor *m);
bool vertical_tile_predict(Monitor *m, Client *c, struct wlr_box *out);
void vertical_deck(Monitor *m);
bool vertical_deck_predict(Monitor *m, Client *c, struct wlr_box *out);
void vertical_grid(Monitor *m);
bool vertical_grid_predict(Monitor *m, Client *c, struct wlr_box *out);
void vertical_fair(Monitor *m);
bool vertical_fair_predict(Monitor *m, Client *c, struct wlr_box *out);

#endif
