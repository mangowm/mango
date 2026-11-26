#include "mango/workspace_overview/workspace_overview_internal.h"

bool workspace_overview_handle_key(uint32_t sym) {
	/* A bind in key_mode=workspace_overview replaces the built-in keys. */
	if (!workspace_overview_is_active() ||
		input_modal_has_binds(MODAL_BIND_KEY))
		return false;

	switch (sym) {
	case XKB_KEY_Escape:
		if (!wsov.closing)
			wsov_close(wsov.target_tag, NULL);
		return true;
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		if (!wsov.closing)
			wsov_close(wsov.target_tag, NULL);
		return true;
	case XKB_KEY_Left:
	case XKB_KEY_h:
		if (!wsov.closing)
			wsov_select_direction(-1, 0);
		return true;
	case XKB_KEY_Right:
	case XKB_KEY_l:
		if (!wsov.closing)
			wsov_select_direction(1, 0);
		return true;
	case XKB_KEY_Up:
	case XKB_KEY_k:
		if (!wsov.closing)
			wsov_select_direction(0, -1);
		return true;
	case XKB_KEY_Down:
	case XKB_KEY_j:
		if (!wsov.closing)
			wsov_select_direction(0, 1);
		return true;
	default:
		return false;
	}
}

void wsov_sloppy_focus_at(double x, double y) {
	if (!config.sloppyfocus || wsov.pressed || wsov.closing || !wsov.mon)
		return;

	struct wsov_cell *cell = wsov_hit_cell_at(x, y);
	struct wsov_item *item = wsov_item_at(cell, x, y);
	if (!item || item->is_snapshot || !wsov_client_eligible(item->client))
		return;

	Client *focus = item->client;
	Client *previous = wsov.mon->sel;
	if (focus == previous && focus->isfocusing)
		return;

	client_focus(focus, 0);
	wsov_render_selection_clients(previous, focus);
	wsov_update_styles();
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

void workspace_overview_handle_motion(double x, double y) {
	if (!workspace_overview_is_active() || wsov.closing)
		return;

	if (wsov_set_target_from_point(x, y)) {
		wsov_refresh_target_selection();
	}
	wsov_sloppy_focus_at(x, y);

	if (!wsov.pressed)
		return;

	double dx = x - wsov.drag_start_x;
	double dy = y - wsov.drag_start_y;
	int32_t drag_threshold = config.workspace_overview_drag_threshold;
	if (!wsov.moved && dx * dx + dy * dy >= drag_threshold * drag_threshold) {
		wsov.moved = true;
		if (!server.cursor_hidden)
			wlr_cursor_set_xcursor(server.cursor, server.cursor_manager,
								   "grabbing");
		if (wsov.has_drag_client) {
			struct wsov_cell *source = wsov_cell_for_tag(wsov.source_tag);
			if (source) {
				struct wsov_item *item = NULL;
				for (int i = 0; i < source->item_count; i++) {
					if (source->items[i].client == wsov.drag_client) {
						item = &source->items[i];
						break;
					}
				}
				if (item) {
					wsov_create_drag_buffer(item);
					wsov_begin_drag_floating(item->client);
					uint32_t tags = wsov_client_tags(item->client) |
									wsov_tag_bit(wsov.source_tag);
					wsov_refresh_workspace_tags(tags);
				}
			}
		}
	}

	if (!wsov.moved)
		return;

	if (wsov.drag_buffer) {
		int source_width = MANGO_MAX(1, wsov.drag_source_width);
		int source_height = MANGO_MAX(1, wsov.drag_source_height);
		int window_height =
			MANGO_MAX(1, wsov.drag_height - wsov.drag_bar_height);
		int grab_x = (int)lround((double)wsov.drag_grab_x * wsov.drag_width /
								 source_width);
		int grab_y = (int)lround((double)wsov.drag_grab_y * window_height /
								 source_height);
		wsov.drag_preview_x = (int)lround(x - wsov.mon->m.x) - grab_x;
		wsov.drag_preview_y =
			(int)lround(y - wsov.mon->m.y) - wsov.drag_bar_height - grab_y;
		wsov_place_buffer_clipped(wsov.drag_buffer, NULL, wsov.drag_preview_x,
								  wsov.drag_preview_y, wsov.drag_width,
								  wsov.drag_height);
	}
	wsov_update_drag_scroll(x, y);
	wsov_update_drop_indicator(x, y);
	wlr_output_schedule_frame(wsov.mon->wlr_output);
}

/* Close the window whose preview is under the pointer. */
static void wsov_kill_at_pointer(void) {
	struct wsov_cell *cell =
		wsov_hit_cell_at(server.cursor->x, server.cursor->y);
	struct wsov_item *item =
		wsov_item_at(cell, server.cursor->x, server.cursor->y);

	if (item && !item->is_snapshot && item->client)
		pending_kill_client(item->client);
}

/* Press on the workspace under the pointer: a release without motion
 * switches to it, a drag moves the window preview under the pointer. */
static void wsov_begin_press(uint32_t button,
							 WorkspaceOverviewDropMode drop_mode) {
	struct wsov_cell *cell =
		wsov_hit_cell_at(server.cursor->x, server.cursor->y);
	struct wsov_item *item =
		wsov_item_at(cell, server.cursor->x, server.cursor->y);

	wsov.press_button = button;
	wsov.drop_mode = drop_mode;
	wsov_hide_drop_indicator();
	wsov.pressed = true;
	wsov.moved = false;
	wsov.drag_scroll_direction = 0;
	wsov.drag_scroll_frame_valid = false;
	wsov.drag_start_x = (int)lround(server.cursor->x);
	wsov.drag_start_y = (int)lround(server.cursor->y);
	wsov.source_tag = cell ? cell->tag : 0;
	wsov.target_tag = cell ? cell->tag : 0;
	wsov.drag_return_tag = wsov.target_tag;
	wsov_refresh_target_selection();
	wsov.has_drag_client = item != NULL;
	wsov.drag_client = item ? item->client : NULL;
	if (item) {
		struct wlr_box item_box = wsov_item_display_box(cell, item);
		wsov.drag_grab_x =
			(int)lround(server.cursor->x - wsov.mon->m.x - item_box.x);
		wsov.drag_grab_y =
			(int)lround(server.cursor->y - wsov.mon->m.y - item_box.y);
	}
	wsov_update_styles();
}

bool workspace_overview_handle_button(
	const struct wlr_pointer_button_event *event) {
	bool ran;

	if (!workspace_overview_is_active())
		return false;
	if (wsov.closing) {
		if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
			server.cursor_mode = CurNormal;
		return true;
	}

	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		server.cursor_mode = CurPressed;
		/* A press started by a key ends on the next release. */
		if (wsov.pressed && !wsov.press_button)
			return true;

		/* Binds in key_mode=workspace_overview run first. Any mousebind
		 * there replaces the built-in buttons. */
		wsov.event_button = event->button;
		ran = pointer_run_mousebinds(event->button, true);
		wsov.event_button = 0;
		if (ran || input_modal_has_binds(MODAL_BIND_MOUSE))
			return true;

		if (event->button == BTN_RIGHT)
			wsov_kill_at_pointer();
		else if (event->button == BTN_LEFT)
			wsov_begin_press(BTN_LEFT, WORKSPACE_OVERVIEW_DROP_AUTO);
		return true;
	}

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		bool ends_press =
			wsov.pressed
				? !wsov.press_button || event->button == wsov.press_button
				: event->button == BTN_LEFT &&
					  !input_modal_has_binds(MODAL_BIND_MOUSE);

		if (!ends_press) {
			server.cursor_mode = CurNormal;
			return true;
		}
		if (wsov.moved && wsov.has_drag_client) {
			wsov_finish_drop(server.cursor->x, server.cursor->y);
		} else {
			struct wsov_cell *cell =
				wsov_hit_cell_at(server.cursor->x, server.cursor->y);
			uint32_t tag = cell ? cell->tag : wsov.source_tag;
			struct wsov_item *item =
				wsov_item_at(cell, server.cursor->x, server.cursor->y);
			Client *focus = item ? item->client : NULL;
			client_focus(focus, 1);
			wsov_close(tag, focus);
		}
		server.cursor_mode = CurNormal;
		return true;
	}
	return true;
}

