#include "mango/ext-protocol/hotkey.h"

#include <stdlib.h>
#include <string.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "mango/common/input-event-codes.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/config/parse.h"
#include "mango/input/keyboard.h"
#include "xx-hotkey-v1-protocol.h"

#define HOTKEY_MODMASK                                                         \
	(WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT |               \
	 WLR_MODIFIER_LOGO)

enum hotkey_trigger_type {
	HOTKEY_TRIGGER_NONE,
	HOTKEY_TRIGGER_KEY,
	HOTKEY_TRIGGER_BUTTON,
};

struct MangoHotkeyManager;

struct MangoHotkey {
	struct wl_list link; /* hotkeys */
	struct wl_resource *resource;
	struct MangoHotkeyManager *manager; /* NULL once the manager is destroyed */

	char *pending_description;
	struct wl_resource *pending_seat;
	bool pending_trigger_set;
	enum hotkey_trigger_type pending_type;
	uint32_t pending_code; /* keysym or BTN_* */
	uint32_t pending_mods; /* wlr bits */

	/* Applied binding. */
	bool bound;
	enum hotkey_trigger_type type;
	uint32_t code;
	uint32_t mods;
	char *description;
	struct wl_resource *seat;
	char *app_id;

	bool pressed;
	bool pressed_reported;
	enum hotkey_trigger_type pressed_type;
	uint32_t pressed_code;
	struct wlr_keyboard *pressed_keyboard;

	bool seat_listener_added;
	struct wl_listener seat_destroy;
	bool pending_seat_listener_added;
	struct wl_listener pending_seat_destroy;
	bool pending_seat_removed;
};

struct MangoHotkeyManager {
	struct wl_list link; /* managers */
	struct wl_resource *resource;
	char *app_id;
	bool app_id_set;
	bool committed;
};

static struct wl_list managers;
static struct wl_list hotkeys;
static struct wl_global *hotkey_global;
static uint32_t last_input_time;

static struct {
	bool armed;
	uint32_t keycode;
	struct wlr_keyboard *keyboard;
	struct MangoHotkey *hotkey;
} pending_tap;
static bool tap_dirty;

static uint32_t wlr_mods_from_protocol(uint32_t mods) {
	uint32_t out = 0;

	if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_SHIFT)
		out |= WLR_MODIFIER_SHIFT;
	if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_CTRL)
		out |= WLR_MODIFIER_CTRL;
	if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_ALT)
		out |= WLR_MODIFIER_ALT;
	if (mods & XX_HOTKEY_MANAGER_V1_MODIFIERS_SUPER)
		out |= WLR_MODIFIER_LOGO;
	return out;
}

static bool protocol_mods_valid(uint32_t mods) {
	return (mods & ~(uint32_t)(XX_HOTKEY_MANAGER_V1_MODIFIERS_SHIFT |
							   XX_HOTKEY_MANAGER_V1_MODIFIERS_CTRL |
							   XX_HOTKEY_MANAGER_V1_MODIFIERS_ALT |
							   XX_HOTKEY_MANAGER_V1_MODIFIERS_SUPER)) == 0;
}

static bool keysym_is_modifier(xkb_keysym_t keysym) {
	return (keysym >= XKB_KEY_Shift_L && keysym <= XKB_KEY_Hyper_R) ||
		   keysym == XKB_KEY_ISO_Level3_Shift ||
		   keysym == XKB_KEY_ISO_Level5_Shift;
}

static uint32_t keysym_own_modifier(xkb_keysym_t keysym) {
	switch (keysym) {
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
		return WLR_MODIFIER_SHIFT;
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
		return WLR_MODIFIER_CTRL;
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Meta_L:
	case XKB_KEY_Meta_R:
		return WLR_MODIFIER_ALT;
	case XKB_KEY_Super_L:
	case XKB_KEY_Super_R:
		return WLR_MODIFIER_LOGO;
	default:
		return 0;
	}
}

static bool keyboard_produces_keysym(struct wlr_keyboard *keyboard,
									 uint32_t keycode, xkb_keysym_t keysym) {
	if (!keyboard || !keyboard->keymap)
		return false;

	xkb_layout_index_t layouts = xkb_keymap_num_layouts(keyboard->keymap);
	for (xkb_layout_index_t layout = 0; layout < layouts; layout++) {
		const xkb_keysym_t *syms = NULL;
		int32_t nsyms = xkb_keymap_key_get_syms_by_level(
			keyboard->keymap, keycode, layout, 0, &syms);
		for (int32_t i = 0; i < nsyms; i++) {
			if (syms[i] == keysym)
				return true;
		}
	}

	return false;
}

