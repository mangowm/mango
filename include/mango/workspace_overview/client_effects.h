#ifndef MANGO_WORKSPACE_OVERVIEW_CLIENT_EFFECTS_H
#define MANGO_WORKSPACE_OVERVIEW_CLIENT_EFFECTS_H

#include <stdbool.h>
#include <wlr/util/box.h>

#if defined(MANGO_SCENEFX)
#define MANGO_WORKSPACE_OVERVIEW_SCENEFX 1
#endif

struct Client;
struct wlr_scene_buffer;
struct wlr_scene_tree;
struct wsov_live_effects;

/*
 * Scenefx-specific effects for workspace-overview live client projections.
 * Keep this API limited to lifecycle and layout so the workspace overview can
 * build and position clients without depending on the individual effect nodes.
 */
struct wsov_live_effects *
wsov_live_effects_create(struct wlr_scene_tree *parent, struct Client *client);
bool wsov_live_effects_has_border(struct wsov_live_effects *effects);
void wsov_live_effects_apply_surface(struct wsov_live_effects *effects,
									 struct wlr_scene_buffer *buffer,
									 double scale);
void wsov_live_effects_update(struct wsov_live_effects *effects,
							  const struct wlr_box *window_box,
							  const struct wlr_box *monitor_clip, double scale);
void wsov_live_effects_destroy(struct wsov_live_effects *effects);

#endif
