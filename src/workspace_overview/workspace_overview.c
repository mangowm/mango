#include "mango/workspace_overview/workspace_overview.h"
#include "mango/animation/client.h"
#include "mango/animation/common.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/parse.h"
#include "mango/draw/dim-node.h"
#include "mango/draw/text-node.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/layout/arrange.h"
#include "mango/layout/tag_ratio.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/monitor.h"
#include "mango/manage/tab.h"
#include <GLES2/gl2.h>
#include <drm_fourcc.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/wlr_scene.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_color_representation_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/transform.h>

struct wsov_item {
	Client *client;
	uint32_t layer;
	struct wlr_box box;
	struct wlr_box workspace_geom;
	double render_scale;
	double render_scale_x;
	double render_scale_y;
};

struct wsov_bar_rect {
	struct wlr_box box;
	float color[4];
};

struct wsov_bar_buffer {
	struct wlr_box box;
	struct wlr_buffer *buffer;
	struct wlr_fbox src_box;
	int dst_width;
	int dst_height;
	enum wl_output_transform transform;
	float opacity;
};

struct wsov_bar_item {
	struct wsov_bar_rect rects[2];
	int rect_count;
	struct wsov_bar_buffer buffers[2];
	int buffer_count;
	uint32_t layer;
};

struct wsov_texture_ref {
	struct wlr_texture *texture;
	bool owned;
	struct wsov_texture_ref *next;
};

struct wsov_render_resources {
	struct wsov_texture_ref *textures;
	struct wlr_buffer *target;
};

struct wsov_layer_preview {
	struct wlr_scene_buffer *node;
	LayerSurface *layer;
	int layer_index;
	int workspace_x;
	int workspace_y;
	int width;
	int height;
	struct wlr_fbox src_box;
};

static bool wsov_retain_texture(struct wsov_render_resources *resources,
								struct wlr_texture *texture, bool owned) {
	if (!resources || !texture)
		return false;
	struct wsov_texture_ref *ref = malloc(sizeof(*ref));
	if (!ref)
		return false;
	ref->texture = texture;
	ref->owned = owned;
	ref->next = resources->textures;
	resources->textures = ref;
	return true;
}

static void wsov_release_textures(struct wsov_render_resources *resources) {
	if (!resources)
		return;
	struct wsov_texture_ref *ref = resources->textures;
	while (ref) {
		struct wsov_texture_ref *next = ref->next;
		if (ref->owned)
			wlr_texture_destroy(ref->texture);
		free(ref);
		ref = next;
	}
	resources->textures = NULL;
}

struct wsov_cell {
	uint32_t tag;
	struct wlr_box box;
	struct wlr_scene_rect *frame;
	struct wlr_scene_rect *border[4];
	struct wlr_scene_buffer *contents;
	struct wlr_scene_buffer *foreground;
	MangoDimNode *dim;
	struct wsov_item *items;
	int item_count;
	int item_capacity;
	struct wsov_bar_item *bar_items;
	int bar_item_count;
	int bar_item_capacity;
	struct wsov_layer_preview *layers;
	int layer_count;
	int layer_capacity;
	struct wlr_scene_node *overlay_top;
	int buffer_w;
	int buffer_h;
	struct wlr_box layout_from;
	struct wlr_box layout_current;
	bool borders_enabled;
	bool dim_enabled;
	bool contents_dirty;
	bool foreground_dirty;
};

static struct {
	Monitor *mon;
	struct wlr_scene_tree *root;
	struct wlr_scene_rect *background;
	struct wsov_cell *cells;
	int cell_count;
	struct wl_listener frame;
	struct wl_event_source *render_idle;
	struct wl_event_source *rebuild_idle;

	bool animating;
	bool closing;
	bool layers_dirty;
	bool layout_animating;
	double progress;
	double animation_from;
	double animation_to;
	struct timespec animation_start;
	struct timespec layout_animation_start;
	uint32_t camera_tag;
	uint32_t close_tag;
	Client *close_client;
	bool close_focus_explicit;

	bool pressed;
	bool moved;
	bool has_drag_client;
	uint32_t source_tag;
	uint32_t target_tag;
	uint32_t drag_return_tag;
	Client *drag_client;
	int drag_start_x;
	int drag_start_y;
	int drag_grab_x;
	int drag_grab_y;
	struct wlr_scene_buffer *drag_buffer;
	int drag_width;
	int drag_height;
	int drag_bar_height;
	struct wlr_scene_rect *drop_fill;
	struct wlr_scene_rect *drop_border[4];
	bool drop_indicator_visible;
	bool drag_floating_changed;
	bool drag_was_floating;
	bool drag_was_drag_to_tile;
	struct wlr_box drag_saved_float_geom;
} wsov;

static void wsov_finalize_close(uint32_t focus_tag);
static void wsov_close(uint32_t focus_tag, Client *focus_client);
static void wsov_apply_camera(double progress);
static void wsov_attach_frame_listener(void);
static void wsov_capture_workspace_tags(uint32_t tags);
static void wsov_capture_workspace_layouts(void);
static void wsov_detach_frame_listener(void);
static void wsov_refresh_layer_previews(void);
static void wsov_refresh_layer_preview(LayerSurface *layer);
static void wsov_render_all(void);
static void wsov_render_tags(uint32_t tags);
static void wsov_rebuild(void);
static void wsov_activate_target_tag(void);
static bool wsov_refresh_workspace_tags(uint32_t tags);
static void wsov_refresh_workspace_or_rebuild(uint32_t tags);
static void wsov_flush_dirty_cells(void *data);
static void wsov_schedule_dirty_render(void);
static void wsov_cancel_scheduled_render(void);
static void wsov_schedule_rebuild(void);
static void wsov_cancel_scheduled_rebuild(void);
static struct wlr_box wsov_cell_layout_box(const struct wsov_cell *cell);
static struct wlr_box wsov_item_display_box(const struct wsov_cell *cell,
											const struct wsov_item *item);
static void wsov_update_layout_animation(void);
static bool wsov_scene_node_layout_coords(struct wlr_scene_node *node, int *x,
										  int *y);
static void wsov_update_styles(void);
static void wsov_update_styles_tags(uint32_t tags);
static bool wsov_create_drop_indicator(void);
static void wsov_hide_drop_indicator(void);
static void wsov_update_drop_indicator(double x, double y);

static uint32_t wsov_tag_bit(uint32_t tag) {
	if (tag < 1 || tag > (uint32_t)config.tag_num)
		return 0;
	return 1u << (tag - 1);
}

static uint32_t wsov_client_tags(Client *c) {
	if (!c)
		return 0;
	return c->isglobal ? TAGMASK : (c->tags & TAGMASK);
}

static uint32_t wsov_cell_tags_for_client(Client *c) {
	uint32_t tags = 0;
	if (!c)
		return tags;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		for (int j = 0; j < cell->item_count; j++) {
			if (cell->items[j].client == c) {
				tags |= wsov_tag_bit(cell->tag);
				break;
			}
		}
	}
	return tags;
}

static uint32_t wsov_layout_affected_tags(Monitor *m) {
	uint32_t active_tags = m->tagset[m->seltags] & TAGMASK;
	if (!active_tags)
		active_tags = wsov_tag_bit(get_mon_curtag(m));
	uint32_t tags = active_tags;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || client_is_unmanaged(c) || c->iskilling)
			continue;
		if (c->isglobal || (c->tags & active_tags))
			tags |= wsov_client_tags(c);
	}
	return tags;
}

static bool wsov_client_eligible(Client *c) {
	if (!c || !c->mon || c->mon != wsov.mon || c->iskilling || c->isminimized ||
		c->is_tab_hidden || c->isunglobal || !client_surface(c) ||
		!client_surface(c)->mapped || client_is_unmanaged(c) ||
		client_is_x11_popup(c) || (c->tags & TAG0_MASK))
		return false;
	return true;
}

bool workspace_overview_active_workspace_will_empty(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return false;
	/* The swallowed client will re-enter the view during unmap processing. */
	if (c->swallowing)
		return false;

	uint32_t active = wsov.mon->tagset[wsov.mon->seltags] & TAGMASK;
	if (!(c->tags & active))
		return false;

	Client *other;
	wl_list_for_each(other, &server.clients, link) {
		if (other == c || !wsov_client_eligible(other))
			continue;
		if (other->isglobal || (other->tags & active))
			return false;
	}
	return true;
}

static bool wsov_client_on_tag(Client *c, uint32_t tag) {
	if (!wsov_client_eligible(c))
		return false;
	if (c->isglobal)
		return true;
	uint32_t bit = wsov_tag_bit(tag);
	return bit && (c->tags & bit);
}

static Client *wsov_focus_top_for_tag(uint32_t tag) {
	Client *c;
	wl_list_for_each(c, &server.focus_stack, flink) {
		if (wsov_client_on_tag(c, tag))
			return c;
	}
	return NULL;
}

static void wsov_sync_monitor_selection(void) {
	if (!wsov.mon || !wsov.target_tag)
		return;
	Client *top = wsov_focus_top_for_tag(wsov.target_tag);
	if (!top)
		return;
	if (wsov.mon->sel != top || !top->isfocusing)
		client_focus(top, 0);
	if (wsov.mon->sel != top)
		wsov.mon->sel = top;
}

static bool wsov_tag_has_normal_client(uint32_t tag) {
	uint32_t bit = wsov_tag_bit(tag);
	if (!bit)
		return false;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (wsov_client_eligible(c) && !c->isglobal && (c->tags & bit))
			return true;
	}
	return false;
}

static uint32_t wsov_collect_workspace_tags(Monitor *m, int *count) {
	uint32_t mask = 0;
	int n = 0;

	if (!m) {
		if (count)
			*count = 0;
		return 0;
	}

	if (m->workspace_overview_scope == ALL_WORKSPACE) {
		if (count)
			*count = config.tag_num;
		return ((uint32_t)1 << config.tag_num) - 1;
	}

	for (int tag = 1; tag <= config.tag_num; tag++) {
		if (wsov_tag_has_normal_client((uint32_t)tag)) {
			mask |= wsov_tag_bit((uint32_t)tag);
			n++;
		}
	}
	if (count)
		*count = n;
	return mask;
}

static float wsov_premultiplied(float component, float alpha) {
	return component * alpha;
}

static void wsov_set_rect_color(struct wlr_scene_rect *rect,
								const float color[4]) {
	float premultiplied[4] = {
		wsov_premultiplied(color[0], color[3]),
		wsov_premultiplied(color[1], color[3]),
		wsov_premultiplied(color[2], color[3]),
		color[3],
	};
	wlr_scene_rect_set_color(rect, premultiplied);
}

static bool wsov_create_drop_indicator(void) {
	if (!wsov.root)
		return false;

	float border[4] = {config.dropcolor[0], config.dropcolor[1],
					   config.dropcolor[2], 1.0f};
	wsov.drop_fill = wlr_scene_rect_create(wsov.root, 1, 1, config.dropcolor);
	for (int i = 0; i < 4; i++) {
		wsov.drop_border[i] = wlr_scene_rect_create(wsov.root, 1, 1, border);
		if (!wsov.drop_border[i])
			return false;
	}
	if (!wsov.drop_fill)
		return false;
	wsov_set_rect_color(wsov.drop_fill, config.dropcolor);
	for (int i = 0; i < 4; i++)
		wsov_set_rect_color(wsov.drop_border[i], border);
	wsov_hide_drop_indicator();
	return true;
}

static struct wsov_cell *wsov_cell_for_tag(uint32_t tag) {
	for (int i = 0; i < wsov.cell_count; i++) {
		if (wsov.cells[i].tag == tag)
			return &wsov.cells[i];
	}
	return NULL;
}

static struct wlr_box wsov_cell_layout_box(const struct wsov_cell *cell) {
	if (!cell)
		return (struct wlr_box){0};
	return wsov.layout_animating ? cell->layout_current : cell->box;
}

static struct wlr_box wsov_item_display_box(const struct wsov_cell *cell,
											const struct wsov_item *item) {
	if (!cell || !item)
		return (struct wlr_box){0};

	struct wlr_box cell_box = wsov_cell_layout_box(cell);
	double scale_x =
		(double)MANGO_MAX(1, cell_box.width -
								 2 * config.workspace_overview_border_width) /
		MANGO_MAX(1, cell->buffer_w);
	double scale_y =
		(double)MANGO_MAX(1, cell_box.height -
								 2 * config.workspace_overview_border_width) /
		MANGO_MAX(1, cell->buffer_h);
	double local_x =
		item->box.x - cell->box.x - config.workspace_overview_border_width;
	double local_y =
		item->box.y - cell->box.y - config.workspace_overview_border_width;
	return (struct wlr_box){
		.x = cell_box.x + config.workspace_overview_border_width +
			 (int)lround(local_x * scale_x),
		.y = cell_box.y + config.workspace_overview_border_width +
			 (int)lround(local_y * scale_y),
		.width = MANGO_MAX(1, (int)lround(item->box.width * scale_x)),
		.height = MANGO_MAX(1, (int)lround(item->box.height * scale_y)),
	};
}

static struct wlr_box wsov_monitor_clip(void) {
	return wsov.mon
			   ? (struct wlr_box){0, 0, wsov.mon->m.width, wsov.mon->m.height}
			   : (struct wlr_box){0};
}

static bool wsov_intersect_monitor(int x, int y, int width, int height,
								   struct wlr_box *visible) {
	struct wlr_box box = {x, y, width, height};
	struct wlr_box clip = wsov_monitor_clip();
	*visible = (struct wlr_box){0};
	if (box.width <= 0 || box.height <= 0 || clip.width <= 0 ||
		clip.height <= 0 || !wlr_box_intersection(visible, &box, &clip))
		return false;
	return !wlr_box_empty(visible);
}

static void wsov_place_rect_clipped(struct wlr_scene_rect *rect, int x, int y,
									int width, int height, bool enabled) {
	struct wlr_box visible;
	if (!rect)
		return;
	if (!enabled || !wsov_intersect_monitor(x, y, width, height, &visible)) {
		wlr_scene_node_set_enabled(&rect->node, false);
		return;
	}
	wlr_scene_node_set_enabled(&rect->node, true);
	wlr_scene_node_set_position(&rect->node, visible.x, visible.y);
	wlr_scene_rect_set_size(rect, visible.width, visible.height);
}

