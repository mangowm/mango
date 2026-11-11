#include "mango/manage/zoom.h"
#include "mango/animation/common.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/manage/monitor.h"
#include <math.h>
#include <pixman.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/box.h>
#include <wlr/util/transform.h>

#define ZOOM_OFF 1.0f

/*
 * One level for every output. `from` and `target` bound the step that started
 * at `started_at`; the level is a pure function of the time since then, so
 * every output that renders during a step sees the same magnification. Only
 * `zoom_active()` reads this on the hot path, and `zoom_settle()` collapses
 * `from` once a frame has seen the step land.
 */
static struct {
	float from;
	float target;
	uint32_t started_at;
} magnifier = {.from = ZOOM_OFF, .target = ZOOM_OFF};

static uint32_t zoom_duration(void) {
	return config.animations ? config.screen_zoom_duration : 0;
}

/* Unsigned subtraction keeps this right across get_now_in_ms() wrapping. */
static uint32_t zoom_elapsed(void) {
	return get_now_in_ms() - magnifier.started_at;
}

static float zoom_level(void) {
	uint32_t duration = zoom_duration();
	uint32_t elapsed = zoom_elapsed();
	if (elapsed >= duration)
		return magnifier.target;

	double progress = find_animation_curve_at((double)elapsed / duration, MOVE);
	float level =
		magnifier.from + (magnifier.target - magnifier.from) * (float)progress;
	/* An overshooting curve can carry a zoom-out past 1x, and the crop would
	 * then be larger than the frame: the magnifier never shows less than it. */
	return MANGO_MAX(level, ZOOM_OFF);
}

static bool screen_zoom_animating(void) {
	return zoom_elapsed() < zoom_duration();
}

/*
 * Whether the magnifier is doing anything: on, or still on its way back to 1x.
 * `target` alone misses the first frame of a zoom-in, which can arrive with the
 * level still at 1x; `from` only says something while its step runs, and every
 * pointer event asks this, so a step that has landed must not leave `from`
 * behind.
 */
static bool zoom_active(void) {
	return magnifier.target > ZOOM_OFF ||
		   (magnifier.from > ZOOM_OFF && screen_zoom_animating());
}

/* The step has landed: `from` has nothing left to say. */
static void zoom_settle(void) {
	if (!zoom_active())
		magnifier.from = magnifier.target;
}

/* The output the magnifier applies to. */
static Monitor *zoom_monitor(void) {
	Monitor *under = monitor_at_point(server.cursor->x, server.cursor->y);
	/* The pointer can rest outside every output; fall back to the selected
	 * one so the magnifier still has somewhere to draw. */
	return under ? under : server.selected_monitor;
}

/* Whether `m` is the output the magnifier draws. */
static bool zoom_draws_on(const Monitor *m) {
	return zoom_active() && zoom_monitor() == m;
}

/*
 * Queue a frame for the output under the pointer and for any output that still
 * shows a crop: the frame that leaves the magnifier needs one too. Moving the
 * pointer damages nothing in the scene, so each one is damaged whole or the
 * commit would have nothing to do.
 */
static void zoom_queue_frames(void) {
	Monitor *target = zoom_monitor();
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (!m->wlr_output->enabled)
			continue;
		if (m != target && !m->zoom_swapchain)
			continue;
		wlr_damage_ring_add_whole(&m->scene_output->damage_ring);
		wlr_output_schedule_frame(m->wlr_output);
	}
}

static void zoom_set_target(float new_target) {
	new_target =
		MANGO_MIN(MANGO_MAX(new_target, ZOOM_OFF), config.screen_zoom_max);
	if (new_target == magnifier.target)
		return;

	magnifier.from = zoom_level();
	magnifier.target = new_target;
	magnifier.started_at = get_now_in_ms();
	zoom_queue_frames();
}

/*
 * This frame cannot be magnified: drop to 1x at once so it is not retried, and
 * report it at `importance` because the expected case is not an error.
 */
static void zoom_drop(const char *output_name,
					  enum wlr_log_importance importance) {
	/* Already on its way out: the step needs no help. */
	if (magnifier.target == ZOOM_OFF)
		return;

	mango_error(true, importance, "cannot magnify %s, zooming out",
				output_name);
	magnifier.from = ZOOM_OFF;
	magnifier.target = ZOOM_OFF;
	/* Dated back by a whole step, so the drop itself does not animate. */
	magnifier.started_at = get_now_in_ms() - zoom_duration();
	/* The frame that shows a crop still has to make way for the scene's own. */
	zoom_queue_frames();
}

void screen_zoom_by(float delta) { zoom_set_target(magnifier.target + delta); }

void screen_zoom_off(void) { zoom_set_target(ZOOM_OFF); }

