#ifndef __MANAGE_CAPTURE_FRAME_EVENT_H__
#define __MANAGE_CAPTURE_FRAME_EVENT_H__ 1

#include <time.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>

/* Matches the private frame event emitted by wlroots' raw output source. */
struct capture_shield_frame_event {
	struct wlr_ext_image_capture_source_v1_frame_event base;
	struct wlr_buffer *buffer;
	struct timespec when;
};

#endif