static void wsov_place_buffer_clipped(struct wlr_scene_buffer *node,
									  const struct wlr_fbox *base_src, int x,
									  int y, int width, int height) {
	struct wlr_box visible;
	if (!node)
		return;
	if (!node->buffer ||
		!wsov_intersect_monitor(x, y, width, height, &visible)) {
		wlr_scene_node_set_enabled(&node->node, false);
		return;
	}

	struct wlr_fbox src = base_src
							  ? *base_src
							  : (struct wlr_fbox){0, 0, node->buffer->width,
												  node->buffer->height};
	if (src.width <= 0 || src.height <= 0)
		src =
			(struct wlr_fbox){0, 0, node->buffer->width, node->buffer->height};

	if (node->transform == WL_OUTPUT_TRANSFORM_NORMAL) {
		src.x += src.width * (visible.x - x) / width;
		src.y += src.height * (visible.y - y) / height;
		src.width *= (double)visible.width / width;
		src.height *= (double)visible.height / height;
	} else {
		int buffer_w = node->buffer->width;
		int buffer_h = node->buffer->height;
		struct wlr_fbox oriented;
		wlr_fbox_transform(&oriented, &src, node->transform, buffer_w,
						   buffer_h);
		oriented.x += oriented.width * (visible.x - x) / width;
		oriented.y += oriented.height * (visible.y - y) / height;
		oriented.width *= (double)visible.width / width;
		oriented.height *= (double)visible.height / height;
		bool swapped = node->transform == WL_OUTPUT_TRANSFORM_90 ||
					   node->transform == WL_OUTPUT_TRANSFORM_270 ||
					   node->transform == WL_OUTPUT_TRANSFORM_FLIPPED_90 ||
					   node->transform == WL_OUTPUT_TRANSFORM_FLIPPED_270;
		int oriented_w = swapped ? buffer_h : buffer_w;
		int oriented_h = swapped ? buffer_w : buffer_h;
		wlr_fbox_transform(&src, &oriented,
						   wlr_output_transform_invert(node->transform),
						   oriented_w, oriented_h);
	}

	wlr_scene_node_set_enabled(&node->node, true);
	wlr_scene_node_set_position(&node->node, visible.x, visible.y);
	wlr_scene_buffer_set_dest_size(node, visible.width, visible.height);
	wlr_scene_buffer_set_source_box(node, &src);
}

static double wsov_elapsed_ms(const struct timespec *start) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - start->tv_sec) * 1000.0 +
		   (now.tv_nsec - start->tv_nsec) / 1000000.0;
}

static double wsov_output_scale(void) {
	return wsov.mon && wsov.mon->wlr_output
			   ? fmax(1.0, wsov.mon->wlr_output->scale)
			   : 1.0;
}

static void wsov_rebind_pass(struct wsov_render_resources *resources) {
	if (!resources || !resources->target)
		return;
	GLuint fbo = fx_renderer_get_buffer_fbo(server.renderer, resources->target);
	if (fbo)
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

static void wsov_handle_frame(struct wl_listener *listener, void *data) {
	if (!workspace_overview_is_active())
		return;

	if (wsov.layers_dirty) {
		wsov.layers_dirty = false;
		wlr_scene_node_raise_to_top(&wsov.root->node);
		wsov_refresh_layer_previews();
	}

	if (!config.animations) {
		wsov.layout_animating = false;
		wsov.progress = wsov.animation_to;
		wsov.animating = false;
		wsov_apply_camera(wsov.progress);
		if (wsov.closing)
			wsov_finalize_close(wsov.close_tag);
		else
			wsov_detach_frame_listener();
		return;
	}

	bool layout_was_animating = wsov.layout_animating;
	if (layout_was_animating)
		wsov_update_layout_animation();
	if (layout_was_animating && !wsov.animating) {
		wsov_apply_camera(wsov.progress);
		if (!wsov.layout_animating)
			wsov_detach_frame_listener();
		return;
	}

	if (!wsov.animating) {
		wsov_detach_frame_listener();
		return;
	}

	double duration = wsov.closing ? config.animation_duration_wsov_close
								   : config.animation_duration_wsov_open;
	double t = wsov_elapsed_ms(&wsov.animation_start) /
			   MANGO_MAX(1.0, (double)duration);
	t = fmax(0.0, fmin(1.0, t));
	double eased = find_animation_curve_for(
		t, wsov.closing ? config.animation_curve_wsov_close
						: config.animation_curve_wsov_open);
	wsov.progress =
		wsov.animation_from + (wsov.animation_to - wsov.animation_from) * eased;
	wsov_apply_camera(wsov.progress);

	if (t >= 1.0) {
		wsov.progress = wsov.animation_to;
		wsov.animating = false;
		if (wsov.closing)
			wsov_finalize_close(wsov.close_tag);
		else if (!wsov.layout_animating)
			wsov_detach_frame_listener();
	}
}

static void wsov_attach_frame_listener(void) {
	if (!wsov.mon)
		return;
	if (wsov.frame.link.prev && wsov.frame.link.next &&
		wsov.frame.link.prev != &wsov.frame.link)
		return;
	wsov.frame.notify = wsov_handle_frame;
	wl_signal_add(&wsov.mon->wlr_output->events.frame, &wsov.frame);
}

static void wsov_detach_frame_listener(void) {
	if (!wsov.frame.link.prev || !wsov.frame.link.next ||
		wsov.frame.link.prev == &wsov.frame.link)
		return;
	wl_list_remove(&wsov.frame.link);
	wl_list_init(&wsov.frame.link);
}

static void wsov_select_default_target(void) {
	if (wsov.cell_count <= 0) {
		wsov.target_tag = 0;
		return;
	}
	if (wsov_cell_for_tag(wsov.target_tag))
		return;

	uint32_t active = wsov.mon->tagset[wsov.mon->seltags] & TAGMASK;
	for (int i = 0; i < wsov.cell_count; i++) {
		if (active & wsov_tag_bit(wsov.cells[i].tag)) {
			wsov.target_tag = wsov.cells[i].tag;
			return;
		}
	}
	wsov.target_tag = wsov.cells[0].tag;
}

static struct wsov_cell *wsov_cell_at(double x, double y) {
	if (!wsov.root || !wsov.mon)
		return NULL;

	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wlr_box box = wsov_cell_layout_box(&wsov.cells[i]);
		if (wlr_box_contains_point(&box, lx, ly))
			return &wsov.cells[i];
	}
	return NULL;
}

static struct wsov_cell *wsov_drop_cell_at(double x, double y) {
	struct wsov_cell *cell = wsov_cell_at(x, y);
	if (cell || !wsov.mon || x < wsov.mon->m.x ||
		x >= wsov.mon->m.x + wsov.mon->m.width || y < wsov.mon->m.y ||
		y >= wsov.mon->m.y + wsov.mon->m.height)
		return cell;

	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	struct wsov_cell *nearest = NULL;
	double best = 1e30;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wlr_box box = wsov_cell_layout_box(&wsov.cells[i]);
		double cx = box.x + box.width / 2.0;
		double cy = box.y + box.height / 2.0;
		double dx = cx - lx;
		double dy = cy - ly;
		double distance = dx * dx + dy * dy;
		if (distance < best) {
			best = distance;
			nearest = &wsov.cells[i];
		}
	}
	return nearest;
}

static bool wsov_set_target_from_point(double x, double y) {
	struct wsov_cell *cell = wsov_cell_at(x, y);
	if (!cell || cell->tag == wsov.target_tag)
		return false;

	wsov.target_tag = cell->tag;
	wsov.camera_tag = cell->tag;
	return true;
}

static struct wsov_item *wsov_item_at(struct wsov_cell *cell, double x,
									  double y) {
	if (!cell || !wsov.mon)
		return NULL;
	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	struct wsov_item *hit = NULL;
	for (int i = 0; i < cell->item_count; i++) {
		struct wsov_item *item = &cell->items[i];
		struct wlr_box box = wsov_item_display_box(cell, item);
		if (wlr_box_contains_point(&box, lx, ly) &&
			(!hit || item->layer >= hit->layer))
			hit = item;
	}
	return hit;
}

static void wsov_item_add(struct wsov_cell *cell, Client *c) {
	if (cell->item_count == cell->item_capacity) {
		int capacity = cell->item_capacity ? cell->item_capacity * 2 : 8;
		struct wsov_item *items =
			realloc(cell->items, capacity * sizeof(*items));
		if (!items)
			return;
		cell->items = items;
		cell->item_capacity = capacity;
	}
	cell->items[cell->item_count++] = (struct wsov_item){.client = c};
	cell->items[cell->item_count - 1].layer = client_target_layer(c);
}

static void wsov_layer_preview_add(struct wsov_cell *cell,
								   struct wlr_scene_buffer *source,
								   LayerSurface *layer, int layer_index, int sx,
								   int sy) {
	if (!source || !source->buffer)
		return;

	if (cell->layer_count == cell->layer_capacity) {
		int capacity = cell->layer_capacity ? cell->layer_capacity * 2 : 8;
		struct wsov_layer_preview *layers =
			realloc(cell->layers, capacity * sizeof(*layers));
		if (!layers)
			return;
		cell->layers = layers;
		cell->layer_capacity = capacity;
	}

	struct wlr_scene_buffer *node =
		wlr_scene_buffer_create(wsov.root, source->buffer);
	if (!node)
		return;
	wlr_scene_buffer_set_source_box(node, &source->src_box);
	wlr_scene_buffer_set_transform(node, source->transform);
	wlr_scene_buffer_set_filter_mode(node, WLR_SCALE_FILTER_BILINEAR);
	wlr_scene_buffer_set_opacity(node, 1.0f);
	wlr_scene_buffer_set_transfer_function(node, source->transfer_function);
	wlr_scene_buffer_set_primaries(node, source->primaries);
	wlr_scene_buffer_set_color_encoding(node, source->color_encoding);
	wlr_scene_buffer_set_color_range(node, source->color_range);
	if (layer_index == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		struct wlr_scene_node *anchor =
			cell->overlay_top ? cell->overlay_top : &cell->foreground->node;
		wlr_scene_node_place_above(&node->node, anchor);
		cell->overlay_top = &node->node;
	} else if (layer_index == ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
		wlr_scene_node_place_below(&node->node, &cell->foreground->node);
	} else {
		wlr_scene_node_place_below(&node->node, &cell->contents->node);
	}

	struct wlr_fbox src_box = source->src_box;
	if (src_box.width <= 0 || src_box.height <= 0)
		src_box = (struct wlr_fbox){0, 0, source->buffer->width,
									source->buffer->height};
	cell->layers[cell->layer_count++] = (struct wsov_layer_preview){
		.node = node,
		.layer = layer,
		.layer_index = layer_index,
		.workspace_x = sx - wsov.mon->m.x,
		.workspace_y = sy - wsov.mon->m.y,
		.width =
			source->dst_width > 0 ? source->dst_width : source->buffer->width,
		.height = source->dst_height > 0 ? source->dst_height
										 : source->buffer->height,
		.src_box = src_box,
	};
}

typedef struct {
	struct wsov_cell *cell;
	LayerSurface *layer;
	int layer_index;
} wsov_layer_iter_data;

static void wsov_collect_layer_buffer(struct wlr_scene_buffer *buffer, int sx,
									  int sy, void *data) {
	wsov_layer_iter_data *iter = data;
	wsov_layer_preview_add(iter->cell, buffer, iter->layer, iter->layer_index,
						   sx, sy);
}

static void wsov_create_layer_previews_for_layer(struct wsov_cell *cell,
												 LayerSurface *layer,
												 int layer_index) {
	if (!cell || !layer || !layer->scene || !layer->scene->node.enabled)
		return;
	wsov_layer_iter_data iter = {
		.cell = cell,
		.layer = layer,
		.layer_index = layer_index,
	};
	wlr_scene_node_for_each_buffer(&layer->scene->node,
								   wsov_collect_layer_buffer, &iter);
}

static void wsov_create_layer_previews(void) {
	for (int cell_index = 0; cell_index < wsov.cell_count; cell_index++) {
		struct wsov_cell *cell = &wsov.cells[cell_index];
		for (int layer_index = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
			 layer_index <= ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY; layer_index++) {
			LayerSurface *layer;
			wl_list_for_each_reverse(layer, &wsov.mon->layers[layer_index],
									 link)
				wsov_create_layer_previews_for_layer(cell, layer, layer_index);
		}
	}
}

static void wsov_remove_layer_previews(struct wsov_cell *cell,
									   LayerSurface *layer) {
	if (!cell || !layer)
		return;
	int write = 0;
	for (int read = 0; read < cell->layer_count; read++) {
		struct wsov_layer_preview *preview = &cell->layers[read];
		if (preview->layer == layer) {
			wlr_scene_node_destroy(&preview->node->node);
			continue;
		}
		if (write != read)
			cell->layers[write] = cell->layers[read];
		write++;
	}
	cell->layer_count = write;
	cell->overlay_top = NULL;
	for (int i = 0; i < cell->layer_count; i++) {
		if (cell->layers[i].layer_index == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)
			cell->overlay_top = &cell->layers[i].node->node;
	}
}

static void wsov_destroy_layer_previews(struct wsov_cell *cell) {
	for (int i = 0; i < cell->layer_count; i++)
		wlr_scene_node_destroy(&cell->layers[i].node->node);
	free(cell->layers);
	cell->layers = NULL;
	cell->layer_count = 0;
	cell->layer_capacity = 0;
	cell->overlay_top = NULL;
}

static void wsov_refresh_layer_previews(void) {
	if (!workspace_overview_is_active())
		return;
	for (int i = 0; i < wsov.cell_count; i++)
		wsov_destroy_layer_previews(&wsov.cells[i]);
	wsov_create_layer_previews();
	wsov_update_styles();
	if (!wsov.animating)
		wsov_apply_camera(wsov.progress);
}

static void wsov_refresh_layer_preview(LayerSurface *layer) {
	if (!workspace_overview_is_active_on(layer ? layer->mon : NULL) ||
		wsov.closing)
		return;
	int layer_index = layer->layer_surface->current.layer;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		wsov_remove_layer_previews(cell, layer);
		wsov_create_layer_previews_for_layer(cell, layer, layer_index);
	}
	wlr_scene_node_raise_to_top(&wsov.root->node);
	if (!wsov.animating)
		wsov_apply_camera(wsov.progress);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

static bool wsov_drag_hides_item(struct wsov_cell *cell, Client *c) {
	return wsov.moved && wsov.has_drag_client && c == wsov.drag_client &&
		   cell->tag == wsov.source_tag;
}

static bool wsov_drag_hides_group(struct wsov_cell *cell, Client *group_head) {
	if (!wsov.moved || !wsov.has_drag_client || !group_head ||
		cell->tag != wsov.source_tag)
		return false;
	for (Client *cur = group_head; cur; cur = cur->group_next)
		if (cur == wsov.drag_client)
			return true;
	return false;
}

static void wsov_capture_cell_items(struct wsov_cell *cell) {
	cell->item_count = 0;
	Client *c;
	wl_list_for_each_reverse(c, &server.focus_stack, flink) {
		if (!wsov_client_on_tag(c, cell->tag))
			continue;
		wsov_item_add(cell, c);
		cell->items[cell->item_count - 1].workspace_geom = c->geom;
	}
}

