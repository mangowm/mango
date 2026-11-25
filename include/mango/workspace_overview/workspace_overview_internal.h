#ifndef MANGO_WORKSPACE_OVERVIEW_INTERNAL_H
#define MANGO_WORKSPACE_OVERVIEW_INTERNAL_H

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
#include "mango/layout/layout.h"
#include "mango/layout/tag_ratio.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/monitor.h"
#include "mango/manage/tab.h"
#include "mango/workspace_overview/client_effects.h"
#include "mango/workspace_overview/workspace_overview.h"
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
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/transform.h>
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#endif

struct wsov_item {
	Client *client;
	struct wlr_scene_tree *scene;
	uint32_t layer;
	int32_t border_width;
	bool is_snapshot;
	struct wlr_box box;
	struct wlr_box workspace_geom;
	double render_scale;
	double render_scale_x;
	double render_scale_y;
};

/*
 * A live workspace copy binds scene_surface nodes directly to the client's
 * existing wlr_surface. Unlike the offscreen cell buffers, commits are consumed
 * by the output scene and do not require re-compositing the whole workspace.
 */
struct wsov_live_surface {
	Client *client;
	struct wsov_cell *cell;
	struct wsov_live_effects *effects;
	struct wlr_surface *surface;
	struct wlr_scene_surface *scene_surface;
	int sx;
	int sy;
	uint64_t effect_frame_serial;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener frame_done;
};

struct wsov_live_surface_data {
	Client *client;
	struct wsov_cell *cell;
	struct wlr_scene_tree *tree;
	struct wsov_live_effects *effects;
};

struct wsov_live_client {
	Client *client;
	struct wlr_scene_tree *tree;
	struct wsov_live_effects *effects;
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
	int workspace_x;
	int workspace_y;
	int width;
	int height;
	struct wlr_fbox src_box;
};

struct wsov_background_effects;

struct wsov_cell {
	uint32_t tag;
	struct wlr_box box;
	struct wlr_scene_rect *frame;
	struct wlr_scene_rect *border[4];
	struct wlr_scene_buffer *contents;
	struct wlr_scene_buffer *foreground;
	struct wlr_buffer *contents_buffers[2];
	struct wlr_buffer *foreground_buffers[2];
	uint64_t contents_render_frame;
	uint64_t foreground_render_frame;
	uint64_t content_revision;
	uint64_t contents_render_revision;
	uint64_t foreground_render_revision;
	int pool_width;
	int pool_height;
	struct wlr_scene_tree *live_lower_tree;
	struct wlr_scene_tree *live_upper_tree;
	struct wlr_scene_tree *live_layer_trees[NUM_LAYERS];
	struct wsov_live_surface **live_surfaces;
	int live_surface_count;
	int live_surface_capacity;
	struct wsov_live_client *live_clients;
	int live_client_count;
	int live_client_capacity;
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
	int buffer_w;
	int buffer_h;
	struct wlr_box layout_from;
	struct wlr_box layout_current;
	bool borders_enabled;
	bool dim_enabled;
	bool contents_dirty;
	bool foreground_dirty;
	bool capture_pending;
	bool live_enabled;
	bool live_desired;
	bool live_rebuild_pending;
	uint8_t live_priority;
};

typedef struct {
	double accum;
	uint32_t last_time_msec;
	int32_t direction;
	bool tracking;
} WsovScrollWheelState;

struct wsov_state {
	Monitor *mon;
	WorkspaceOverviewLayout layout;
	struct wlr_scene_tree *root;
	struct wlr_scene_rect *background;
	struct wsov_background_effects *background_effects;
	/* Actual TOP/OVERLAY layer-shell scenes live above root while wsov is open.
	 */
	struct wlr_scene_tree *direct_layer_tree;
	struct wlr_scene_tree *direct_top_tree;
	struct wlr_scene_tree *direct_overlay_tree;
	struct wlr_scene_tree *direct_fadeout_tree;
	struct wsov_cell *cells;
	int cell_count;
	struct wl_listener frame;
	struct wl_event_source *render_idle;
	struct wl_event_source *rebuild_idle;

	bool animating;
	bool closing;
	bool layers_dirty;
	bool capturing_layout;
	bool client_animating;
	bool layout_animating;
	bool live_layout_dirty;
	bool live_layout_flushing;
	bool frame_active;
	double progress;
	double animation_from;
	double animation_to;
	struct timespec animation_start;
	struct timespec layout_animation_start;
	double layout_animation_duration_ms;
	uint32_t camera_tag;
	uint32_t retained_tags;
	uint32_t close_tag;
	Client *close_client;
	bool close_focus_explicit;

