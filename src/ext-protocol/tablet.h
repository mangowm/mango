#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/util/log.h>

static const int tabletmaptosurface =
	0; /* map tablet input to surface(1) or monitor(0) */

static void createtablet(struct wlr_input_device *device);
static void destroytablet(struct wl_listener *listener, void *data);
static void destroytabletsurfacenotify(struct wl_listener *listener,
									   void *data);
static void destroytablettool(struct wl_listener *listener, void *data);

static void tablettoolmotion(struct wlr_tablet_v2_tablet_tool *tool,
							 bool change_x, bool change_y, double x, double y,
							 double dx, double dy);
static void tablettoolproximity(struct wl_listener *listener, void *data);
static void tablettoolaxis(struct wl_listener *listener, void *data);
static void tablettoolbutton(struct wl_listener *listener, void *data);
static void tablettooltip(struct wl_listener *listener, void *data);
static struct wlr_tablet_manager_v2 *tablet_mgr;
static struct wlr_tablet_v2_tablet *tablet = NULL;
static struct wlr_tablet_v2_tablet_tool *tablet_tool = NULL;
static struct wlr_tablet_v2_tablet_pad *tablet_pad = NULL;
static struct wlr_surface *tablet_curr_surface = NULL;
static struct wl_listener destroy_tablet_surface_listener = {
	.notify = destroytabletsurfacenotify};
static struct wl_listener tablet_device_destroy = {.notify = destroytablet};
static struct wl_listener tablet_tool_axis = {.notify = tablettoolaxis};
static struct wl_listener tablet_tool_button = {.notify = tablettoolbutton};
static struct wl_listener tablet_tool_destroy = {.notify = destroytablettool};
static struct wl_listener tablet_tool_proximity = {.notify =
													   tablettoolproximity};
static struct wl_listener tablet_tool_tip = {.notify = tablettooltip};
static Monitor *find_monitor_by_name(const char *output_name);

void createtablet(struct wlr_input_device *device) {
	if (!tablet) {
		struct libinput_device *device_handle = NULL;
		if (!wlr_input_device_is_libinput(device) ||
			!(device_handle = wlr_libinput_get_device_handle(device)))
			return;

		tablet = wlr_tablet_create(tablet_mgr, seat, device);
		wl_signal_add(&tablet->wlr_device->events.destroy,
					  &tablet_device_destroy);
		if (libinput_device_config_send_events_get_modes(device_handle)) {
			libinput_device_config_send_events_set_mode(device_handle,
														send_events_mode);
			wlr_cursor_attach_input_device(cursor, device);
			// Map tablet to specific monitor if configured
			if (config.tablet_output_name) {
				Monitor *target_monitor = find_monitor_by_name(config.tablet_output_name);
				if (target_monitor) {
					wlr_log(WLR_INFO, "Mapping input to output for device: %s", config.tablet_output_name);
					wlr_cursor_map_input_to_output(cursor, device, 
																				 target_monitor->wlr_output);
				} else {
					wlr_log(WLR_WARN, "No monitor found with name: %s", config.tablet_output_name);
				}
			}
		}
	} else if (device == tablet->wlr_device) {
		wlr_log(WLR_ERROR, "createtablet: duplicate device");
	} else {
		wlr_log(WLR_ERROR, "createtablet: already have one tablet");
	}
}

void destroytablet(struct wl_listener *listener, void *data) {
	wl_list_remove(&listener->link);
	tablet = NULL;
}

void destroytabletsurfacenotify(struct wl_listener *listener, void *data) {
	if (tablet_curr_surface)
		wl_list_remove(&destroy_tablet_surface_listener.link);
	tablet_curr_surface = NULL;
}

void destroytablettool(struct wl_listener *listener, void *data) {
	destroytabletsurfacenotify(NULL, NULL);
	tablet_tool = NULL;
}

void tabletapplymap(double tablet_width, double tablet_height,
					struct wlr_fbox box, double *x, double *y) {
	if ((!box.x && !box.y && !box.width && !box.height) || !tablet_width ||
		!tablet_height) {
		return;
	}

	if (!box.width) {
		box.width = tablet_width - box.x;
	}
	if (!box.height) {
		box.height = tablet_height - box.y;
	}

	if (box.x + box.width <= tablet_width) {
		const double max_x = 1;
		double width_offset = max_x * box.x / tablet_width;
		*x = (*x - width_offset) * tablet_width / box.width;
	}
	if (box.y + box.height <= tablet_height) {
		const double max_y = 1;
		double height_offset = max_y * box.y / tablet_height;
		*y = (*y - height_offset) * tablet_height / box.height;
	}
}