static void wsov_capture_bar_rect(struct wsov_bar_item *item,
								  struct wlr_scene_rect *rect) {
	if (!rect || !rect->node.enabled || item->rect_count >= 2)
		return;
	int lx, ly;
	if (!wsov_scene_node_layout_coords(&rect->node, &lx, &ly))
		return;
	struct wsov_bar_rect *snapshot = &item->rects[item->rect_count++];
	snapshot->box = (struct wlr_box){lx, ly, rect->width, rect->height};
	memcpy(snapshot->color, rect->color, sizeof(snapshot->color));
}

static void wsov_capture_bar_buffer(struct wsov_bar_item *item,
									struct wlr_scene_buffer *node) {
	if (!node || !node->node.enabled || !node->buffer ||
		item->buffer_count >= 2)
		return;
	int lx, ly;
	if (!wsov_scene_node_layout_coords(&node->node, &lx, &ly))
		return;
	struct wlr_buffer *buffer = wlr_buffer_lock(node->buffer);
	if (!buffer)
		return;
	int width = node->dst_width > 0 ? node->dst_width : node->buffer->width;
	int height = node->dst_height > 0 ? node->dst_height : node->buffer->height;
	item->buffers[item->buffer_count++] = (struct wsov_bar_buffer){
		.box = {lx, ly, width, height},
		.buffer = buffer,
		.src_box = node->src_box,
		.dst_width = node->dst_width,
		.dst_height = node->dst_height,
		.transform = node->transform,
		.opacity = node->opacity,
	};
}

static void wsov_capture_bar(struct wsov_cell *cell, MangoBarDecoration *bar) {
	if (!bar)
		return;
	if (cell->bar_item_count == cell->bar_item_capacity) {
		int capacity =
			cell->bar_item_capacity ? cell->bar_item_capacity * 2 : 8;
		struct wsov_bar_item *items =
			realloc(cell->bar_items, capacity * sizeof(*items));
		if (!items)
			return;
		cell->bar_items = items;
		cell->bar_item_capacity = capacity;
	}
	struct wsov_bar_item *item = &cell->bar_items[cell->bar_item_count++];
	memset(item, 0, sizeof(*item));
	Client *owner = bar->node_data;
	item->layer = owner ? client_target_layer(owner) : LyrTile;
	if (owner && !bar->scene_buffer->buffer)
		mango_bar_decoration_update(
			bar, client_get_title(owner),
			wsov.mon->wlr_output ? wsov.mon->wlr_output->scale : 1.0f);
	wsov_capture_bar_rect(item, bar->border);
	wsov_capture_bar_rect(item, bar->bg);
	wsov_capture_bar_buffer(item, bar->scene_buffer);
	wsov_capture_bar_buffer(item, bar->close_buffer);
}

static void wsov_clear_cell_bars(struct wsov_cell *cell) {
	for (int i = 0; i < cell->bar_item_count; i++) {
		struct wsov_bar_item *item = &cell->bar_items[i];
		for (int j = 0; j < item->buffer_count; j++)
			wlr_buffer_unlock(item->buffers[j].buffer);
	}
	cell->bar_item_count = 0;
}

static void wsov_draw_group_bar_for_capture(Client *c) {
	if (!c)
		return;
	struct wlr_box saved = c->animation.current;
	/* Bar layout must use the arranged workspace geometry. During tag changes
	 * animation.current can still contain the source/floating size. */
	c->animation.current = c->geom;
	client_draw_group_bar(c, (struct ivec2){0});
	c->animation.current = saved;
}

static void wsov_draw_tabbar_for_capture(Client *c) {
	if (!c)
		return;
	struct wlr_box saved = c->animation.current;
	c->animation.current = c->geom;
	client_draw_tabbar(c, (struct ivec2){0});
	c->animation.current = saved;
}

static bool wsov_group_capture_owner(Client *c) {
	for (Client *cur = client_group_head(c); cur; cur = cur->group_next) {
		if (cur == c)
			return true;
		if (!client_is_parked(cur))
			return false;
	}
	return true;
}

static void wsov_capture_cell_bars(struct wsov_cell *cell) {
	wsov_clear_cell_bars(cell);
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (!c->group_bar && !c->tab_bar)
			continue;

		Client *group_head = c->group_bar ? client_group_head(c) : NULL;
		if (group_head && wsov_group_capture_owner(c)) {
			Client *group_focus = client_group_focused(group_head);
			Client *group_anchor = NULL;
			if (group_focus && group_focus->mon == wsov.mon &&
				!group_focus->is_tab_hidden &&
				(group_focus->isglobal ||
				 (group_focus->tags & wsov_tag_bit(cell->tag))) &&
				client_wants_group_bar(group_focus) && ISNORMAL(group_focus) &&
				!group_focus->isfullscreen) {
				group_anchor = group_focus;
			} else {
				for (Client *cur = group_head; cur; cur = cur->group_next) {
					if (cur->mon == wsov.mon && !cur->is_tab_hidden &&
						(cur->isglobal ||
						 (cur->tags & wsov_tag_bit(cell->tag))) &&
						client_wants_group_bar(cur) && ISNORMAL(cur) &&
						!cur->isfullscreen) {
						group_anchor = cur;
						break;
					}
				}
			}
			if (group_anchor && !wsov_drag_hides_group(cell, group_head)) {
				/* Arrange does not render decorations. Recalculate the group
				 * strip against this tag's arranged geometry before taking the
				 * snapshots, including members that are parked from
				 * server.clients. */
				wsov_draw_group_bar_for_capture(group_anchor);
				for (Client *cur = group_head; cur; cur = cur->group_next) {
					if (cur->group_bar)
						wsov_capture_bar(cell, cur->group_bar);
				}
			}
		}

		if (!c->tab_prev) {
			Client *tab_head = c;
			Client *tab_anchor = NULL;
			for (Client *cur = tab_head; cur; cur = cur->tab_next) {
				if (cur->mon == wsov.mon && cur->is_tab_focus &&
					!cur->is_tab_hidden &&
					(cur->isglobal || (cur->tags & wsov_tag_bit(cell->tag)))) {
					tab_anchor = cur;
					break;
				}
			}
			if (tab_anchor)
				wsov_draw_tabbar_for_capture(tab_anchor);
			for (Client *cur = tab_head; cur; cur = cur->tab_next) {
				if (cur->tab_bar && cur->tab_bar->scene->node.enabled)
					wsov_capture_bar(cell, cur->tab_bar);
			}
		}
	}
}

static void wsov_position_cell_items(struct wsov_cell *cell) {
	double scale_x = (double)cell->buffer_w / MANGO_MAX(1, wsov.mon->m.width);
	double scale_y = (double)cell->buffer_h / MANGO_MAX(1, wsov.mon->m.height);
	for (int i = 0; i < cell->item_count; i++) {
		struct wlr_box geo = cell->items[i].workspace_geom;
		int x = cell->box.x + config.workspace_overview_border_width +
				(int)lround((geo.x - wsov.mon->m.x) * scale_x);
		int y = cell->box.y + config.workspace_overview_border_width +
				(int)lround((geo.y - wsov.mon->m.y) * scale_y);
		int w = MANGO_MAX(1, (int)lround(geo.width * scale_x));
		int h = MANGO_MAX(1, (int)lround(geo.height * scale_y));
		cell->items[i].box = (struct wlr_box){x, y, w, h};
		cell->items[i].render_scale = fmin(scale_x, scale_y);
		cell->items[i].render_scale_x = scale_x;
		cell->items[i].render_scale_y = scale_y;
	}
}

static void wsov_settle_capture_clients(Monitor *m, uint32_t tags) {
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || client_is_unmanaged(c) || c->iskilling)
			continue;
		if (!c->isglobal && !(c->tags & tags))
			continue;
		c->animation.running = false;
		c->animation.tagining = false;
		c->animation.tagouting = false;
		c->need_output_flush = false;
		client_set_finish_geometry(c);
		client_update_visibility(c);
	}
}

static void wsov_capture_workspace_tags(uint32_t tags) {
	if (!workspace_overview_is_active() || wsov.cell_count <= 0)
		return;

	Monitor *m = wsov.mon;
	uint32_t selected = 0;
	for (int i = 0; i < wsov.cell_count; i++) {
		uint32_t bit = wsov_tag_bit(wsov.cells[i].tag);
		if (tags & bit)
			selected |= bit;
	}
	if (!selected)
		return;

	uint32_t saved_tags[2] = {m->tagset[0], m->tagset[1]};
	uint32_t saved_seltags = m->seltags;
	uint32_t saved_curtag = m->pertag->curtag;
	uint32_t saved_prevtag = m->pertag->prevtag;
	Client *saved_sel = m->sel;
	Client *saved_prevsel = m->prevsel;
	bool saved_tag_gather = config.tag_gather;

	sync_tag_ratio_state_from_clients(m, saved_curtag);
	config.tag_gather = false;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !client_is_unmanaged(c) &&
			(c->isglobal || (c->tags & selected))) {
			c->wsov_hidden = false;
			client_update_visibility(c);
		}
	}

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		uint32_t tag = cell->tag;
		if (!(selected & wsov_tag_bit(tag)))
			continue;
		m->tagset[saved_seltags] = wsov_tag_bit(tag);
		m->pertag->curtag = tag;
		arrange(m, false, true);
		wsov_settle_capture_clients(m, selected);
		wsov_capture_cell_items(cell);
		wsov_capture_cell_bars(cell);
	}

	m->seltags = saved_seltags;
	m->tagset[0] = saved_tags[0];
	m->tagset[1] = saved_tags[1];
	m->pertag->curtag = saved_curtag;
	m->pertag->prevtag = saved_prevtag;
	arrange(m, false, true);
	wsov_settle_capture_clients(m, selected);
	m->sel = saved_sel;
	m->prevsel = saved_prevsel;
	config.tag_gather = saved_tag_gather;

	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !client_is_unmanaged(c) &&
			(c->isglobal || (c->tags & selected))) {
			c->wsov_hidden = true;
			client_update_visibility(c);
		}
	}
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (selected & wsov_tag_bit(cell->tag))
			wsov_position_cell_items(cell);
	}
}

static void wsov_capture_workspace_layouts(void) {
	uint32_t tags = 0;
	for (int i = 0; i < wsov.cell_count; i++)
		tags |= wsov_tag_bit(wsov.cells[i].tag);
	wsov_capture_workspace_tags(tags);
}

static const struct wlr_drm_format *wsov_pick_buffer_format(void) {
	uint32_t caps = server.allocator->buffer_caps;
	const struct wlr_drm_format_set *formats =
		wlr_renderer_get_texture_formats(server.renderer, caps);
	if (!formats)
		return NULL;

	static const uint32_t candidates[] = {
		DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_XRGB8888,
		DRM_FORMAT_XBGR8888, DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888,
	};
	for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
		const struct wlr_drm_format *format =
			wlr_drm_format_set_get(formats, candidates[i]);
		if (format)
			return format;
	}
	return NULL;
}

static struct wlr_buffer *wsov_create_buffer(int width, int height) {
	if (!server.allocator || !server.renderer || width <= 0 || height <= 0)
		return NULL;
	const struct wlr_drm_format *format = wsov_pick_buffer_format();
	if (!format)
		return NULL;
	return wlr_allocator_create_buffer(server.allocator, width, height, format);
}

struct wsov_surface_render_data {
	struct wlr_render_pass *pass;
	struct wsov_cell *cell;
	struct wsov_item *item;
	struct wsov_render_resources *resources;
	double scale_x;
	double scale_y;
	double cell_scale;
	pixman_region32_t clip;
};

static void wsov_render_scene_buffer(struct wlr_scene_buffer *scene_buffer,
									 int sx, int sy, void *data) {
	struct wsov_surface_render_data *render = data;
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface || !scene_buffer->buffer)
		return;

	struct wlr_texture *texture =
		wlr_texture_from_buffer(server.renderer, scene_buffer->buffer);
	bool owned = true;
	if (!texture && scene_surface->surface->buffer) {
		texture = wlr_texture_from_buffer(
			server.renderer, &scene_surface->surface->buffer->base);
	}
	if (!texture && scene_surface->surface->buffer &&
		scene_surface->surface->buffer->texture) {
		texture = scene_surface->surface->buffer->texture;
		owned = false;
	}
	if (!texture && scene_buffer->WLR_PRIVATE.texture) {
		texture = scene_buffer->WLR_PRIVATE.texture;
		owned = false;
	}
	if (!texture)
		return;
	wsov_rebind_pass(render->resources);
	if (!wsov_retain_texture(render->resources, texture, owned)) {
		if (owned)
			wlr_texture_destroy(texture);
		return;
	}

	int surface_width = scene_buffer->dst_width > 0
							? scene_buffer->dst_width
							: scene_buffer->buffer->width;
	int surface_height = scene_buffer->dst_height > 0
							 ? scene_buffer->dst_height
							 : scene_buffer->buffer->height;

	int cell_x = render->cell->box.x + config.workspace_overview_border_width;
	int cell_y = render->cell->box.y + config.workspace_overview_border_width;
	int dx = (int)lround((render->item->box.x - cell_x) * render->cell_scale +
						 sx * render->scale_x);
	int dy = (int)lround((render->item->box.y - cell_y) * render->cell_scale +
						 sy * render->scale_y);
	int dw = MANGO_MAX(1, (int)lround(surface_width * render->scale_x));
	int dh = MANGO_MAX(1, (int)lround(surface_height * render->scale_y));

	float alpha = scene_buffer->opacity;
	struct wlr_fbox source_box = scene_buffer->src_box;
	if (source_box.width <= 0 || source_box.height <= 0) {
		source_box = (struct wlr_fbox){0, 0, scene_buffer->buffer->width,
									   scene_buffer->buffer->height};
	}
	struct wlr_color_primaries primaries = {0};
	wlr_color_primaries_from_named(&primaries, scene_buffer->primaries);
	struct wlr_linux_drm_syncobj_surface_v1_state *syncobj =
		wlr_linux_drm_syncobj_v1_get_surface_state(scene_surface->surface);
	wlr_render_pass_add_texture(
		render->pass,
		&(struct wlr_render_texture_options){
			.texture = texture,
			.src_box = source_box,
			.dst_box = {dx, dy, dw, dh},
			.alpha = &alpha,
			.clip = &render->clip,
			.transform = scene_buffer->transform,
			.filter_mode = WLR_SCALE_FILTER_BILINEAR,
			.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
			.transfer_function = scene_buffer->transfer_function,
			.primaries = &primaries,
			.color_encoding = scene_buffer->color_encoding,
			.color_range = scene_buffer->color_range,
			.wait_timeline = syncobj ? syncobj->acquire_timeline : NULL,
			.wait_point = syncobj ? syncobj->acquire_point : 0,
		});
}