	bool pressed;
	bool moved;
	bool has_drag_client;
	uint32_t source_tag;
	uint32_t target_tag;
	uint32_t scroll_center_tag;
	uint32_t drag_return_tag;
	Client *drag_client;
	int drag_start_x;
	int drag_start_y;
	int drag_grab_x;
	int drag_grab_y;
	int drag_source_width;
	int drag_source_height;
	int drag_preview_x;
	int drag_preview_y;
	struct wlr_scene_buffer *drag_buffer;
	int drag_width;
	int drag_height;
	int drag_bar_height;
	struct wlr_scene_rect *drop_fill;
	bool drop_indicator_visible;
	bool drag_floating_changed;
	bool drag_was_floating;
	bool drag_was_drag_to_tile;
	struct wlr_box drag_saved_float_geom;

	/* Primary workspace-strip coordinate shown at the viewport origin. */
	double scroll_offset;
	double scroll_min;
	double scroll_max;
	bool scroll_animating;
	double scroll_animation_from;
	double scroll_animation_to;
	double scroll_animation_duration_ms;
	struct timespec scroll_animation_start;
	WsovScrollWheelState scroll_wheel;
	int32_t drag_scroll_direction;
	struct timespec drag_scroll_last_frame;
	bool drag_scroll_frame_valid;
	struct wsov_cell *live_cell;
};

struct wsov_camera_view {
	double x;
	double y;
	double scale_x;
	double scale_y;
};

extern struct wsov_state wsov;

struct wsov_background_effects *
wsov_background_effects_create(struct wlr_scene_tree *parent, int width,
							   int height);
void wsov_background_effects_update(struct wsov_background_effects *effects,
									int width, int height);
void wsov_background_effects_destroy(struct wsov_background_effects *effects);

bool wsov_retain_texture(struct wsov_render_resources *resources,
						 struct wlr_texture *texture, bool owned);
void wsov_release_textures(struct wsov_render_resources *resources);
uint32_t wsov_tag_bit(uint32_t tag);
uint32_t wsov_client_tags(Client *c);
uint32_t wsov_cell_tags_for_client(Client *c);
uint32_t wsov_layout_affected_tags(Monitor *m);
bool wsov_client_base_eligible(Client *c);
bool wsov_client_eligible(Client *c);
bool workspace_overview_active_workspace_will_empty(Client *c);
bool wsov_client_on_tag(Client *c, uint32_t tag);
Client *wsov_focus_top_for_tag(uint32_t tag);
void wsov_sync_monitor_selection(void);
bool wsov_tag_has_normal_client(uint32_t tag);
uint32_t wsov_collect_workspace_tags(Monitor *m, int *count);
float wsov_premultiplied(float component, float alpha);
void wsov_set_rect_color(struct wlr_scene_rect *rect, const float color[4]);
bool wsov_create_drop_indicator(void);
struct wsov_cell *wsov_cell_for_tag(uint32_t tag);
struct wlr_box wsov_cell_layout_box(const struct wsov_cell *cell);
bool wsov_scrolls_horizontally(void);
bool wsov_scrolls_vertically(void);
int wsov_border_width(void);
bool wsov_live_clips_to_workspace(const struct wsov_cell *cell);
struct wlr_box wsov_live_overflow_clip(const struct wlr_box *workspace,
									   const struct wlr_box *monitor);
struct wlr_box wsov_item_display_box(const struct wsov_cell *cell,
									 const struct wsov_item *item);
struct wlr_box wsov_item_final_display_box(const struct wsov_cell *cell,
										   const struct wsov_item *item);
struct wlr_box wsov_monitor_clip(void);
bool wsov_intersect_monitor(int x, int y, int width, int height,
							struct wlr_box *visible);
bool wsov_get_camera_view(double progress, struct wsov_camera_view *view);
struct wlr_box wsov_cell_screen_box(const struct wsov_cell *cell,
									const struct wsov_camera_view *view);
bool wsov_cell_box_visible(const struct wlr_box *box);
bool wsov_cell_preview_visible(const struct wsov_cell *cell, double progress);
bool wsov_cell_live_content_visible(const struct wsov_cell *cell,
									double progress);
bool wsov_cell_live_content_visible_in_view(
	const struct wsov_cell *cell, const struct wsov_camera_view *view);
