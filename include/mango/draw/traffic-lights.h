#ifndef MANGO_DRAW_TRAFFIC_LIGHTS_H
#define MANGO_DRAW_TRAFFIC_LIGHTS_H

#include "mango/common/types.h"
#include <stdbool.h>

/* Close / minimize / maximize dots drawn in the top-right corner of floating
 * windows. */
void client_add_traffic_lights(Client *c);
void client_remove_traffic_lights(Client *c);

/* Position, show or hide the dots; called every time the window is drawn. */
void client_draw_traffic_lights(Client *c);

/* Run the action of the dot under (x, y), in layout coordinates. Returns true
 * when a dot was hit, so the click must not reach the window. */
bool traffic_lights_click(Client *c, double x, double y);

#endif