static void wsov_render_scene_surfaces(struct wlr_scene_tree *tree,
									   int origin_x, int origin_y, void *data) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		if (!node->enabled)
			continue;
		int x = origin_x + node->x;
		int y = origin_y + node->y;
		if (node->type == WLR_SCENE_NODE_BUFFER) {
			struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
			wsov_render_scene_buffer(buffer, x, y, data);
		} else if (node->type == WLR_SCENE_NODE_TREE) {
			wsov_render_scene_surfaces(wlr_scene_tree_from_node(node), x, y,
									   data);
		}
	}
}

static void wsov_add_rect(struct wlr_render_pass *pass, struct wlr_box box,
						  const float color[4],
						  enum wlr_render_blend_mode blend_mode) {
	if (box.width <= 0 || box.height <= 0)
		return;
	wlr_render_pass_add_rect(
		pass, &(struct wlr_render_rect_options){
				  .box = box,
				  .color =
					  {
						  .r = wsov_premultiplied(color[0], color[3]),
						  .g = wsov_premultiplied(color[1], color[3]),
						  .b = wsov_premultiplied(color[2], color[3]),
						  .a = color[3],
					  },
				  .blend_mode = blend_mode,
			  });
}

static void wsov_render_client(struct wlr_render_pass *pass,
							   struct wsov_cell *cell, struct wsov_item *item,
							   struct wsov_render_resources *resources,
							   double buffer_scale) {
	Client *c = item->client;
	int cell_x = cell->box.x + config.workspace_overview_border_width;
	int cell_y = cell->box.y + config.workspace_overview_border_width;
	double render_scale_x = item->render_scale_x * buffer_scale;
	double render_scale_y = item->render_scale_y * buffer_scale;
	double render_scale = fmin(render_scale_x, render_scale_y);
	struct wlr_box window_box = {
		.x = (int)lround((item->box.x - cell_x) * buffer_scale),
		.y = (int)lround((item->box.y - cell_y) * buffer_scale),
		.width = (int)lround(item->box.width * buffer_scale),
		.height = (int)lround(item->box.height * buffer_scale),
	};

	const float *frame_color =
		(c == wsov.mon->sel) ? config.focuscolor : config.bordercolor;
	int border_x =
		c->bw > 0 ? MANGO_MAX(1, (int)lround((double)c->bw * render_scale_x))
				  : 0;
	int border_y =
		c->bw > 0 ? MANGO_MAX(1, (int)lround((double)c->bw * render_scale_y))
				  : 0;
	if (!c->no_shadow && config.shadows_blur > 0) {
		for (int layer = 4; layer >= 1; layer--) {
			int spread = MANGO_MAX(1, (int)lround(config.shadows_blur *
												  render_scale * layer / 4.0f));
			float shadow_color[4] = {
				config.shadowscolor[0],
				config.shadowscolor[1],
				config.shadowscolor[2],
				config.shadowscolor[3] * 0.055f,
			};
			wsov_add_rect(pass,
						  (struct wlr_box){window_box.x - spread,
										   window_box.y - spread,
										   window_box.width + 2 * spread,
										   window_box.height + 2 * spread},
						  shadow_color, WLR_RENDER_BLEND_MODE_PREMULTIPLIED);
		}
	}
	wsov_add_rect(pass,
				  (struct wlr_box){window_box.x, window_box.y, window_box.width,
								   border_y},
				  frame_color, WLR_RENDER_BLEND_MODE_NONE);
	wsov_add_rect(pass,
				  (struct wlr_box){window_box.x,
								   window_box.y + window_box.height - border_y,
								   window_box.width, border_y},
				  frame_color, WLR_RENDER_BLEND_MODE_NONE);
	wsov_add_rect(pass,
				  (struct wlr_box){window_box.x, window_box.y, border_x,
								   window_box.height},
				  frame_color, WLR_RENDER_BLEND_MODE_NONE);
	wsov_add_rect(pass,
				  (struct wlr_box){window_box.x + window_box.width - border_x,
								   window_box.y, border_x, window_box.height},
				  frame_color, WLR_RENDER_BLEND_MODE_NONE);

	if (window_box.width <= 2 || window_box.height <= 2)
		return;

	if (render_scale <= 0.0)
		return;

	pixman_region32_t clip;
	int clip_w = MANGO_MAX(1, (int)lround(cell->buffer_w * buffer_scale));
	int clip_h = MANGO_MAX(1, (int)lround(cell->buffer_h * buffer_scale));
	pixman_region32_init_rect(&clip, 0, 0, clip_w, clip_h);
	struct wsov_surface_render_data data = {
		.pass = pass,
		.cell = cell,
		.item = item,
		.resources = resources,
		.scale_x = render_scale_x,
		.scale_y = render_scale_y,
		.cell_scale = buffer_scale,
		.clip = clip,
	};
	wsov_render_scene_surfaces(c->scene, 0, 0, &data);
	pixman_region32_fini(&clip);
}

static bool wsov_scene_node_layout_coords(struct wlr_scene_node *node, int *x,
										  int *y) {
	if (!node)
		return false;
	int lx = 0, ly = 0;
	for (struct wlr_scene_node *cur = node; cur;
		 cur = cur->parent ? &cur->parent->node : NULL) {
		lx += cur->x;
		ly += cur->y;
	}
	*x = lx;
	*y = ly;
	return true;
}

static void wsov_render_bar_item_at(struct wlr_render_pass *pass,
									struct wsov_bar_item *item,
									struct wsov_render_resources *resources,
									int origin_x, int origin_y, double scale_x,
									double scale_y, pixman_region32_t *clip) {
	for (int i = 0; i < item->rect_count; i++) {
		struct wsov_bar_rect *rect = &item->rects[i];
		struct wlr_box box = {
			.x = (int)lround((rect->box.x - origin_x) * scale_x),
			.y = (int)lround((rect->box.y - origin_y) * scale_y),
			.width = MANGO_MAX(1, (int)lround(rect->box.width * scale_x)),
			.height = MANGO_MAX(1, (int)lround(rect->box.height * scale_y)),
		};
		wsov_add_rect(pass, box, rect->color,
					  WLR_RENDER_BLEND_MODE_PREMULTIPLIED);
	}

	for (int i = 0; i < item->buffer_count; i++) {
		struct wsov_bar_buffer *snapshot = &item->buffers[i];
		struct wlr_texture *texture =
			wlr_texture_from_buffer(server.renderer, snapshot->buffer);
		if (!texture)
			continue;
		if (!wsov_retain_texture(resources, texture, true)) {
			wlr_texture_destroy(texture);
			continue;
		}
		int buffer_w = snapshot->buffer->width;
		int buffer_h = snapshot->buffer->height;
		struct wlr_fbox src = snapshot->src_box;
		if (src.width <= 0 || src.height <= 0)
			src = (struct wlr_fbox){0, 0, buffer_w, buffer_h};
		int dst_w = snapshot->dst_width > 0 ? snapshot->dst_width : buffer_w;
		int dst_h = snapshot->dst_height > 0 ? snapshot->dst_height : buffer_h;
		float alpha = snapshot->opacity;
		struct wlr_box dst = {
			.x = (int)lround((snapshot->box.x - origin_x) * scale_x),
			.y = (int)lround((snapshot->box.y - origin_y) * scale_y),
			.width = MANGO_MAX(1, (int)lround(dst_w * scale_x)),
			.height = MANGO_MAX(1, (int)lround(dst_h * scale_y)),
		};
		wlr_render_pass_add_texture(
			pass, &(struct wlr_render_texture_options){
					  .texture = texture,
					  .src_box = src,
					  .dst_box = dst,
					  .alpha = &alpha,
					  .clip = clip,
					  .transform = snapshot->transform,
					  .filter_mode = WLR_SCALE_FILTER_BILINEAR,
					  .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
					  .transfer_function = WLR_COLOR_TRANSFER_FUNCTION_GAMMA22,
					  .color_encoding = WLR_COLOR_ENCODING_NONE,
					  .color_range = WLR_COLOR_RANGE_NONE,
				  });
	}
}

static void wsov_render_cell_layer_range(
	struct wlr_render_pass *pass, struct wsov_cell *cell,
	struct wsov_render_resources *resources, uint32_t first_layer,
	uint32_t end_layer, double buffer_scale) {
	double scale_x =
		(double)cell->buffer_w / MANGO_MAX(1, wsov.mon->m.width) * buffer_scale;
	double scale_y = (double)cell->buffer_h / MANGO_MAX(1, wsov.mon->m.height) *
					 buffer_scale;
	pixman_region32_t clip;
	int clip_w = MANGO_MAX(1, (int)lround(cell->buffer_w * buffer_scale));
	int clip_h = MANGO_MAX(1, (int)lround(cell->buffer_h * buffer_scale));
	pixman_region32_init_rect(&clip, 0, 0, clip_w, clip_h);

	for (uint32_t layer = first_layer; layer < end_layer; layer++) {
		for (int i = 0; i < cell->item_count; i++) {
			struct wsov_item *item = &cell->items[i];
			if (item->layer != layer ||
				wsov_drag_hides_item(cell, item->client))
				continue;
			wsov_render_client(pass, cell, item, resources, buffer_scale);
		}
		for (int i = 0; i < cell->bar_item_count; i++) {
			struct wsov_bar_item *item = &cell->bar_items[i];
			if (item->layer == layer) {
				wsov_render_bar_item_at(pass, item, resources, wsov.mon->m.x,
										wsov.mon->m.y, scale_x, scale_y, &clip);
			}
		}
	}
	pixman_region32_fini(&clip);
}

static bool wsov_cell_has_layer_range(const struct wsov_cell *cell,
									  uint32_t first_layer,
									  uint32_t end_layer) {
	for (int i = 0; i < cell->item_count; i++) {
		if (cell->items[i].layer >= first_layer &&
			cell->items[i].layer < end_layer)
			return true;
	}
	for (int i = 0; i < cell->bar_item_count; i++) {
		if (cell->bar_items[i].layer >= first_layer &&
			cell->bar_items[i].layer < end_layer)
			return true;
	}
	return false;
}

static void wsov_render_cell_part(struct wsov_cell *cell,
								  struct wlr_scene_buffer *target,
								  uint32_t first_layer, uint32_t end_layer,
								  double buffer_scale) {
	if (!target)
		return;
	if (!wsov_cell_has_layer_range(cell, first_layer, end_layer)) {
		wlr_scene_buffer_set_buffer(target, NULL);
		return;
	}

	int buffer_w = MANGO_MAX(1, (int)lround(cell->buffer_w * buffer_scale));
	int buffer_h = MANGO_MAX(1, (int)lround(cell->buffer_h * buffer_scale));
	struct wlr_buffer *buffer = wsov_create_buffer(buffer_w, buffer_h);
	if (!buffer)
		return;

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(server.renderer, buffer, NULL);
	if (!pass) {
		wlr_buffer_drop(buffer);
		return;
	}

	float background[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	struct wsov_render_resources resources = {0};
	resources.target = buffer;
	wsov_add_rect(pass, (struct wlr_box){0, 0, buffer_w, buffer_h}, background,
				  WLR_RENDER_BLEND_MODE_NONE);
	wsov_render_cell_layer_range(pass, cell, &resources, first_layer, end_layer,
								 buffer_scale);

	if (!wlr_render_pass_submit(pass)) {
		wsov_release_textures(&resources);
		wlr_buffer_drop(buffer);
		return;
	}
	wsov_release_textures(&resources);

	wlr_scene_buffer_set_buffer(target, buffer);
	wlr_scene_buffer_set_source_box(target, NULL);
	wlr_scene_buffer_set_filter_mode(target, WLR_SCALE_FILTER_BILINEAR);
	wlr_buffer_drop(buffer);
}

static void wsov_render_cell(struct wsov_cell *cell, double buffer_scale) {
	if (!cell || !cell->contents || !cell->foreground || cell->buffer_w <= 0 ||
		cell->buffer_h <= 0)
		return;

	wsov_render_cell_part(cell, cell->contents, 0, LyrTop, buffer_scale);
	wsov_render_cell_part(cell, cell->foreground, LyrTop, NUM_LAYERS,
						  buffer_scale);
	cell->contents_dirty = false;
	cell->foreground_dirty = false;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < cell->item_count; i++)
		client_send_frame_done(cell->items[i].client, &now);
}

static void wsov_flush_dirty_cells(void *data) {
	(void)data;
	wsov.render_idle = NULL;
	if (!workspace_overview_is_active() || wsov.closing)
		return;

	bool rendered = false;
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!cell->contents_dirty && !cell->foreground_dirty)
			continue;

		if (cell->contents_dirty)
			wsov_render_cell_part(cell, cell->contents, 0, LyrTop,
								  wsov_output_scale());
		if (cell->foreground_dirty)
			wsov_render_cell_part(cell, cell->foreground, LyrTop, NUM_LAYERS,
								  wsov_output_scale());
		cell->contents_dirty = false;
		cell->foreground_dirty = false;
		rendered = true;

		for (int j = 0; j < cell->item_count; j++)
			client_send_frame_done(cell->items[j].client, &now);
	}

	if (rendered)
		wsov_apply_camera(wsov.progress);
}

static void wsov_schedule_dirty_render(void) {
	if (wsov.render_idle)
		return;
	wsov.render_idle =
		wl_event_loop_add_idle(server.event_loop, wsov_flush_dirty_cells, NULL);
	if (!wsov.render_idle)
		wsov_flush_dirty_cells(NULL);
}

static void wsov_cancel_scheduled_render(void) {
	if (!wsov.render_idle)
		return;
	wl_event_source_remove(wsov.render_idle);
	wsov.render_idle = NULL;
}

static void wsov_flush_rebuild(void *data) {
	(void)data;
	wsov.rebuild_idle = NULL;
	wsov_rebuild();
}

static void wsov_schedule_rebuild(void) {
	if (wsov.rebuild_idle)
		return;
	wsov.rebuild_idle =
		wl_event_loop_add_idle(server.event_loop, wsov_flush_rebuild, NULL);
	if (!wsov.rebuild_idle)
		wsov_rebuild();
}

static void wsov_cancel_scheduled_rebuild(void) {
	if (!wsov.rebuild_idle)
		return;
	wl_event_source_remove(wsov.rebuild_idle);
	wsov.rebuild_idle = NULL;
}

static void wsov_render_all(void) {
	double buffer_scale = wsov_output_scale();
	for (int i = 0; i < wsov.cell_count; i++)
		wsov_render_cell(&wsov.cells[i], buffer_scale);
}

