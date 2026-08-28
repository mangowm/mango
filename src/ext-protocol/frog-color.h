#ifndef FROG_COLOR_H
#define FROG_COLOR_H

#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include "frog-color-management-v1-protocol.h"
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_compositor.h>

struct frog_color_surface {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wl_listener surface_destroy;
	struct wl_listener surface_commit;
	struct wl_list link;

	enum frog_color_managed_surface_transfer_function transfer_function;
	enum frog_color_managed_surface_primaries primaries;
	enum frog_color_managed_surface_render_intent render_intent;

	bool has_hdr_metadata;
	uint32_t red_x, red_y;
	uint32_t green_x, green_y;
	uint32_t blue_x, blue_y;
	uint32_t white_x, white_y;
	uint32_t max_luminance;
	uint32_t min_luminance;
	uint32_t max_cll;
	uint32_t max_fall;
};

struct frog_color_manager_global {
	struct wl_global *global;
	struct wl_listener display_destroy;
	struct wl_list surfaces; // struct frog_color_surface.link
};

static struct frog_color_manager_global frog_cm_global = {
	.global = NULL,
};

static void frog_color_surface_send_metadata(struct frog_color_surface *fcs, Monitor *m) {
	if (!fcs || !fcs->resource)
		return;

	if (!m)
		m = selmon;

	bool is_hdr = m && (m->hdr_enable || m->is_hdr_enabling);

	uint32_t tf;
	uint32_t red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y;
	uint32_t max_lum, min_lum, max_full_frame;

	if (is_hdr) {
		tf = FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_ST2084_PQ;

		/* Rec.2020 Color Primaries (x, y * 50000) */
		red_x = 34000;   /* 0.680 */
		red_y = 16000;   /* 0.320 */
		green_x = 13250; /* 0.265 */
		green_y = 37250; /* 0.745 */
		blue_x = 7500;   /* 0.150 */
		blue_y = 3000;   /* 0.060 */
		white_x = 15635; /* 0.3127 (D65) */
		white_y = 16450; /* 0.3290 (D65) */

		max_lum = (m->hdr_max_lum > 0) ? (uint32_t)m->hdr_max_lum : 1000;
		min_lum = (m->hdr_min_lum > 0) ? (uint32_t)(m->hdr_min_lum * 10000.0f) : 1; /* 1 = 0.0001 cd/m2 */
		max_full_frame = (m->hdr_max_avg_lum > 0) ? (uint32_t)m->hdr_max_avg_lum : 250;
	} else {
		tf = FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_SRGB;

		/* Rec.709 / sRGB Color Primaries (x, y * 50000) */
		red_x = 32000;   /* 0.640 */
		red_y = 16500;   /* 0.330 */
		green_x = 15000; /* 0.300 */
		green_y = 30000; /* 0.600 */
		blue_x = 7500;   /* 0.150 */
		blue_y = 3000;   /* 0.060 */
		white_x = 15635; /* 0.3127 (D65) */
		white_y = 16450; /* 0.3290 (D65) */

		max_lum = 203;   /* ITU-R BT.2408 SDR reference */
		min_lum = 1;
		max_full_frame = 203;
	}

	frog_color_managed_surface_send_preferred_metadata(
		fcs->resource, tf,
		red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y,
		max_lum, min_lum, max_full_frame);

	mango_error(true, WLR_DEBUG,
		"frog-color: Sent preferred_metadata to surface %p: HDR=%d, TF=%u, MaxLum=%u, MinLum=%u, MaxAvg=%u",
		fcs->surface, is_hdr, tf, max_lum, min_lum, max_full_frame);
}

static void frog_color_update_all_surfaces(Monitor *m) {
	if (!frog_cm_global.global)
		return;

	struct frog_color_surface *fcs;
	wl_list_for_each(fcs, &frog_cm_global.surfaces, link) {
		frog_color_surface_send_metadata(fcs, m);
	}
}

static void frog_color_surface_destroy(struct frog_color_surface *fcs) {
	if (!fcs)
		return;

	wl_list_remove(&fcs->surface_destroy.link);
	wl_list_remove(&fcs->surface_commit.link);
	wl_list_remove(&fcs->link);
	free(fcs);
}

