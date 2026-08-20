/*
 * Input 设备管理：输入设备新增/销毁分发，以及 switch 设备处理。
 */

void destroyinputdevice(struct wl_listener *listener, void *data) {
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

void switch_toggle(struct wl_listener *listener, void *data) {
	// 获取包含监听器的结构体
	Switch *sw = wl_container_of(listener, sw, toggle);

	// 处理切换事件
	struct wlr_switch_toggle_event *event = data;
	SwitchBinding *s;
	int32_t ji;

	ipc_notify_device_event(&sw->wlr_switch->base);

	for (ji = 0; ji < config.switch_bindings_count; ji++) {
		if (config.switch_bindings_count < 1)
			break;
		s = &config.switch_bindings[ji];
		if ((s->iscommonmode || (s->isdefaultmode && keymode.isdefault) ||
			 (strcmp(keymode.mode, s->mode) == 0)) &&
			event->switch_state == s->fold && s->func) {
			s->func(&s->arg);
			return;
		}
	}
}

void createswitch(struct wlr_switch *switch_device) {

	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&switch_device->base) &&
		(device = wlr_libinput_get_device_handle(&switch_device->base))) {

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &switch_device->base;
		input_dev->libinput_device = device;
		input_dev->device_data = NULL; // 初始化为 NULL

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&switch_device->base.events.destroy,
					  &input_dev->destroy_listener);

		// 创建 Switch 特定数据
		Switch *sw = calloc(1, sizeof(Switch));
		sw->wlr_switch = switch_device;
		sw->toggle.notify = switch_toggle;
		sw->input_dev = input_dev;

		// 将 Switch 指针保存到 input_device 中
		input_dev->device_data = sw;

		// 添加 toggle 监听器
		wl_signal_add(&switch_device->events.toggle, &sw->toggle);

		// 添加到全局列表
		wl_list_insert(&inputdevices, &input_dev->link);
	}
}

void inputdevice(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available.
	 * when the backend is a headless backend, this event will never be
	 * triggered.
	 */
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TABLET:
		createtablet(device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		createtabletpad(device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		createtouch(wlr_touch_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		createswitch(wlr_switch_from_input_device(device));
		break;
	default:
		/* TODO handle other input device types */
		break;
	}

	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In dwl we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability.
	 */
	/* TODO do we actually require a cursor? */
	caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH;
	if (!wl_list_empty(&kb_group->wlr_group->devices) ||
		!wl_list_empty(&standalone_keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}