static void wsov_render_tags(uint32_t tags) {
	double buffer_scale = wsov_output_scale();
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (tags & wsov_tag_bit(cell->tag))
			wsov_render_cell(cell, buffer_scale);
	}
}

static void wsov_render_cell_at_resolution(struct wsov_cell *cell, int width,
										   int height) {
	if (!cell || width <= 0 || height <= 0 || cell->item_count <= 0 ||
		(cell->buffer_w >= width && cell->buffer_h >= height))
		return;

	/* The close animation expands the selected preview to the monitor size.
	 * Replacing its small preview texture once with an output-sized texture
	 * keeps content sharp while it expands, without changing layout state. */
	struct wsov_item *items = malloc(cell->item_count * sizeof(*items));
	if (!items)
		return;
	memcpy(items, cell->items, cell->item_count * sizeof(*items));

	struct wsov_item *saved_items = cell->items;
	struct wlr_box saved_box = cell->box;
	int saved_buffer_w = cell->buffer_w;
	int saved_buffer_h = cell->buffer_h;

	cell->items = items;
	cell->box = (struct wlr_box){
		.width = width + 2 * config.workspace_overview_border_width,
		.height = height + 2 * config.workspace_overview_border_width,
	};
	cell->buffer_w = width;
	cell->buffer_h = height;
	wsov_position_cell_items(cell);
	wsov_render_cell(cell, 1.0);

	cell->items = saved_items;
	cell->box = saved_box;
	cell->buffer_w = saved_buffer_w;
	cell->buffer_h = saved_buffer_h;
	free(items);
}

static void wsov_update_styles_tags(uint32_t tags) {
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!(tags & wsov_tag_bit(cell->tag)))
			continue;
		bool focused = cell->tag == wsov.target_tag;
		float dim_opacity =
			focused ? config.workspace_overview_focused_dim_opacity
					: config.workspace_overview_unfocused_dim_opacity;
		float dim_color[4] = {0.0f, 0.0f, 0.0f, dim_opacity};
		for (int j = 0; j < 4; j++) {
			wsov_set_rect_color(cell->border[j], focused ? config.focuscolor
														 : config.bordercolor);
		}
		mango_dim_node_set_color(cell->dim, dim_color);
		mango_dim_node_set_box(
			cell->dim, cell->box.x + config.workspace_overview_border_width,
			cell->box.y + config.workspace_overview_border_width,
			cell->buffer_w, cell->buffer_h);
		cell->dim_enabled = dim_opacity > 0.0f;
		mango_dim_node_set_enabled(cell->dim, cell->dim_enabled);
		wlr_scene_node_raise_to_top(&cell->dim->scene_buffer->node);
	}
	if (wsov.drop_fill) {
		float border[4] = {config.dropcolor[0], config.dropcolor[1],
						   config.dropcolor[2], 1.0f};
		wsov_set_rect_color(wsov.drop_fill, config.dropcolor);
		for (int i = 0; i < 4; i++)
			wsov_set_rect_color(wsov.drop_border[i], border);
		if (wsov.drop_indicator_visible) {
			wlr_scene_node_raise_to_top(&wsov.drop_fill->node);
			for (int i = 0; i < 4; i++)
				wlr_scene_node_raise_to_top(&wsov.drop_border[i]->node);
		}
	}
	if (wsov.drag_buffer)
		wlr_scene_node_raise_to_top(&wsov.drag_buffer->node);
}

static void wsov_update_styles(void) {
	uint32_t tags = 0;
	for (int i = 0; i < wsov.cell_count; i++)
		tags |= wsov_tag_bit(wsov.cells[i].tag);
	wsov_update_styles_tags(tags);
}

static bool wsov_cell_has_client(const struct wsov_cell *cell,
								 const Client *client) {
	if (!client)
		return false;
	for (int i = 0; i < cell->item_count; i++) {
		if (cell->items[i].client == client)
			return true;
	}
	return false;
}

static void wsov_render_selection_clients(Client *previous, Client *current) {
	if (previous == current)
		return;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (wsov_cell_has_client(cell, previous) ||
			wsov_cell_has_client(cell, current))
			wsov_render_cell(cell, wsov_output_scale());
	}
}

static void wsov_refresh_target_selection(void) {
	if (!wsov.mon || wsov.closing)
		return;
	Client *previous = wsov.mon->sel;
	wsov_activate_target_tag();
	wsov_sync_monitor_selection();
	wsov_render_selection_clients(previous, wsov.mon->sel);
	wsov_update_styles();
	wsov_apply_camera(wsov.progress);
}

static void wsov_set_dim_enabled(bool enabled) {
	for (int i = 0; i < wsov.cell_count; i++) {
		wsov.cells[i].dim_enabled = enabled;
		mango_dim_node_set_enabled(wsov.cells[i].dim, enabled);
	}
}

static void wsov_set_borders_enabled(bool enabled) {
	for (int i = 0; i < wsov.cell_count; i++) {
		wsov.cells[i].borders_enabled = enabled;
		for (int j = 0; j < 4; j++)
			wlr_scene_node_set_enabled(&wsov.cells[i].border[j]->node, enabled);
	}
}

static int wsov_grid_cols(int count) {
	return MANGO_MAX(1, (int)ceil(sqrt((double)MANGO_MAX(1, count))));
}

static void wsov_layout_cells(void) {
	if (!wsov.mon || wsov.cell_count <= 0)
		return;

	int n = wsov.cell_count;
	int cols = wsov_grid_cols(n);
	int rows = (n + cols - 1) / cols;
	int outer = 0;
	int gap = config.workspace_overview_gap;
	int avail_w = MANGO_MAX(1, wsov.mon->m.width - 2 * outer);
	int avail_h = MANGO_MAX(1, wsov.mon->m.height - 2 * outer);
	double aspect = (double)wsov.mon->m.width / wsov.mon->m.height;
	int cell_w = MANGO_MAX(1, (avail_w - (cols - 1) * gap) / cols);
	int cell_h = (int)lround(cell_w / aspect);
	int max_cell_h = MANGO_MAX(1, (avail_h - (rows - 1) * gap) / rows);
	if (cell_h > max_cell_h) {
		cell_h = max_cell_h;
		cell_w = (int)lround(cell_h * aspect);
	}
	if (n == 1) {
		int max_single_w =
			MANGO_MIN(avail_w, MANGO_MAX(1, (int)lround(avail_w * 0.9)));
		if (cell_w > max_single_w) {
			cell_w = max_single_w;
			cell_h = MANGO_MAX(1, (int)lround(cell_w / aspect));
		}
	}
	cell_w = MANGO_MAX(2 * config.workspace_overview_border_width + 1, cell_w);
	cell_h = MANGO_MAX(2 * config.workspace_overview_border_width + 1, cell_h);
	int grid_h = rows * cell_h + (rows - 1) * gap;
	int grid_y = MANGO_MAX(0, (avail_h - grid_h) / 2);

	for (int i = 0; i < n; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		int col = i % cols;
		int row = i / cols;
		int row_start = row * cols;
		int row_end = MANGO_MIN(row_start + cols, n);
		int row_len = row_end - row_start;
		int row_w = row_len * cell_w + (row_len - 1) * gap;
		int row_x = MANGO_MAX(0, (avail_w - row_w) / 2);
		int sx = row_x + col * (cell_w + gap);
		int sy = grid_y + row * (cell_h + gap);

		cell->box = (struct wlr_box){
			.x = sx,
			.y = sy,
			.width = cell_w,
			.height = cell_h,
		};
		cell->buffer_w =
			cell->box.width - 2 * config.workspace_overview_border_width;
		cell->buffer_h =
			cell->box.height - 2 * config.workspace_overview_border_width;
		cell->layout_current = cell->box;

		wlr_scene_node_set_position(&cell->frame->node, cell->box.x,
									cell->box.y);
		wlr_scene_rect_set_size(cell->frame, cell->box.width, cell->box.height);
		wlr_scene_node_set_position(
			&cell->contents->node,
			cell->box.x + config.workspace_overview_border_width,
			cell->box.y + config.workspace_overview_border_width);
		wlr_scene_buffer_set_dest_size(cell->contents, cell->buffer_w,
									   cell->buffer_h);
		wlr_scene_node_set_position(
			&cell->foreground->node,
			cell->box.x + config.workspace_overview_border_width,
			cell->box.y + config.workspace_overview_border_width);
		wlr_scene_buffer_set_dest_size(cell->foreground, cell->buffer_w,
									   cell->buffer_h);
	}
}

static void wsov_update_layout_animation(void) {
	if (!wsov.layout_animating)
		return;

	double t = wsov_elapsed_ms(&wsov.layout_animation_start) /
			   MANGO_MAX(1.0, (double)config.animation_duration_wsov_layout);
	t = fmax(0.0, fmin(1.0, t));
	/* Preserve the original layout motion: soften the large geometry change
	 * before applying the configured cubic-bezier curve. */
	double shaped_t = t * t * (3.0 - 2.0 * t);
	double eased =
		find_animation_curve_for(shaped_t, config.animation_curve_wsov_layout);
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (t >= 1.0) {
			cell->layout_current = cell->box;
			continue;
		}
		cell->layout_current = (struct wlr_box){
			.x = (int)lround(cell->layout_from.x +
							 (cell->box.x - cell->layout_from.x) * eased),
			.y = (int)lround(cell->layout_from.y +
							 (cell->box.y - cell->layout_from.y) * eased),
			.width = MANGO_MAX(
				1, (int)lround(cell->layout_from.width +
							   (cell->box.width - cell->layout_from.width) *
								   eased)),
			.height = MANGO_MAX(
				1, (int)lround(cell->layout_from.height +
							   (cell->box.height - cell->layout_from.height) *
								   eased)),
		};
	}
	if (t >= 1.0)
		wsov.layout_animating = false;
}

static void wsov_apply_camera(double progress) {
	if (!wsov.mon || wsov.cell_count <= 0)
		return;

	struct wsov_cell *target = wsov_cell_for_tag(wsov.camera_tag);
	if (!target)
		target = &wsov.cells[0];

	progress = fmax(0.0, fmin(1.0, progress));
	struct wlr_box target_box = wsov_cell_layout_box(target);

	/* Interpolate one shared viewport, as Wayfire does. Every workspace is
	 * transformed by the same scale and offset, so active and inactive
	 * previews move and grow in lockstep. */
	double target_x = target_box.x + config.workspace_overview_border_width;
	double target_y = target_box.y + config.workspace_overview_border_width;
	double target_w = MANGO_MAX(
		1, target_box.width - 2 * config.workspace_overview_border_width);
	double target_h = MANGO_MAX(
		1, target_box.height - 2 * config.workspace_overview_border_width);
	double viewport_x = target_x * (1.0 - progress);
	double viewport_y = target_y * (1.0 - progress);
	double viewport_w = target_w + (wsov.mon->m.width - target_w) * progress;
	double viewport_h = target_h + (wsov.mon->m.height - target_h) * progress;
	double scale_x = (double)wsov.mon->m.width / MANGO_MAX(1.0, viewport_w);
	double scale_y = (double)wsov.mon->m.height / MANGO_MAX(1.0, viewport_h);

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		struct wlr_box cell_box = wsov_cell_layout_box(cell);
		double width = cell_box.width * scale_x;
		double height = cell_box.height * scale_y;
		double x = (cell_box.x - viewport_x) * scale_x;
		double y = (cell_box.y - viewport_y) * scale_y;

		wsov_place_rect_clipped(cell->frame, (int)lround(x), (int)lround(y),
								MANGO_MAX(1, (int)lround(width)),
								MANGO_MAX(1, (int)lround(height)), true);
		int content_x = (int)lround(
			(cell_box.x + config.workspace_overview_border_width - viewport_x) *
			scale_x);
		int content_y = (int)lround(
			(cell_box.y + config.workspace_overview_border_width - viewport_y) *
			scale_y);
		int content_w = MANGO_MAX(
			1,
			(int)lround(
				MANGO_MAX(1, cell_box.width -
								 2 * config.workspace_overview_border_width) *
				scale_x));
		int content_h = MANGO_MAX(
			1,
			(int)lround(
				MANGO_MAX(1, cell_box.height -
								 2 * config.workspace_overview_border_width) *
				scale_y));
		wsov_place_buffer_clipped(cell->contents, NULL, content_x, content_y,
								  content_w, content_h);
		wsov_place_buffer_clipped(cell->foreground, NULL, content_x, content_y,
								  content_w, content_h);
		struct wlr_box dim_visible;
		if (!cell->dim_enabled ||
			!wsov_intersect_monitor(content_x, content_y, content_w, content_h,
									&dim_visible)) {
			mango_dim_node_set_enabled(cell->dim, false);
		} else {
			mango_dim_node_set_enabled(cell->dim, true);
			mango_dim_node_set_box(cell->dim, dim_visible.x, dim_visible.y,
								   dim_visible.width, dim_visible.height);
		}
		int frame_w = MANGO_MAX(1, (int)lround(width));
		int frame_h = MANGO_MAX(1, (int)lround(height));
		int border_w =
			MANGO_MAX(1, (int)lround(config.workspace_overview_border_width *
									 fmin(scale_x, scale_y)));
		int frame_x = (int)lround(x);
		int frame_y = (int)lround(y);
		wsov_place_rect_clipped(cell->border[0], frame_x, frame_y, frame_w,
								border_w, cell->borders_enabled);
		wsov_place_rect_clipped(cell->border[1], frame_x,
								frame_y + frame_h - border_w, frame_w, border_w,
								cell->borders_enabled);
		wsov_place_rect_clipped(cell->border[2], frame_x, frame_y, border_w,
								frame_h, cell->borders_enabled);
		wsov_place_rect_clipped(cell->border[3], frame_x + frame_w - border_w,
								frame_y, border_w, frame_h,
								cell->borders_enabled);
		for (int j = 0; j < cell->layer_count; j++) {
			struct wsov_layer_preview *preview = &cell->layers[j];
			double local_scale_x =
				(double)content_w / MANGO_MAX(1, wsov.mon->m.width);
			double local_scale_y =
				(double)content_h / MANGO_MAX(1, wsov.mon->m.height);
			int px =
				(int)lround(content_x + preview->workspace_x * local_scale_x);
			int py =
				(int)lround(content_y + preview->workspace_y * local_scale_y);
			wsov_place_buffer_clipped(
				preview->node, &preview->src_box, px, py,
				MANGO_MAX(1, (int)lround(preview->width * local_scale_x)),
				MANGO_MAX(1, (int)lround(preview->height * local_scale_y)));
		}
	}
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

