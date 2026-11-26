#include "mango/workspace_overview/client_effects.h"

#ifdef MANGO_WORKSPACE_OVERVIEW_SCENEFX

#include "mango/animation/client.h"
#include "mango/common/util.h"
#include "mango/config/parse.h"
#include "mango/manage/client.h"
#include <math.h>
#include <scenefx/types/wlr_scene.h>
#include <stdlib.h>
#include <string.h>

struct wsov_live_effects {
	Client *client;
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_blur *blur;
	struct wlr_scene_rect *border;
	bool layout_valid;
	bool layout_hidden;
	struct wlr_box window_box;
	struct wlr_box monitor_clip;
	double scale;
	int32_t border_width;
	bool has_shadow;
	bool has_blur;
	struct fx_corner_radii corners;
	float color[4];
};

static struct fx_corner_radii wsov_scale_corners(struct fx_corner_radii corners,
												 double scale) {
	if (!isfinite(scale) || scale <= 0.0)
		return corner_radii_none();

	return corner_radii_new((int)lround(corners.top_left * scale),
							(int)lround(corners.top_right * scale),
							(int)lround(corners.bottom_right * scale),
							(int)lround(corners.bottom_left * scale));
}

static struct fx_corner_radii wsov_live_client_corners(Client *client,
													   double scale) {
	if (!client)
		return corner_radii_none();
	return wsov_scale_corners(set_client_corner_location(client), scale);
}

static bool wsov_client_has_shadow(const Client *client) {
	if (!client || client->no_shadow || client->isfullscreen)
		return false;
	return config.shadows &&
		   (!config.shadow_only_floating || client->isfloating);
}

static bool wsov_corners_equal(struct fx_corner_radii a,
							   struct fx_corner_radii b) {
	return a.top_left == b.top_left && a.top_right == b.top_right &&
		   a.bottom_right == b.bottom_right && a.bottom_left == b.bottom_left;
}

static bool wsov_live_effects_layout_unchanged(
	struct wsov_live_effects *effects, const struct wlr_box *window_box,
	const struct wlr_box *monitor_clip, double scale,
	struct fx_corner_radii corners, bool has_shadow, bool has_blur,
	const float color[4]) {
	if (!effects->layout_valid)
		return false;
	if (!window_box || !monitor_clip)
		return effects->layout_hidden;
	if (effects->layout_hidden)
		return false;

	return wlr_box_equal(&effects->window_box, window_box) &&
		   wlr_box_equal(&effects->monitor_clip, monitor_clip) &&
		   effects->scale == scale &&
		   effects->border_width == effects->client->bw &&
		   effects->has_shadow == has_shadow && effects->has_blur == has_blur &&
		   wsov_corners_equal(effects->corners, corners) &&
		   memcmp(effects->color, color, sizeof(effects->color)) == 0;
}

static void wsov_live_effects_cache_layout(struct wsov_live_effects *effects,
										   const struct wlr_box *window_box,
										   const struct wlr_box *monitor_clip,
										   double scale,
										   struct fx_corner_radii corners,
										   bool has_shadow, bool has_blur,
										   const float color[4]) {
	effects->layout_valid = true;
	effects->layout_hidden = !window_box || !monitor_clip;
	if (effects->layout_hidden)
		return;

	effects->window_box = *window_box;
	effects->monitor_clip = *monitor_clip;
	effects->scale = scale;
	effects->border_width = effects->client->bw;
	effects->has_shadow = has_shadow;
	effects->has_blur = has_blur;
	effects->corners = corners;
	memcpy(effects->color, color, sizeof(effects->color));
}

struct wsov_live_effects *
wsov_live_effects_create(struct wlr_scene_tree *parent, struct Client *client) {
	if (!parent || !client)
		return NULL;

	struct wsov_live_effects *effects = calloc(1, sizeof(*effects));
	if (!effects)
		return NULL;
	effects->client = client;

	if (config.shadows) {
		effects->shadow =
			wlr_scene_shadow_create(parent, 0, 0, config.border_radius,
									config.shadows_blur, config.shadowscolor);
		if (effects->shadow)
			wlr_scene_node_set_enabled(&effects->shadow->node, false);
	}
	if (config.blur && !client->no_blur) {
		effects->blur = wlr_scene_blur_create(parent, 0, 0);
		if (effects->blur)
			wlr_scene_node_set_enabled(&effects->blur->node, false);
	}
	effects->border =
		wlr_scene_rect_create(parent, 0, 0, get_border_color(client));
	if (effects->border)
		wlr_scene_node_set_enabled(&effects->border->node, false);
	return effects;
}