void tablettoolmotion(struct wlr_tablet_v2_tablet_tool *tool, bool change_x,
					  bool change_y, double x, double y, double dx, double dy) {
	struct wlr_surface *surface = NULL;
	double sx, sy;

	if (!change_x && !change_y)
		return;

	tabletapplymap(tablet->wlr_tablet->width_mm, tablet->wlr_tablet->height_mm,
				   (struct wlr_fbox){0}, &x, &y);

	// TODO: apply constraints
	switch (tablet_tool->wlr_tool->type) {
	case WLR_TABLET_TOOL_TYPE_LENS:
	case WLR_TABLET_TOOL_TYPE_MOUSE:
		wlr_cursor_move(cursor, tablet->wlr_device, dx, dy);
		break;
	default:
		wlr_cursor_warp_absolute(cursor, tablet->wlr_device, change_x ? x : NAN,
								 change_y ? y : NAN);
		break;
	}

	motionnotify(0, NULL, 0, 0, 0, 0);

	xytonode(cursor->x, cursor->y, &surface, NULL, NULL, &sx, &sy);
	if (surface && !wlr_surface_accepts_tablet_v2(surface, tablet))
		surface = NULL;

	if (surface != tablet_curr_surface) {
		if (tablet_curr_surface) {
			// TODO: wait until all buttons released before leaving
			if (tablet_tool)
				wlr_tablet_v2_tablet_tool_notify_proximity_out(tablet_tool);
			if (tablet_pad)
				wlr_tablet_v2_tablet_pad_notify_leave(tablet_pad,
													  tablet_curr_surface);
			wl_list_remove(&destroy_tablet_surface_listener.link);
		}
		if (surface) {
			if (tablet_pad)
				wlr_tablet_v2_tablet_pad_notify_enter(tablet_pad, tablet,
													  surface);
			if (tablet_tool)
				wlr_tablet_v2_tablet_tool_notify_proximity_in(tablet_tool,
															  tablet, surface);
			wl_signal_add(&surface->events.destroy,
						  &destroy_tablet_surface_listener);
		}
		tablet_curr_surface = surface;
	}

	if (surface)
		wlr_tablet_v2_tablet_tool_notify_motion(tablet_tool, sx, sy);
}

void tablettoolproximity(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_proximity_event *event = data;
	struct wlr_tablet_tool *tool = event->tool;

	if (!tablet_tool) {
		tablet_tool = wlr_tablet_tool_create(tablet_mgr, seat, tool);
		wl_signal_add(&tablet_tool->wlr_tool->events.destroy,
					  &tablet_tool_destroy);
		wl_signal_add(&tablet_tool->events.set_cursor, &request_cursor);
	}

	switch (event->state) {
	case WLR_TABLET_TOOL_PROXIMITY_OUT:
		wlr_tablet_v2_tablet_tool_notify_proximity_out(tablet_tool);
		destroytabletsurfacenotify(NULL, NULL);
		break;
	case WLR_TABLET_TOOL_PROXIMITY_IN:
		tablettoolmotion(tablet_tool, true, true, event->x, event->y, 0, 0);
		break;
	}
}

void tablettoolaxis(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_axis_event *event = data;

	tablettoolmotion(tablet_tool, event->updated_axes & WLR_TABLET_TOOL_AXIS_X,
					 event->updated_axes & WLR_TABLET_TOOL_AXIS_Y, event->x,
					 event->y, event->dx, event->dy);

	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE)
		wlr_tablet_v2_tablet_tool_notify_pressure(tablet_tool, event->pressure);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE)
		wlr_tablet_v2_tablet_tool_notify_distance(tablet_tool, event->distance);
	if (event->updated_axes &
		(WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
		printf("DEBUGGING: In axis event handling\n");
		wlr_tablet_v2_tablet_tool_notify_tilt(tablet_tool, event->tilt_x,
											  event->tilt_y);
	}
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION)
		wlr_tablet_v2_tablet_tool_notify_rotation(tablet_tool, event->rotation);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER)
		wlr_tablet_v2_tablet_tool_notify_slider(tablet_tool, event->slider);
	if (event->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL)
		wlr_tablet_v2_tablet_tool_notify_wheel(tablet_tool, event->wheel_delta,
											   0);
}

void tablettoolbutton(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_button_event *event = data;
	wlr_tablet_v2_tablet_tool_notify_button(
		tablet_tool, event->button,
		(enum zwp_tablet_pad_v2_button_state)event->state);
}

void tablettooltip(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_tip_event *event = data;

	if (!tablet_curr_surface) {
		struct wlr_pointer_button_event fakeptrbtnevent = {
			.button = BTN_LEFT,
			.state = event->state == WLR_TABLET_TOOL_TIP_UP
						 ? WL_POINTER_BUTTON_STATE_RELEASED
						 : WL_POINTER_BUTTON_STATE_PRESSED,
			.time_msec = event->time_msec,
		};
		buttonpress(NULL, (void *)&fakeptrbtnevent);
	}

	if (event->state == WLR_TABLET_TOOL_TIP_UP) {
		wlr_tablet_v2_tablet_tool_notify_up(tablet_tool);
		return;
	}

	wlr_tablet_v2_tablet_tool_notify_down(tablet_tool);
	wlr_tablet_tool_v2_start_implicit_grab(tablet_tool);
}
