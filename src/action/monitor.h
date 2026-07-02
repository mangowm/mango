bool mango_scene_output_commit(struct wlr_scene_output *scene_output,
							   struct wlr_output_state *state) {
	struct wlr_output *wlr_output = scene_output->output;
	Monitor *m = wlr_output->data;
	bool committed = false;

	bool frame_allow_tearing = check_tearing_frame_allow(m);

	if (!wlr_scene_output_needs_frame(scene_output))
		return true;

	// 构建状态，将场景的 Buffer 附着到 state 上
	if (!wlr_scene_output_build_state(scene_output, state, NULL))
		return false;

	if (frame_allow_tearing) {
		state->tearing_page_flip = true;
	} else {
		state->tearing_page_flip = false;
	}

	// 测试是否支持撕裂
	if (state->tearing_page_flip == true) {
		if (!wlr_output_test_state(wlr_output, state)) {
			// 如果 DRM 拒绝（例如当前输出/驱动不支持撕裂），降级关闭撕裂
			state->tearing_page_flip = false;
		}
	}

	// 提交状态
	committed = wlr_output_commit_state(wlr_output, state);
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
	} else {
		wlr_log(WLR_INFO, "Failed to commit output %s", m->wlr_output->name);
		return false;
	}

	return committed;
}

bool mango_output_commit(Monitor *m) {

	bool committed = wlr_output_commit_state(m->wlr_output, &m->pending);
	if (committed) {
		wlr_output_state_finish(&m->pending);
		wlr_output_state_init(&m->pending);
	} else {
		wlr_log(WLR_ERROR, "Failed to commit frame");
	}
	return committed;
}