void wsov_place_rect_clipped(struct wlr_scene_rect *rect, int x, int y,
							 int width, int height, bool enabled);
void wsov_place_buffer_clipped(struct wlr_scene_buffer *node,
							   const struct wlr_fbox *base_src, int x, int y,
							   int width, int height);
double wsov_elapsed_ms(const struct timespec *start);
double wsov_output_scale(void);
void wsov_rebind_pass(struct wsov_render_resources *resources);
void wsov_process_frame(void);
void wsov_handle_frame(struct wl_listener *listener, void *data);
void wsov_attach_frame_listener(void);
void wsov_detach_frame_listener(void);
void wsov_select_default_target(void);
struct wsov_cell *wsov_cell_at(double x, double y);
struct wsov_cell *wsov_drop_cell_at(double x, double y);
bool wsov_set_target_from_point(double x, double y);
struct wsov_item *wsov_item_at(struct wsov_cell *cell, double x, double y);
struct wsov_cell *wsov_hit_cell_at(double x, double y);
void wsov_item_add(struct wsov_cell *cell, Client *c);
void wsov_snapshot_item_add(struct wsov_cell *cell, Client *snapshot);
struct wsov_item *wsov_cell_item_for_client(struct wsov_cell *cell, Client *c);
int wsov_scene_node_sibling_index(struct wlr_scene_node *node);
int wsov_item_stack_compare(const void *left, const void *right);
bool wsov_item_is_live(const struct wsov_cell *cell,
					   const struct wsov_item *item);
struct wsov_live_client *wsov_cell_live_client_for(const struct wsov_cell *cell,
												   const Client *client);
bool wsov_item_has_effect_border(const struct wsov_cell *cell,
								 const struct wsov_item *item);
bool wsov_cell_contains_live_client(const struct wsov_cell *cell,
									const Client *client);
struct wsov_cell *wsov_live_owner_for_client(Client *client);
void wsov_remove_live_surface(struct wsov_cell *cell,
							  struct wsov_live_surface *entry,
							  bool destroy_node);
void wsov_live_surface_commit(struct wl_listener *listener, void *data);
void wsov_live_surface_destroy(struct wl_listener *listener, void *data);
void wsov_live_surface_frame_done(struct wl_listener *listener, void *data);
struct wsov_live_client *
wsov_live_client_add(struct wsov_cell *cell, Client *client,
					 struct wlr_scene_tree *layer_tree);
void wsov_live_surface_add(struct wlr_surface *surface, int sx, int sy,
						   void *data);
void wsov_clear_cell_live_surfaces(struct wsov_cell *cell, bool mark_dirty);
void wsov_clear_live_projection(bool mark_dirty);
void wsov_destroy_cell_live_trees(struct wsov_cell *cell);
bool wsov_create_cell_live_trees(struct wsov_cell *cell);
void wsov_order_live_projection(struct wsov_cell *cell);
void wsov_order_live_projections(void);
void wsov_order_live_clients(struct wsov_cell *cell);
bool wsov_sync_live_client_layers(struct wsov_cell *cell);
bool wsov_cell_live_projection_matches(const struct wsov_cell *cell);
void wsov_build_cell_live_projection(struct wsov_cell *cell);
void wsov_update_live_desires(const struct wsov_camera_view *view);
bool wsov_sync_live_projection(const struct wsov_camera_view *view);
void wsov_rebuild_live_projection(void);
bool wsov_layer_has_preview(int layer_index);
struct wlr_scene_tree *wsov_layer_normal_parent(LayerSurface *layer);
void wsov_sync_direct_layers(void);
void wsov_restore_direct_layers(void);
void wsov_destroy_layer_fadeouts(void);
void wsov_layer_preview_add(struct wsov_cell *cell,
							struct wlr_scene_buffer *source,
							LayerSurface *layer, int layer_index, int sx,
							int sy);
void wsov_collect_layer_buffer(struct wlr_scene_buffer *buffer, int sx, int sy,
							   void *data);
void wsov_create_layer_previews_for_layer(struct wsov_cell *cell,
										  LayerSurface *layer, int layer_index);
void wsov_create_layer_previews(void);
void wsov_remove_layer_previews(struct wsov_cell *cell, LayerSurface *layer);
void wsov_destroy_layer_previews(struct wsov_cell *cell);
void wsov_refresh_layer_previews(void);
void wsov_refresh_layer_preview(LayerSurface *layer);
bool wsov_drag_hides_item(struct wsov_cell *cell, Client *c);
bool wsov_drag_hides_group(struct wsov_cell *cell, Client *group_head);
void wsov_capture_cell_items(struct wsov_cell *cell);
void wsov_capture_bar_rect(struct wsov_bar_item *item,
						   struct wlr_scene_rect *rect);
