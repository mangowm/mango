#include "mango/shield/capture_frame_event.h"
#include "mango/shield/output_capture_source_compat.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/addon.h>

#include "ext-image-capture-source-v1-protocol.h"

struct mango_output_cursor_source {
	struct wlr_ext_image_capture_source_v1_cursor base;
	struct wlr_output *output;
	struct wlr_buffer *prev_buffer;
	bool initialized;
	bool needs_frame;
	struct wl_listener output_commit;
	struct wl_listener prev_buffer_release;
};

struct mango_output_capture_source {
	struct wlr_ext_image_capture_source_v1 base;
	struct wlr_addon addon;
	struct wlr_output *output;
	struct wl_listener output_commit;
	struct mango_output_cursor_source cursor;
	size_t num_started;
	bool software_cursors_locked;
};

static void output_cursor_source_init(struct mango_output_cursor_source *cursor,
									  struct wlr_output *output);
static void
output_cursor_source_finish(struct mango_output_cursor_source *cursor);

static void output_source_start(struct wlr_ext_image_capture_source_v1 *base,
								bool with_cursors) {
	struct mango_output_capture_source *source =
		wl_container_of(base, source, base);
	source->num_started++;
	if (source->num_started > 1) {
		return;
	}
	wlr_output_lock_attach_render(source->output, true);
	if (with_cursors) {
		wlr_output_lock_software_cursors(source->output, true);
	}
	source->software_cursors_locked = with_cursors;
}

static void output_source_stop(struct wlr_ext_image_capture_source_v1 *base) {
	struct mango_output_capture_source *source =
		wl_container_of(base, source, base);
	assert(source->num_started > 0);
	source->num_started--;
	if (source->num_started > 0) {
		return;
	}
	wlr_output_lock_attach_render(source->output, false);
	if (source->software_cursors_locked) {
		wlr_output_lock_software_cursors(source->output, false);
	}
}

static void
output_source_request_frame(struct wlr_ext_image_capture_source_v1 *base,
							bool schedule_frame) {
	struct mango_output_capture_source *source =
		wl_container_of(base, source, base);
	if (schedule_frame) {
		wlr_output_update_needs_frame(source->output);
	}
}

static void output_source_copy_frame(
	struct wlr_ext_image_capture_source_v1 *base,
	struct wlr_ext_image_copy_capture_frame_v1 *frame,
	struct wlr_ext_image_capture_source_v1_frame_event *base_event) {
	struct mango_output_capture_source *source =
		wl_container_of(base, source, base);
	struct capture_shield_frame_event *event =
		wl_container_of(base_event, event, base);

	if (wlr_ext_image_copy_capture_frame_v1_copy_buffer(
			frame, event->buffer, source->output->renderer)) {
		wlr_ext_image_copy_capture_frame_v1_ready(
			frame, source->output->transform, &event->when);
	}
}

static struct wlr_ext_image_capture_source_v1_cursor *
output_source_get_pointer_cursor(struct wlr_ext_image_capture_source_v1 *base,
								 struct wlr_seat *seat) {
	struct mango_output_capture_source *source =
		wl_container_of(base, source, base);
	return &source->cursor.base;
}

static const struct wlr_ext_image_capture_source_v1_interface
	output_source_impl = {
		.start = output_source_start,
		.stop = output_source_stop,
		.request_frame = output_source_request_frame,
		.copy_frame = output_source_copy_frame,
		.get_pointer_cursor = output_source_get_pointer_cursor,
};

static void
output_source_update_constraints(struct mango_output_capture_source *source) {
	struct wlr_output *output = source->output;
	if (!output->enabled) {
		return;
	}
	if (!wlr_output_configure_primary_swapchain(output, NULL,
												&output->swapchain)) {
		return;
	}
	wlr_ext_image_capture_source_v1_set_constraints_from_swapchain(
		&source->base, output->swapchain, output->renderer);
}