static void frog_color_surface_handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct frog_color_surface *fcs = wl_container_of(listener, fcs, surface_destroy);
	fcs->surface = NULL;
	if (fcs->resource) {
		wl_resource_set_user_data(fcs->resource, NULL);
	}
	frog_color_surface_destroy(fcs);
}

static void frog_color_surface_handle_surface_commit(struct wl_listener *listener, void *data) {
	struct frog_color_surface *fcs = wl_container_of(listener, fcs, surface_commit);
	/* Surface committed with new frame data */
	(void)fcs;
}

static void frog_color_managed_surface_protocol_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void frog_color_managed_surface_protocol_set_known_transfer_function(
		struct wl_client *client, struct wl_resource *resource, uint32_t transfer_function) {
	struct frog_color_surface *fcs = wl_resource_get_user_data(resource);
	if (!fcs)
		return;

	fcs->transfer_function = transfer_function;
	mango_error(true, WLR_DEBUG, "frog-color: surface %p set transfer_function = %u", fcs->surface, transfer_function);
}

static void frog_color_managed_surface_protocol_set_known_container_color_volume(
		struct wl_client *client, struct wl_resource *resource, uint32_t primaries) {
	struct frog_color_surface *fcs = wl_resource_get_user_data(resource);
	if (!fcs)
		return;

	fcs->primaries = primaries;
	mango_error(true, WLR_DEBUG, "frog-color: surface %p set container_color_volume = %u", fcs->surface, primaries);
}

static void frog_color_managed_surface_protocol_set_render_intent(
		struct wl_client *client, struct wl_resource *resource, uint32_t render_intent) {
	struct frog_color_surface *fcs = wl_resource_get_user_data(resource);
	if (!fcs)
		return;

	fcs->render_intent = render_intent;
}

static void frog_color_managed_surface_protocol_set_hdr_metadata(
		struct wl_client *client, struct wl_resource *resource,
		uint32_t mastering_display_primary_red_x, uint32_t mastering_display_primary_red_y,
		uint32_t mastering_display_primary_green_x, uint32_t mastering_display_primary_green_y,
		uint32_t mastering_display_primary_blue_x, uint32_t mastering_display_primary_blue_y,
		uint32_t mastering_white_point_x, uint32_t mastering_white_point_y,
		uint32_t max_display_mastering_luminance, uint32_t min_display_mastering_luminance,
		uint32_t max_cll, uint32_t max_fall) {
	struct frog_color_surface *fcs = wl_resource_get_user_data(resource);
	if (!fcs)
		return;

	fcs->has_hdr_metadata = true;
	fcs->red_x = mastering_display_primary_red_x;
	fcs->red_y = mastering_display_primary_red_y;
	fcs->green_x = mastering_display_primary_green_x;
	fcs->green_y = mastering_display_primary_green_y;
	fcs->blue_x = mastering_display_primary_blue_x;
	fcs->blue_y = mastering_display_primary_blue_y;
	fcs->white_x = mastering_white_point_x;
	fcs->white_y = mastering_white_point_y;
	fcs->max_luminance = max_display_mastering_luminance;
	fcs->min_luminance = min_display_mastering_luminance;
	fcs->max_cll = max_cll;
	fcs->max_fall = max_fall;

	mango_error(true, WLR_DEBUG,
		"frog-color: surface %p set HDR metadata: MaxLum=%u, MinLum=%u, MaxCLL=%u, MaxFALL=%u",
		fcs->surface, max_display_mastering_luminance, min_display_mastering_luminance, max_cll, max_fall);
}

static const struct frog_color_managed_surface_interface frog_color_managed_surface_impl = {
	.destroy = frog_color_managed_surface_protocol_destroy,
	.set_known_transfer_function = frog_color_managed_surface_protocol_set_known_transfer_function,
	.set_known_container_color_volume = frog_color_managed_surface_protocol_set_known_container_color_volume,
	.set_render_intent = frog_color_managed_surface_protocol_set_render_intent,
	.set_hdr_metadata = frog_color_managed_surface_protocol_set_hdr_metadata,
};

