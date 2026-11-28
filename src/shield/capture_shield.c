#include "mango/shield/capture_shield.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/monitor.h"
#include "mango/shield/capture_frame_event.h"
#include "mango/shield/output_capture_source.h"

#include <drm_fourcc.h>
#include <math.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/transform.h>

/*
 * Protection wrapper: Mango custom.
 *
 * Output capture sources live in wlroots and normally copy the output buffer
 * as-is. Wrap their copy operation so shield_when_capture clients can be
 * painted black in the copied frame without changing the displayed frame.
 */
struct protected_output_source {
	struct wl_list link;
	struct wlr_ext_image_capture_source_v1 *source;
	struct wlr_output *output;
	const struct wlr_ext_image_capture_source_v1_interface *impl;
	struct wl_listener destroy;
};

static struct wl_list protected_output_sources;
static bool protected_output_sources_initialized;

static bool capture_shield_frame_info(
	struct wlr_ext_image_capture_source_v1_frame_event *event,
	struct wlr_buffer **buffer, struct timespec *when) {
	if (!event || !buffer || !when) {
		return false;
	}
	struct capture_shield_frame_event *frame_event =
		wl_container_of(event, frame_event, base);
	*buffer = frame_event->buffer;
	*when = frame_event->when;
	return frame_event->buffer != NULL;
}

/* MANGO CUSTOM: black overlay inside the independent window capture scene. */
void client_update_capture_shield(Client *c) {
	if (!c || !c->image_capture_shield)
		return;

	if (!c->shield_when_capture) {
		if (c->image_capture_shield->node.enabled) {
			wlr_scene_node_set_enabled(&c->image_capture_shield->node, false);
			wlr_scene_rect_set_size(c->image_capture_shield, 0, 0);
		}
		return;
	}

	struct wlr_surface *surface = client_surface(c);
	if (!surface) {
		wlr_scene_node_set_enabled(&c->image_capture_shield->node, false);
		wlr_scene_rect_set_size(c->image_capture_shield, 0, 0);
		return;
	}

	/* The xdg scene tree stores its surface at the negative window-geometry
	 * offset. Match that offset so the opaque rectangle covers the complete
	 * live capture node, including CSD margins, without changing its extent. */
	int32_t shield_x = 0;
	int32_t shield_y = 0;
	if (c->type == XDGShell && c->surface.xdg) {
		shield_x = -c->surface.xdg->geometry.x;
		shield_y = -c->surface.xdg->geometry.y;
	}

	int32_t shield_width = surface->current.width;
	int32_t shield_height = surface->current.height;
	if (shield_width <= 0 || shield_height <= 0) {
		wlr_scene_node_set_enabled(&c->image_capture_shield->node, false);
		wlr_scene_rect_set_size(c->image_capture_shield, 0, 0);
		return;
	}

	wlr_scene_node_raise_to_top(&c->image_capture_shield->node);
	wlr_scene_node_set_position(&c->image_capture_shield->node, shield_x,
								shield_y);
	wlr_scene_rect_set_size(c->image_capture_shield, shield_width,
							shield_height);
	wlr_scene_node_set_enabled(&c->image_capture_shield->node, true);
}

static struct protected_output_source *
protected_output_source_find(struct wlr_ext_image_capture_source_v1 *source) {
	if (!protected_output_sources_initialized) {
		return NULL;
	}

	struct protected_output_source *protected_source;
	wl_list_for_each(protected_source, &protected_output_sources, link) {
		if (protected_source->source == source) {
			return protected_source;
		}
	}
	return NULL;
}

struct wlr_output *capture_shield_output_from_source(
	struct wlr_ext_image_capture_source_v1 *source) {
	struct protected_output_source *protected_source =
		protected_output_source_find(source);
	return protected_source ? protected_source->output : NULL;
}

/* MANGO CUSTOM: shield visibility and scene-order handling. */
static bool output_has_shield_surfaces(struct wlr_output *output) {
	Monitor *m = output->data;
	if (!m) {
		return false;
	}

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->shield_when_capture && !c->iskilling && c->mon == m &&
			VISIBLEON(c, m)) {
			return true;
		}
	}

	for (int32_t i = 0; i < 4; i++) {
		LayerSurface *l;
		wl_list_for_each(l, &m->layers[i], link) {
			if (l->shield_when_capture && l->mapped && l->scene) {
				return true;
			}
		}
	}
	return false;
}

static const struct wlr_box *
capture_shield_layer_current_box(const LayerSurface *l) {
	return l->animation.running ? &l->animation.current : &l->geom;
}

