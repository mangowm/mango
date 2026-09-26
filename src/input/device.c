#include "mango/input/device.h"
#include "mango/common/server.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/input/switch.h"
#include "mango/input/tablet.h"
#include "mango/input/touch.h"
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_touch.h>

void handle_input_device_destroy(struct wl_listener *listener, void *data) {
	InputDevice *input_dev =
		wl_container_of(listener, input_dev, destroy_listener);

	if (input_dev->device_data) {
		switch (input_dev->wlr_device->type) {
		case WLR_INPUT_DEVICE_SWITCH: {
			Switch *sw = (Switch *)input_dev->device_data;
			wl_list_remove(&sw->toggle.link);
			free(sw);
			break;
		}
		default:
			break;
		}
		input_dev->device_data = NULL;
	}

	if (input_dev->wlr_device->type == WLR_INPUT_DEVICE_KEYBOARD)
		wl_list_remove(&input_dev->key_watch.link);
	wl_list_remove(&input_dev->link);
	wl_list_remove(&input_dev->destroy_listener.link);
	free(input_dev);
}

typedef struct SeatDevice {
	struct wl_list link;
	struct wlr_input_device *device;
	struct wl_listener destroy;
} SeatDevice;

static bool seat_device_provides_capability(
	const struct wlr_input_device *device, uint32_t capability) {
	switch (device->type) {
	case WLR_INPUT_DEVICE_POINTER:
	case WLR_INPUT_DEVICE_TABLET:
		return capability == WL_SEAT_CAPABILITY_POINTER;
	case WLR_INPUT_DEVICE_TOUCH:
		if (capability == WL_SEAT_CAPABILITY_TOUCH)
			return config.touch_enable != 0;
		return capability == WL_SEAT_CAPABILITY_POINTER &&
			   config.touch_enable && config.touch_enable_mouse_emulation;
	case WLR_INPUT_DEVICE_KEYBOARD:
		return capability == WL_SEAT_CAPABILITY_KEYBOARD;
	default:
		return false;
	}
}

static bool seat_has_capability(uint32_t capability) {
	SeatDevice *seat_device;

	wl_list_for_each(seat_device, &server.seat_devices, link) {
		if (seat_device_provides_capability(seat_device->device, capability))
			return true;
	}
	return false;
}

void update_seat_capabilities(void) {
	uint32_t caps = 0;

	if (!server.seat)
		return;

	/*
	 * seat_has_capability() is also influenced by config.touch_enable and
	 * config.touch_enable_mouse_emulation, so reload_config() re-runs this.
	 */
	if (seat_has_capability(WL_SEAT_CAPABILITY_POINTER))
		caps |= WL_SEAT_CAPABILITY_POINTER;
	if (seat_has_capability(WL_SEAT_CAPABILITY_KEYBOARD))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	if (seat_has_capability(WL_SEAT_CAPABILITY_TOUCH))
		caps |= WL_SEAT_CAPABILITY_TOUCH;
	wlr_seat_set_capabilities(server.seat, caps);
}

static void handle_seat_device_destroy(struct wl_listener *listener,
									   void *data) {
	SeatDevice *seat_device = wl_container_of(listener, seat_device, destroy);

	wl_list_remove(&seat_device->destroy.link);
	wl_list_remove(&seat_device->link);
	free(seat_device);
	update_seat_capabilities();
}

void seat_device_add(struct wlr_input_device *device) {
	SeatDevice *seat_device = calloc(1, sizeof(*seat_device));

	seat_device->device = device;
	seat_device->destroy.notify = handle_seat_device_destroy;
	wl_signal_add(&device->events.destroy, &seat_device->destroy);
	wl_list_insert(&server.seat_devices, &seat_device->link);

	update_seat_capabilities();
}

void handle_new_input_device(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available.
	 * when the backend is a headless backend, this event will never be
	 * triggered.
	 */
	struct wlr_input_device *device = data;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		keyboard_create(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TABLET:
		tablet_create(device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		tablet_pad_create(device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		pointer_create(wlr_pointer_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		touch_create(wlr_touch_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		switch_create(wlr_switch_from_input_device(device));
		break;
	default:
		/* TODO handle other input device types */
		break;
	}

	seat_device_add(device);
}