static void frog_color_managed_surface_resource_destroy(struct wl_resource *resource) {
	struct frog_color_surface *fcs = wl_resource_get_user_data(resource);
	if (fcs) {
		fcs->resource = NULL;
		frog_color_surface_destroy(fcs);
	}
}

static void frog_color_management_factory_protocol_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void frog_color_management_factory_protocol_get_color_managed_surface(
		struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *surface_resource, uint32_t id) {
	struct wlr_surface *wlr_surf = wlr_surface_from_resource(surface_resource);
	if (!wlr_surf) {
		wl_resource_post_error(resource, 0, "Invalid wlr_surface");
		return;
	}

	uint32_t version = wl_resource_get_version(resource);
	struct wl_resource *surf_resource = wl_resource_create(
		client, &frog_color_managed_surface_interface, version, id);
	if (!surf_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	struct frog_color_surface *fcs = calloc(1, sizeof(struct frog_color_surface));
	if (!fcs) {
		wl_resource_destroy(surf_resource);
		wl_client_post_no_memory(client);
		return;
	}

	fcs->resource = surf_resource;
	fcs->surface = wlr_surf;
	fcs->transfer_function = FROG_COLOR_MANAGED_SURFACE_TRANSFER_FUNCTION_UNDEFINED;
	fcs->primaries = FROG_COLOR_MANAGED_SURFACE_PRIMARIES_UNDEFINED;
	fcs->render_intent = FROG_COLOR_MANAGED_SURFACE_RENDER_INTENT_PERCEPTUAL;

	fcs->surface_destroy.notify = frog_color_surface_handle_surface_destroy;
	wl_signal_add(&wlr_surf->events.destroy, &fcs->surface_destroy);

	fcs->surface_commit.notify = frog_color_surface_handle_surface_commit;
	wl_signal_add(&wlr_surf->events.commit, &fcs->surface_commit);

	wl_list_insert(&frog_cm_global.surfaces, &fcs->link);
	wl_resource_set_implementation(surf_resource, &frog_color_managed_surface_impl, fcs,
		frog_color_managed_surface_resource_destroy);

	mango_error(true, WLR_INFO, "frog-color: Created color-managed surface for %p", wlr_surf);

	/* Determine target monitor for this surface */
	Client *c = NULL;
	toplevel_from_wlr_surface(wlr_surf, &c, NULL);
	Monitor *m = c ? c->mon : selmon;

	/* Send initial preferred metadata to the client */
	frog_color_surface_send_metadata(fcs, m);
}

static const struct frog_color_management_factory_v1_interface frog_color_factory_impl = {
	.destroy = frog_color_management_factory_protocol_destroy,
	.get_color_managed_surface = frog_color_management_factory_protocol_get_color_managed_surface,
};

static void frog_color_management_factory_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(
		client, &frog_color_management_factory_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &frog_color_factory_impl, NULL, NULL);
	mango_error(true, WLR_DEBUG, "frog-color: Client bound frog_color_management_factory_v1 version %u", version);
}

static void frog_color_manager_handle_display_destroy(struct wl_listener *listener, void *data) {
	if (frog_cm_global.global) {
		wl_global_destroy(frog_cm_global.global);
		frog_cm_global.global = NULL;
	}
}

void init_frog_color_management(struct wl_display *dpy) {
	if (frog_cm_global.global)
		return;

	wl_list_init(&frog_cm_global.surfaces);

	frog_cm_global.global = wl_global_create(
		dpy, &frog_color_management_factory_v1_interface, 1, NULL, frog_color_management_factory_bind);

	if (!frog_cm_global.global) {
		mango_error(true, WLR_ERROR, "frog-color: Failed to create global frog_color_management_factory_v1");
		return;
	}

	frog_cm_global.display_destroy.notify = frog_color_manager_handle_display_destroy;
	wl_display_add_destroy_listener(dpy, &frog_cm_global.display_destroy);

	mango_error(true, WLR_INFO, "frog-color: Successfully initialized frog_color_management_v1 global");
}

#endif /* FROG_COLOR_H */