static void hotkey_send_triggered(struct MangoHotkey *hk, uint32_t time_msec) {
	uint32_t serial = wl_display_next_serial(server.display);

	xx_hotkey_v1_send_triggered(hk->resource, serial, time_msec);
}

static void hotkey_send_released(struct MangoHotkey *hk, uint32_t time_msec) {
	uint32_t serial = wl_display_next_serial(server.display);

	xx_hotkey_v1_send_released(hk->resource, serial, time_msec);
}

static void hotkey_revoke(struct MangoHotkey *hk, const char *message) {
	if (!hk->bound)
		return;

	xx_hotkey_v1_send_revoked(hk->resource, message ? message : "");
	hk->bound = false;
	hk->pressed = false;
	hk->pressed_reported = false;
	hk->pressed_type = HOTKEY_TRIGGER_NONE;
	hk->pressed_keyboard = NULL;

	if (pending_tap.hotkey == hk) {
		pending_tap.armed = false;
		pending_tap.hotkey = NULL;
	}
}

/* Protocol default: ctrl/alt/super always pass */
static bool trigger_acceptable(enum hotkey_trigger_type type, uint32_t code,
							   uint32_t mods) {
	if (mods & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO))
		return true;

	if (type == HOTKEY_TRIGGER_BUTTON)
		/* Only auxiliary buttons */
		return code >= BTN_SIDE && code <= BTN_TASK;

	if (keysym_is_modifier((xkb_keysym_t)code))
		return true;

	if (code >= XKB_KEY_KP_Space && code <= XKB_KEY_KP_Equal)
		return true;

	/* dead keys... */
	if (code >= 0xfe50 && code <= 0xfe9f)
		return false;

	/* Reserve normal letter/sybol keys! */
	{
		char buf[8];
		return xkb_keysym_to_utf8((xkb_keysym_t)code, buf, sizeof(buf)) == 0;
	}
}

static struct wlr_keyboard *hotkey_config_keyboard(void) {
	if (server.keyboard_group && server.keyboard_group->keyboard &&
		server.keyboard_group->keyboard->keymap)
		return server.keyboard_group->keyboard;
	if (server.last_active_keyboard && server.last_active_keyboard->keymap)
		return server.last_active_keyboard;
	return NULL;
}

static bool config_binding_claims_key(const KeyBinding *k,
									  xkb_keysym_t keysym) {
	xkb_keycode_t keycodes[3];
	struct wlr_keyboard *keyboard;

	if (k->keysymcode.type == KEY_TYPE_SYM)
		return xkb_keysym_to_lower(k->keysymcode.keysym) ==
			   xkb_keysym_to_lower(keysym);

	if (k->keysymcode.type != KEY_TYPE_CODE)
		return false;

	keyboard = hotkey_config_keyboard();
	if (!keyboard)
		return false;

	keycodes[0] = k->keysymcode.keycode.keycode1;
	keycodes[1] = k->keysymcode.keycode.keycode2;
	keycodes[2] = k->keysymcode.keycode.keycode3;

	for (int32_t i = 0; i < 3; i++) {
		if (keycodes[i] &&
			keyboard_produces_keysym(keyboard, keycodes[i], keysym))
			return true;
	}

	return false;
}

static bool hotkey_trigger_conflicts(const struct MangoHotkey *self,
									 enum hotkey_trigger_type type,
									 uint32_t code, uint32_t mods) {
	struct MangoHotkey *other;

	wl_list_for_each(other, &hotkeys, link) {
		if (other == self || !other->bound)
			continue;
		if (other->type == type && other->code == code && other->mods == mods)
			return true;
	}

	/* User binds more important than client ones */
	if (type == HOTKEY_TRIGGER_BUTTON) {
		for (int32_t i = 0; i < config.mouse_bindings_count; i++) {
			const MouseBinding *m = &config.mouse_bindings[i];
			if (m->button == code && (m->mod & HOTKEY_MODMASK) == mods)
				return true;
		}
		return false;
	}

	for (int32_t i = 0; i < config.key_bindings_count; i++) {
		const KeyBinding *k = &config.key_bindings[i];
		if ((k->mod & HOTKEY_MODMASK) != mods)
			continue;
		if (config_binding_claims_key(k, (xkb_keysym_t)code))
			return true;
	}

	for (int32_t i = 0; i < config.window_rules_count; i++) {
		const KeyBinding *k = &config.window_rules[i].globalkeybinding;
		if (!k->mod || (k->mod & HOTKEY_MODMASK) != mods)
			continue;
		if (config_binding_claims_key(k, (xkb_keysym_t)code))
			return true;
	}

	return false;
}