static void wsov_destroy_cells(void) {
	wsov_cancel_scheduled_render();
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (cell->contents)
			wlr_scene_node_destroy(&cell->contents->node);
		if (cell->foreground)
			wlr_scene_node_destroy(&cell->foreground->node);
		mango_dim_node_destroy(cell->dim);
		if (cell->frame)
			wlr_scene_node_destroy(&cell->frame->node);
		for (int j = 0; j < 4; j++) {
			if (cell->border[j])
				wlr_scene_node_destroy(&cell->border[j]->node);
		}
		wsov_destroy_layer_previews(cell);
		wsov_clear_cell_bars(cell);
		free(cell->bar_items);
		free(cell->items);
	}
	free(wsov.cells);
	wsov.cells = NULL;
	wsov.cell_count = 0;
}

static bool wsov_create_cells(void) {
	typedef struct {
		uint32_t tag;
		struct wlr_box box;
	} wsov_layout_snapshot;

	int old_cell_count = wsov.cell_count;
	wsov_layout_snapshot *snapshot = NULL;
	if (old_cell_count > 0) {
		snapshot = malloc(old_cell_count * sizeof(*snapshot));
		if (snapshot) {
			for (int i = 0; i < old_cell_count; i++) {
				snapshot[i] = (wsov_layout_snapshot){
					.tag = wsov.cells[i].tag,
					.box = wsov_cell_layout_box(&wsov.cells[i]),
				};
			}
		}
	}

	wsov_destroy_cells();
	uint32_t occupied = wsov_collect_workspace_tags(wsov.mon, &wsov.cell_count);
	if (wsov.cell_count <= 0) {
		free(snapshot);
		return false;
	}

	wsov.cells = calloc(wsov.cell_count, sizeof(*wsov.cells));
	if (!wsov.cells) {
		wsov.cell_count = 0;
		free(snapshot);
		return false;
	}

	int index = 0;
	for (int tag = 1; tag <= config.tag_num; tag++) {
		if (!(occupied & wsov_tag_bit((uint32_t)tag)))
			continue;

		struct wsov_cell *cell = &wsov.cells[index++];
		cell->tag = (uint32_t)tag;
		cell->borders_enabled = true;
		cell->frame = wlr_scene_rect_create(wsov.root, 1, 1, config.rootcolor);
		for (int i = 0; i < 4; i++) {
			cell->border[i] =
				wlr_scene_rect_create(wsov.root, 1, 1, config.bordercolor);
		}
		cell->contents = wlr_scene_buffer_create(wsov.root, NULL);
		cell->foreground = wlr_scene_buffer_create(wsov.root, NULL);
		cell->dim = mango_dim_node_create(wsov.root, (float[4]){0, 0, 0, 0});
		if (!cell->frame || !cell->contents || !cell->border[0] ||
			!cell->border[1] || !cell->border[2] || !cell->border[3] ||
			!cell->foreground || !cell->dim) {
			free(snapshot);
			return false;
		}
	}

	wsov_layout_cells();
	wsov.layout_animating = false;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		cell->layout_from = cell->box;
		if (!snapshot)
			continue;
		for (int j = 0; j < old_cell_count; j++) {
			if (snapshot[j].tag != cell->tag)
				continue;
			cell->layout_from = snapshot[j].box;
			if (config.animations &&
				!wlr_box_equal(&cell->layout_from, &cell->box))
				wsov.layout_animating = true;
			break;
		}
	}
	free(snapshot);
	if (config.animations && wsov.layout_animating) {
		for (int i = 0; i < wsov.cell_count; i++)
			wsov.cells[i].layout_current = wsov.cells[i].layout_from;
		clock_gettime(CLOCK_MONOTONIC, &wsov.layout_animation_start);
		wsov_attach_frame_listener();
	}
	wsov_create_layer_previews();
	wsov_select_default_target();
	wsov.camera_tag = wsov.target_tag;
	wsov_sync_monitor_selection();
	wsov_capture_workspace_layouts();
	wsov_render_all();
	wsov_update_styles();
	wsov_apply_camera(wsov.progress);
	return true;
}

static void wsov_destroy_drag_buffer(void) {
	if (wsov.drag_buffer) {
		wlr_scene_node_destroy(&wsov.drag_buffer->node);
		wsov.drag_buffer = NULL;
	}
	wsov.drag_width = 0;
	wsov.drag_height = 0;
	wsov.drag_bar_height = 0;
}

static void wsov_begin_drag_floating(Client *c) {
	if (!c || wsov.drag_floating_changed)
		return;

	wsov.drag_was_floating = c->isfloating;
	wsov.drag_was_drag_to_tile = c->drag_to_tile;
	wsov.drag_saved_float_geom = c->float_geom;
	if (c->isfloating)
		return;
	/* The drag itself must not clear fullscreen/maximized. Leave that decision
	 * to the normal arrange path after the window is dropped on its target. */
	if (c->isfullscreen || c->ismaximizescreen)
		return;

	/* Match the normal move-drag path: a tiled window leaves the tiling model
	 * as soon as the drag starts, then returns to it on drop or cancel. */
	c->drag_to_tile = true;
	exit_scroller_stack(c);
	client_set_floating(c, 1, false);
	c->old_stack_inner_per = 0.0f;
	c->old_master_inner_per = 0.0f;
	set_size_per(c->mon, c, false);
	wsov.drag_floating_changed = true;
}

static void wsov_restore_drag_floating(void) {
	Client *c = wsov.drag_client;
	if (wsov.drag_floating_changed && c) {
		c->float_geom = wsov.drag_saved_float_geom;
		c->drag_to_tile = wsov.drag_was_drag_to_tile;
		if (c->mon && c->isfloating)
			client_set_floating(c, 0, false);
		else
			c->isfloating = 0;
	}
	wsov.drag_floating_changed = false;
	wsov.drag_was_floating = false;
	wsov.drag_was_drag_to_tile = false;
	wsov.drag_saved_float_geom = (struct wlr_box){0};
}

static bool wsov_capture_drag_group_bars(Client *c, struct wsov_cell *bars) {
	if (!c || !bars || !client_is_group_member(c) || !c->group_bar)
		return false;

	Client *head = client_group_head(c);
	Client *anchor = client_group_focused(head);
	if (!anchor)
		anchor = c;
	wsov_draw_group_bar_for_capture(anchor);

	bool captured = false;
	for (Client *cur = head; cur; cur = cur->group_next) {
		if (!cur->group_bar || !cur->group_bar->scene->node.enabled)
			continue;
		wsov_capture_bar(bars, cur->group_bar);
		captured = true;
	}
	return captured;
}

static bool wsov_create_drag_buffer(struct wsov_item *item) {
	wsov_destroy_drag_buffer();
	if (!item || item->box.width <= 0 || item->box.height <= 0)
		return false;

	int width = MANGO_MIN(item->box.width, 900);
	int workspace_width =
		MANGO_MAX(1, item->workspace_geom.width > 0 ? item->workspace_geom.width
													: item->box.width);
	int workspace_height = MANGO_MAX(1, item->workspace_geom.height > 0
											? item->workspace_geom.height
											: item->box.height);
	double output_scale = wsov_output_scale();
	double render_scale = (double)width / workspace_width;
	int window_height =
		MANGO_MAX(1, (int)lround(workspace_height * render_scale));
	struct wsov_cell drag_bars = {0};
	int bar_height = 0;
	if (wsov_capture_drag_group_bars(item->client, &drag_bars))
		bar_height =
			MANGO_MAX(1, (int)lround(config.group_bar_height * render_scale));
	int height = bar_height + window_height;
	int buffer_width = MANGO_MAX(1, (int)lround(width * output_scale));
	int buffer_height = MANGO_MAX(1, (int)lround(height * output_scale));

	struct wlr_buffer *buffer = wsov_create_buffer(buffer_width, buffer_height);
	if (!buffer) {
		wsov_clear_cell_bars(&drag_bars);
		free(drag_bars.bar_items);
		return false;
	}
	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(server.renderer, buffer, NULL);
	if (!pass) {
		wsov_clear_cell_bars(&drag_bars);
		free(drag_bars.bar_items);
		wlr_buffer_drop(buffer);
		return false;
	}

	float background[4] = {config.rootcolor[0], config.rootcolor[1],
						   config.rootcolor[2], 1.0f};
	wsov_add_rect(pass, (struct wlr_box){0, 0, buffer_width, buffer_height},
				  background, WLR_RENDER_BLEND_MODE_NONE);

	struct wsov_cell drag_cell = {
		.box = {.x = -config.workspace_overview_border_width,
				.y = -config.workspace_overview_border_width,
				.width = width + 2 * config.workspace_overview_border_width,
				.height =
					window_height + 2 * config.workspace_overview_border_width},
		.buffer_w = buffer_width,
		.buffer_h = buffer_height,
	};
	struct wsov_item drag_item = {
		.client = item->client,
		.box = {.x = 0,
				.y = bar_height,
				.width = width,
				.height = window_height},
		.render_scale = render_scale,
		.render_scale_x = render_scale,
		.render_scale_y = render_scale,
	};
	struct wsov_render_resources resources = {0};
	resources.target = buffer;
	wsov_render_client(pass, &drag_cell, &drag_item, &resources, output_scale);
	if (bar_height > 0) {
		pixman_region32_t clip;
		int scaled_bar_height = (int)lround(bar_height * output_scale);
		pixman_region32_init_rect(&clip, 0, 0, buffer_width, scaled_bar_height);
		double scale_x =
			(double)buffer_width / MANGO_MAX(1, item->client->geom.width);
		double scale_y =
			(double)scaled_bar_height / MANGO_MAX(1, config.group_bar_height);
		int origin_y = item->client->geom.y - config.group_bar_height;
		for (int i = 0; i < drag_bars.bar_item_count; i++) {
			wsov_render_bar_item_at(pass, &drag_bars.bar_items[i], &resources,
									item->client->geom.x, origin_y, scale_x,
									scale_y, &clip);
		}
		pixman_region32_fini(&clip);
	}

	if (!wlr_render_pass_submit(pass)) {
		wsov_release_textures(&resources);
		wsov_clear_cell_bars(&drag_bars);
		free(drag_bars.bar_items);
		wlr_buffer_drop(buffer);
		return false;
	}
	wsov_release_textures(&resources);
	wsov_clear_cell_bars(&drag_bars);
	free(drag_bars.bar_items);

	wsov.drag_buffer = wlr_scene_buffer_create(wsov.root, buffer);
	wlr_buffer_drop(buffer);
	if (!wsov.drag_buffer)
		return false;
	wlr_scene_buffer_set_dest_size(wsov.drag_buffer, width, height);
	wlr_scene_buffer_set_filter_mode(wsov.drag_buffer,
									 WLR_SCALE_FILTER_BILINEAR);
	wlr_scene_node_raise_to_top(&wsov.drag_buffer->node);
	wsov.drag_bar_height = bar_height;
	wsov.drag_width = width;
	wsov.drag_height = height;
	return true;
}

static void wsov_cancel_drag(void) {
	bool had_drag =
		wsov.pressed || wsov.moved || wsov.has_drag_client || wsov.drag_buffer;
	uint32_t return_tag = wsov.drag_return_tag;
	wsov_hide_drop_indicator();
	wsov_restore_drag_floating();
	wsov_destroy_drag_buffer();
	wsov.pressed = false;
	wsov.moved = false;
	wsov.has_drag_client = false;
	wsov.drag_client = NULL;
	wsov.source_tag = 0;
	wsov.drag_return_tag = 0;
	if (had_drag && return_tag) {
		wsov.target_tag = return_tag;
		wsov.camera_tag = return_tag;
	}
	if (wsov.mon && !server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");
}

static uint32_t wsov_active_camera_tag(void) {
	if (!wsov.mon)
		return 0;
	uint32_t current = get_mon_curtag(wsov.mon);
	if (wsov_cell_for_tag(current))
		return current;

	uint32_t active = wsov.mon->tagset[wsov.mon->seltags] & TAGMASK;
	for (int i = 0; i < wsov.cell_count; i++) {
		uint32_t tag = wsov.cells[i].tag;
		if (active & wsov_tag_bit(tag))
			return tag;
	}
	return 0;
}

static struct wsov_cell *wsov_target_cell(void) {
	return wsov_cell_for_tag(wsov.target_tag);
}

static void wsov_select_direction(int dir_x, int dir_y) {
	struct wsov_cell *current = wsov_target_cell();
	if (!current || wsov.cell_count <= 1)
		return;

	struct wlr_box current_box = wsov_cell_layout_box(current);
	int32_t current_l = current_box.x;
	int32_t current_r = current_box.x + current_box.width;
	int32_t current_t = current_box.y;
	int32_t current_b = current_box.y + current_box.height;
	int32_t current_cx = current_l + current_box.width / 2;
	int32_t current_cy = current_t + current_box.height / 2;

	struct wsov_cell *target = NULL;
	int64_t best_distance = LLONG_MAX;
	int64_t best_center_distance = LLONG_MAX;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (cell == current)
			continue;

		struct wlr_box box = wsov_cell_layout_box(cell);
		int32_t left = box.x;
		int32_t right = box.x + box.width;
		int32_t top = box.y;
		int32_t bottom = box.y + box.height;
		int32_t center_x = left + box.width / 2;
		int32_t center_y = top + box.height / 2;
		int64_t main_distance = 0;
		int64_t orth_distance = 0;
		bool match_direction = false;

		if (dir_x < 0) {
			if (center_x < current_cx ||
				(center_x == current_cx && left < current_l)) {
				match_direction = true;
				main_distance = current_l - right;
				orth_distance = bottom < current_t
									? current_t - bottom
									: (top > current_b ? top - current_b : 0);
			}
		} else if (dir_x > 0) {
			if (center_x > current_cx ||
				(center_x == current_cx && left > current_l)) {
				match_direction = true;
				main_distance = left - current_r;
				orth_distance = bottom < current_t
									? current_t - bottom
									: (top > current_b ? top - current_b : 0);
			}
		} else if (dir_y < 0) {
			if (center_y < current_cy ||
				(center_y == current_cy && top < current_t)) {
				match_direction = true;
				main_distance = current_t - bottom;
				orth_distance = right < current_l
									? current_l - right
									: (left > current_r ? left - current_r : 0);
			}
		} else if (dir_y > 0) {
			if (center_y > current_cy ||
				(center_y == current_cy && top > current_t)) {
				match_direction = true;
				main_distance = top - current_b;
				orth_distance = right < current_l
									? current_l - right
									: (left > current_r ? left - current_r : 0);
			}
		}
		if (!match_direction)
			continue;

		bool orth_overlap = dir_x ? (bottom >= current_t && top <= current_b)
								  : (right >= current_l && left <= current_r);
		if (config.focusdir_only_zone_overlap && !orth_overlap)
			continue;

		int64_t penalty = 0;
		if (main_distance < 0) {
			penalty = 10000000000LL;
			main_distance = -main_distance;
		}
		int64_t distance = penalty + main_distance * main_distance +
						   orth_distance * orth_distance;
		int64_t center_distance = dir_y ? llabs((int64_t)center_x - current_cx)
										: llabs((int64_t)center_y - current_cy);
		if (distance < best_distance ||
			(distance == best_distance &&
			 center_distance < best_center_distance)) {
			best_distance = distance;
			best_center_distance = center_distance;
			target = cell;
		}
	}

	if (!target)
		return;

	wsov.target_tag = target->tag;
	wsov.camera_tag = wsov.target_tag;
	wsov_refresh_target_selection();
}

