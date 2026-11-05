#ifndef __FLOATING_H__
#define __FLOATING_H__

#include "mango/common/types.h"
#include <stdbool.h>

/* Layout: every window floats; geometry is remembered per window in
 * ~/.config/mango/floating.txt. */
void floating_layout(Monitor *m);

/* True when the monitor's current tag uses the floating layout. */
bool floating_layout_active(Monitor *m);

/* Called at the start of arrange(): turns windows floating when the current
 * tag uses the floating layout, and returns them to tiling when it does not. */
void floating_prepare(Monitor *m);

/* Called from resize(): keep the window's saved geometry up to date. */
void floating_note(Client *c);

/* Write floating.txt if anything changed. */
void floating_flush(void);

#endif
