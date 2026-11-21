#ifndef __EXT_PROTOCOL_HOTKEY_H__
#define __EXT_PROTOCOL_HOTKEY_H__ 1

/*
 * xx_hotkey_v1
 */
#include <stdbool.h>
#include <stdint.h>

struct wlr_keyboard;

/* Declarations */
void hotkey_init(void);
/* Returns true when the event must be swallowed
 * `keycode` is an xkb keycode, `mods` a wlr modifier mask. */
bool hotkey_handle_key(struct wlr_keyboard *keyboard, uint32_t keycode,
					   uint32_t mods, uint32_t state, uint32_t time_msec);
/* `button` is a BTN_* code. */
bool hotkey_handle_button(uint32_t button, uint32_t mods, uint32_t state,
						  uint32_t time_msec);
void hotkey_note_activity(void);
void hotkey_handle_keyboard_destroy(struct wlr_keyboard *keyboard);
void hotkey_recheck_policy(void);

#endif