static bool capture_shield_scene_box(struct wlr_scene_node *node,
									 const struct wlr_box *current, Monitor *m,
									 struct wlr_output *output,
									 struct wlr_buffer *buffer,
									 struct wlr_box *box) {
	if (!node || !current || !m) {
		return false;
	}

	int32_t scene_x, scene_y;
	if (!wlr_scene_node_coords(node, &scene_x, &scene_y)) {
		return false;
	}

	struct wlr_box logical = {
		.x = scene_x - m->m.x,
		.y = scene_y - m->m.y,
		.width = current->width,
		.height = current->height,
	};
	int logical_width, logical_height;
	wlr_output_effective_resolution(output, &logical_width, &logical_height);
	if (!wlr_box_intersection(&logical, &logical,
							  &(struct wlr_box){
								  .width = logical_width,
								  .height = logical_height,
							  })) {
		return false;
	}

	float scale = output->scale;
	int32_t x1 = (int32_t)lround(logical.x * scale);
	int32_t y1 = (int32_t)lround(logical.y * scale);
	int32_t x2 = (int32_t)lround((logical.x + logical.width) * scale);
	int32_t y2 = (int32_t)lround((logical.y + logical.height) * scale);
	struct wlr_box scaled = {
		.x = x1,
		.y = y1,
		.width = x2 - x1,
		.height = y2 - y1,
	};

	int transformed_width, transformed_height;
	wlr_output_transformed_resolution(output, &transformed_width,
									  &transformed_height);
	wlr_box_transform(&scaled, &scaled,
					  wlr_output_transform_invert(output->transform),
					  transformed_width, transformed_height);
	*box = scaled;

	return wlr_box_intersection(
		box, box,
		&(struct wlr_box){.width = buffer->width, .height = buffer->height});
}

static int32_t capture_shield_scene_layer_rank(struct wlr_scene_tree *parent) {
	for (int32_t i = 0; i < NUM_LAYERS; i++) {
		if (server.layers[i] == parent) {
			return i;
		}
	}
	return -1;
}

static bool capture_shield_node_is_above(struct wlr_scene_node *other,
										 struct wlr_scene_node *target) {
	if (!other || !target || !other->parent || !target->parent) {
		return false;
	}

	if (other->parent == target->parent) {
		struct wlr_scene_node *node;
		wl_list_for_each_reverse(node, &target->parent->children, link) {
			if (node == other) {
				return true;
			}
			if (node == target) {
				return false;
			}
		}
		return false;
	}

	int32_t other_rank = capture_shield_scene_layer_rank(other->parent);
	int32_t target_rank = capture_shield_scene_layer_rank(target->parent);
	return other_rank >= 0 && target_rank >= 0 && other_rank > target_rank;
}

static void
capture_shield_build_mask(struct protected_output_source *protected_source,
						  struct wlr_scene_node *target_node,
						  const struct wlr_box *target_box,
						  struct wlr_buffer *buffer, pixman_region32_t *mask) {
	pixman_region32_init_rect(mask, target_box->x, target_box->y,
							  target_box->width, target_box->height);

	Monitor *m = protected_source->output->data;
	if (!m) {
		return;
	}

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		struct wlr_box box;
		if (!c->scene || &c->scene->node == target_node || c->iskilling ||
			!VISIBLEON(c, m) ||
			!capture_shield_node_is_above(&c->scene->node, target_node) ||
			!capture_shield_scene_box(&c->scene->node, &c->animation.current, m,
									  protected_source->output, buffer, &box)) {
			continue;
		}

		pixman_region32_t occluder;
		pixman_region32_init_rect(&occluder, box.x, box.y, box.width,
								  box.height);
		pixman_region32_subtract(mask, mask, &occluder);
		pixman_region32_fini(&occluder);
	}

	for (int32_t i = 0; i < 4; i++) {
		LayerSurface *l;
		wl_list_for_each(l, &m->layers[i], link) {
			if (!l->scene || &l->scene->node == target_node || !l->mapped ||
				!capture_shield_node_is_above(&l->scene->node, target_node)) {
				continue;
			}

			struct wlr_box box;
			if (!capture_shield_scene_box(
					&l->scene->node, capture_shield_layer_current_box(l), m,
					protected_source->output, buffer, &box)) {
				continue;
			}

			pixman_region32_t occluder;
			pixman_region32_init_rect(&occluder, box.x, box.y, box.width,
									  box.height);
			pixman_region32_subtract(mask, mask, &occluder);
			pixman_region32_fini(&occluder);
		}
	}
}

