#ifndef __HORIZONTAL_H__
#define __HORIZONTAL_H__

#include "mango/common/types.h"
#include <stdbool.h>

void tile(Monitor *m);
bool tile_predict(Monitor *m, Client *c, struct wlr_box *out);

void right_tile(Monitor *m);
bool right_tile_predict(Monitor *m, Client *c, struct wlr_box *out);

void center_tile(Monitor *m);
bool center_tile_predict(Monitor *m, Client *c, struct wlr_box *out);

void deck(Monitor *m);
bool deck_predict(Monitor *m, Client *c, struct wlr_box *out);

void monocle(Monitor *m);
bool monocle_predict(Monitor *m, Client *c, struct wlr_box *out);

void grid(Monitor *m);
bool grid_predict(Monitor *m, Client *c, struct wlr_box *out);

void fair(Monitor *m);
bool fair_predict(Monitor *m, Client *c, struct wlr_box *out);

#endif