static void hotkey_watch_seat(struct MangoHotkey *hk, struct wl_resource *seat);

static void hotkey_seat_destroy(struct wl_listener *listener, void *data) {
	struct MangoHotkey *hk = wl_container_of(listener, hk, seat_destroy);

	wl_list_remove(&hk->seat_destroy.link);
	hk->seat_listener_added = false;

	if (!hk->seat)
		return;

	hk->seat = NULL;
	hotkey_revoke(hk, "the seat this hotkey was bound to was removed");
}

static void hotkey_watch_seat(struct MangoHotkey *hk,
							  struct wl_resource *seat) {
	if (hk->seat_listener_added) {
		wl_list_remove(&hk->seat_destroy.link);
		hk->seat_listener_added = false;
	}

	if (!seat)
		return;

	hk->seat_destroy.notify = hotkey_seat_destroy;
	wl_resource_add_destroy_listener(seat, &hk->seat_destroy);
	hk->seat_listener_added = true;
}

static void hotkey_pending_seat_destroy(struct wl_listener *listener,
										void *data) {
	struct MangoHotkey *hk =
		wl_container_of(listener, hk, pending_seat_destroy);

	wl_list_remove(&hk->pending_seat_destroy.link);
	hk->pending_seat_listener_added = false;
	hk->pending_seat = NULL;
	/* Do not fall back to every seat: deny until the client picks another. */
	hk->pending_seat_removed = true;
}

static void hotkey_watch_pending_seat(struct MangoHotkey *hk,
									  struct wl_resource *seat) {
	if (hk->pending_seat_listener_added) {
		wl_list_remove(&hk->pending_seat_destroy.link);
		hk->pending_seat_listener_added = false;
	}

	hk->pending_seat = seat;
	hk->pending_seat_removed = false;

	if (!seat)
		return;

	hk->pending_seat_destroy.notify = hotkey_pending_seat_destroy;
	wl_resource_add_destroy_listener(seat, &hk->pending_seat_destroy);
	hk->pending_seat_listener_added = true;
}

static void hotkey_resource_destroy(struct wl_resource *resource) {
	struct MangoHotkey *hk = wl_resource_get_user_data(resource);

	if (!hk)
		return;

	if (pending_tap.hotkey == hk) {
		pending_tap.armed = false;
		pending_tap.hotkey = NULL;
	}
	if (hk->seat_listener_added)
		wl_list_remove(&hk->seat_destroy.link);
	if (hk->pending_seat_listener_added)
		wl_list_remove(&hk->pending_seat_destroy.link);

	free(hk->pending_description);
	free(hk->description);
	free(hk->app_id);

	wl_list_remove(&hk->link);
	free(hk);
}

static void hotkey_set_description(struct wl_client *client,
								   struct wl_resource *resource,
								   const char *description) {
	struct MangoHotkey *hk = wl_resource_get_user_data(resource);
	char *copy = NULL;

	if (description && *description)
		copy = strdup(description);

	free(hk->pending_description);
	hk->pending_description = copy;
}

static void hotkey_set_seat(struct wl_client *client,
							struct wl_resource *resource,
							struct wl_resource *seat) {
	struct MangoHotkey *hk = wl_resource_get_user_data(resource);

	hotkey_watch_pending_seat(hk, seat);
}

static void hotkey_set_key_trigger(struct wl_client *client,
								   struct wl_resource *resource,
								   uint32_t keysym, uint32_t mods) {
	struct MangoHotkey *hk = wl_resource_get_user_data(resource);

	if (keysym == XKB_KEY_NoSymbol) {
		wl_resource_post_error(resource, XX_HOTKEY_V1_ERROR_INVALID_TRIGGER,
							   "keysym must not be null");
		return;
	}
	if (!protocol_mods_valid(mods)) {
		wl_resource_post_error(resource, XX_HOTKEY_V1_ERROR_INVALID_TRIGGER,
							   "unknown modifier bits");
		return;
	}

	hk->pending_trigger_set = true;
	hk->pending_type = HOTKEY_TRIGGER_KEY;
	hk->pending_code = keysym;
	hk->pending_mods = wlr_mods_from_protocol(mods);
}