static void
capture_shield_add_damage(struct wlr_ext_image_copy_capture_frame_v1 *frame,
						  const struct wlr_box *box) {
	pixman_region32_union_rect(&frame->buffer_damage, &frame->buffer_damage,
							   box->x, box->y, box->width, box->height);
}

static bool capture_shield_shm_black_pixel(uint32_t format, uint32_t *pixel) {
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
		*pixel = 0;
		return true;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
		*pixel = 0xff000000;
		return true;
	default:
		return false;
	}
}

static void
capture_shield_blacken_shm(struct protected_output_source *protected_source,
						   struct wlr_scene_node *target_node,
						   const struct wlr_box *target_box,
						   struct wlr_buffer *buffer,
						   struct wlr_ext_image_copy_capture_frame_v1 *frame,
						   void *data, size_t stride, uint32_t pixel) {
	pixman_region32_t mask;
	capture_shield_build_mask(protected_source, target_node, target_box, buffer,
							  &mask);

	int n_rects;
	const pixman_box32_t *rects = pixman_region32_rectangles(&mask, &n_rects);
	for (int i = 0; i < n_rects; i++) {
		const pixman_box32_t *rect = &rects[i];
		for (int32_t y = rect->y1; y < rect->y2; y++) {
			uint32_t *row = (uint32_t *)((uint8_t *)data + (size_t)y * stride);
			for (int32_t x = rect->x1; x < rect->x2; x++) {
				row[x] = pixel;
			}
		}

		struct wlr_box damage = {
			.x = rect->x1,
			.y = rect->y1,
			.width = rect->x2 - rect->x1,
			.height = rect->y2 - rect->y1,
		};
		capture_shield_add_damage(frame, &damage);
	}
	pixman_region32_fini(&mask);
}

static void capture_shield_blacken_dmabuf(
	struct protected_output_source *protected_source,
	struct wlr_scene_node *target_node, const struct wlr_box *target_box,
	struct wlr_buffer *buffer, struct wlr_render_pass *pass,
	struct wlr_ext_image_copy_capture_frame_v1 *frame) {
	pixman_region32_t mask;
	capture_shield_build_mask(protected_source, target_node, target_box, buffer,
							  &mask);

	int n_rects;
	const pixman_box32_t *rects = pixman_region32_rectangles(&mask, &n_rects);
	for (int i = 0; i < n_rects; i++) {
		const pixman_box32_t *rect = &rects[i];
		struct wlr_box box = {
			.x = rect->x1,
			.y = rect->y1,
			.width = rect->x2 - rect->x1,
			.height = rect->y2 - rect->y1,
		};
		wlr_render_pass_add_rect(pass,
								 &(struct wlr_render_rect_options){
									 .box = box,
									 .color = {0},
									 .blend_mode = WLR_RENDER_BLEND_MODE_NONE,
								 });
		capture_shield_add_damage(frame, &box);
	}
	pixman_region32_fini(&mask);
}

static bool
capture_shield_copy_shm(struct protected_output_source *protected_source,
						struct wlr_buffer *src_buffer,
						struct wlr_ext_image_copy_capture_frame_v1 *frame) {
	struct wlr_ext_image_capture_source_v1 *source = protected_source->source;
	struct wlr_buffer *dst_buffer = frame->buffer;
	struct wlr_renderer *renderer = protected_source->output->renderer;

	/*
	 * ADAPTED FROM WLROOTS 0.20
	 * (types/wlr_ext_image_copy_capture_v1.c:copy_shm). Mango adds the
	 * protected-region overwrite after wlroots' pixel read.
	 */
	if (!dst_buffer || !renderer || source->shm_formats_len == 0) {
		return false;
	}

	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(dst_buffer,
										  WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
										  &data, &format, &stride)) {
		return false;
	}
	if (stride < (size_t)dst_buffer->width * sizeof(uint32_t)) {
		wlr_buffer_end_data_ptr_access(dst_buffer);
		return false;
	}

	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, src_buffer);
	bool ok = texture != NULL &&
			  wlr_texture_read_pixels(texture,
									  &(struct wlr_texture_read_pixels_options){
										  .data = data,
										  .format = format,
										  .stride = stride,
									  });
	wlr_texture_destroy(texture);

	uint32_t pixel;
	if (ok && !capture_shield_shm_black_pixel(format, &pixel)) {
		ok = false;
	}

	if (ok) {
		Monitor *m = protected_source->output->data;
		Client *c;
		wl_list_for_each(c, &server.clients, link) {
			struct wlr_box box;
			if (!c->shield_when_capture || c->iskilling || !c->scene ||
				!VISIBLEON(c, m) ||
				!capture_shield_scene_box(
					&c->scene->node, &c->animation.current, m,
					protected_source->output, dst_buffer, &box)) {
				continue;
			}

			capture_shield_blacken_shm(protected_source, &c->scene->node, &box,
									   dst_buffer, frame, data, stride, pixel);
		}

		for (int32_t i = 0; i < 4; i++) {
			LayerSurface *l;
			wl_list_for_each(l, &m->layers[i], link) {
				struct wlr_box box;
				if (!l->shield_when_capture || !l->mapped || !l->scene ||
					!capture_shield_scene_box(
						&l->scene->node, capture_shield_layer_current_box(l), m,
						protected_source->output, dst_buffer, &box)) {
					continue;
				}

				capture_shield_blacken_shm(protected_source, &l->scene->node,
										   &box, dst_buffer, frame, data,
										   stride, pixel);
			}
		}
	}

	wlr_buffer_end_data_ptr_access(dst_buffer);
	return ok;
}

