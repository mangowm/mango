bool mango_scene_output_commit(struct wlr_scene_output *scene_output,
							   struct wlr_output_state *state) {
	struct wlr_output *wlr_output = scene_output->output;
	Monitor *m = wlr_output->data;
	bool committed = false;

	bool frame_allow_tearing = check_tearing_frame_allow(m);

	if (!wlr_scene_output_needs_frame(scene_output))
		return true;

	// build the state, attaching the scene's Buffer to it
	if (!wlr_scene_output_build_state(scene_output, state, NULL))
		return false;

	if (frame_allow_tearing) {
		state->tearing_page_flip = true;
	} else {
		state->tearing_page_flip = false;
	}

	// test whether tearing is supported
	if (state->tearing_page_flip == true) {
		if (!wlr_output_test_state(wlr_output, state)) {
			// if DRM rejects (e.g. the current output/driver doesn't support
			// tearing), fall back to disabling tearing
			state->tearing_page_flip = false;
		}
	}

	// commit state
	committed = wlr_output_commit_state(wlr_output, state);
	if (!committed && state->tearing_page_flip) {
		// retry once
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
		wlr_log(WLR_ERROR, "Failed to commit frame for output %s",
				m->wlr_output->name);
	}
	return committed;
}