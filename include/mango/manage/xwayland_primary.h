#ifndef __MANAGE_XWAYLAND_PRIMARY_H__
#define __MANAGE_XWAYLAND_PRIMARY_H__

#include "mango/common/types.h"

void xwayland_primary_init(void);
void xwayland_primary_set(Monitor *m);
void xwayland_primary_invalidate(void);

#endif