static bool
capture_shield_copy_dmabuf(struct protected_output_source *protected_source,
						   struct wlr_buffer *src_buffer,
						   struct wlr_ext_image_copy_capture_frame_v1 *frame) {
	struct wlr_ext_image_capture_source_v1 *source = protected_source->source;
	struct wlr_buffer *dst_buffer = frame->buffer;
	struct wlr_renderer *renderer = protected_source->output->renderer;

	/*
	 * ADAPTED FROM WLROOTS 0.20
	 * (types/wlr_ext_image_copy_capture_v1.c:copy_dmabuf). The source texture
	 * copy is wlroots' approach; the protected mask is Mango custom.
	 */
	if (!dst_buffer || !renderer || source->dmabuf_formats.len == 0) {
		return false;
	}

	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, src_buffer);
	if (!texture) {
		return false;
	}

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(renderer, dst_buffer, NULL);
	if (!pass) {
		wlr_texture_destroy(texture);
		return false;
	}

	wlr_render_pass_add_texture(pass,
								&(struct wlr_render_texture_options){
									.texture = texture,
									.dst_box =
										{
											.width = dst_buffer->width,
											.height = dst_buffer->height,
										},
									.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
								});

	Monitor *m = protected_source->output->data;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		struct wlr_box box;
		if (!c->shield_when_capture || c->iskilling || !c->scene ||
			!VISIBLEON(c, m) ||
			!capture_shield_scene_box(&c->scene->node, &c->animation.current, m,
									  protected_source->output, dst_buffer,
									  &box)) {
			continue;
		}

		capture_shield_blacken_dmabuf(protected_source, &c->scene->node, &box,
									  dst_buffer, pass, frame);
	}

	for (int32_t i = 0; i < 4; i++) {
		LayerSurface *l;
		wl_list_for_each(l, &m->layers[i], link) {
			struct wlr_box box;
			if (!l->shield_when_capture || !l->mapped || !l->scene ||
				!capture_shield_scene_box(
					&l->scene->node, capture_shield_layer_current_box(l), m,
					protected_source->output, dst_buffer, &box)) {
				continue;
			}

			capture_shield_blacken_dmabuf(protected_source, &l->scene->node,
										  &box, dst_buffer, pass, frame);
		}
	}

	bool ok = wlr_render_pass_submit(pass);
	wlr_texture_destroy(texture);
	return ok;
}

/*
 * WLROOTS ORIGINAL LOGIC, FORWARDED.
 * These callbacks delegate start/stop/frame scheduling/cursor lookup back to
 * the implementation saved before the Mango wrapper was installed.
 */
static void
protected_output_source_start(struct wlr_ext_image_capture_source_v1 *source,
							  bool with_cursors) {
	struct protected_output_source *protected_source =
		protected_output_source_find(source);
	if (protected_source && protected_source->impl->start) {
		protected_source->impl->start(source, with_cursors);
	}
}

static void
protected_output_source_stop(struct wlr_ext_image_capture_source_v1 *source) {
	struct protected_output_source *protected_source =
		protected_output_source_find(source);
	if (protected_source && protected_source->impl->stop) {
		protected_source->impl->stop(source);
	}
}

static void protected_output_source_request_frame(
	struct wlr_ext_image_capture_source_v1 *source, bool schedule_frame) {
	struct protected_output_source *protected_source =
		protected_output_source_find(source);
	if (protected_source && protected_source->impl->request_frame) {
		protected_source->impl->request_frame(source, schedule_frame);
	}
}

