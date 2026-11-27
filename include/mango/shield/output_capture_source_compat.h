#ifndef __MANAGE_OUTPUT_CAPTURE_SOURCE_COMPAT_H__
#define __MANAGE_OUTPUT_CAPTURE_SOURCE_COMPAT_H__ 1

#include "mango/common/types.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>

struct mango_output_capture_source_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal capture_request;
	} events;

	struct wl_listener display_destroy;
};

struct mango_output_capture_source_request_event {
	struct wlr_output *output;
	struct wl_client *client;
	uint32_t new_id;
};

struct mango_output_capture_source_manager_v1 *
mango_output_capture_source_manager_v1_create(struct wl_display *display,
											  uint32_t version);

bool mango_output_capture_source_manager_v1_request_accept(
	struct mango_output_capture_source_request_event *request,
	struct wlr_ext_image_capture_source_v1 *source);

struct wlr_ext_image_capture_source_v1 *
mango_output_capture_source_v1_create_with_raw_output(
	struct wlr_output *output);

struct wlr_output *mango_output_capture_source_v1_get_output(
	struct wlr_ext_image_capture_source_v1 *source);

#define wlr_ext_output_image_capture_source_manager_v1                         \
	mango_output_capture_source_manager_v1
#define wlr_ext_output_image_capture_source_manager_v1_create                  \
	mango_output_capture_source_manager_v1_create
#define wlr_ext_output_image_capture_source_manager_v1_request_event           \
	mango_output_capture_source_request_event
#define wlr_ext_output_image_capture_source_manager_v1_request_accept          \
	mango_output_capture_source_manager_v1_request_accept
#define wlr_ext_image_capture_source_v1_create_with_raw_output                 \
	mango_output_capture_source_v1_create_with_raw_output
#define wlr_output_try_from_ext_image_capture_source_v1                        \
	mango_output_capture_source_v1_get_output

#endif