bool workspace_overview_handle_key(uint32_t sym) {
	if (!workspace_overview_is_active())
		return false;

	switch (sym) {
	case XKB_KEY_Escape:
		if (!wsov.closing)
			wsov_close(wsov.target_tag, NULL);
		return true;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		if (!wsov.closing)
			wsov_close(wsov.target_tag, NULL);
		return true;
	case XKB_KEY_Left:
	case XKB_KEY_h:
		if (!wsov.closing)
			wsov_select_direction(-1, 0);
		return true;
	case XKB_KEY_Right:
	case XKB_KEY_l:
		if (!wsov.closing)
			wsov_select_direction(1, 0);
		return true;
	case XKB_KEY_Up:
	case XKB_KEY_k:
		if (!wsov.closing)
			wsov_select_direction(0, -1);
		return true;
	case XKB_KEY_Down:
	case XKB_KEY_j:
		if (!wsov.closing)
			wsov_select_direction(0, 1);
		return true;
	default:
		return false;
	}
}

static void wsov_hide_clients(bool hidden) {
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != wsov.mon || client_is_unmanaged(c))
			continue;
		c->wsov_hidden = hidden;
		client_update_visibility(c);
	}
}

static void wsov_start_animation(bool closing, double target) {
	if (!config.animations) {
		wsov.closing = closing;
		wsov.layout_animating = false;
		wsov.animating = false;
		wsov.animation_to = target;
		wsov.progress = target;
		wsov_apply_camera(target);
		if (closing)
			wsov_finalize_close(wsov.close_tag);
		else
			wsov_detach_frame_listener();
		return;
	}

	wsov.animation_from = fmax(0.0, fmin(1.0, wsov.progress));
	wsov.animation_to = fmax(0.0, fmin(1.0, target));
	wsov.closing = closing;
	wsov.animating = true;
	clock_gettime(CLOCK_MONOTONIC, &wsov.animation_start);
	wsov_attach_frame_listener();
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

static void wsov_resume_open_animation(void) {
	if (!workspace_overview_is_active() || !wsov.closing)
		return;

	wsov.target_tag = wsov.camera_tag;
	wsov.close_tag = 0;
	wsov.close_client = NULL;
	wsov.close_focus_explicit = false;
	wsov.layers_dirty = true;
	wsov_set_borders_enabled(true);
	wsov_update_styles();
	wsov_start_animation(false, 1.0);
}

static void wsov_close(uint32_t focus_tag, Client *focus_client) {
	if (!workspace_overview_is_active() || wsov.closing)
		return;
	bool pointer_close = wsov.pressed;
	uint32_t previous_target = wsov.target_tag;
	wsov_cancel_scheduled_render();
	wsov_cancel_drag();
	wsov.close_client = focus_client;
	wsov.close_focus_explicit = pointer_close;
	if (!config.animations) {
		wsov_finalize_close(focus_tag);
		return;
	}

	wsov_set_dim_enabled(false);
	wsov_set_borders_enabled(false);
	wsov.closing = true;
	wsov.layers_dirty = false;
	wsov.close_tag = focus_tag;
	uint32_t camera_tag = focus_tag ? focus_tag : wsov_active_camera_tag();
	if (!camera_tag || !wsov_cell_for_tag(camera_tag))
		camera_tag = previous_target;
	wsov.camera_tag = camera_tag;
	struct wsov_cell *target = wsov_cell_for_tag(wsov.camera_tag);
	if (target) {
		double scale =
			wsov.mon->wlr_output ? fmax(1.0, wsov.mon->wlr_output->scale) : 1.0;
		wsov_render_cell_at_resolution(target,
									   (int)lround(wsov.mon->m.width * scale),
									   (int)lround(wsov.mon->m.height * scale));
	}
	wsov_apply_camera(wsov.progress);
	wsov_start_animation(true, 0.0);
}

static void wsov_finalize_close(uint32_t focus_tag) {
	if (!workspace_overview_is_active())
		return;

	Monitor *m = wsov.mon;
	Client *focus_client = wsov.close_client;
	bool focus_explicit = wsov.close_focus_explicit;
	wsov_cancel_scheduled_rebuild();
	wsov_cancel_drag();
	wsov.closing = true;
	m->is_ws_overview = false;

	wsov_detach_frame_listener();
	wlr_seat_pointer_clear_focus(server.seat);
	wlr_seat_keyboard_notify_clear_focus(server.seat);
	wsov_destroy_cells();
	if (wsov.root)
		wlr_scene_node_destroy(&wsov.root->node);
	wsov.root = NULL;
	wsov.background = NULL;

	if (focus_tag) {
		client_view_on_monitor(&(Arg){.ui = wsov_tag_bit(focus_tag)}, false, m,
							   false);
	} else {
		arrange(m, false, false);
	}

	/* Arrange the destination view while windows are still hidden. Only then
	 * reveal the scene, avoiding a one-frame flash of every workspace. */
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m && !client_is_unmanaged(c))
			client_set_finish_geometry(c);
	}
	wsov_hide_clients(false);
	memset(&wsov, 0, sizeof(wsov));
	pointer_cursor_activity();

	if (!focus_explicit && !focus_client)
		focus_client = client_focus_top(m);
	client_focus(focus_client, 1);
	/* Opening the overview cleared the keyboard focus, so client_focus() can no
	 * longer see that an exclusive layer owned it. Restore the layer after the
	 * client focus state has been updated to keep exclusive focus
	 * authoritative.
	 */
	reset_exclusive_layers_focus(m);
	pointer_process_motion(0, NULL, 0, 0, 0, 0);
	wlr_output_schedule_frame(m->wlr_output);
}

bool workspace_overview_is_active(void) {
	return wsov.mon && wsov.mon->is_ws_overview && wsov.root;
}

bool workspace_overview_is_active_on(const Monitor *m) {
	return workspace_overview_is_active() && wsov.mon == m;
}

bool workspace_overview_is_closing_on(const Monitor *m) {
	return m && wsov.mon == m && wsov.closing;
}

static void wsov_update_root_geometry(Monitor *m) {
	if (!wsov.root || !m)
		return;
	wlr_scene_node_set_position(&wsov.root->node, m->m.x, m->m.y);
	if (wsov.background) {
		wlr_scene_rect_set_size(wsov.background, m->m.width, m->m.height);
		wlr_scene_rect_set_color(wsov.background, config.rootcolor);
	}
}