static void hotkey_set_button_trigger(struct wl_client *client,
									  struct wl_resource *resource,
									  uint32_t button, uint32_t mods) {
	struct MangoHotkey *hk = wl_resource_get_user_data(resource);

	if (!protocol_mods_valid(mods)) {
		wl_resource_post_error(resource, XX_HOTKEY_V1_ERROR_INVALID_TRIGGER,
							   "unknown modifier bits");
		return;
	}

	hk->pending_trigger_set = true;
	hk->pending_type = HOTKEY_TRIGGER_BUTTON;
	hk->pending_code = button;
	hk->pending_mods = wlr_mods_from_protocol(mods);
}

static void hotkey_commit(struct wl_client *client,
						  struct wl_resource *resource) {
	struct MangoHotkey *hk = wl_resource_get_user_data(resource);
	const char *message = "";
	uint32_t reason;

	if (!hk->pending_trigger_set) {
		wl_resource_post_error(resource, XX_HOTKEY_V1_ERROR_NO_TRIGGER,
							   "commit sent before a trigger was described");
		return;
	}

	/* Any commit freezes the app id */
	if (hk->manager)
		hk->manager->committed = true;

	if (hk->pending_type == HOTKEY_TRIGGER_KEY &&
		keysym_is_modifier((xkb_keysym_t)hk->pending_code) &&
		(hk->pending_mods &
		 keysym_own_modifier((xkb_keysym_t)hk->pending_code))) {
		reason = XX_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
		message = "a modifier tap cannot require that same modifier";
	} else if (!trigger_acceptable(hk->pending_type, hk->pending_code,
								   hk->pending_mods)) {
		reason = XX_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
		message = "trigger is reserved for normal typing";
	} else if (hotkey_trigger_conflicts(hk, hk->pending_type, hk->pending_code,
										hk->pending_mods)) {
		reason = XX_HOTKEY_V1_DENY_REASON_ALREADY_BOUND;
		message = "trigger is already bound";
	} else if (hk->pending_seat_removed) {
		reason = XX_HOTKEY_V1_DENY_REASON_NOT_PERMITTED;
		message = "the seat this hotkey was restricted to was removed";
	} else {
		if (hk->pressed) {
			hotkey_send_released(hk, last_input_time);
			if (hk->pressed_type == HOTKEY_TRIGGER_KEY) {
				hk->pressed = false;
				hk->pressed_type = HOTKEY_TRIGGER_NONE;
				hk->pressed_keyboard = NULL;
			} else {
				hk->pressed_reported = true;
			}
		}

		hk->bound = true;
		hk->type = hk->pending_type;
		hk->code = hk->pending_code;
		hk->mods = hk->pending_mods;
		hk->seat = hk->pending_seat;
		hotkey_watch_seat(hk, hk->seat);

		free(hk->description);
		hk->description =
			hk->pending_description ? strdup(hk->pending_description) : NULL;

		if (!hk->app_id) {
			const char *app_id = hk->manager ? hk->manager->app_id : NULL;
			if (app_id)
				hk->app_id = strdup(app_id);
		}

		mango_error(true, WLR_DEBUG, "xx_hotkey: bound %s%s%#x mods=%#x",
					hk->app_id ? hk->app_id : "(no app_id)",
					hk->type == HOTKEY_TRIGGER_KEY ? " key " : " button ",
					hk->code, hk->mods);
		xx_hotkey_v1_send_bound(resource);
		return;
	}

	xx_hotkey_v1_send_denied(resource, reason, message);
}

