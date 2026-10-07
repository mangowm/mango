#include "mango/config/internal.h"
#include "mango/config/parse.h"
#include "mango/config/preset.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mango/common/input-event-codes.h"
#include "mango/common/log.h"
#include "mango/dispatch/bind.h"
#include "mango/input/pointer.h"
#include "mango/manage/client.h"
#include "mango/switcher/switcher.h"
#include <wlr/types/wlr_keyboard.h>

static struct xkb_keymap *reference_keymap_instance = NULL;

static struct xkb_keymap *reference_keymap(void) {
	if (reference_keymap_instance == NULL && config.ctx != NULL) {
		reference_keymap_instance = xkb_keymap_new_from_names(
			config.ctx, &xkb_fallback_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}

	return reference_keymap_instance;
}

void cleanup_config_keymap(void) {
	if (config.keymap != NULL) {
		xkb_keymap_unref(config.keymap);
		config.keymap = NULL;
	}
	if (reference_keymap_instance != NULL) {
		xkb_keymap_unref(reference_keymap_instance);
		reference_keymap_instance = NULL;
	}
	if (config.ctx != NULL) {
		xkb_context_unref(config.ctx);
		config.ctx = NULL;
	}
}

void create_config_keymap(void) {
	// Initializes the xkb context and keymap.

	if (config.ctx == NULL) {
		config.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	}

	if (config.keymap == NULL) {
		config.keymap = xkb_keymap_new_from_names(
			config.ctx, &xkb_fallback_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}
}

// Parses the bind combination string.
void parse_bind_flags(const char *str, KeyBinding *kb) {
	// Checks whether it starts with "bind".
	if (strncmp(str, "bind", 4) != 0) {
		return;
	}

	const char *suffix = str + 4; // Skips "bind".

	// Walks the remaining suffix characters.
	for (int32_t i = 0; suffix[i] != '\0'; i++) {
		switch (suffix[i]) {
		case 's':
			kb->keysymcode.type = KEY_TYPE_SYM;
			break;
		case 'l':
			kb->islockapply = true;
			break;
		case 'r':
			kb->isreleaseapply = true;
			break;
		case 'p':
			kb->ispassapply = true;
			break;
		case 'c':
			kb->isallowconflict = true;
			break;
		default:
			mango_error(false, WLR_ERROR,
						"Unknown bind flag: \033[1m\033[31m%c\033[0m\n",
						suffix[i]);
			break;
		}
	}
}

void set_binding_keymode(Config *config, char mode[28], bool *iscommonmode,
						 bool *isdefaultmode) {
	strcpy(mode, config->keymode);
	if (strcmp(mode, "common") == 0) {
		*iscommonmode = true;
		*isdefaultmode = false;
	} else if (strcmp(mode, "default") == 0) {
		*isdefaultmode = true;
		*iscommonmode = false;
	} else {
		*isdefaultmode = false;
		*iscommonmode = false;
	}
}

uint32_t parse_mod(const char *mod_str) {
	if (!mod_str || !*mod_str) {
		return UINT32_MAX;
	}

	uint32_t mod = 0;
	char input_copy[256];
	char *token;
	char *saveptr = NULL;
	bool match_success = false;

	// Copies and converts to lowercase.
	strncpy(input_copy, mod_str, sizeof(input_copy) - 1);
	input_copy[sizeof(input_copy) - 1] = '\0';
	for (char *p = input_copy; *p; p++) {
		*p = tolower(*p);
	}

	// Splits and processes each part.
	token = strtok_r(input_copy, "+", &saveptr);
	while (token != NULL) {
		// Strips leading/trailing whitespace.
		trim_whitespace(token);

		// Skips tokens that became empty.
		if (*token == '\0') {
			token = strtok_r(NULL, "+", &saveptr);
			continue;
		}

		if (strncmp(token, "code:", 5) == 0) {
			// Handles the code: form.
			char *endptr;
			long keycode = strtol(token + 5, &endptr, 10);
			if (endptr != token + 5 && (*endptr == '\0' || *endptr == ' ')) {
				switch (keycode) {
				case 133:
				case 134:
					mod |= WLR_MODIFIER_LOGO;
					match_success = true;
					break;
				case 37:
				case 105:
					mod |= WLR_MODIFIER_CTRL;
					match_success = true;
					break;
				case 50:
				case 62:
					mod |= WLR_MODIFIER_SHIFT;
					match_success = true;
					break;
				case 64:
				case 108:
					mod |= WLR_MODIFIER_ALT;
					match_success = true;
					break;
				default:
					mango_error(false, WLR_ERROR,
								"unknown modifier keycode: "
								"\033[1m\033[31m%s\033[0m\n",
								token);
					break;
				}
			}
		} else {
			if (!strcmp(token, "super") || !strcmp(token, "super_l") ||
				!strcmp(token, "super_r")) {
				mod |= WLR_MODIFIER_LOGO;
				match_success = true;
			}
			if (!strcmp(token, "ctrl") || !strcmp(token, "ctrl_l") ||
				!strcmp(token, "ctrl_r")) {
				mod |= WLR_MODIFIER_CTRL;
				match_success = true;
			}
			if (!strcmp(token, "shift") || !strcmp(token, "shift_l") ||
				!strcmp(token, "shift_r")) {
				mod |= WLR_MODIFIER_SHIFT;
				match_success = true;
			}
			if (!strcmp(token, "alt") || !strcmp(token, "alt_l") ||
				!strcmp(token, "alt_r")) {
				mod |= WLR_MODIFIER_ALT;
				match_success = true;
			}
			if (!strcmp(token, "hyper") || !strcmp(token, "hyper_l") ||
				!strcmp(token, "hyper_r")) {
				mod |= WLR_MODIFIER_MOD3;
				match_success = true;
			}
			if (!strcmp(token, "none")) {
				match_success = true;
			}
		}

		token = strtok_r(NULL, "+", &saveptr);
	}

	if (!match_success) {
		mod = UINT32_MAX;
		mango_error(false, WLR_ERROR,
					"Unknown modifier: "
					"\033[1m\033[31m%s\033[0m\n",
					mod_str);
	}

	return mod;
}

static int32_t find_keycodes_in_layout(struct xkb_keymap *keymap,
									   xkb_layout_index_t layout,
									   xkb_keysym_t sym,
									   MultiKeycode *multi_kc) {
	xkb_keycode_t min_keycode = xkb_keymap_min_keycode(keymap);
	xkb_keycode_t max_keycode = xkb_keymap_max_keycode(keymap);
	int32_t found_count = 0;

	for (xkb_keycode_t keycode = min_keycode;
		 keycode <= max_keycode && found_count < 3; keycode++) {
		xkb_level_index_t levels =
			xkb_keymap_num_levels_for_key(keymap, keycode, layout);
		bool matched = false;

		for (xkb_level_index_t level = 0; level < levels && !matched; level++) {
			const xkb_keysym_t *syms;
			int32_t num_syms = xkb_keymap_key_get_syms_by_level(
				keymap, keycode, layout, level, &syms);

			for (int32_t i = 0; i < num_syms; i++) {
				if (syms[i] == sym) {
					matched = true;
					break;
				}
			}
		}

		if (!matched)
			continue;

		switch (found_count) {
		case 0:
			multi_kc->keycode1 = keycode;
			break;
		case 1:
			multi_kc->keycode2 = keycode;
			break;
		case 2:
			multi_kc->keycode3 = keycode;
			break;
		}
		found_count++;
	}

	return found_count;
}

int32_t find_keycodes_for_keysym(struct xkb_keymap *keymap, xkb_keysym_t sym,
								 MultiKeycode *multi_kc) {
	multi_kc->keycode1 = 0;
	multi_kc->keycode2 = 0;
	multi_kc->keycode3 = 0;

	int32_t found_count = 0;

	if (keymap != NULL) {
		xkb_layout_index_t layouts = xkb_keymap_num_layouts(keymap);

		for (xkb_layout_index_t layout = 0;
			 layout < layouts && found_count == 0; layout++) {
			found_count =
				find_keycodes_in_layout(keymap, layout, sym, multi_kc);
		}
	}

	if (found_count == 0) {
		struct xkb_keymap *fallback = reference_keymap();

		if (fallback != NULL && fallback != keymap) {
			xkb_layout_index_t layouts = xkb_keymap_num_layouts(fallback);

			for (xkb_layout_index_t layout = 0;
				 layout < layouts && found_count == 0; layout++) {
				found_count =
					find_keycodes_in_layout(fallback, layout, sym, multi_kc);
			}
		}
	}

	return found_count;
}

int32_t find_keycodes_for_char(char c_char, MultiKeycode *multi_kc) {
	multi_kc->keycode1 = 0;
	multi_kc->keycode2 = 0;
	multi_kc->keycode3 = 0;

	if (c_char == '\0')
		return 0;

	return find_keycodes_for_keysym(
		config.keymap, xkb_utf32_to_keysym((uint32_t)(unsigned char)c_char),
		multi_kc);
}

KeySymCode parse_key(const char *key_str, bool isbindsym) {
	KeySymCode kc = {0}; // Initialized to 0.

	if (config.keymap == NULL || config.ctx == NULL) {
		// Handles errors.
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		return kc;
	}

	// Handles the code: prefix case.
	if (strncmp(key_str, "code:", 5) == 0) {
		char *endptr;
		xkb_keycode_t keycode = (xkb_keycode_t)strtol(key_str + 5, &endptr, 10);
		kc.type = KEY_TYPE_CODE;
		kc.keycode.keycode1 = keycode; // Sets only the first one.
		kc.keycode.keycode2 = 0;
		kc.keycode.keycode3 = 0;
		return kc;
	}

	// change key string to keysym, case insensitive
	xkb_keysym_t sym =
		xkb_keysym_from_name(key_str, XKB_KEYSYM_CASE_INSENSITIVE);

	if (isbindsym) {
		kc.type = KEY_TYPE_SYM;
		kc.keysym = sym;
		return kc;
	}

	if (sym != XKB_KEY_NoSymbol) {
		// Tries to find all matching keycodes.
		int32_t found_count =
			find_keycodes_for_keysym(config.keymap, sym, &kc.keycode);
		if (found_count > 0) {
			kc.type = KEY_TYPE_CODE;
			kc.keysym = sym; // Still keeps the keysym for reference.
		} else {
			kc.type = KEY_TYPE_SYM;
			kc.keysym = sym;
			kc.unresolved = true;
			// keycode field stays 0.
		}
	} else {
		// Unparseable key name.
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		mango_error(false, WLR_ERROR, "Unknown key: \033[1m\033[31m%s\033[0m\n",
					key_str);
		// keycode field stays 0.
	}

	return kc;
}

uint32_t parse_button(const char *str) {
	// Converts the input string to lowercase.
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // Ensures the string is properly terminated.

	// Parses the "code:number" format.
	if (strncmp(lowerStr, "code:", 5) == 0) {
		const char *numStart = lowerStr + 5; // Skips "code:".
		char *endptr;
		unsigned long val = strtoul(numStart, &endptr, 10);

		// Checks that the conversion succeeded with no leftover characters and
		// no overflow (within uint32_t).
		if (endptr != numStart && *endptr == '\0' && val <= UINT32_MAX) {
			return (uint32_t)val;
		} else {
			mango_error(false, WLR_ERROR,
						"Invalid code format: "
						"\033[1m\033[31m%s\033[0m\n",
						str);
			return UINT32_MAX;
		}
	}

	// Returns the matching button number from the lowercased string.
	if (strcmp(lowerStr, "btn_left") == 0) {
		return BTN_LEFT;
	} else if (strcmp(lowerStr, "btn_right") == 0) {
		return BTN_RIGHT;
	} else if (strcmp(lowerStr, "btn_middle") == 0) {
		return BTN_MIDDLE;
	} else if (strcmp(lowerStr, "btn_side") == 0) {
		return BTN_SIDE;
	} else if (strcmp(lowerStr, "btn_extra") == 0) {
		return BTN_EXTRA;
	} else if (strcmp(lowerStr, "btn_forward") == 0) {
		return BTN_FORWARD;
	} else if (strcmp(lowerStr, "btn_back") == 0) {
		return BTN_BACK;
	} else if (strcmp(lowerStr, "btn_task") == 0) {
		return BTN_TASK;
	} else {
		mango_error(false, WLR_ERROR,
					"Unknown button: "
					"\033[1m\033[31m%s\033[0m\n",
					str);
		return UINT32_MAX;
	}
}

int32_t parse_mouse_action(const char *str) {
	// Converts the input string to lowercase.
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // Ensures the string is properly terminated.

	// Returns the matching button number from the lowercased string.
	if (strcmp(lowerStr, "curmove") == 0) {
		return CurMove;
	} else if (strcmp(lowerStr, "curresize") == 0) {
		return CurResize;
	} else if (strcmp(lowerStr, "curnormal") == 0) {
		return CurNormal;
	} else if (strcmp(lowerStr, "curpressed") == 0) {
		return CurPressed;
	} else {
		return 0;
	}
}

// Helper: checks whether a string starts with the given prefix
// (case-insensitive).
char *combine_args_until_empty(char *values[], int count) {
	if (count <= 0)
		return strdup("");

	// find the first empty string
	int first_empty = count;
	for (int i = 0; i < count; i++) {
		// check if it's empty: empty string or only contains "0" (initialized)
		if (values[i][0] == '\0' ||
			(strlen(values[i]) == 1 && values[i][0] == '0')) {
			first_empty = i;
			break;
		}
	}

	// 	if there are no valid parameters, return an empty string
	if (first_empty == 0) {
		return strdup("");
	}

	// 	calculate the total length
	size_t total_len = 1; /* NUL terminator */
	for (int i = 0; i < first_empty; i++) {
		total_len += strlen(values[i]);
	}
	// 	plus the number of commas (first_empty-1 commas)
	total_len += (size_t)(first_empty - 1);

	// 	allocate memory and concatenate
	char *combined = malloc(total_len);
	if (combined == NULL) {
		return strdup("");
	}

	combined[0] = '\0';
	for (int i = 0; i < first_empty; i++) {
		if (i > 0) {
			strcat(combined, ",");
		}
		strcat(combined, values[i]);
	}

	return combined;
}

FuncType parse_func_name(char *func_name, Arg *arg, char *arg_value,
						 char *arg_value2, char *arg_value3, char *arg_value4,
						 char *arg_value5) {

	FuncType func = NULL;
	(*arg).i = 0;
	(*arg).i2 = 0;
	(*arg).f = 0.0f;
	(*arg).f2 = 0.0f;
	(*arg).ui = 0;
	(*arg).ui2 = 0;
	(*arg).v = NULL;
	(*arg).v2 = NULL;
	(*arg).v3 = NULL;

	if (strcmp(func_name, "focusstack") == 0) {
		func = focus_stack;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "overcircle") == 0) {
		func = over_circle;
		(*arg).i = parse_overcircle_direction(arg_value);
	} else if (strcmp(func_name, "groupfocus") == 0) {
		func = group_focus;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "focusdir") == 0) {
		func = focus_direction;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "focus_window_or_workspace") == 0) {
		func = focus_window_or_workspace;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "groupjoin") == 0) {
		func = group_join;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "groupleave") == 0) {
		func = group_leave;
	} else if (strcmp(func_name, "focusid") == 0) {
		func = focus_by_id;
	} else if (strcmp(func_name, "incnmaster") == 0) {
		func = inc_nmaster;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "setmfact") == 0) {
		func = set_master_factor;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "zoom") == 0) {
		func = zoom;
	} else if (strcmp(func_name, "exchange_client") == 0) {
		func = exchange_client;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "move_client") == 0) {
		func = move_client;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "exchange_stack_client") == 0) {
		func = exchange_stack_client;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "toggleglobal") == 0) {
		func = toggle_global;
	} else if (strcmp(func_name, "togglehdr") == 0) {
		/* togglehdr[,on|off|toggle][,<monitor name>|all] */
		func = toggle_hdr;
		if (strcmp(arg_value, "on") == 0)
			(*arg).i = 1;
		else if (strcmp(arg_value, "off") == 0)
			(*arg).i = 0;
		else
			(*arg).i = -1; // toggle, and the default for an empty argument
		// "not given" is "" from the IPC path and "0" from the keybinding
		// parser -- same rule as combine_args_until_empty().
		bool has_name = arg_value2 && arg_value2[0] != '\0' &&
						!(strlen(arg_value2) == 1 && arg_value2[0] == '0');
		(*arg).v = has_name ? strdup(arg_value2) : NULL;
	} else if (strcmp(func_name, "toggleoverview") == 0) {
		func = toggle_overview;
		(*arg).i = atoi(arg_value) == 1;
	} else if (strcmp(func_name, "enteroverview") == 0) {
		func = enter_overview;
	} else if (strcmp(func_name, "leaveoverview") == 0) {
		func = leave_overview;
	} else if (strcmp(func_name, "togglejump") == 0) {
		func = toggle_jump;
	} else if (strcmp(func_name, "set_proportion") == 0) {
		func = set_proportion;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "switch_proportion_preset") == 0) {
		func = switch_proportion_preset;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "viewtoleft") == 0) {
		func = view_to_left;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright") == 0) {
		func = view_to_right;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "view_insert") == 0) {
		func = view_insert;
		(*arg).i = strcmp(arg_value, "next") == 0 ? NEXT : PREV;
	} else if (strcmp(func_name, "tagsilent") == 0) {
		func = tag_silent;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "tagtoleft") == 0) {
		func = tag_to_left;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "tagtoright") == 0) {
		func = tag_to_right;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "killclient") == 0) {
		func = kill_client;
		(*arg).i = parse_force(arg_value);
	} else if (strcmp(func_name, "centerwin") == 0) {
		func = center_window;
	} else if (strcmp(func_name, "focuslast") == 0) {
		func = focus_last;
	} else if (strcmp(func_name, "switcher") == 0) {
		func = switcher;
		if (strcmp(arg_value, "all_next") == 0) {
			(*arg).i = NEXT;
			(*arg).i2 = SW_ALL_MON;
		} else if (strcmp(arg_value, "all_prev") == 0) {
			(*arg).i = PREV;
			(*arg).i2 = SW_ALL_MON;
		} else if (strcmp(arg_value, "all_tag_next") == 0) {
			(*arg).i = NEXT;
			(*arg).i2 = SW_ALL_TAG;
		} else if (strcmp(arg_value, "all_tag_prev") == 0) {
			(*arg).i = PREV;
			(*arg).i2 = SW_ALL_TAG;
		} else if (strcmp(arg_value, "prev") == 0) {
			(*arg).i = PREV;
			(*arg).i2 = SW_CURRENT_TAG;
		} else {
			(*arg).i = NEXT;
			(*arg).i2 = SW_CURRENT_TAG;
		}
	} else if (strcmp(func_name, "toggle_trackpad_enable") == 0) {
		func = toggle_trackpad_enable;
	} else if (strcmp(func_name, "setoption") == 0) {
		func = setoption;

		(*arg).v = strdup(arg_value);

		// Collects the parameters to concatenate.
		const char *non_empty_params[4] = {NULL};
		int32_t param_index = 0;

		if (arg_value2 && arg_value2[0] != '\0')
			non_empty_params[param_index++] = arg_value2;
		if (arg_value3 && arg_value3[0] != '\0')
			non_empty_params[param_index++] = arg_value3;
		if (arg_value4 && arg_value4[0] != '\0')
			non_empty_params[param_index++] = arg_value4;
		if (arg_value5 && arg_value5[0] != '\0')
			non_empty_params[param_index++] = arg_value5;

		// Handles concatenation.
		if (param_index == 0) {
			(*arg).v2 = strdup("");
		} else {
			// Computes the total length.
			size_t len = 0;
			for (int32_t i = 0; i < param_index; i++) {
				len += strlen(non_empty_params[i]);
			}
			len += (param_index - 1) + 1; // Comma count + null terminator.

			char *temp = malloc(len);
			if (temp) {
				char *cursor_str = temp;
				for (int32_t i = 0; i < param_index; i++) {
					if (i > 0) {
						*cursor_str++ = ',';
					}
					size_t param_len = strlen(non_empty_params[i]);
					memcpy(cursor_str, non_empty_params[i], param_len);
					cursor_str += param_len;
				}
				*cursor_str = '\0';
				(*arg).v2 = temp;
			}
		}
	} else if (strcmp(func_name, "setkeymode") == 0) {
		func = set_key_mode;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_keyboard_layout") == 0) {
		func = switch_keyboard_layout;
		(*arg).i = CLAMP_INT(atoi(arg_value), 0, 100);
	} else if (strcmp(func_name, "setlayout") == 0) {
		func = set_layout;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_layout") == 0) {
		func = switch_layout;
	} else if (strcmp(func_name, "togglefloating") == 0) {
		func = toggle_floating;
	} else if (strcmp(func_name, "togglefullscreen") == 0) {
		func = toggle_fullscreen;
	} else if (strcmp(func_name, "togglefakefullscreen") == 0) {
		func = toggle_fake_fullscreen;
	} else if (strcmp(func_name, "toggleoverlay") == 0) {
		func = toggle_overlay;
	} else if (strcmp(func_name, "minimized") == 0) {
		func = minimize_window;
	} else if (strcmp(func_name, "restore_minimized") == 0) {
		func = restore_minimized;
	} else if (strcmp(func_name, "toggle_scratchpad") == 0) {
		func = toggle_scratchpad;
	} else if (strcmp(func_name, "toggle_render_border") == 0) {
		func = toggle_render_border;
	} else if (strcmp(func_name, "focusmon") == 0) {
		func = focus_monitor;
		(*arg).i = parse_monitor_arg(arg_value);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		}
	} else if (strcmp(func_name, "tagmon") == 0) {
		func = tag_monitor;
		(*arg).i = parse_monitor_arg(arg_value);
		(*arg).i2 = atoi(arg_value2);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		};
	} else if (strcmp(func_name, "incgaps") == 0) {
		func = increase_gaps;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "togglegaps") == 0) {
		func = toggle_gaps;
	} else if (strcmp(func_name, "togglegroupbar") == 0) {
		func = toggle_group_bar;
	} else if (strcmp(func_name, "chvt") == 0) {
		func = change_vt;
		(*arg).ui = atoi(arg_value);
	} else if (strcmp(func_name, "spawn") == 0) {
		func = spawn;
		char *values[] = {arg_value, arg_value2, arg_value3, arg_value4,
						  arg_value5};
		(*arg).v = combine_args_until_empty(values, 5);
	} else if (strcmp(func_name, "spawn_shell") == 0) {
		func = spawn_shell;
		char *values[] = {arg_value, arg_value2, arg_value3, arg_value4,
						  arg_value5};
		(*arg).v = combine_args_until_empty(values, 5);
	} else if (strcmp(func_name, "spawn_on_empty") == 0) {
		func = spawn_on_empty;
		(*arg).v = strdup(arg_value);
		(*arg).ui = parse_tag_mask(arg_value2);
	} else if (strcmp(func_name, "quit") == 0) {
		func = quit;
	} else if (strcmp(func_name, "create_virtual_output") == 0) {
		func = create_virtual_output;
		if (arg_value && arg_value[0] != '\0') {
			(*arg).v = strdup(arg_value);
		}
	} else if (strcmp(func_name, "destroy_all_virtual_output") == 0) {
		func = destroy_all_virtual_output;
	} else if (strcmp(func_name, "moveresize") == 0) {
		func = move_resize;
		(*arg).ui = parse_mouse_action(arg_value);
	} else if (strcmp(func_name, "togglemaximizescreen") == 0) {
		func = toggle_maximize_screen;
	} else if (strcmp(func_name, "viewprev_have_client") == 0) {
		func = viewprev_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewnext_have_client") == 0) {
		func = viewnext_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoleft_have_client") == 0) {
		func = view_to_left_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright_have_client") == 0) {
		func = view_to_right_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "reload_config") == 0) {
		func = reload_config;
	} else if (strcmp(func_name, "load_config_file") == 0) {
		func = load_config_file;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "tag") == 0) {
		func = tag;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "view") == 0) {
		func = bind_to_view;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "viewcrossmon") == 0) {
		func = view_cross_monitor;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "tagcrossmon") == 0) {
		func = tag_cross_monitor;
		(*arg).ui = parse_tag_mask(arg_value);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "toggletag") == 0) {
		func = toggle_tag;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "toggleview") == 0) {
		func = toggle_view;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "comboview") == 0) {
		func = combo_view;
		(*arg).ui = parse_tag_mask(arg_value);
	} else if (strcmp(func_name, "smartmovewin") == 0) {
		func = smart_move_window;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "smartresizewin") == 0) {
		func = smart_resize_window;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "resizewin") == 0) {
		func = resize_window;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "movewin") == 0) {
		func = move_window;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "toggle_named_scratchpad") == 0) {
		func = toggle_named_scratchpad;
		(*arg).v = strdup(arg_value);
		(*arg).v2 = strdup(arg_value2);
		(*arg).v3 = strdup(arg_value3);
	} else if (strcmp(func_name, "toggle_special_tag") == 0) {
		func = toggle_special_tag;
	} else if (strcmp(func_name, "tag_special_tag") == 0) {
		func = tag_special_tag;
	} else if (strcmp(func_name, "tag_special_silent") == 0) {
		func = tag_special_silent;
	} else if (strcmp(func_name, "disable_monitor") == 0) {
		func = disable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "enable_monitor") == 0) {
		func = enable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "toggle_monitor") == 0) {
		func = toggle_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "sleep_monitor") == 0) {
		func = sleep_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "wakeup_monitor") == 0) {
		func = wakeup_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "sleep_toggle_monitor") == 0) {
		func = sleep_toggle_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "scroller_stack") == 0) {
		func = scroller_stack;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "toggle_all_floating") == 0) {
		func = toggle_all_floating;
	} else if (strcmp(func_name, "dwindle_toggle_split_direction") == 0) {
		func = dwindle_toggle_split_direction;
	} else if (strcmp(func_name, "dwindle_split_horizontal") == 0) {
		func = dwindle_split_horizontal;
	} else if (strcmp(func_name, "dwindle_split_vertical") == 0) {
		func = dwindle_split_vertical;
	} else if (strcmp(func_name, "dwindle_toggle_current_split") == 0) {
		func = dwindle_toggle_current_split;
	} else {
		return NULL;
	}
	return func;
}
