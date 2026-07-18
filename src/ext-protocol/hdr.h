#include <drm_fourcc.h>
#include <glob.h>
#include <libdisplay-info/info.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static uint32_t output_formats_8bit[] = {
	DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888, DRM_FORMAT_BGRA8888, DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
};

static uint32_t output_formats_10bit[] = {
	DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_RGBX1010102,
	DRM_FORMAT_BGRX1010102, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_RGBA1010102, DRM_FORMAT_BGRA1010102,
};

static bool output_set_render_format(Monitor *m, uint32_t candidates[],
									 size_t count,
									 struct wlr_output_state *state) {
	for (size_t i = 0; i < count; i++) {
		wlr_output_state_set_render_format(state, candidates[i]);
		if (wlr_output_test_state(m->wlr_output, state))
			return true;
	}
	return false;
}

static bool output_format_in_candidates(uint32_t format, uint32_t candidates[],
										size_t count) {
	for (size_t i = 0; i < count; i++)
		if (candidates[i] == format)
			return true;
	return false;
}

static enum render_bit_depth bit_depth_from_format(uint32_t render_format) {
	if (output_format_in_candidates(render_format, output_formats_10bit,
									ARRAY_SIZE(output_formats_10bit)))
		return MANGO_RENDER_BIT_DEPTH_10;
	if (output_format_in_candidates(render_format, output_formats_8bit,
									ARRAY_SIZE(output_formats_8bit)))
		return MANGO_RENDER_BIT_DEPTH_8;
	return MANGO_RENDER_BIT_DEPTH_DEFAULT;
}

static bool output_supports_hdr(const struct wlr_output *output,
								const char **reason) {
	const char *r = NULL;
	if (!(output->supported_primaries & WLR_COLOR_NAMED_PRIMARIES_BT2020))
		r = "BT2020 primaries not supported";
	else if (!(output->supported_transfer_functions &
			   WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ))
		r = "PQ transfer function not supported";
	else if (!drw->features.output_color_transform)
		r = "renderer doesn't support output color transforms";
	if (reason)
		*reason = r;
	return !r;
}

/* hdr:2 — fill the image description's mastering data from the panel's
 * EDID so it tone-maps against its real limits instead of guessing. */
static bool output_fill_edid_hdr_caps(Monitor *m,
									  struct wlr_output_image_description *desc) {
	char pattern[128];
	snprintf(pattern, sizeof(pattern), "/sys/class/drm/card*-%s/edid",
			 m->wlr_output->name);
	glob_t g;
	if (glob(pattern, 0, NULL, &g) != 0 || g.gl_pathc == 0) {
		globfree(&g);
		return false;
	}
	FILE *f = fopen(g.gl_pathv[0], "rb");
	globfree(&g);
	if (!f)
		return false;
	uint8_t edid[4096];
	size_t len = fread(edid, 1, sizeof(edid), f);
	fclose(f);
	if (len < 128)
		return false;

	struct di_info *info = di_info_parse_edid(edid, len);
	if (!info)
		return false;

	bool filled = false;
	const struct di_hdr_static_metadata *hsm =
		di_info_get_hdr_static_metadata(info);
	if (hsm && hsm->desired_content_max_luminance > 0) {
		desc->mastering_luminance.min = hsm->desired_content_min_luminance;
		desc->mastering_luminance.max = hsm->desired_content_max_luminance;
		desc->max_cll = hsm->desired_content_max_luminance;
		desc->max_fall = hsm->desired_content_max_frame_avg_luminance > 0
							 ? hsm->desired_content_max_frame_avg_luminance
							 : hsm->desired_content_max_luminance;
		filled = true;
	}

	const struct di_color_primaries *cp =
		di_info_get_default_color_primaries(info);
	if (cp && cp->has_primaries) {
		desc->mastering_display_primaries = (struct wlr_color_primaries){
			.red = {cp->primary[0].x, cp->primary[0].y},
			.green = {cp->primary[1].x, cp->primary[1].y},
			.blue = {cp->primary[2].x, cp->primary[2].y},
			.white = {cp->default_white.x, cp->default_white.y},
		};
		filled = true;
	}
	di_info_destroy(info);
	return filled;
}

void output_enable_hdr(Monitor *m, struct wlr_output_state *os, bool enabled,
					   bool silent) {
	if (enabled && !output_supports_hdr(m->wlr_output, NULL))
		enabled = false;

	if (!enabled) {
		if (m->wlr_output->supported_primaries ||
			m->wlr_output->supported_transfer_functions) {
			if (!silent)
				wlr_log(WLR_DEBUG, "Disabling HDR on output %s",
						m->wlr_output->name);
			wlr_output_state_set_image_description(os, NULL);
		}
		return;
	}

	if (!silent)
		wlr_log(WLR_DEBUG, "Enabling HDR on output %s", m->wlr_output->name);
	struct wlr_output_image_description desc = {
		.primaries = WLR_COLOR_NAMED_PRIMARIES_BT2020,
		.transfer_function = WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ,
	};
	if (m->hdr_enable >= 2 && output_fill_edid_hdr_caps(m, &desc) && !silent)
		wlr_log(WLR_DEBUG,
				"HDR mastering data from EDID for %s: max %.0f avg %.0f min %.4f",
				m->wlr_output->name, desc.mastering_luminance.max,
				desc.max_fall, desc.mastering_luminance.min);
	wlr_output_state_set_image_description(os, &desc);
}

void output_state_setup_hdr(Monitor *m, bool silent,
							struct wlr_output_state *state) {
	uint32_t render_format = m->wlr_output->render_format;
	const char *unsupported_reason = NULL;
	bool hdr_supported =
		output_supports_hdr(m->wlr_output, &unsupported_reason);
	bool hdr_succeeded = false;

	enum render_bit_depth depth = config.hdr_depth;
	if (depth == MANGO_RENDER_BIT_DEPTH_DEFAULT)
		depth = bit_depth_from_format(render_format);

	if (!hdr_supported && depth == MANGO_RENDER_BIT_DEPTH_10) {
		if (!silent)
			wlr_log(WLR_INFO, "Cannot enable HDR on output %s: %s",
					m->wlr_output->name, unsupported_reason);
		depth = MANGO_RENDER_BIT_DEPTH_8;
	}

	if (depth == MANGO_RENDER_BIT_DEPTH_10 &&
		bit_depth_from_format(render_format) == depth) {
		hdr_succeeded = true; // 上次已经成功设置10位，直接复用
	} else if (depth == MANGO_RENDER_BIT_DEPTH_10) {
		hdr_succeeded = output_set_render_format(
			m, output_formats_10bit, ARRAY_SIZE(output_formats_10bit), state);
		if (!hdr_succeeded) {
			if (!silent)
				wlr_log(WLR_INFO,
						"No 10 bit color formats supported, HDR disabled.");
			hdr_succeeded = output_set_render_format(
				m, output_formats_8bit, ARRAY_SIZE(output_formats_8bit), state);
			if (!hdr_succeeded) {
				if (!silent)
					wlr_log(WLR_ERROR, "No 8 bit color formats supported!");
			}
		}
	} else {
		// 明确要求8位或自动降级
		hdr_succeeded = output_set_render_format(
			m, output_formats_8bit, ARRAY_SIZE(output_formats_8bit), state);
		if (!hdr_succeeded) {
			if (!silent)
				wlr_log(WLR_ERROR, "No 8 bit color formats supported!");
		}
	}

	output_enable_hdr(m, state, hdr_succeeded, silent);
}