bool wsov_live_effects_has_border(struct wsov_live_effects *effects) {
	return effects && effects->border;
}

void wsov_live_effects_apply_surface(struct wsov_live_effects *effects,
									 struct wlr_scene_buffer *buffer,
									 double scale) {
	if (!effects || !buffer)
		return;
	wlr_scene_buffer_set_corner_radii(
		buffer, wsov_live_client_corners(effects->client, scale));
}

static void wsov_live_effects_update_impl(struct wsov_live_effects *effects,
										  const struct wlr_box *window_box,
										  const struct wlr_box *monitor_clip,
										  double scale) {
	if (!effects)
		return;
	if (effects->shadow)
		wlr_scene_node_set_enabled(&effects->shadow->node, false);
	if (effects->blur)
		wlr_scene_node_set_enabled(&effects->blur->node, false);
	if (effects->border)
		wlr_scene_node_set_enabled(&effects->border->node, false);
	if (!window_box || !monitor_clip || window_box->width <= 0 ||
		window_box->height <= 0 || monitor_clip->width <= 0 ||
		monitor_clip->height <= 0 || !isfinite(scale) || scale <= 0.0)
		return;

	Client *client = effects->client;
	struct fx_corner_radii corners = wsov_live_client_corners(client, scale);

	if (effects->shadow) {
		bool enabled = wsov_client_has_shadow(client);
		struct wlr_box shadow_node = {0};
		struct clipped_region clipped = clipped_region_get_default();
		if (enabled) {
			int spread = (int)lround(config.shadows_size * scale);
			struct wlr_box shadow_box = {
				.x = window_box->x +
					 (int)lround(config.shadows_position_x * scale) - spread,
				.y = window_box->y +
					 (int)lround(config.shadows_position_y * scale) - spread,
				.width = window_box->width + 2 * spread,
				.height = window_box->height + 2 * spread,
			};
			struct wlr_box window_clip = {0};
			if (wlr_box_intersection(&shadow_node, &shadow_box, monitor_clip) &&
				wlr_box_intersection(&window_clip, window_box, &shadow_node)) {
				clipped.area = (struct wlr_box){
					.x = window_clip.x - shadow_node.x,
					.y = window_clip.y - shadow_node.y,
					.width = window_clip.width,
					.height = window_clip.height,
				};
				clipped.corners = corners;
			} else {
				enabled = false;
			}
		}

		if (enabled) {
			wlr_scene_node_set_position(&effects->shadow->node, shadow_node.x,
										shadow_node.y);
			wlr_scene_shadow_set_size(effects->shadow, shadow_node.width,
									  shadow_node.height);
			wlr_scene_shadow_set_corner_radius(
				effects->shadow, (int)lround(config.border_radius * scale));
			wlr_scene_shadow_set_blur_sigma(effects->shadow,
											config.shadows_blur * (float)scale);
			wlr_scene_shadow_set_clipped_region(effects->shadow, clipped);
		}
		wlr_scene_node_set_enabled(&effects->shadow->node, enabled);
	}

	if (effects->blur) {
		bool enabled = config.blur && !client->no_blur;
		bool optimized = client_blur_uses_wallpaper(client);
		struct wlr_box blur_box = {
			.x = window_box->x + (int)lround(client->bw * scale),
			.y = window_box->y + (int)lround(client->bw * scale),
			.width = window_box->width - (int)lround(2 * client->bw * scale),
			.height = window_box->height - (int)lround(2 * client->bw * scale),
		};
		struct wlr_box visible = {0};
		if (blur_box.width <= 0 || blur_box.height <= 0 ||
			!wlr_box_intersection(&visible, &blur_box, monitor_clip)) {
			enabled = false;
		}

		if (enabled) {
			wlr_scene_node_set_position(&effects->blur->node, visible.x,
										visible.y);
			wlr_scene_blur_set_size(effects->blur, visible.width,
									visible.height);
			wlr_scene_blur_set_corner_radii(effects->blur, corners);
			wlr_scene_blur_set_should_only_blur_bottom_layer(effects->blur,
															 optimized);
			wlr_scene_blur_set_strength(effects->blur, 1.0f);
			wlr_scene_blur_set_alpha(effects->blur, 1.0f);
		}
		wlr_scene_node_set_enabled(&effects->blur->node, enabled);
	}

	if (effects->border) {
		bool enabled = client->bw > 0;
		struct wlr_box border_box = {0};
		struct wlr_box inner = {0};
		if (enabled &&
			wlr_box_intersection(&border_box, window_box, monitor_clip)) {
			int border_width = MANGO_MAX(1, (int)lround(client->bw * scale));
			struct wlr_box window_inner = {
				.x = window_box->x + border_width,
				.y = window_box->y + border_width,
				.width = window_box->width - 2 * border_width,
				.height = window_box->height - 2 * border_width,
			};
			if (window_inner.width <= 0 || window_inner.height <= 0 ||
				!wlr_box_intersection(&inner, &window_inner, &border_box)) {
				enabled = false;
			}
		} else {
			enabled = false;
		}

		if (enabled) {
			struct clipped_region clipped = {
				.area =
					{
						.x = inner.x - border_box.x,
						.y = inner.y - border_box.y,
						.width = inner.width,
						.height = inner.height,
					},
				.corners = corners,
			};
			wlr_scene_node_set_position(&effects->border->node, border_box.x,
										border_box.y);
			wlr_scene_rect_set_size(effects->border, border_box.width,
									border_box.height);
			wlr_scene_rect_set_corner_radii(effects->border, corners);
			wlr_scene_rect_set_clipped_region(effects->border, clipped);
			wlr_scene_rect_set_color(effects->border, get_border_color(client));
		}
		wlr_scene_node_set_enabled(&effects->border->node, enabled);
	}
}

