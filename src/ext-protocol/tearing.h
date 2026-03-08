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
	if (!allow_tearing) {
		return false;
	}

	Client *c = selmon->sel;

	/* tearing is only allowed for the output with the active client */
	if (!c || c->mon != m) {
		return false;
	}

	/* allow tearing for any window when requested or forced */
	if (allow_tearing == TEARING_ENABLED) {
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
		return c->tearing_hint || allow_tearing == TEARING_FULLSCREEN_ONLY;
	}

	/* honor tearing as requested by action */
	return c->force_tearing == STATE_ENABLED;
}

bool custom_wlr_scene_output_commit(struct wlr_scene_output *scene_output,
									struct wlr_output_state *state) {
	struct wlr_output *wlr_output = scene_output->output;
	Monitor *m = wlr_output->data;

	// 检查是否需要帧
	if (!wlr_scene_output_needs_frame(scene_output)) {
		wlr_log(WLR_DEBUG, "No frame needed for output %s", wlr_output->name);
		return true;
	}

	// 构建输出状态
	if (!wlr_scene_output_build_state(scene_output, state, NULL)) {
		wlr_log(WLR_ERROR, "Failed to build output state for %s",
				wlr_output->name);
		return false;
	}

	// 测试撕裂翻页
	if (state->tearing_page_flip) {
		if (!wlr_output_test_state(wlr_output, state)) {
			state->tearing_page_flip = false;
		}
	}

	// 尝试提交
	bool committed = wlr_output_commit_state(wlr_output, state);

	// 如果启用撕裂翻页但提交失败，重试禁用撕裂翻页
	if (!committed && state->tearing_page_flip) {
		wlr_log(WLR_DEBUG, "Retrying commit without tearing for %s",
				wlr_output->name);
		state->tearing_page_flip = false;
		committed = wlr_output_commit_state(wlr_output, state);
	}

	// 处理状态清理
	if (committed) {
		wlr_log(WLR_DEBUG, "Successfully committed output %s",
				wlr_output->name);
		if (state == &m->pending) {
			wlr_output_state_finish(&m->pending);
			wlr_output_state_init(&m->pending);
		}
	} else {
		wlr_log(WLR_ERROR, "Failed to commit output %s", wlr_output->name);
		// 即使提交失败，也清理状态避免积累
		if (state == &m->pending) {
			wlr_output_state_finish(&m->pending);
			wlr_output_state_init(&m->pending);
		}
		return false;
	}

	return true;
}

void apply_tear_state(Monitor *m) {
	if (wlr_scene_output_needs_frame(m->scene_output)) {
		wlr_output_state_init(&m->pending);
		if (wlr_scene_output_build_state(m->scene_output, &m->pending, NULL)) {
			struct wlr_output_state *pending = &m->pending;
			pending->tearing_page_flip = true;

			if (!custom_wlr_scene_output_commit(m->scene_output, pending)) {
				wlr_log(WLR_ERROR, "Failed to commit output %s",
						m->scene_output->output->name);
			}
		} else {
			wlr_log(WLR_ERROR, "Failed to build state for output %s",
					m->scene_output->output->name);
			wlr_output_state_finish(&m->pending);
		}
	}
}