bool screen_zoom_needs_frame(const Monitor *m) {
	/*
	 * The magnified output needs a frame while a step runs, and until it has a
	 * buffer to draw into. Any other output needs one only to retire the crop
	 * it still holds, which is the frame that leaves the magnifier.
	 */
	if (zoom_draws_on(m))
		return screen_zoom_animating() || !m->zoom_swapchain;

	return m->zoom_swapchain != NULL;
}

void screen_zoom_refresh(void) {
	/* The hot path of every pointer event: no clock read unless it is on. */
	if (zoom_active())
		zoom_queue_frames();
}

void screen_zoom_reapply(void) { zoom_set_target(magnifier.target); }

void screen_zoom_release(Monitor *m) {
	if (!m->zoom_swapchain)
		return;

	wlr_swapchain_destroy(m->zoom_swapchain);
	m->zoom_swapchain = NULL;
	zoom_settle();
}

/*
 * The frame this commit is about to produce. A mode change carries its own
 * size and `output->width` only follows once the commit lands, so both ends of
 * the blit read the size from the state that is being committed.
 */
static void zoom_buffer_size(const Monitor *m,
							 const struct wlr_output_state *state, int *width,
							 int *height) {
	struct wlr_output *output = m->wlr_output;
	if (!(state->committed & WLR_OUTPUT_STATE_MODE)) {
		*width = output->width;
		*height = output->height;
		return;
	}

	if (state->mode) {
		*width = state->mode->width;
		*height = state->mode->height;
		return;
	}

	*width = state->custom_mode.width;
	*height = state->custom_mode.height;
}

/* The transform this commit applies, which decides where the cursor sits. */
static enum wl_output_transform
zoom_transform(const Monitor *m, const struct wlr_output_state *state) {
	if (state->committed & WLR_OUTPUT_STATE_TRANSFORM)
		return state->transform;

	return m->wlr_output->transform;
}

/* The same frame after the output transform: the space the cursor lives in. */
static void zoom_screen_size(const Monitor *m,
							 const struct wlr_output_state *state, int *width,
							 int *height) {
	int buffer_width = 0, buffer_height = 0;
	zoom_buffer_size(m, state, &buffer_width, &buffer_height);

	switch (zoom_transform(m, state)) {
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		*width = buffer_height;
		*height = buffer_width;
		return;
	default:
		*width = buffer_width;
		*height = buffer_height;
		return;
	}
}

/*
 * The part of the frame that stays visible, in buffer coordinates. `level` is
 * at least 1, so the crop never reaches past the frame; it can pass
 * `screen_zoom_max` mid-step when the curve overshoots.
 */
static void zoom_crop(const Monitor *m, const struct wlr_output_state *state,
					  float level, struct wlr_fbox *crop) {
	struct wlr_output *output = m->wlr_output;
	int buffer_width = 0, buffer_height = 0;
	zoom_buffer_size(m, state, &buffer_width, &buffer_height);

	/*
	 * The pointer starts in display space: output pixels after the output
	 * transform, the space wlroots places the cursor in. wlr_box_transform
	 * undoes the transform in that same space, exactly like
	 * wlr_output_add_software_cursors_to_render_pass, so the sizes it takes are
	 * the transformed resolution and not the buffer size.
	 */
	int screen_width = 0, screen_height = 0;
	zoom_screen_size(m, state, &screen_width, &screen_height);
	float scale = (state->committed & WLR_OUTPUT_STATE_SCALE) ? state->scale
															  : output->scale;

	struct wlr_box screen_point = {
		.x = (int32_t)roundf((server.cursor->x - m->m.x) * scale),
		.y = (int32_t)roundf((server.cursor->y - m->m.y) * scale),
	};
	struct wlr_box buffer_point;
	wlr_box_transform(&buffer_point, &screen_point,
					  wlr_output_transform_invert(zoom_transform(m, state)),
					  screen_width, screen_height);

	/*
	 * Shrinking the frame by `level` around the point keeps it fixed: it ends
	 * up at (point - origin) * level on screen. Everything below is buffer
	 * coordinates again, so cropping happens before the scanout transform. At
	 * 2x on a 1280x720 output the crop is 640x360 at (320, 180) when the
	 * pointer sits in the middle, and the point lands back at 640.
	 */
	double crop_width = buffer_width / level;
	double crop_height = buffer_height / level;
	double x = buffer_point.x * (1.0 - 1.0 / level);
	double y = buffer_point.y * (1.0 - 1.0 / level);

	*crop = (struct wlr_fbox){
		.x = MANGO_MIN(MANGO_MAX(x, 0.0), buffer_width - crop_width),
		.y = MANGO_MIN(MANGO_MAX(y, 0.0), buffer_height - crop_height),
		.width = crop_width,
		.height = crop_height,
	};
}

/*
 * Scale the crop of `source` up to the output and commit it in place. The crop
 * is read from the frame the scene just drew, so it needs a buffer of its own:
 * one pass cannot sample and overwrite the same target.
 */