void wsov_live_effects_update(struct wsov_live_effects *effects,
							  const struct wlr_box *window_box,
							  const struct wlr_box *monitor_clip,
							  double scale) {
	if (!effects) {
		wsov_live_effects_update_impl(NULL, window_box, monitor_clip, scale);
		return;
	}

	struct fx_corner_radii corners =
		wsov_live_client_corners(effects->client, scale);
	bool has_shadow = wsov_client_has_shadow(effects->client);
	bool has_blur = config.blur && !effects->client->no_blur;
	const float *color = get_border_color(effects->client);
	if (wsov_live_effects_layout_unchanged(effects, window_box, monitor_clip,
										   scale, corners, has_shadow, has_blur,
										   color))
		return;
	wsov_live_effects_update_impl(effects, window_box, monitor_clip, scale);
	wsov_live_effects_cache_layout(effects, window_box, monitor_clip, scale,
								   corners, has_shadow, has_blur, color);
}

void wsov_live_effects_destroy(struct wsov_live_effects *effects) {
	if (!effects)
		return;
	if (effects->shadow)
		wlr_scene_node_destroy(&effects->shadow->node);
	if (effects->blur)
		wlr_scene_node_destroy(&effects->blur->node);
	if (effects->border)
		wlr_scene_node_destroy(&effects->border->node);
	free(effects);
}

#else

struct wsov_live_effects *
wsov_live_effects_create(struct wlr_scene_tree *parent, struct Client *client) {
	(void)parent;
	(void)client;
	return NULL;
}

bool wsov_live_effects_has_border(struct wsov_live_effects *effects) {
	(void)effects;
	return false;
}

void wsov_live_effects_apply_surface(struct wsov_live_effects *effects,
									 struct wlr_scene_buffer *buffer,
									 double scale) {
	(void)effects;
	(void)buffer;
	(void)scale;
}

void wsov_live_effects_update(struct wsov_live_effects *effects,
							  const struct wlr_box *window_box,
							  const struct wlr_box *monitor_clip,
							  double scale) {
	(void)effects;
	(void)window_box;
	(void)monitor_clip;
	(void)scale;
}

void wsov_live_effects_destroy(struct wsov_live_effects *effects) {
	(void)effects;
}

#endif