void wsov_capture_bar_buffer(struct wsov_bar_item *item,
							 struct wlr_scene_buffer *node);
void wsov_capture_bar(struct wsov_cell *cell, MangoBarDecoration *bar);
void wsov_clear_cell_bars(struct wsov_cell *cell);
void wsov_draw_group_bar_for_capture(Client *c);
void wsov_draw_tabbar_for_capture(Client *c);
bool wsov_group_capture_owner(Client *c);
void wsov_capture_cell_bars(struct wsov_cell *cell);
void wsov_position_cell_items(struct wsov_cell *cell);
void wsov_live_client_clip(Client *c, struct wlr_box *clip);
void wsov_layout_live_projection_impl(void);
void wsov_layout_live_projection(void);
void wsov_mark_live_layout_dirty(void);
void wsov_flush_live_layout(void);
void workspace_overview_prepare_output_frame(Monitor *m);
void wsov_settle_tag_animations(Monitor *m, uint32_t tags);
void wsov_capture_workspace_tags(uint32_t tags);
void wsov_capture_workspace_layouts(void);
const struct wlr_drm_format *wsov_pick_buffer_format(void);
struct wlr_buffer *wsov_create_buffer(int width, int height);
void wsov_release_cell_buffers(struct wsov_cell *cell);
struct wlr_buffer *wsov_acquire_cell_buffer(struct wsov_cell *cell,
											struct wlr_scene_buffer *target,
											int width, int height);
void wsov_render_scene_buffer(struct wlr_scene_buffer *scene_buffer, int sx,
							  int sy, void *data);
void wsov_render_scene_surfaces(struct wlr_scene_tree *tree, int origin_x,
								int origin_y, void *data);
void wsov_add_rect(struct wlr_render_pass *pass, struct wlr_box box,
				   const float color[4], enum wlr_render_blend_mode blend_mode);
bool wsov_fadeout_client_alive(const Client *client);
void wsov_render_client(struct wlr_render_pass *pass, struct wsov_cell *cell,
						struct wsov_item *item,
						struct wsov_render_resources *resources,
						double buffer_scale);
bool wsov_scene_node_layout_coords(struct wlr_scene_node *node, int *x, int *y);
void wsov_render_bar_item_at(struct wlr_render_pass *pass,
							 struct wsov_bar_item *item,
							 struct wsov_render_resources *resources,
							 int origin_x, int origin_y, double scale_x,
							 double scale_y, pixman_region32_t *clip);
void wsov_render_cell_layer_range(struct wlr_render_pass *pass,
								  struct wsov_cell *cell,
								  struct wsov_render_resources *resources,
								  uint32_t first_layer, uint32_t end_layer,
								  double buffer_scale);
bool wsov_cell_has_layer_range(const struct wsov_cell *cell,
							   uint32_t first_layer, uint32_t end_layer);
bool wsov_render_cell_part(struct wsov_cell *cell,
						   struct wlr_scene_buffer *target,
						   uint32_t first_layer, uint32_t end_layer,
						   double buffer_scale);
void wsov_prepare_cell_for_render(struct wsov_cell *cell);
bool wsov_render_cell_raw(struct wsov_cell *cell, double buffer_scale);
bool wsov_render_cell(struct wsov_cell *cell, double buffer_scale);
bool wsov_refresh_animated_cells(void);
bool wsov_render_dirty_cells(bool apply_camera);
void wsov_flush_dirty_cells(void *data);
void wsov_schedule_dirty_render(void);
void wsov_cancel_scheduled_render(void);
void wsov_flush_rebuild(void *data);
void wsov_schedule_rebuild(void);
void wsov_cancel_scheduled_rebuild(void);
void wsov_render_all(void);
void wsov_render_tags(uint32_t tags);
void wsov_update_styles_tags(uint32_t tags);
void wsov_update_styles(void);
bool wsov_cell_has_client(const struct wsov_cell *cell, const Client *client);
void wsov_render_selection_clients(Client *previous, Client *current);
void wsov_apply_target_selection(bool activate_view);
void wsov_refresh_target_selection(void);
void wsov_set_dim_enabled(bool enabled);
void wsov_set_borders_enabled(bool enabled);
void wsov_update_scroll_bounds(void);
double wsov_clamp_scroll(double offset);
bool wsov_scroll_to(double offset);
bool wsov_animate_scroll_to(double offset);
void wsov_update_scroll_animation(void);
double wsov_scroll_stride(void);
bool wsov_step_scroll_workspaces(int32_t direction, uint32_t count);
void wsov_apply_scroll_wheel(double notches, uint32_t time_msec);
void wsov_update_drag_scroll(double x, double y);
bool wsov_advance_drag_scroll(void);
void wsov_scroll_to_target(void);
void wsov_animate_scroll_to_final_target(void);
void wsov_animate_scroll_to_target(void);
void wsov_set_cell_box(struct wsov_cell *cell, int x, int y, int width,
					   int height);