static bool zoom_blit(Monitor *m, struct wlr_output_state *state,
					  struct wlr_texture *source, float level) {
	struct wlr_output *output = m->wlr_output;
	int width = 0, height = 0;
	zoom_buffer_size(m, state, &width, &height);

	if (!wlr_output_configure_primary_swapchain(output, state,
												&m->zoom_swapchain))
		return false;

	struct wlr_buffer *buffer = wlr_swapchain_acquire(m->zoom_swapchain);
	if (!buffer)
		return false;

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		output->renderer, buffer, &(struct wlr_buffer_pass_options){0});
	if (!pass) {
		wlr_buffer_unlock(buffer);
		return false;
	}

	struct wlr_fbox crop;
	zoom_crop(m, state, level, &crop);

	struct wlr_render_texture_options options = {
		.texture = source,
		.src_box = crop,
		.dst_box = {.width = width, .height = height},
		.filter_mode = WLR_SCALE_FILTER_BILINEAR,
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	};
	wlr_render_pass_add_texture(pass, &options);

	if (!wlr_render_pass_submit(pass)) { /* submit destroys the pass */
		wlr_buffer_unlock(buffer);
		return false;
	}

	/*
	 * This frame is a different buffer than the one the scene drew into, so
	 * its damage is the whole frame; the scene's damage ring only describes
	 * the scene's own buffer.
	 */
	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, 0, 0, width, height);
	wlr_output_state_set_damage(state, &damage);
	pixman_region32_fini(&damage);

	/* Replacing the buffer is the last step: nothing can fail after it. */
	wlr_output_state_set_buffer(state, buffer);
	wlr_buffer_unlock(buffer);

	return true;
}

/* The frame this commit carries, if it is one the magnifier can crop. */
static struct wlr_buffer *zoom_source(Monitor *m,
									  struct wlr_output_state *state) {
	struct wlr_output *output = m->wlr_output;
	struct wlr_buffer *source = state->buffer;

	/* A commit that carries no frame of its own has nothing to crop. */
	if (!source)
		return NULL;

	/*
	 * The scene's own frame is always the output's mode resolution, whatever
	 * its scale and transform. Another size is a client buffer on its way to
	 * scanout: it holds none of the scene and cannot be scaled to fit.
	 */
	int width = 0, height = 0;
	zoom_buffer_size(m, state, &width, &height);
	if (source->width != width || source->height != height) {
		/* Expected: mango handed a client buffer straight to the display. */
		zoom_drop(output->name, WLR_DEBUG);
		return NULL;
	}

	return source;
}

void screen_zoom_compose(Monitor *m, struct wlr_output_state *state) {
	/* The common case for every ordinary frame: off, and holding nothing. */
	if (!m->zoom_swapchain && !zoom_active())
		return;

	/* An output switched off shows nothing: keep no crop for it. */
	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
		screen_zoom_release(m);
		return;
	}

	/* Only the output the magnifier draws is cropped. Any other still holding a
	 * crop has just lost it, and the frame it is building is the scene's own:
	 * dropping our reference is enough, the backend keeps its own until it has
	 * finished with the buffer. */
	if (!zoom_draws_on(m)) {
		screen_zoom_release(m);
		return;
	}

	float level = zoom_level();
	/* A step can arrive before it has moved off 1x, or land here after a
	 * zoom-out: either way the next frame is the scene's own. */
	if (level <= ZOOM_OFF) {
		zoom_settle();
		wlr_output_schedule_frame(m->wlr_output);
		return;
	}

	struct wlr_output *output = m->wlr_output;
	struct wlr_buffer *source = zoom_source(m, state);
	if (!source) {
		/* A step still needs its next frame, which may carry one. */
		if (screen_zoom_animating())
			wlr_output_schedule_frame(output);
		return;
	}

	/*
	 * `source` is the frame the scene just built, so HDR and ICC transforms are
	 * already applied to it, and the destination buffer comes from the output's
	 * own swapchain configuration. Both ends of the blit keep their formats.
	 *
	 * Imported per frame on purpose: a cached texture keeps its buffer locked,
	 * which holds one of the WLR_SWAPCHAIN_CAP slots of the scene's swapchain
	 * and starves it until nothing can be committed any more. The buffer
	 * itself stays locked by the state for as long as we use it here.
	 */
	struct wlr_texture *texture =
		wlr_texture_from_buffer(output->renderer, source);
	bool magnified = texture && zoom_blit(m, state, texture, level);
	if (texture)
		wlr_texture_destroy(texture);

	if (!magnified) {
		zoom_drop(output->name, WLR_ERROR);
		return;
	}

	/* Tearing would put the frame on screen before the magnifier scaled it. */
	state->tearing_page_flip = false;

	/*
	 * While a step runs nothing in the scene damages anything, so every
	 * magnified frame asks for the next one.
	 */
	if (screen_zoom_animating())
		wlr_output_schedule_frame(output);
}