static void output_source_handle_output_commit(struct wl_listener *listener,
											   void *data) {
	struct mango_output_capture_source *source =
		wl_container_of(listener, source, output_commit);
	struct wlr_output_event_commit *event = data;

	if (event->state->committed &
		(WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_RENDER_FORMAT |
		 WLR_OUTPUT_STATE_ENABLED)) {
		output_source_update_constraints(source);
	}

	if (!(event->state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		return;
	}

	struct wlr_buffer *buffer = event->state->buffer;
	pixman_region32_t full_damage;
	pixman_region32_init_rect(&full_damage, 0, 0, buffer->width,
							  buffer->height);
	const pixman_region32_t *damage =
		event->state->committed & WLR_OUTPUT_STATE_DAMAGE
			? &event->state->damage
			: &full_damage;

	struct capture_shield_frame_event frame_event = {
		.base = {.damage = damage},
		.buffer = buffer,
		.when = event->when,
	};
	wl_signal_emit_mutable(&source->base.events.frame, &frame_event);
	pixman_region32_fini(&full_damage);
}

static void output_addon_destroy(struct wlr_addon *addon) {
	struct mango_output_capture_source *source =
		wl_container_of(addon, source, addon);
	wlr_ext_image_capture_source_v1_finish(&source->base);
	output_cursor_source_finish(&source->cursor);
	wl_list_remove(&source->output_commit.link);
	wlr_addon_finish(&source->addon);
	free(source);
}

static const struct wlr_addon_interface output_addon_impl = {
	.name = "mango_output_capture_source",
	.destroy = output_addon_destroy,
};

static struct mango_output_capture_source *
output_source_find(struct wlr_output *output) {
	struct wlr_addon *addon =
		wlr_addon_find(&output->addons, NULL, &output_addon_impl);
	if (!addon) {
		return NULL;
	}
	struct mango_output_capture_source *source;
	return wl_container_of(addon, source, addon);
}

struct wlr_ext_image_capture_source_v1 *
mango_output_capture_source_v1_create_with_raw_output(
	struct wlr_output *output) {
	if (!output) {
		return NULL;
	}

	struct mango_output_capture_source *source = output_source_find(output);
	if (source) {
		return &source->base;
	}

	source = calloc(1, sizeof(*source));
	if (!source) {
		return NULL;
	}
	wlr_ext_image_capture_source_v1_init(&source->base, &output_source_impl);
	wlr_addon_init(&source->addon, &output->addons, NULL, &output_addon_impl);
	source->output = output;
	source->output_commit.notify = output_source_handle_output_commit;
	wl_signal_add(&output->events.commit, &source->output_commit);
	output_source_update_constraints(source);
	output_cursor_source_init(&source->cursor, output);
	return &source->base;
}

struct wlr_output *mango_output_capture_source_v1_get_output(
	struct wlr_ext_image_capture_source_v1 *source) {
	struct mango_output_capture_source *output_source;
	if (!source || source->impl != &output_source_impl) {
		return NULL;
	}
	output_source = wl_container_of(source, output_source, base);
	return output_source->output;
}

static void
output_cursor_source_request_frame(struct wlr_ext_image_capture_source_v1 *base,
								   bool schedule_frame) {
	struct mango_output_cursor_source *cursor =
		wl_container_of(base, cursor, base);
	if (schedule_frame) {
		wlr_output_update_needs_frame(cursor->output);
		cursor->needs_frame = true;
	}
}

static void output_cursor_source_copy_frame(
	struct wlr_ext_image_capture_source_v1 *base,
	struct wlr_ext_image_copy_capture_frame_v1 *frame,
	struct wlr_ext_image_capture_source_v1_frame_event *base_event) {
	struct mango_output_cursor_source *cursor =
		wl_container_of(base, cursor, base);
	struct wlr_buffer *src_buffer = cursor->output->cursor_front_buffer;
	if (!src_buffer) {
		wlr_ext_image_copy_capture_frame_v1_fail(
			frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
		return;
	}
	if (!wlr_ext_image_copy_capture_frame_v1_copy_buffer(
			frame, src_buffer, cursor->output->renderer)) {
		return;
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_ext_image_copy_capture_frame_v1_ready(frame, cursor->output->transform,
											  &now);
}

static const struct wlr_ext_image_capture_source_v1_interface
	output_cursor_source_impl = {
		.request_frame = output_cursor_source_request_frame,
		.copy_frame = output_cursor_source_copy_frame,
};

static void
output_cursor_source_update(struct mango_output_cursor_source *cursor) {
	struct wlr_output *output = cursor->output;
	if (output->cursor_swapchain && !cursor->initialized) {
		wlr_ext_image_capture_source_v1_set_constraints_from_swapchain(
			&cursor->base.base, output->cursor_swapchain, output->renderer);
		cursor->initialized = true;
	}

	struct wlr_output_cursor *output_cursor = output->hardware_cursor;
	if (!output_cursor || !output_cursor->visible) {
		cursor->base.entered = false;
		wl_signal_emit_mutable(&cursor->base.events.update, NULL);
		return;
	}

	if (output->cursor_swapchain &&
		((int)cursor->base.base.width != output->cursor_swapchain->width ||
		 (int)cursor->base.base.height != output->cursor_swapchain->height)) {
		cursor->base.base.width = output->cursor_swapchain->width;
		cursor->base.base.height = output->cursor_swapchain->height;
		wl_signal_emit_mutable(&cursor->base.base.events.constraints_update,
							   NULL);
	}

	cursor->base.entered = true;
	cursor->base.x = (int32_t)lround(output_cursor->x);
	cursor->base.y = (int32_t)lround(output_cursor->y);
	cursor->base.hotspot.x = output_cursor->hotspot_x;
	cursor->base.hotspot.y = output_cursor->hotspot_y;
	wl_signal_emit_mutable(&cursor->base.events.update, NULL);
}

static void
output_cursor_source_handle_prev_buffer_release(struct wl_listener *listener,
												void *data) {
	struct mango_output_cursor_source *cursor =
		wl_container_of(listener, cursor, prev_buffer_release);
	wl_list_remove(&cursor->prev_buffer_release.link);
	wl_list_init(&cursor->prev_buffer_release.link);
	cursor->prev_buffer = NULL;
}

static void
output_cursor_source_handle_output_commit(struct wl_listener *listener,
										  void *data) {
	struct mango_output_cursor_source *cursor =
		wl_container_of(listener, cursor, output_commit);
	(void)data;
	output_cursor_source_update(cursor);

	struct wlr_buffer *buffer = cursor->output->cursor_front_buffer;
	if (buffer && (buffer != cursor->prev_buffer || cursor->needs_frame)) {
		pixman_region32_t full_damage;
		pixman_region32_init_rect(&full_damage, 0, 0, buffer->width,
								  buffer->height);
		struct wlr_ext_image_capture_source_v1_frame_event frame_event = {
			.damage = &full_damage,
		};
		wl_signal_emit_mutable(&cursor->base.base.events.frame, &frame_event);
		pixman_region32_fini(&full_damage);

		assert(buffer->n_locks > 0);
		cursor->prev_buffer = buffer;
		wl_list_remove(&cursor->prev_buffer_release.link);
		cursor->prev_buffer_release.notify =
			output_cursor_source_handle_prev_buffer_release;
		wl_signal_add(&buffer->events.release, &cursor->prev_buffer_release);
	}
	cursor->needs_frame = false;
}

static void output_cursor_source_init(struct mango_output_cursor_source *cursor,
									  struct wlr_output *output) {
	wlr_ext_image_capture_source_v1_cursor_init(&cursor->base,
												&output_cursor_source_impl);
	cursor->output = output;
	cursor->output_commit.notify = output_cursor_source_handle_output_commit;
	wl_signal_add(&output->events.commit, &cursor->output_commit);
	wl_list_init(&cursor->prev_buffer_release.link);
	output_cursor_source_update(cursor);
}

static void
output_cursor_source_finish(struct mango_output_cursor_source *cursor) {
	wlr_ext_image_capture_source_v1_cursor_finish(&cursor->base);
	wl_list_remove(&cursor->output_commit.link);
	wl_list_remove(&cursor->prev_buffer_release.link);
}

static void manager_handle_create_source(struct wl_client *client,
										 struct wl_resource *manager_resource,
										 uint32_t new_id,
										 struct wl_resource *output_resource) {
	struct mango_output_capture_source_manager_v1 *manager =
		wl_resource_get_user_data(manager_resource);
	struct wlr_output *output = wlr_output_from_resource(output_resource);
	if (!output) {
		wlr_ext_image_capture_source_v1_create_resource(NULL, client, new_id);
		return;
	}

	struct mango_output_capture_source_request_event request = {
		.output = output,
		.client = client,
		.new_id = new_id,
	};
	wl_signal_emit_mutable(&manager->events.capture_request, &request);
}

static void manager_handle_destroy(struct wl_client *client,
								   struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct ext_output_image_capture_source_manager_v1_interface
	manager_impl = {
		.create_source = manager_handle_create_source,
		.destroy = manager_handle_destroy,
};

static void manager_bind(struct wl_client *client, void *data, uint32_t version,
						 uint32_t id) {
	struct wl_resource *resource = wl_resource_create(
		client, &ext_output_image_capture_source_manager_v1_interface, version,
		id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, data, NULL);
}

static void manager_handle_display_destroy(struct wl_listener *listener,
										   void *data) {
	struct mango_output_capture_source_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, NULL);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct mango_output_capture_source_manager_v1 *
mango_output_capture_source_manager_v1_create(struct wl_display *display,
											  uint32_t version) {
	struct mango_output_capture_source_manager_v1 *manager =
		calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}
	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.capture_request);
	manager->global = wl_global_create(
		display, &ext_output_image_capture_source_manager_v1_interface, version,
		manager, manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}
	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);
	return manager;
}

bool mango_output_capture_source_manager_v1_request_accept(
	struct mango_output_capture_source_request_event *request,
	struct wlr_ext_image_capture_source_v1 *source) {
	if (!request) {
		return false;
	}
	return wlr_ext_image_capture_source_v1_create_resource(
		source, request->client, request->new_id);
}