int wsov_grid_cols(int count);
void wsov_layout_grid_cells(void);
void wsov_layout_scroll_cells(void);
void wsov_layout_cells(void);
void wsov_update_layout_animation(void);
void wsov_apply_camera(double progress);
void wsov_destroy_cells(void);
bool wsov_create_cells(void);
void wsov_destroy_drag_buffer(void);
void wsov_begin_drag_floating(Client *c);
void wsov_restore_drag_floating(void);
bool wsov_capture_drag_group_bars(Client *c, struct wsov_cell *bars);
bool wsov_create_drag_buffer(struct wsov_item *item);
void wsov_cancel_drag(void);
uint32_t wsov_active_camera_tag(void);
struct wsov_cell *wsov_target_cell(void);
void wsov_select_direction(int dir_x, int dir_y);
bool workspace_overview_handle_key(uint32_t sym);
void wsov_hide_clients(bool hidden);
void wsov_start_animation(bool closing, double target);
void wsov_resume_open_animation(void);
void wsov_close(uint32_t focus_tag, Client *focus_client);
void wsov_finish_client_geometry(Client *c);
void wsov_finalize_close(uint32_t focus_tag);
bool workspace_overview_is_active(void);
bool workspace_overview_is_active_on(const Monitor *m);
bool workspace_overview_is_closing_on(const Monitor *m);
bool workspace_overview_pointer_is_pressed(void);
void wsov_update_root_geometry(Monitor *m);
bool wsov_open(Monitor *m, WorkspaceOverviewLayout layout);
void wsov_rebuild(void);
bool wsov_refresh_workspace_tags(uint32_t tags);
void wsov_refresh_workspace_or_rebuild(uint32_t tags);
void wsov_activate_target_tag(void);
int32_t toggle_workspace_overview(const Arg *arg);
int32_t enter_workspace_overview(const Arg *arg);
int32_t leave_workspace_overview(const Arg *arg);
void wsov_sloppy_focus_at(double x, double y);
void workspace_overview_handle_motion(double x, double y);
bool wsov_item_is_drop_target(const struct wsov_item *item);
int32_t wsov_cell_tiling_count(const struct wsov_cell *cell);
struct wsov_item *wsov_drop_item_at(struct wsov_cell *cell, double x, double y);
bool wsov_get_drop_target(double x, double y, struct wsov_cell **cell_out,
						  struct wsov_item **item_out, struct wlr_box *zone_out,
						  int32_t *direction_out);
void wsov_hide_drop_indicator(void);
void wsov_update_drop_indicator(double x, double y);
void wsov_place_floating(Client *c, struct wsov_cell *cell);
void wsov_finish_drop(double x, double y);
bool workspace_overview_handle_button(
	const struct wlr_pointer_button_event *event);
bool workspace_overview_handle_axis(const struct wlr_pointer_axis_event *event);
void workspace_overview_client_commit(Client *c);
void workspace_overview_client_frame(Client *c);
void workspace_overview_layers_changed(LayerSurface *layer);
struct wlr_scene_tree *
workspace_overview_layer_fadeout_parent(LayerSurface *layer);
void workspace_overview_client_added(Client *c);
void workspace_overview_client_removed(Client *c);
void workspace_overview_client_replaced(Client *old, Client *new);
void workspace_overview_client_tags_changed(Client *c);
void workspace_overview_client_title_changed(Client *c);
void workspace_overview_client_stack_changed(Client *c);
void workspace_overview_layout_changed(Monitor *m);
void workspace_overview_client_unmapped(Client *c, bool activate_target);
void workspace_overview_monitor_destroy(Monitor *m);
void workspace_overview_monitor_changed(Monitor *m);
void workspace_overview_reload(void);

#endif
