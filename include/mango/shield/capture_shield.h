#ifndef __MANAGE_CAPTURE_SHIELD_H__
#define __MANAGE_CAPTURE_SHIELD_H__ 1

#include "mango/common/types.h"

void client_update_capture_shield(Client *c);
void capture_shield_wrap_output_source(
	struct wlr_ext_image_capture_source_v1 *source);
struct wlr_output *capture_shield_output_from_source(
	struct wlr_ext_image_capture_source_v1 *source);
void handle_output_capture_request(struct wl_listener *listener, void *data);

#endif