static void protected_output_source_copy_frame(
	struct wlr_ext_image_capture_source_v1 *source,
	struct wlr_ext_image_copy_capture_frame_v1 *frame,
	struct wlr_ext_image_capture_source_v1_frame_event *base_event) {
	struct protected_output_source *protected_source =
		protected_output_source_find(source);
	if (!protected_source ||
		!output_has_shield_surfaces(protected_source->output)) {
		/* MANGO policy decision: no shielded surface, use wlroots untouched. */
		if (protected_source && protected_source->impl->copy_frame) {
			protected_source->impl->copy_frame(source, frame, base_event);
		}
		return;
	}

	struct wlr_buffer *src_buffer;
	struct timespec when;
	if (!capture_shield_frame_info(base_event, &src_buffer, &when)) {
		wlr_ext_image_copy_capture_frame_v1_fail(
			frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
		return;
	}
	struct wlr_buffer *dst_buffer = frame->buffer;
	if (!dst_buffer || !src_buffer || dst_buffer->width != src_buffer->width ||
		dst_buffer->height != src_buffer->height) {
		wlr_ext_image_copy_capture_frame_v1_fail(
			frame,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
		return;
	}

	struct wlr_dmabuf_attributes dmabuf;
	bool ok =
		wlr_buffer_get_dmabuf(dst_buffer, &dmabuf)
			? capture_shield_copy_dmabuf(protected_source, src_buffer, frame)
			: capture_shield_copy_shm(protected_source, src_buffer, frame);
	if (!ok) {
		wlr_ext_image_copy_capture_frame_v1_fail(
			frame, EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
		return;
	}

	wlr_ext_image_copy_capture_frame_v1_ready(
		frame, protected_source->output->transform, &when);
}

static struct wlr_ext_image_capture_source_v1_cursor *
protected_output_source_get_pointer_cursor(
	struct wlr_ext_image_capture_source_v1 *source, struct wlr_seat *seat) {
	struct protected_output_source *protected_source =
		protected_output_source_find(source);
	if (protected_source && protected_source->impl->get_pointer_cursor) {
		return protected_source->impl->get_pointer_cursor(source, seat);
	}
	return NULL;
}

static const struct wlr_ext_image_capture_source_v1_interface
	protected_output_source_impl = {
		.start = protected_output_source_start,
		.stop = protected_output_source_stop,
		.request_frame = protected_output_source_request_frame,
		.copy_frame = protected_output_source_copy_frame,
		.get_pointer_cursor = protected_output_source_get_pointer_cursor,
};

static void protected_output_source_destroy(struct wl_listener *listener,
											void *data) {
	struct protected_output_source *protected_source =
		wl_container_of(listener, protected_source, destroy);
	wl_list_remove(&protected_source->destroy.link);
	wl_list_remove(&protected_source->link);
	free(protected_source);
}

void handle_output_capture_request(struct wl_listener *listener, void *data) {
	(void)listener;
	struct wlr_ext_output_image_capture_source_manager_v1_request_event
		*request = data;
	struct wlr_ext_image_capture_source_v1 *source =
		wlr_ext_image_capture_source_v1_create_with_raw_output(request->output);
	if (!source) {
		wlr_ext_output_image_capture_source_manager_v1_request_accept(request,
																	  NULL);
		return;
	}
	capture_shield_wrap_output_source(source);
	wlr_ext_output_image_capture_source_manager_v1_request_accept(request,
																  source);
}

/* MANGO CUSTOM: install the vtable wrapper and retain wlroots' original. */
void capture_shield_wrap_output_source(
	struct wlr_ext_image_capture_source_v1 *source) {
	if (!source || protected_output_source_find(source)) {
		return;
	}

	struct wlr_output *output =
		wlr_output_try_from_ext_image_capture_source_v1(source);
	if (!output) {
		return;
	}

	if (!protected_output_sources_initialized) {
		wl_list_init(&protected_output_sources);
		protected_output_sources_initialized = true;
	}

	struct protected_output_source *protected_source =
		calloc(1, sizeof(*protected_source));
	if (!protected_source) {
		mango_error(true, WLR_ERROR,
					"Failed to allocate protected output capture source");
		return;
	}

	protected_source->source = source;
	protected_source->output = output;
	protected_source->impl = source->impl;
	protected_source->destroy.notify = protected_output_source_destroy;
	wl_signal_add(&source->events.destroy, &protected_source->destroy);
	wl_list_insert(&protected_output_sources, &protected_source->link);
	source->impl = &protected_output_source_impl;
}
