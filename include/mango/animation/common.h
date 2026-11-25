#ifndef __ANIMATION_COMMON_H__
#define __ANIMATION_COMMON_H__ 1

#include "mango/config/parse.h"
#include <scenefx/types/fx/clipped_region.h>
#include <scenefx/types/wlr_scene.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/util/box.h>

/* What is currently being animated / which animation phase is running. */
enum { NONE, OPEN, MOVE, CLOSE, TAG, FOCUS, OPAFADEIN, OPAFADEOUT, OVERVIEW };

struct dvec2 {
	double x, y;
};

struct ivec2 {
	int32_t x, y, width, height;
};

struct mango_animation {
	bool should_animate;
	bool running;
	bool tagining;
	bool tagouted;
	bool tagouting;
	bool begin_fade_in;
	bool tag_from_rule;
	bool overining;
	bool overview_enter_anim_set;
	uint32_t time_started;
	uint32_t duration;
	struct wlr_box initial;
	struct wlr_box current;
	int32_t action;
};

struct mango_opacity_animation {
	bool running;
	float current_opacity;
	float target_opacity;
	float initial_opacity;
	uint32_t time_started;
	uint32_t duration;
	float current_border_color[4];
	float target_border_color[4];
	float initial_border_color[4];
	float current_dim_color[4];
	float target_dim_color[4];
	float initial_dim_color[4];
};

typedef struct {
	float width_scale;
	float height_scale;
	int32_t width;
	int32_t height;
	struct fx_corner_radii corner_location;
	bool should_scale;
} BufferData;

typedef struct SnapshotMetadata {
	uint32_t type; // must at first in struct
	int32_t orig_width;
	int32_t orig_height;
	struct wlr_fbox orig_src_box;
	int32_t orig_x;
	int32_t orig_y;
	bool is_subsurface;
	struct wl_listener destroy;
} SnapshotMetadata;

double find_animation_curve_for(double t, const AnimationCurve *curve);
double animation_curve_ease(const AnimationCurve *curve, double progress);
uint32_t animation_curve_duration_ms(const AnimationCurve *curve,
									 uint32_t fallback_ms);
uint32_t animation_duration_for_type(int32_t type, uint32_t fallback_ms);
void handle_snapshot_meta_destroy(struct wl_listener *listener, void *data);
double find_animation_curve_at(double t, int32_t type);

bool scene_node_snapshot(struct wlr_scene_node *node, int32_t lx, int32_t ly,
						 struct wlr_scene_tree *snapshot_tree);

struct wlr_scene_tree *wlr_scene_tree_snapshot(struct wlr_scene_node *node,
											   struct wlr_scene_tree *parent);
void scene_snapshot_clip_to_box(struct wlr_scene_tree *snapshot,
								const struct wlr_box *bounds, bool scale,
								int32_t width, int32_t height);
void scene_snapshot_apply_opacity(struct wlr_scene_tree *snapshot,
								  double opacity);
void request_fresh_all_monitors(void);
#endif
