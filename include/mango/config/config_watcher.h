#ifndef __MANGO_CONFIG_WATCHER_H__
#define __MANGO_CONFIG_WATCHER_H__ 1

#include <wayland-server-core.h>

void config_watcher_init(struct wl_event_loop *loop);

void config_watcher_update(void);

void config_watcher_destroy(void);

#endif