static void hotkey_destroy_request(struct wl_client *client,
								   struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct xx_hotkey_v1_interface hotkey_impl = {
	.destroy = hotkey_destroy_request,
	.set_description = hotkey_set_description,
	.set_seat = hotkey_set_seat,
	.set_key_trigger = hotkey_set_key_trigger,
	.set_button_trigger = hotkey_set_button_trigger,
	.commit = hotkey_commit,
};

static void manager_resource_destroy(struct wl_resource *resource) {
	struct MangoHotkeyManager *manager = wl_resource_get_user_data(resource);
	struct MangoHotkey *hk;

	if (!manager)
		return;

	wl_list_for_each(hk, &hotkeys, link) {
		if (hk->manager == manager)
			hk->manager = NULL;
	}

	free(manager->app_id);
	wl_list_remove(&manager->link);
	free(manager);
}

static void manager_set_app_id(struct wl_client *client,
							   struct wl_resource *resource,
							   const char *app_id) {
	struct MangoHotkeyManager *manager = wl_resource_get_user_data(resource);

	if (!app_id || !*app_id) {
		wl_resource_post_error(resource,
							   XX_HOTKEY_MANAGER_V1_ERROR_INVALID_APP_ID,
							   "app_id must not be empty");
		return;
	}
	if (manager->app_id_set) {
		wl_resource_post_error(resource,
							   XX_HOTKEY_MANAGER_V1_ERROR_INVALID_APP_ID,
							   "app_id was already set");
		return;
	}
	if (manager->committed) {
		wl_resource_post_error(resource,
							   XX_HOTKEY_MANAGER_V1_ERROR_INVALID_APP_ID,
							   "app_id must be set before the first commit");
		return;
	}

	manager->app_id = strdup(app_id);
	manager->app_id_set = true;
}

static void manager_create_hotkey(struct wl_client *client,
								  struct wl_resource *resource, uint32_t id) {
	struct MangoHotkeyManager *manager = wl_resource_get_user_data(resource);
	struct MangoHotkey *hk;
	struct wl_resource *hotkey_resource;

	hotkey_resource = wl_resource_create(client, &xx_hotkey_v1_interface,
										 wl_resource_get_version(resource), id);
	if (!hotkey_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	hk = calloc(1, sizeof(*hk));
	if (!hk) {
		wl_resource_destroy(hotkey_resource);
		wl_client_post_no_memory(client);
		return;
	}

	hk->resource = hotkey_resource;
	hk->manager = manager;
	wl_list_insert(&hotkeys, &hk->link);
	wl_resource_set_implementation(hotkey_resource, &hotkey_impl, hk,
								   hotkey_resource_destroy);
}

static void manager_destroy_request(struct wl_client *client,
									struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct xx_hotkey_manager_v1_interface manager_impl = {
	.destroy = manager_destroy_request,
	.set_app_id = manager_set_app_id,
	.create_hotkey = manager_create_hotkey,
};

static void hotkey_manager_bind(struct wl_client *client, void *data,
								uint32_t version, uint32_t id) {
	struct MangoHotkeyManager *manager;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &xx_hotkey_manager_v1_interface,
								  version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	manager = calloc(1, sizeof(*manager));
	if (!manager) {
		wl_resource_destroy(resource);
		wl_client_post_no_memory(client);
		return;
	}

	manager->resource = resource;
	wl_list_insert(&managers, &manager->link);
	wl_resource_set_implementation(resource, &manager_impl, manager,
								   manager_resource_destroy);
}

static bool hotkey_key_pressed(struct wlr_keyboard *keyboard, uint32_t keycode,
							   uint32_t mods, uint32_t time_msec) {
	struct MangoHotkey *hk;

	if (pending_tap.armed && pending_tap.keycode != keycode)
		tap_dirty = true;

	wl_list_for_each(hk, &hotkeys, link) {
		if (!hk->bound || hk->type != HOTKEY_TRIGGER_KEY || hk->pressed)
			continue;
		if (!keyboard_produces_keysym(keyboard, keycode,
									  (xkb_keysym_t)hk->code))
			continue;

		if (keysym_is_modifier((xkb_keysym_t)hk->code)) {
			uint32_t before =
				(mods & ~keysym_own_modifier((xkb_keysym_t)hk->code)) &
				HOTKEY_MODMASK;
			if (before == hk->mods && !pending_tap.armed) {
				pending_tap.armed = true;
				pending_tap.keycode = keycode;
				pending_tap.keyboard = keyboard;
				pending_tap.hotkey = hk;
				tap_dirty = false;
			}
			continue;
		}

		if ((mods & HOTKEY_MODMASK) != hk->mods)
			continue;

		hotkey_send_triggered(hk, time_msec);
		hk->pressed = true;
		hk->pressed_reported = false;
		hk->pressed_type = HOTKEY_TRIGGER_KEY;
		hk->pressed_code = keycode;
		hk->pressed_keyboard = keyboard;
		return true;
	}

	return false;
}

static bool hotkey_key_released(uint32_t keycode, uint32_t time_msec) {
	struct MangoHotkey *hk;

	if (pending_tap.armed && pending_tap.keycode == keycode) {
		struct MangoHotkey *tap = pending_tap.hotkey;
		/* ignore when locked */
		bool fire = tap && tap->bound && !tap_dirty && !server.session_locked;

		pending_tap.armed = false;
		pending_tap.hotkey = NULL;
		tap_dirty = false;

		if (fire) {
			uint32_t serial = wl_display_next_serial(server.display);

			xx_hotkey_v1_send_triggered(tap->resource, serial, time_msec);
			xx_hotkey_v1_send_released(tap->resource, serial, time_msec);
		}
		return false;
	}

	wl_list_for_each(hk, &hotkeys, link) {
		if (!hk->bound || !hk->pressed ||
			hk->pressed_type != HOTKEY_TRIGGER_KEY ||
			hk->pressed_code != keycode)
			continue;

		if (!hk->pressed_reported)
			hotkey_send_released(hk, time_msec);
		hk->pressed = false;
		hk->pressed_reported = false;
		hk->pressed_type = HOTKEY_TRIGGER_NONE;
		hk->pressed_keyboard = NULL;
		return true;
	}

	return false;
}

bool hotkey_handle_key(struct wlr_keyboard *keyboard, uint32_t keycode,
					   uint32_t mods, uint32_t state, uint32_t time_msec) {
	if (wl_list_empty(&hotkeys))
		return false;

	/* ignore when locked */
	if (server.session_locked && state == WL_KEYBOARD_KEY_STATE_PRESSED)
		return false;

	last_input_time = time_msec;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return hotkey_key_released(keycode, time_msec);

	return hotkey_key_pressed(keyboard, keycode, mods, time_msec);
}

bool hotkey_handle_button(uint32_t button, uint32_t mods, uint32_t state,
						  uint32_t time_msec) {
	struct MangoHotkey *hk;

	if (wl_list_empty(&hotkeys))
		return false;

	/* ignore when locked */
	if (server.session_locked && state == WL_POINTER_BUTTON_STATE_PRESSED)
		return false;

	last_input_time = time_msec;

	if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
		wl_list_for_each(hk, &hotkeys, link) {
			if (!hk->bound || !hk->pressed ||
				hk->pressed_type != HOTKEY_TRIGGER_BUTTON ||
				hk->pressed_code != button)
				continue;

			if (!hk->pressed_reported)
				hotkey_send_released(hk, time_msec);
			hk->pressed = false;
			hk->pressed_reported = false;
			hk->pressed_type = HOTKEY_TRIGGER_NONE;
			return true;
		}
		return false;
	}

	tap_dirty = true;

	wl_list_for_each(hk, &hotkeys, link) {
		if (!hk->bound || hk->type != HOTKEY_TRIGGER_BUTTON || hk->pressed)
			continue;
		if (hk->code != button || (mods & HOTKEY_MODMASK) != hk->mods)
			continue;

		hotkey_send_triggered(hk, time_msec);
		hk->pressed = true;
		hk->pressed_reported = false;
		hk->pressed_type = HOTKEY_TRIGGER_BUTTON;
		hk->pressed_code = button;
		return true;
	}

	return false;
}

void hotkey_handle_keyboard_destroy(struct wlr_keyboard *keyboard) {
	struct MangoHotkey *hk;

	if (!keyboard || wl_list_empty(&hotkeys))
		return;

	if (pending_tap.armed && pending_tap.keyboard == keyboard) {
		pending_tap.armed = false;
		pending_tap.hotkey = NULL;
		tap_dirty = false;
	}

	wl_list_for_each(hk, &hotkeys, link) {
		if (!hk->pressed || hk->pressed_type != HOTKEY_TRIGGER_KEY ||
			hk->pressed_keyboard != keyboard)
			continue;

		hotkey_send_released(hk, last_input_time);
		hk->pressed = false;
		hk->pressed_type = HOTKEY_TRIGGER_NONE;
		hk->pressed_keyboard = NULL;
	}
}

void hotkey_note_activity(void) { tap_dirty = true; }

void hotkey_recheck_policy(void) {
	struct MangoHotkey *hk, *tmp;

	wl_list_for_each_safe(hk, tmp, &hotkeys, link) {
		if (!hk->bound)
			continue;
		if (!config.experimental_hotkeys) {
			hotkey_revoke(hk, "global hotkeys are disabled");
			continue;
		}
		if (hotkey_trigger_conflicts(hk, hk->type, hk->code, hk->mods))
			hotkey_revoke(hk, "trigger is now bound by the compositor");
	}
}

void hotkey_init(void) {
	wl_list_init(&managers);
	wl_list_init(&hotkeys);

	hotkey_global =
		wl_global_create(server.display, &xx_hotkey_manager_v1_interface, 1,
						 NULL, hotkey_manager_bind);
	if (!hotkey_global) {
		mango_error(true, WLR_ERROR,
					"Failed to create the xx_hotkey_manager_v1 global");
	}
}