static bool wsov_open(Monitor *m) {
	if (!m || workspace_overview_is_active() || m->isoverview)
		return false;

	memset(&wsov, 0, sizeof(wsov));
	wsov.mon = m;
	int count = 0;
	wsov_collect_workspace_tags(m, &count);
	if (count <= 0) {
		memset(&wsov, 0, sizeof(wsov));
		return false;
	}

	m->is_ws_overview = true;

	wsov.root = wlr_scene_tree_create(server.layers[LyrSpecialOverlay]);
	if (!wsov.root) {
		m->is_ws_overview = false;
		memset(&wsov, 0, sizeof(wsov));
		return false;
	}
	wsov.background = wlr_scene_rect_create(wsov.root, m->m.width, m->m.height,
											config.rootcolor);
	wsov_update_root_geometry(m);
	if (!wsov_create_drop_indicator()) {
		wsov_finalize_close(0);
		return false;
	}
	wsov.animating = config.animations;
	wsov.progress = config.animations ? 0.0 : 1.0;
	wsov.animation_from = 0.0;
	wsov.animation_to = 1.0;
	clock_gettime(CLOCK_MONOTONIC, &wsov.animation_start);

	wsov_hide_clients(true);
	wlr_seat_pointer_clear_focus(server.seat);
	wlr_seat_keyboard_notify_clear_focus(server.seat);
	pointer_cursor_activity();
	if (!server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");

	if (!wsov_create_cells()) {
		wsov_finalize_close(0);
		return false;
	}
	if (wsov.animating || wsov.layout_animating)
		wsov_attach_frame_listener();
	wlr_output_schedule_frame(m->wlr_output);
	return true;
}

static void wsov_rebuild(void) {
	if (!workspace_overview_is_active())
		return;
	if (wsov.closing)
		return;
	wsov_cancel_scheduled_rebuild();

	wsov_cancel_drag();
	if (!wsov_create_cells()) {
		wsov_finalize_close(0);
		return;
	}
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

static bool wsov_refresh_workspace_tags(uint32_t tags) {
	if (!workspace_overview_is_active() || wsov.closing || !wsov.mon ||
		wsov.cell_count <= 0)
		return false;

	uint32_t current = 0;
	for (int i = 0; i < wsov.cell_count; i++)
		current |= wsov_tag_bit(wsov.cells[i].tag);

	/* Empty workspaces keep their cells in ALL_WORKSPACE mode. Only occupied
	 * mode can remove a cell and thereby change every preview's grid position.
	 */
	if (wsov.mon->workspace_overview_scope == OCCUPIED_WORKSPACE) {
		int expected_count = 0;
		uint32_t expected =
			wsov_collect_workspace_tags(wsov.mon, &expected_count);
		if (expected_count != wsov.cell_count || expected != current)
			return false;
	}

	tags &= current;
	if (!tags)
		return true;

	wsov_sync_monitor_selection();
	wsov_capture_workspace_tags(tags);

	wsov_render_tags(tags);
	wsov_update_styles_tags(tags);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
	return true;
}

static void wsov_refresh_workspace_or_rebuild(uint32_t tags) {
	if (!wsov_refresh_workspace_tags(tags))
		wsov_rebuild();
}

static void wsov_activate_target_tag(void) {
	if (!workspace_overview_is_active() || wsov.closing || !wsov.mon ||
		!wsov.target_tag)
		return;
	uint32_t target_bit = wsov_tag_bit(wsov.target_tag);
	if (!target_bit ||
		(wsov.mon->tagset[wsov.mon->seltags] & TAGMASK) == target_bit)
		return;
	client_view_on_monitor(&(Arg){.ui = target_bit}, false, wsov.mon, true);
}

int32_t toggle_workspace_overview(const Arg *arg) {
	if (!server.selected_monitor || server.grab_client ||
		server.selected_monitor->isoverview)
		return 0;
	if (workspace_overview_is_active()) {
		if (wsov.closing)
			wsov_resume_open_animation();
		else
			wsov_close(wsov.target_tag, NULL);
	} else {
		Monitor *m = server.selected_monitor;
		m->workspace_overview_scope =
			arg->i == OCCUPIED_WORKSPACE ? OCCUPIED_WORKSPACE : ALL_WORKSPACE;
		wsov_open(m);
	}
	return 0;
}

int32_t enter_workspace_overview(const Arg *arg) {
	if (!server.selected_monitor || server.grab_client ||
		server.selected_monitor->isoverview)
		return 0;
	if (workspace_overview_is_active()) {
		wsov_resume_open_animation();
	} else {
		Monitor *m = server.selected_monitor;
		m->workspace_overview_scope =
			arg->i == OCCUPIED_WORKSPACE ? OCCUPIED_WORKSPACE : ALL_WORKSPACE;
		wsov_open(m);
	}
	return 0;
}

int32_t leave_workspace_overview(const Arg *arg) {
	if (workspace_overview_is_active())
		wsov_close(0, NULL);
	return 0;
}

void workspace_overview_handle_motion(double x, double y) {
	if (!workspace_overview_is_active() || wsov.closing)
		return;

	if (wsov_set_target_from_point(x, y)) {
		wsov_refresh_target_selection();
	}

	if (!wsov.pressed)
		return;

	double dx = x - wsov.drag_start_x;
	double dy = y - wsov.drag_start_y;
	int32_t drag_threshold = config.workspace_overview_drag_threshold;
	if (!wsov.moved && dx * dx + dy * dy >= drag_threshold * drag_threshold) {
		wsov.moved = true;
		if (!server.cursor_hidden)
			wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
								   "grabbing");
		if (wsov.has_drag_client) {
			struct wsov_cell *source = wsov_cell_for_tag(wsov.source_tag);
			if (source) {
				struct wsov_item *item = NULL;
				for (int i = 0; i < source->item_count; i++) {
					if (source->items[i].client == wsov.drag_client) {
						item = &source->items[i];
						break;
					}
				}
				if (item) {
					wsov_create_drag_buffer(item);
					wsov_begin_drag_floating(item->client);
					uint32_t tags = wsov_client_tags(item->client) |
									wsov_tag_bit(wsov.source_tag);
					wsov_refresh_workspace_tags(tags);
				}
			}
		}
	}

	if (!wsov.moved)
		return;

	if (wsov.drag_buffer) {
		wsov_place_buffer_clipped(wsov.drag_buffer, NULL,
								  (int)lround(x - wsov.mon->m.x) -
									  wsov.drag_grab_x,
								  (int)lround(y - wsov.mon->m.y) -
									  wsov.drag_grab_y - wsov.drag_bar_height,
								  wsov.drag_width, wsov.drag_height);
	}
	wsov_update_drop_indicator(x, y);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

static bool wsov_item_is_drop_target(const struct wsov_item *item) {
	return item && item->client && item->client != wsov.drag_client &&
		   ISTILED(item->client);
}

static int32_t wsov_cell_tiling_count(const struct wsov_cell *cell) {
	int32_t count = 0;
	if (!cell)
		return 0;
	for (int i = 0; i < cell->item_count; i++) {
		const struct wsov_item *item = &cell->items[i];
		if (item->client != wsov.drag_client && ISTILED(item->client))
			count++;
	}
	return count;
}

static struct wsov_item *wsov_drop_item_at(struct wsov_cell *cell, double x,
										   double y) {
	struct wsov_item *hit = wsov_item_at(cell, x, y);
	if (wsov_item_is_drop_target(hit))
		return hit;
	if (!cell || !wsov.mon)
		return NULL;

	struct wsov_item *nearest = NULL;
	double best = 1e30;
	double lx = x - wsov.mon->m.x;
	double ly = y - wsov.mon->m.y;
	for (int i = 0; i < cell->item_count; i++) {
		struct wsov_item *item = &cell->items[i];
		if (!wsov_item_is_drop_target(item))
			continue;
		struct wlr_box box = wsov_item_display_box(cell, item);
		double cx = box.x + box.width / 2.0;
		double cy = box.y + box.height / 2.0;
		double distance = (cx - lx) * (cx - lx) + (cy - ly) * (cy - ly);
		if (distance < best) {
			best = distance;
			nearest = item;
		}
	}
	return nearest;
}

static bool wsov_get_drop_target(double x, double y,
								 struct wsov_cell **cell_out,
								 struct wsov_item **item_out,
								 struct wlr_box *zone_out,
								 int32_t *direction_out) {
	struct wsov_cell *cell = wsov_drop_cell_at(x, y);
	if (!cell || !wsov.mon || !wsov.drag_client || wsov.drag_was_floating) {
		return false;
	}

	struct wsov_item *item = wsov_drop_item_at(cell, x, y);
	if (!item)
		return false;

	const Layout *layout = wsov.mon->pertag->ltidxs[cell->tag];
	struct wlr_box item_box = wsov_item_display_box(cell, item);
	double rel_x = x - wsov.mon->m.x - item_box.x;
	double rel_y = y - wsov.mon->m.y - item_box.y;
	struct wlr_box zone;
	int32_t direction = client_drop_zone(
		layout, item->client->ismaster, wsov_cell_tiling_count(cell), rel_x,
		rel_y, item_box.width, item_box.height, &zone);
	zone.x += item_box.x;
	zone.y += item_box.y;
	item->client->drop_direction = direction;

	if (cell_out)
		*cell_out = cell;
	if (item_out)
		*item_out = item;
	if (zone_out)
		*zone_out = zone;
	if (direction_out)
		*direction_out = direction;
	return true;
}

static void wsov_hide_drop_indicator(void) {
	wsov.drop_indicator_visible = false;
	if (wsov.drop_fill)
		wlr_scene_node_set_enabled(&wsov.drop_fill->node, false);
	for (int i = 0; i < 4; i++) {
		if (wsov.drop_border[i])
			wlr_scene_node_set_enabled(&wsov.drop_border[i]->node, false);
	}
}

static void wsov_update_drop_indicator(double x, double y) {
	struct wsov_cell *cell = NULL;
	struct wlr_box zone;
	if (!wsov_get_drop_target(x, y, &cell, NULL, &zone, NULL) ||
		zone.width <= 0 || zone.height <= 0) {
		wsov_hide_drop_indicator();
		return;
	}

	if (cell->tag != wsov.target_tag) {
		wsov.target_tag = cell->tag;
		wsov.camera_tag = cell->tag;
		wsov_refresh_target_selection();
	}

	int32_t width = MANGO_MAX(1, zone.width);
	int32_t height = MANGO_MAX(1, zone.height);
	wsov_place_rect_clipped(wsov.drop_fill, zone.x, zone.y, width, height,
							true);
	if (!wsov.drop_fill)
		return;

	int32_t bw = MANGO_MIN(4, MANGO_MAX(2, MANGO_MIN(width, height) / 8));
	bw = MANGO_MIN(bw, MANGO_MAX(1, MANGO_MIN(width, height) / 2));
	wsov_place_rect_clipped(wsov.drop_border[0], zone.x, zone.y, width, bw,
							true);
	wsov_place_rect_clipped(wsov.drop_border[1], zone.x, zone.y + height - bw,
							width, bw, true);
	wsov_place_rect_clipped(wsov.drop_border[2], zone.x, zone.y, bw, height,
							true);
	wsov_place_rect_clipped(wsov.drop_border[3], zone.x + width - bw, zone.y,
							bw, height, true);
	wsov.drop_indicator_visible = true;

	wlr_scene_node_raise_to_top(&wsov.drop_fill->node);
	for (int i = 0; i < 4; i++)
		wlr_scene_node_raise_to_top(&wsov.drop_border[i]->node);
	if (wsov.drag_buffer)
		wlr_scene_node_raise_to_top(&wsov.drag_buffer->node);
}

static void wsov_place_floating(Client *c, struct wsov_cell *cell, double x,
								double y) {
	if (!c || !cell || !wsov.mon)
		return;
	int width = MANGO_MAX(64, c->float_geom.width > 0 ? c->float_geom.width
													  : c->geom.width);
	int height = MANGO_MAX(64, c->float_geom.height > 0 ? c->float_geom.height
														: c->geom.height);
	struct wlr_box cell_box = wsov_cell_layout_box(cell);
	double scale_x =
		(double)wsov.mon->m.width /
		MANGO_MAX(1,
				  cell_box.width - 2 * config.workspace_overview_border_width);
	double scale_y =
		(double)wsov.mon->m.height /
		MANGO_MAX(1,
				  cell_box.height - 2 * config.workspace_overview_border_width);
	double content_x =
		x - wsov.mon->m.x - cell_box.x - config.workspace_overview_border_width;
	double content_y =
		y - wsov.mon->m.y - cell_box.y - config.workspace_overview_border_width;
	int nx = wsov.mon->w.x +
			 (int)lround(content_x * scale_x - wsov.drag_grab_x * scale_x);
	int ny = wsov.mon->w.y +
			 (int)lround(content_y * scale_y - wsov.drag_grab_y * scale_y);
	nx = MANGO_MAX(wsov.mon->w.x,
				   MANGO_MIN(nx, wsov.mon->w.x + wsov.mon->w.width - width));
	ny = MANGO_MAX(wsov.mon->w.y,
				   MANGO_MIN(ny, wsov.mon->w.y + wsov.mon->w.height - height));
	c->float_geom = (struct wlr_box){nx, ny, width, height};
	resize(c, c->float_geom, (ResizeOpts){.interact = 0});
}

static void wsov_finish_drop(double x, double y) {
	Client *c = wsov.drag_client;
	struct wsov_cell *target = wsov_drop_cell_at(x, y);
	if (!c || !target || !wsov_client_eligible(c)) {
		uint32_t tags = wsov_client_tags(c) | wsov_tag_bit(wsov.source_tag);
		wsov_cancel_drag();
		wsov_refresh_workspace_or_rebuild(tags);
		return;
	}

	struct wsov_item *over = NULL;
	wsov_get_drop_target(x, y, &target, &over, NULL, NULL);
	bool was_floating = wsov.drag_was_floating;

	uint32_t source_bit = wsov_tag_bit(wsov.source_tag);
	uint32_t target_bit = wsov_tag_bit(target->tag);
	uint32_t old_tags = c->isglobal ? 0 : c->tags;
	uint32_t mods = keyboard_hard_modifiers();

	if (!c->isglobal && target->tag != wsov.source_tag) {
		if (mods & WLR_MODIFIER_CTRL)
			c->tags = target_bit;
		else if (mods & WLR_MODIFIER_SHIFT)
			c->tags |= target_bit;
		else
			c->tags = (c->tags & ~source_bit) | target_bit;
		c->tags &= TAGMASK;
		if (!c->tags)
			c->tags = target_bit;
		if (old_tags != c->tags)
			client_reparent_group(c);
	}
	clear_fullscreen_for_incoming_tiled_client(c);

	client_view_on_monitor(&(Arg){.ui = target_bit}, false, wsov.mon, false);
	if (over)
		pointer_place_drag_tile_on(c, over->client);
	if (was_floating)
		wsov_place_floating(c, target, x, y);

	wsov_restore_drag_floating();
	client_focus(c, 1);
	arrange(wsov.mon, false, false);

	wsov_hide_drop_indicator();
	wsov.pressed = false;
	wsov.moved = false;
	wsov.has_drag_client = false;
	wsov.drag_client = NULL;
	wsov.source_tag = 0;
	wsov.target_tag = target->tag;
	wsov.camera_tag = target->tag;
	wsov.drag_return_tag = 0;
	wsov_destroy_drag_buffer();
	if (!server.cursor_hidden)
		wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "default");
	uint32_t affected_tags = source_bit | target_bit;
	if (c->isglobal)
		affected_tags |= TAGMASK;
	else
		affected_tags |= old_tags | c->tags;
	wsov_refresh_workspace_or_rebuild(affected_tags);
}

bool workspace_overview_handle_button(
	const struct wlr_pointer_button_event *event) {
	if (!workspace_overview_is_active())
		return false;
	if (wsov.closing)
		return true;

	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		server.cursor_mode = CurPressed;
		struct wsov_cell *cell =
			wsov_cell_at(server.cursor->x, server.cursor->y);
		struct wsov_item *item =
			wsov_item_at(cell, server.cursor->x, server.cursor->y);

		if (event->button == BTN_RIGHT && item) {
			pending_kill_client(item->client);
			return true;
		}
		if (event->button != BTN_LEFT)
			return true;

		wsov_hide_drop_indicator();
		wsov.pressed = true;
		wsov.moved = false;
		wsov.drag_start_x = (int)lround(server.cursor->x);
		wsov.drag_start_y = (int)lround(server.cursor->y);
		wsov.source_tag = cell ? cell->tag : 0;
		wsov.target_tag = cell ? cell->tag : 0;
		wsov.drag_return_tag = wsov.target_tag;
		wsov_refresh_target_selection();
		wsov.has_drag_client = item != NULL;
		wsov.drag_client = item ? item->client : NULL;
		if (item) {
			struct wlr_box item_box = wsov_item_display_box(cell, item);
			wsov.drag_grab_x =
				(int)lround(server.cursor->x - wsov.mon->m.x - item_box.x);
			wsov.drag_grab_y =
				(int)lround(server.cursor->y - wsov.mon->m.y - item_box.y);
		}
		wsov_update_styles();
		return true;
	}

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (event->button != BTN_LEFT) {
			server.cursor_mode = CurNormal;
			return true;
		}
		if (wsov.moved && wsov.has_drag_client) {
			wsov_finish_drop(server.cursor->x, server.cursor->y);
		} else {
			struct wsov_cell *cell =
				wsov_cell_at(server.cursor->x, server.cursor->y);
			uint32_t tag = cell ? cell->tag : wsov.source_tag;
			struct wsov_item *item =
				wsov_item_at(cell, server.cursor->x, server.cursor->y);
			Client *focus = item ? item->client : NULL;
			client_focus(focus, 1);
			wsov_close(tag, focus);
		}
		server.cursor_mode = CurNormal;
		return true;
	}
	return true;
}

void workspace_overview_client_commit(Client *c) {
	if (!workspace_overview_is_active() || wsov.closing || c->mon != wsov.mon)
		return;
	uint32_t current_layer = client_target_layer(c);
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!wsov_client_on_tag(c, cell->tag))
			continue;

		bool found = false;
		for (int j = 0; j < cell->item_count; j++) {
			struct wsov_item *item = &cell->items[j];
			if (item->client != c)
				continue;
			found = true;
			if (item->layer < LyrTop || current_layer < LyrTop)
				cell->contents_dirty = true;
			if (item->layer >= LyrTop || current_layer >= LyrTop)
				cell->foreground_dirty = true;
			break;
		}
		if (!found) {
			cell->contents_dirty = true;
			cell->foreground_dirty = true;
		}
	}
	wsov_schedule_dirty_render();
}

void workspace_overview_layers_changed(LayerSurface *layer) {
	wsov_refresh_layer_preview(layer);
}

void workspace_overview_client_added(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return;
	if (!client_is_unmanaged(c)) {
		c->wsov_hidden = true;
		client_update_visibility(c);
	}
	wlr_seat_pointer_clear_focus(server.seat);
	wlr_seat_keyboard_notify_clear_focus(server.seat);
	wsov_refresh_workspace_or_rebuild(wsov_client_tags(c));
}

void workspace_overview_client_removed(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return;
	if (c == wsov.close_client)
		wsov.close_client = NULL;
	if (c == wsov.drag_client)
		wsov_cancel_drag();
	c->wsov_hidden = false;
	wsov_refresh_workspace_or_rebuild(wsov_client_tags(c));
}

void workspace_overview_client_replaced(Client *old, Client *new) {
	if (!workspace_overview_is_active() || !old || !new || old == new)
		return;
	if (wsov.drag_client == old)
		wsov.drag_client = new;
	if (wsov.close_client == old)
		wsov.close_client = new;
}

void workspace_overview_client_tags_changed(Client *c) {
	if (!workspace_overview_is_active() || wsov.closing || !c ||
		c->mon != wsov.mon)
		return;
	uint32_t old_tags = wsov_cell_tags_for_client(c);
	uint32_t new_tags = wsov_client_tags(c);
	wsov_refresh_workspace_or_rebuild(old_tags | new_tags);
}

void workspace_overview_client_title_changed(Client *c) {
	if (!workspace_overview_is_active() || wsov.closing || !c ||
		c->mon != wsov.mon)
		return;

	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (!wsov_client_on_tag(c, cell->tag))
			continue;
		wsov_capture_cell_bars(cell);
		wsov_render_cell(cell, wsov_output_scale());
	}
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void workspace_overview_layout_changed(Monitor *m) {
	if (!workspace_overview_is_active_on(m) || wsov.closing)
		return;
	uint32_t tags = wsov_layout_affected_tags(m);
	wsov_refresh_workspace_or_rebuild(tags);
}

void workspace_overview_client_unmapped(Client *c, bool activate_target) {
	if (!workspace_overview_is_active() || wsov.closing || !c)
		return;
	uint32_t tags = wsov_client_tags(c);
	uint32_t old_target_tag = wsov.target_tag;
	uint32_t previous_camera_tag = wsov.camera_tag;
	wsov_select_default_target();
	wsov.camera_tag = wsov.target_tag;
	Client *previous = wsov.mon->sel;
	uint32_t target_bit = wsov_tag_bit(wsov.target_tag);
	bool switch_workspace = activate_target && target_bit &&
							!(wsov.mon->tagset[wsov.mon->seltags] & target_bit);
	if (switch_workspace) {
		client_view_on_monitor(&(Arg){.ui = target_bit}, false, wsov.mon, true);
		wsov_capture_workspace_tags(target_bit);
	}
	wsov_sync_monitor_selection();
	if (switch_workspace) {
		wsov_render_tags(target_bit);
	} else {
		wsov_render_selection_clients(previous, wsov.mon->sel);
	}
	wsov_update_styles_tags(tags | wsov_tag_bit(old_target_tag) | target_bit);
	if (switch_workspace || previous_camera_tag != wsov.camera_tag)
		wsov_apply_camera(wsov.progress);
}

void workspace_overview_monitor_destroy(Monitor *m) {
	if (workspace_overview_is_active_on(m))
		wsov_finalize_close(0);
}

void workspace_overview_monitor_changed(Monitor *m) {
	if (!workspace_overview_is_active_on(m) || wsov.closing)
		return;
	wsov_update_root_geometry(m);
	wsov_schedule_rebuild();
}

void workspace_overview_reload(void) {
	if (!workspace_overview_is_active() || wsov.closing || !wsov.mon)
		return;
	wsov_update_root_geometry(wsov.mon);
	wsov_schedule_rebuild();
}