bool workspace_overview_handle_axis(
	const struct wlr_pointer_axis_event *event) {
	if (!workspace_overview_is_active())
		return false;
	if (wsov.closing || !event)
		return true;
	/* An axisbind in key_mode=workspace_overview replaces the wheel. */
	if (wsov.layout != WORKSPACE_OVERVIEW_SCROLL ||
		input_modal_has_binds(MODAL_BIND_AXIS))
		return true;
	if (wsov.pressed && wsov.has_drag_client)
		return true;

	double notches = event->delta_discrete != 0 ? event->delta_discrete / 120.0
												: event->delta / 15.0;
	if (!isfinite(notches) || fabs(notches) < 0.001)
		return true;

	uint32_t now = event->time_msec;
	if (!now) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		now = (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
	}

	wsov_apply_scroll_wheel(notches, now);
	return true;
}

/* The dispatchers below do nothing while the workspace overview is closed. */
int32_t workspace_overview_close(const Arg *arg) {
	if (workspace_overview_is_active() && !wsov.closing)
		wsov_close(wsov.target_tag, NULL);
	return 0;
}

int32_t workspace_overview_select(const Arg *arg) {
	if (!workspace_overview_is_active() || wsov.closing)
		return 0;
	if (arg->i == LEFT)
		wsov_select_direction(-1, 0);
	else if (arg->i == RIGHT)
		wsov_select_direction(1, 0);
	else if (arg->i == UP)
		wsov_select_direction(0, -1);
	else if (arg->i == DOWN)
		wsov_select_direction(0, 1);
	return 0;
}

int32_t workspace_overview_kill(const Arg *arg) {
	if (workspace_overview_is_active() && !wsov.closing)
		wsov_kill_at_pointer();
	return 0;
}

/* From a mousebind, the release of that button ends the press. */
int32_t workspace_overview_drag(const Arg *arg) {
	if (workspace_overview_is_active() && !wsov.closing)
		wsov_begin_press(wsov.event_button, arg->i);
	return 0;
}
