#include <wlr/types/wlr_tearing_control_v1.h>

struct tearing_controller {
	struct wlr_tearing_control_v1 *tearing_control;
	struct wl_listener set_hint;
	struct wl_listener destroy;
};

struct wlr_tearing_control_manager_v1 *tearing_control;
struct wl_listener tearing_new_object;

static void handle_controller_set_hint(struct wl_listener *listener,
									   void *data) {
	struct tearing_controller *controller =
		wl_container_of(listener, controller, set_hint);
	Client *c = NULL;

	toplevel_from_wlr_surface(controller->tearing_control->surface, &c, NULL);

	if (c) {
		/*
		 * tearing_control->current is actually an enum:
		 * WP_TEARING_CONTROL_V1_PRESENTATION_HINT_VSYNC = 0
		 * WP_TEARING_CONTROL_V1_PRESENTATION_HINT_ASYNC = 1
		 *
		 * Using it as a bool here allows us to not ship the XML.
		 */
		c->tearing_hint = controller->tearing_control->current;
	}
}

static void handle_controller_destroy(struct wl_listener *listener,
									  void *data) {
	struct tearing_controller *controller =
		wl_container_of(listener, controller, destroy);
	wl_list_remove(&controller->set_hint.link);
	wl_list_remove(&controller->destroy.link);
	free(controller);
}

void handle_tearing_new_object(struct wl_listener *listener, void *data) {
	struct wlr_tearing_control_v1 *new_tearing_control = data;

	enum wp_tearing_control_v1_presentation_hint hint =
		wlr_tearing_control_manager_v1_surface_hint_from_surface(
			tearing_control, new_tearing_control->surface);
	wlr_log(WLR_DEBUG, "New presentation hint %d received for surface %p", hint,
			new_tearing_control->surface);

	struct tearing_controller *controller =
		ecalloc(1, sizeof(struct tearing_controller));
	controller->tearing_control = new_tearing_control;

	controller->set_hint.notify = handle_controller_set_hint;
	wl_signal_add(&new_tearing_control->events.set_hint, &controller->set_hint);

	controller->destroy.notify = handle_controller_destroy;
	wl_signal_add(&new_tearing_control->events.destroy, &controller->destroy);
}

bool check_tearing_frame_allow(Monitor *m) {
	/* never allow tearing when disabled */
	if (!config.allow_tearing) {
		return false;
	}

	Client *c = selmon->sel;

	/* tearing is only allowed for the output with the active client */
	if (!c || c->mon != m) {
		return false;
	}

	/* allow tearing for any window when requested or forced */
	if (config.allow_tearing == TEARING_ENABLED) {
		if (c->force_tearing == STATE_UNSPECIFIED) {
			return c->tearing_hint;
		} else {
			return c->force_tearing == STATE_ENABLED;
		}
	}

	/* remaining tearing options apply only to full-screen windows */
	if (!c->isfullscreen) {
		return false;
	}

	if (c->force_tearing == STATE_UNSPECIFIED) {
		/* honor the tearing hint or the fullscreen-force preference */
		return c->tearing_hint ||
			   config.allow_tearing == TEARING_FULLSCREEN_ONLY;
	}

	/* honor tearing as requested by action */
	return c->force_tearing == STATE_ENABLED;
}

bool custom_wlr_scene_output_commit(struct wlr_scene_output *scene_output,
									struct wlr_output_state *state) {
	struct wlr_output *wlr_output = scene_output->output;
	Monitor *m = wlr_output->data;

	if (!wlr_scene_output_needs_frame(scene_output))
		return true;

	// 构建状态，将场景的 Buffer 附着到 state 上
	if (!wlr_scene_output_build_state(scene_output, state, NULL))
		return false;

	// 测试是否支持撕裂
	if (!wlr_output_test_state(wlr_output, state)) {
		// 如果 DRM 拒绝（例如当前输出/驱动不支持撕裂），降级关闭撕裂
		state->tearing_page_flip = false;
	}

	// 提交状态
	bool committed = wlr_output_commit_state(wlr_output, state);
	if (!committed && state->tearing_page_flip) {
		// 重试一次
		state->tearing_page_flip = false;
		committed = wlr_output_commit_state(wlr_output, state);
	}

	if (committed) {
		if (state == &m->pending) {
			wlr_output_state_finish(&m->pending);
			wlr_output_state_init(&m->pending);
		}
	}
	return committed;
}

void apply_tear_state(Monitor *m) {
	if (!wlr_scene_output_needs_frame(m->scene_output))
		return;

	wlr_output_state_init(&m->pending);
	m->pending.tearing_page_flip = true;
	if (!custom_wlr_scene_output_commit(m->scene_output, &m->pending)) {
		wlr_log(WLR_ERROR, "Failed to commit output %s",
				m->scene_output->output->name);
	}
}