#include "mango/config/internal.h"
#include "mango/config/parse.h"

#include <ctype.h>
#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mango/animation/common.h"
#include "mango/common/input-event-codes.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/error_nag.h"
#include "mango/config/error_store.h"
#include "mango/config/watcher.h"
#include "mango/dispatch/bind.h"
#include "mango/ext-protocol/hdr.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/ipc/ipc.h"
#include "mango/layout/arrange.h"
#include "mango/layout/layout.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"
#include "mango/manage/tab.h"
#include "mango/switcher/switcher.h"
#include <scenefx/types/wlr_scene.h>
#include <unistd.h>
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>

bool parse_option(Config *config, char *key, char *value, int line_number) {
	if (strcmp(key, "key_mode") == 0) {
		snprintf(config->keymode, sizeof(config->keymode), "%.27s", value);
	} else if (strcmp(key, "animations") == 0) {
		config->animations = atoi(value);
	} else if (strcmp(key, "layer_animations") == 0) {
		config->layer_animations = atoi(value);
	} else if (strcmp(key, "animation_type_open") == 0) {
		config->animation_type_open = animation_type_from_string(value);
	} else if (strcmp(key, "animation_type_close") == 0) {
		config->animation_type_close = animation_type_from_string(value);
	} else if (strcmp(key, "layer_animation_type_open") == 0) {
		config->layer_animation_type_open = animation_type_from_string(value);
	} else if (strcmp(key, "layer_animation_type_close") == 0) {
		config->layer_animation_type_close = animation_type_from_string(value);
	} else if (strcmp(key, "animation_fade_in") == 0) {
		config->animation_fade_in = atoi(value);
	} else if (strcmp(key, "animation_fade_out") == 0) {
		config->animation_fade_out = atoi(value);
	} else if (strcmp(key, "tag_animation_direction") == 0) {
		config->tag_animation_direction = atoi(value);
	} else if (strcmp(key, "zoom_initial_ratio") == 0) {
		config->zoom_initial_ratio = atof(value);
	} else if (strcmp(key, "zoom_end_ratio") == 0) {
		config->zoom_end_ratio = atof(value);
	} else if (strcmp(key, "fade_in_begin_opacity") == 0) {
		config->fadein_begin_opacity = atof(value);
	} else if (strcmp(key, "fade_out_begin_opacity") == 0) {
		config->fadeout_begin_opacity = atof(value);
	} else if (strcmp(key, "animation_duration_move") == 0) {
		config->animation_duration_move = atoi(value);
	} else if (strcmp(key, "animation_duration_open") == 0) {
		config->animation_duration_open = atoi(value);
	} else if (strcmp(key, "animation_duration_tag") == 0) {
		config->animation_duration_tag = atoi(value);
	} else if (strcmp(key, "animation_duration_close") == 0) {
		config->animation_duration_close = atoi(value);
	} else if (strcmp(key, "animation_duration_focus") == 0) {
		config->animation_duration_focus = atoi(value);
	} else if (strcmp(key, "animation_curve_move") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_move, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_move: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_open") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_open, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_open: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_tag") == 0) {
		int32_t num = parse_double_array(value, config->animation_curve_tag, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_tag: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_close") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_close, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_close: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_focus") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_focus, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_focus: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opacity_fade_in") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadein, 4);
		if (num != 4) {
			mango_error(false, WLR_ERROR,
						"Failed to parse "
						"animation_curve_opafadein: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opacity_fade_out") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadeout, 4);
		if (num != 4) {
			mango_error(
				false, WLR_ERROR,
				"Failed to parse "
				"animation_curve_opafadeout: \033[1m\033[31m%s\033[0m\n",
				value);
			return false;
		}
	} else if (strcmp(key, "scroller_structs") == 0) {
		config->scroller_structs = atoi(value);
	} else if (strcmp(key, "scroller_default_proportion") == 0) {
		config->scroller_default_proportion = atof(value);
	} else if (strcmp(key, "scroller_default_proportion_single") == 0) {
		config->scroller_default_proportion_single = atof(value);
	} else if (strcmp(key, "scroller_ignore_proportion_single") == 0) {
		config->scroller_ignore_proportion_single = atoi(value);
	} else if (strcmp(key, "scroller_focus_center") == 0) {
		config->scroller_focus_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_center") == 0) {
		config->scroller_prefer_center = atoi(value);
	} else if (strcmp(key, "scroller_prefer_overspread") == 0) {
		config->scroller_prefer_overspread = atoi(value);
	} else if (strcmp(key, "edge_scroller_pointer_focus") == 0) {
		config->edge_scroller_pointer_focus = atoi(value);
	} else if (strcmp(key, "edge_scroller_focus_allow_speed") == 0) {
		config->edge_scroller_focus_allow_speed = atof(value);
	} else if (strcmp(key, "focus_cross_monitor") == 0) {
		config->focus_cross_monitor = atoi(value);
	} else if (strcmp(key, "focus_direction_only_zone_overlap") == 0) {
		config->focusdir_only_zone_overlap = atoi(value);
	} else if (strcmp(key, "exchange_cross_monitor") == 0) {
		config->exchange_cross_monitor = atoi(value);
	} else if (strcmp(key, "scratchpad_cross_monitor") == 0) {
		config->scratchpad_cross_monitor = atoi(value);
	} else if (strcmp(key, "focus_cross_tag") == 0) {
		config->focus_cross_tag = atoi(value);
	} else if (strcmp(key, "view_current_to_back") == 0) {
		config->view_current_to_back = atoi(value);
	} else if (strcmp(key, "blur") == 0) {
		config->blur = atoi(value);
	} else if (strcmp(key, "blur_layer") == 0) {
		config->blur_layer = atoi(value);
	} else if (strcmp(key, "blur_optimized") == 0) {
		config->blur_optimized = atoi(value);
	} else if (strcmp(key, "border_radius") == 0) {
		config->border_radius = atoi(value);
	} else if (strcmp(key, "blur_params_num_passes") == 0) {
		config->blur_params.num_passes = atoi(value);
	} else if (strcmp(key, "blur_params_radius") == 0) {
		config->blur_params.radius = atoi(value);
	} else if (strcmp(key, "blur_params_noise") == 0) {
		config->blur_params.noise = atof(value);
	} else if (strcmp(key, "blur_params_brightness") == 0) {
		config->blur_params.brightness = atof(value);
	} else if (strcmp(key, "blur_params_contrast") == 0) {
		config->blur_params.contrast = atof(value);
	} else if (strcmp(key, "blur_params_saturation") == 0) {
		config->blur_params.saturation = atof(value);
	} else if (strcmp(key, "shadows") == 0) {
		config->shadows = atoi(value);
	} else if (strcmp(key, "shadow_only_floating") == 0) {
		config->shadow_only_floating = atoi(value);
	} else if (strcmp(key, "layer_shadows") == 0) {
		config->layer_shadows = atoi(value);
	} else if (strcmp(key, "shadows_size") == 0) {
		config->shadows_size = atoi(value);
	} else if (strcmp(key, "shadows_blur") == 0) {
		config->shadows_blur = atof(value);
	} else if (strcmp(key, "shadows_position_x") == 0) {
		config->shadows_position_x = atoi(value);
	} else if (strcmp(key, "shadows_position_y") == 0) {
		config->shadows_position_y = atoi(value);
	} else if (strcmp(key, "single_scratchpad") == 0) {
		config->single_scratchpad = atoi(value);
	} else if (strcmp(key, "xwayland_persistence") == 0) {
		config->xwayland_persistence = atoi(value);
	} else if (strcmp(key, "xwayland_ignore_scale") == 0) {
		config->xwayland_ignore_scale = atoi(value);
	} else if (strcmp(key, "sync_obj_enable") == 0) {
		config->syncobj_enable = atoi(value);
	} else if (strcmp(key, "tag_carousel") == 0) {
		config->tag_carousel = atoi(value);
	} else if (strcmp(key, "drag_tile_refresh_interval") == 0) {
		config->drag_tile_refresh_interval = atof(value);
	} else if (strcmp(key, "drag_floating_refresh_interval") == 0) {
		config->drag_floating_refresh_interval = atof(value);
	} else if (strcmp(key, "allow_tearing") == 0) {
		config->allow_tearing = atoi(value);
	} else if (strcmp(key, "hdr_depth") == 0) {
		config->hdr_depth = atoi(value);
	} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
		config->allow_shortcuts_inhibit = atoi(value);
	} else if (strcmp(key, "disable_middle_paste") == 0) {
		config->disable_middle_paste = atoi(value);
	} else if (strcmp(key, "allow_lock_transparent") == 0) {
		config->allow_lock_transparent = atoi(value);
	} else if (strcmp(key, "auto_reload_config") == 0) {
		config->auto_reload_config = atoi(value);
	} else if (strcmp(key, "no_border_when_single") == 0) {
		config->no_border_when_single = atoi(value);
	} else if (strcmp(key, "no_radius_when_single") == 0) {
		config->no_radius_when_single = atoi(value);
	} else if (strcmp(key, "snap_distance") == 0) {
		config->snap_distance = atoi(value);
	} else if (strcmp(key, "enable_floating_snap") == 0) {
		config->enable_floating_snap = atoi(value);
	} else if (strcmp(key, "float_full_to_top") == 0) {
		config->float_full_to_top = atoi(value);
	} else if (strcmp(key, "drag_tile_to_tile") == 0) {
		config->drag_tile_to_tile = atoi(value);
	} else if (strcmp(key, "drag_tile_small") == 0) {
		config->drag_tile_small = atoi(value);
	} else if (strcmp(key, "swipe_min_threshold") == 0) {
		config->swipe_min_threshold = atoi(value);
	} else if (strcmp(key, "gesture_live") == 0) {
		config->gesture_live = atoi(value);
	} else if (strcmp(key, "gesture_swipe_distance") == 0) {
		config->gesture_swipe_distance = atoi(value);
	} else if (strcmp(key, "gesture_swipe_cancel_ratio") == 0) {
		config->gesture_swipe_cancel_ratio = atof(value);
	} else if (strcmp(key, "gesture_swipe_min_speed_to_force") == 0) {
		config->gesture_swipe_min_speed_to_force = atof(value);
	} else if (strcmp(key, "focused_opacity") == 0) {
		config->focused_opacity = atof(value);
	} else if (strcmp(key, "unfocused_opacity") == 0) {
		config->unfocused_opacity = atof(value);
	} else if (strcmp(key, "xkb_rules_rules") == 0) {
		strncpy(config->xkb_rules_rules, value,
				sizeof(config->xkb_rules_rules) - 1);
		config->xkb_rules_rules[sizeof(config->xkb_rules_rules) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_model") == 0) {
		strncpy(config->xkb_rules_model, value,
				sizeof(config->xkb_rules_model) - 1);
		config->xkb_rules_model[sizeof(config->xkb_rules_model) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_layout") == 0) {
		strncpy(config->xkb_rules_layout, value,
				sizeof(config->xkb_rules_layout) - 1);
		config->xkb_rules_layout[sizeof(config->xkb_rules_layout) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_variant") == 0) {
		strncpy(config->xkb_rules_variant, value,
				sizeof(config->xkb_rules_variant) - 1);
		config->xkb_rules_variant[sizeof(config->xkb_rules_variant) - 1] = '\0';
	} else if (strcmp(key, "xkb_rules_options") == 0) {
		strncpy(config->xkb_rules_options, value,
				sizeof(config->xkb_rules_options) - 1);
		config->xkb_rules_options[sizeof(config->xkb_rules_options) - 1] = '\0';
	} else if (strcmp(key, "scroller_proportion_preset") == 0) {
		// 1. Counts commas in value to determine how many floats to parse.
		int32_t count = 0; // Initialized to 0.
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t float_count = count + 1; // Float count is comma count + 1.

		// 2. Allocates memory for the floats.
		// Frees the old memory first (avoids leaks when the option is set
		// repeatedly).
		if (config->scroller_proportion_preset) {
			free(config->scroller_proportion_preset);
			config->scroller_proportion_preset = NULL;
			config->scroller_proportion_preset_count = 0;
		}
		config->scroller_proportion_preset =
			(float *)malloc(float_count * sizeof(float));
		if (!config->scroller_proportion_preset) {
			mango_error(false, WLR_ERROR,
						"Memory "
						"allocation failed\n");
			return false;
		}

		// 3. Parses the floats in value.
		char *value_copy = strdup(
			value); // Copies value because strtok modifies the original string.
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		float value_set;

		while (token != NULL && i < float_count) {
			if (sscanf(token, "%f", &value_set) != 1) {
				mango_error(
					false, WLR_ERROR,
					"Invalid float "
					"value in "
					"scroller_proportion_preset: \033[1m\033[31m%s\033[0m\n",
					token);
				free(value_copy);
				free(config->scroller_proportion_preset);
				config->scroller_proportion_preset = NULL;
				return false;
			}

			// Clamp the value between 0.0 and 1.0 (or your desired
			// range)
			config->scroller_proportion_preset[i] =
				CLAMP_FLOAT(value_set, 0.1f, 1.0f);

			token = strtok(NULL, ",");
			i++;
		}

		// 4. Checks that the parsed float count matches.
		if (i != float_count) {
			mango_error(
				false, WLR_ERROR,
				"Invalid "
				"scroller_proportion_preset format: \033[1m\033[31m%s\033[0m\n",
				value);
			free(value_copy);
			free(config->scroller_proportion_preset); // Frees the allocated
													  // memory.
			config->scroller_proportion_preset =
				NULL; // Prevents a dangling pointer.
			config->scroller_proportion_preset_count = 0;
			return false;
		}
		config->scroller_proportion_preset_count = float_count;

		// 5. Frees the temporary string copy.
		free(value_copy);
	} else if (strcmp(key, "circle_layout") == 0) {
		// 1. Counts commas in value to determine how many strings to parse.
		int32_t count = 0; // Initialized to 0.
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t string_count = count + 1; // String count is comma count + 1.

		// 2. Allocates memory for the string pointers.
		// Frees the old memory first (avoids leaks when the option is set
		// repeatedly).
		if (config->circle_layout) {
			for (int32_t j = 0; j < config->circle_layout_count; j++) {
				if (config->circle_layout[j])
					free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			config->circle_layout = NULL;
			config->circle_layout_count = 0;
		}
		config->circle_layout = (char **)malloc(string_count * sizeof(char *));
		if (!config->circle_layout) {
			mango_error(false, WLR_ERROR,
						"Memory "
						"allocation failed\n");
			return false;
		}
		memset(config->circle_layout, 0, string_count * sizeof(char *));

		// 3. Parses the strings in value.
		char *value_copy = strdup(
			value); // Copies value because strtok modifies the original string.
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		char *cleaned_token;
		while (token != NULL && i < string_count) {
			// Allocates memory for each string and copies the content.
			cleaned_token = sanitize_string(token);
			config->circle_layout[i] = strdup(cleaned_token);
			if (!config->circle_layout[i]) {
				mango_error(false, WLR_ERROR,
							"Memory allocation "
							"failed for "
							"string: %s\n",
							token);
				// Frees previously allocated memory.
				for (int32_t j = 0; j < i; j++) {
					free(config->circle_layout[j]);
				}
				free(config->circle_layout);
				free(value_copy);
				config->circle_layout = NULL; // Prevents a dangling pointer.
				config->circle_layout_count = 0;
				return false;
			}
			token = strtok(NULL, ",");
			i++;
		}

		// 4. Checks that the parsed string count matches.
		if (i != string_count) {
			mango_error(false, WLR_ERROR,
						"Invalid circle_layout "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			// Frees previously allocated memory.
			for (int32_t j = 0; j < i; j++) {
				free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			free(value_copy);
			config->circle_layout = NULL; // Prevents a dangling pointer.
			config->circle_layout_count = 0;
			return false;
		}
		config->circle_layout_count = string_count;

		// 5. Frees the temporary string copy.
		free(value_copy);
	} else if (strcmp(key, "new_is_master") == 0) {
		config->new_is_master = atoi(value);
	} else if (strcmp(key, "default_master_factor") == 0) {
		config->default_mfact = atof(value);
	} else if (strcmp(key, "default_master_count") == 0) {
		config->default_nmaster = atoi(value);
	} else if (strcmp(key, "tag_num") == 0) {
		config->tag_num = atoi(value);
	} else if (strcmp(key, "tag_gather") == 0) {
		config->tag_gather = atoi(value);
	} else if (strcmp(key, "center_master_overspread") == 0) {
		config->center_master_overspread = atoi(value);
	} else if (strcmp(key, "center_when_single_stack") == 0) {
		config->center_when_single_stack = atoi(value);
	} else if (strcmp(key, "dwindle_vertical_split") == 0) {
		config->dwindle_vsplit = atoi(value);
	} else if (strcmp(key, "dwindle_horizontal_split") == 0) {
		config->dwindle_hsplit = atoi(value);
	} else if (strcmp(key, "dwindle_preserve_split") == 0) {
		config->dwindle_preserve_split = atoi(value);
	} else if (strcmp(key, "dwindle_smart_split") == 0) {
		config->dwindle_smart_split = atoi(value);
	} else if (strcmp(key, "dwindle_smart_resize") == 0) {
		config->dwindle_smart_resize = atoi(value);
	} else if (strcmp(key, "dwindle_drop_simple_split") == 0) {
		config->dwindle_drop_simple_split = atoi(value);
	} else if (strcmp(key, "dwindle_manual_split") == 0) {
		config->dwindle_manual_split = atoi(value);
	} else if (strcmp(key, "dwindle_split_ratio") == 0) {
		config->dwindle_split_ratio = atof(value);
	} else if (strcmp(key, "hotarea_size") == 0) {
		config->hotarea_size = atoi(value);
	} else if (strcmp(key, "hotarea_corner") == 0) {
		config->hotarea_corner = atoi(value);
	} else if (strcmp(key, "enable_hotarea") == 0) {
		config->enable_hotarea = atoi(value);
	} else if (strcmp(key, "hotarea_disable_on_fullscreen") == 0) {
		config->hotarea_disable_on_fullscreen = atoi(value);
	} else if (strcmp(key, "overview_gap_inner") == 0) {
		config->overviewgappi = atoi(value);
	} else if (strcmp(key, "overview_gap_outer") == 0) {
		config->overviewgappo = atoi(value);
	} else if (strcmp(key, "overcircle_center_ratio") == 0) {
		config->overcircle_center_ratio = atof(value);
	} else if (strcmp(key, "jump_labels") == 0) {
		if (config->jump_labels)
			free(config->jump_labels);
		config->jump_labels = strdup(value);
	} else if (strcmp(key, "cursor_hide_timeout") == 0) {
		config->cursor_hide_timeout = atoi(value);
	} else if (strcmp(key, "cursor_hide_on_keypress") == 0) {
		config->cursor_hide_on_keypress = atoi(value);
	} else if (strcmp(key, "axis_bind_apply_timeout") == 0) {
		config->axis_bind_apply_timeout = atoi(value);
	} else if (strcmp(key, "focus_on_activate") == 0) {
		config->focus_on_activate = atoi(value);
	} else if (strcmp(key, "numlock_on") == 0) {
		config->numlockon = atoi(value);
	} else if (strcmp(key, "idle_inhibit_ignore_visible") == 0) {
		config->idleinhibit_ignore_visible = atoi(value);
	} else if (strcmp(key, "idle_inhibit_when_fullscreen") == 0) {
		config->idleinhibit_when_fullscreen = atoi(value);
	} else if (strcmp(key, "sloppy_focus") == 0) {
		config->sloppyfocus = atoi(value);
	} else if (strcmp(key, "warp_cursor") == 0) {
		config->warpcursor = atoi(value);
	} else if (strcmp(key, "drag_corner") == 0) {
		config->drag_corner = atoi(value);
	} else if (strcmp(key, "drag_warp_cursor") == 0) {
		config->drag_warp_cursor = atoi(value);
	} else if (strcmp(key, "smart_gaps") == 0) {
		config->smartgaps = atoi(value);
	} else if (strcmp(key, "monocle_tab_mode") == 0) {
		config->monocle_tab_mode = atoi(value);
	} else if (strcmp(key, "deck_tab_mode") == 0) {
		config->deck_tab_mode = atoi(value);
	} else if (strcmp(key, "repeat_rate") == 0) {
		config->repeat_rate = atoi(value);
	} else if (strcmp(key, "repeat_delay") == 0) {
		config->repeat_delay = atoi(value);
	} else if (strcmp(key, "disable_trackpad") == 0) {
		config->disable_trackpad = atoi(value);
	} else if (strcmp(key, "touch_enable") == 0) {
		config->touch_enable = atoi(value);
	} else if (strcmp(key, "touch_enable_mouse_emulation") == 0) {
		config->touch_enable_mouse_emulation = atoi(value);
	} else if (strcmp(key, "tap_to_click") == 0) {
		config->tap_to_click = atoi(value);
	} else if (strcmp(key, "tap_and_drag") == 0) {
		config->tap_and_drag = atoi(value);
	} else if (strcmp(key, "drag_lock") == 0) {
		config->drag_lock = atoi(value);
	} else if (strcmp(key, "mouse_natural_scrolling") == 0) {
		config->mouse_natural_scrolling = atoi(value);
	} else if (strcmp(key, "trackpad_natural_scrolling") == 0) {
		config->trackpad_natural_scrolling = atoi(value);
	} else if (strcmp(key, "cursor_size") == 0) {
		config->cursor_size = atoi(value);
	} else if (strcmp(key, "cursor_theme") == 0) {
		if (config->cursor_theme)
			free(config->cursor_theme);
		config->cursor_theme = strdup(value);
	} else if (strcmp(key, "group_bar_decorate_font_desc") == 0) {
		if (config->groupbardata.font_desc)
			free((void *)config->groupbardata.font_desc);
		config->groupbardata.font_desc = strdup(value);
	} else if (strcmp(key, "group_bar_decorate_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_fg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.fg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_bg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.bg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_focus_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_focus_fg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.focus_fg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_focus_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_focus_bg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.focus_bg_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_border_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"group_bar_decorate_border_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->groupbardata.border_color, color);
		}
	} else if (strcmp(key, "group_bar_decorate_border_width") == 0) {
		config->groupbardata.border_width = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "group_bar_decorate_corner_radius") == 0) {
		config->groupbardata.corner_radius = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "group_bar_decorate_padding_x") == 0) {
		config->groupbardata.padding_x = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "group_bar_decorate_padding_y") == 0) {
		config->groupbardata.padding_y = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "tab_bar_decorate_font_desc") == 0) {
		if (config->tabbardata.font_desc)
			free((void *)config->tabbardata.font_desc);
		config->tabbardata.font_desc = strdup(value);
	} else if (strcmp(key, "tab_bar_decorate_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"tab_bar_decorate_fg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->tabbardata.fg_color, color);
		}
	} else if (strcmp(key, "tab_bar_decorate_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"tab_bar_decorate_bg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->tabbardata.bg_color, color);
		}
	} else if (strcmp(key, "tab_bar_decorate_focus_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"tab_bar_decorate_focus_fg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->tabbardata.focus_fg_color, color);
		}
	} else if (strcmp(key, "tab_bar_decorate_focus_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"tab_bar_decorate_focus_bg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->tabbardata.focus_bg_color, color);
		}
	} else if (strcmp(key, "tab_bar_decorate_border_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"tab_bar_decorate_border_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->tabbardata.border_color, color);
		}
	} else if (strcmp(key, "tab_bar_decorate_border_width") == 0) {
		config->tabbardata.border_width = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "tab_bar_decorate_corner_radius") == 0) {
		config->tabbardata.corner_radius = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "tab_bar_decorate_padding_x") == 0) {
		config->tabbardata.padding_x = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "tab_bar_decorate_padding_y") == 0) {
		config->tabbardata.padding_y = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_font_desc") == 0) {
		if (config->jumplabeldata.font_desc)
			free((void *)config->jumplabeldata.font_desc);
		config->jumplabeldata.font_desc = strdup(value);
	} else if (strcmp(key, "jump_label_decorate_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_fg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.fg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_bg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.bg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_focus_fg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_focus_fg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.focus_fg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_focus_bg_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_focus_bg_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.focus_bg_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_border_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"jump_label_decorate_border_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->jumplabeldata.border_color, color);
		}
	} else if (strcmp(key, "jump_label_decorate_border_width") == 0) {
		config->jumplabeldata.border_width = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_corner_radius") == 0) {
		config->jumplabeldata.corner_radius = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_padding_x") == 0) {
		config->jumplabeldata.padding_x = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "jump_label_decorate_padding_y") == 0) {
		config->jumplabeldata.padding_y = CLAMP_INT(atoi(value), 0, 100);
	} else if (strcmp(key, "mouse_accel_profile") == 0) {
		config->mouse_accel_profile = atoi(value);
	} else if (strcmp(key, "mouse_accel_speed") == 0) {
		config->mouse_accel_speed = atof(value);
	} else if (strcmp(key, "trackpad_accel_profile") == 0) {
		config->trackpad_accel_profile = atoi(value);
	} else if (strcmp(key, "trackpad_accel_speed") == 0) {
		config->trackpad_accel_speed = atof(value);
	} else if (strcmp(key, "mouse_left_handed") == 0) {
		config->mouse_left_handed = atoi(value);
	} else if (strcmp(key, "mouse_middle_button_emulation") == 0) {
		config->mouse_middle_button_emulation = atoi(value);
	} else if (strcmp(key, "mouse_scroll_method") == 0) {
		config->mouse_scroll_method = atoi(value);
	} else if (strcmp(key, "mouse_scroll_button") == 0) {
		config->mouse_scroll_button = atoi(value);
	} else if (strcmp(key, "mouse_click_method") == 0) {
		config->mouse_click_method = atoi(value);
	} else if (strcmp(key, "mouse_send_events_mode") == 0) {
		config->mouse_send_events_mode = atoi(value);
	} else if (strcmp(key, "trackpad_left_handed") == 0) {
		config->trackpad_left_handed = atoi(value);
	} else if (strcmp(key, "trackpad_middle_button_emulation") == 0) {
		config->trackpad_middle_button_emulation = atoi(value);
	} else if (strcmp(key, "trackpad_disable_while_typing") == 0) {
		config->trackpad_disable_while_typing = atoi(value);
	} else if (strcmp(key, "trackpad_scroll_method") == 0) {
		config->trackpad_scroll_method = atoi(value);
	} else if (strcmp(key, "trackpad_scroll_button") == 0) {
		config->trackpad_scroll_button = atoi(value);
	} else if (strcmp(key, "trackpad_click_method") == 0) {
		config->trackpad_click_method = atoi(value);
	} else if (strcmp(key, "trackpad_send_events_mode") == 0) {
		config->trackpad_send_events_mode = atoi(value);
	} else if (strcmp(key, "send_events_mode") == 0) {
		config->send_events_mode = atoi(value);
	} else if (strcmp(key, "button_map") == 0) {
		config->button_map = atoi(value);
	} else if (strcmp(key, "axis_scroll_factor") == 0) {
		config->axis_scroll_factor = atof(value);
	} else if (strcmp(key, "trackpad_scroll_factor") == 0) {
		config->trackpad_scroll_factor = atof(value);
	} else if (strcmp(key, "gap_inner_horizontal") == 0) {
		config->gappih = atoi(value);
	} else if (strcmp(key, "gap_inner_vertical") == 0) {
		config->gappiv = atoi(value);
	} else if (strcmp(key, "gap_outer_horizontal") == 0) {
		config->gappoh = atoi(value);
	} else if (strcmp(key, "gap_outer_vertical") == 0) {
		config->gappov = atoi(value);
	} else if (strcmp(key, "scratchpad_width_ratio") == 0) {
		config->scratchpad_width_ratio = atof(value);
	} else if (strcmp(key, "scratchpad_height_ratio") == 0) {
		config->scratchpad_height_ratio = atof(value);
	} else if (strcmp(key, "special_dim") == 0) {
		config->special_dim = atof(value);
	} else if (strcmp(key, "special_gap_inner_horizontal") == 0) {
		config->special_gappih = atoi(value);
	} else if (strcmp(key, "special_gap_inner_vertical") == 0) {
		config->special_gappiv = atoi(value);
	} else if (strcmp(key, "special_gap_outer_horizontal") == 0) {
		config->special_gappoh = atoi(value);
	} else if (strcmp(key, "special_gap_outer_vertical") == 0) {
		config->special_gappov = atoi(value);
	} else if (strcmp(key, "border_px") == 0) {
		config->borderpx = atoi(value);
	} else if (strcmp(key, "group_bar_height") == 0) {
		config->group_bar_height = atoi(value);
	} else if (strcmp(key, "tab_bar_height") == 0) {
		config->tab_bar_height = atoi(value);
	} else if (strcmp(key, "root_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid rootcolor "
						"format: "
						"\033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->rootcolor, color);
		}

	} else if (strcmp(key, "shadows_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid shadowscolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->shadowscolor, color);
		}
	} else if (strcmp(key, "dim_enable") == 0) {
		config->dim_enable = atoi(value);
	} else if (strcmp(key, "dim_focused_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid dim_focused_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->dim_focused_color, color);
		}
	} else if (strcmp(key, "dim_unfocused_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid dim_unfocused_color "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->dim_unfocused_color, color);
		}
	} else if (strcmp(key, "border_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid bordercolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->bordercolor, color);
		}
	} else if (strcmp(key, "drop_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid dropcolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->dropcolor, color);
		}
	} else if (strcmp(key, "split_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid splitcolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->splitcolor, color);
		}
	} else if (strcmp(key, "focus_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid focuscolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->focuscolor, color);
		}
	} else if (strcmp(key, "maximized_screen_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"maximizescreencolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->maximizescreencolor, color);
		}
	} else if (strcmp(key, "urgent_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid urgentcolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->urgentcolor, color);
		}
	} else if (strcmp(key, "scratchpad_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid "
						"scratchpadcolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->scratchpadcolor, color);
		}
	} else if (strcmp(key, "global_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid globalcolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->globalcolor, color);
		}
	} else if (strcmp(key, "overlay_color") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			mango_error(false, WLR_ERROR,
						"Invalid overlaycolor "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		} else {
			convert_hex_to_rgba(config->overlaycolor, color);
		}
	} else if (strcmp(key, "monitor_rule") == 0) {
		config->monitor_rules =
			realloc(config->monitor_rules, (config->monitor_rules_count + 1) *
											   sizeof(ConfigMonitorRule));
		if (!config->monitor_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for monitor rules\n");
			return false;
		}

		ConfigMonitorRule *rule =
			&config->monitor_rules[config->monitor_rules_count];
		memset(rule, 0, sizeof(ConfigMonitorRule));

		// Sets default values.
		rule->name = NULL;
		rule->make = NULL;
		rule->model = NULL;
		rule->serial = NULL;
		rule->rr = 0;
		rule->scale = 1.0f;
		rule->x = INT32_MAX;
		rule->y = INT32_MAX;
		rule->width = -1;
		rule->height = -1;
		rule->refresh = 0.0f;
		rule->vrr = 0;
		rule->hdr = 0;
		rule->hdr_min_lum = 0.0f;
		rule->hdr_max_lum = 0.0f;
		rule->hdr_max_avg_lum = 0.0f;
		rule->hdr_force = 0;
		rule->icc = NULL;
		rule->custom = 0;
		rule->disable = 0;
		rule->primary = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "name") == 0) {
					rule->name = strdup(val);
				} else if (strcmp(key, "make") == 0) {
					rule->make = strdup(val);
				} else if (strcmp(key, "model") == 0) {
					rule->model = strdup(val);
				} else if (strcmp(key, "serial") == 0) {
					rule->serial = strdup(val);
				} else if (strcmp(key, "rr") == 0) {
					rule->rr = CLAMP_INT(atoi(val), 0, 7);
				} else if (strcmp(key, "scale") == 0) {
					rule->scale = CLAMP_FLOAT(atof(val), 0.001f, 1000.0f);
				} else if (strcmp(key, "x") == 0) {
					rule->x = atoi(val);
				} else if (strcmp(key, "y") == 0) {
					rule->y = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = CLAMP_INT(atoi(val), 1, INT32_MAX);
				} else if (strcmp(key, "height") == 0) {
					rule->height = CLAMP_INT(atoi(val), 1, INT32_MAX);
				} else if (strcmp(key, "refresh") == 0) {
					rule->refresh = CLAMP_FLOAT(atof(val), 0.001f, 1000.0f);
				} else if (strcmp(key, "vrr") == 0) {
					rule->vrr = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "hdr") == 0) {
					rule->hdr = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "hdr_min_lum") == 0) {
					// cd/m². OLED blacks sit well below 0.01, so the floor has
					// to allow small fractions -- do not clamp to >= 1.
					rule->hdr_min_lum = CLAMP_FLOAT(atof(val), 0.0f, 10000.0f);
				} else if (strcmp(key, "hdr_max_lum") == 0) {
					rule->hdr_max_lum = CLAMP_FLOAT(atof(val), 0.0f, 10000.0f);
				} else if (strcmp(key, "hdr_max_avg_lum") == 0) {
					rule->hdr_max_avg_lum =
						CLAMP_FLOAT(atof(val), 0.0f, 10000.0f);
				} else if (strcmp(key, "hdr_force") == 0) {
					rule->hdr_force = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "icc") == 0) {
					free(rule->icc);
					rule->icc = strdup(val);
				} else if (strcmp(key, "disable") == 0) {
					rule->disable = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "primary") == 0) {
					rule->primary = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "custom") == 0) {
					rule->custom = CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"monitor rule "
								"option:\033[1m\033[31m%s\033[0m\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		if (!rule->name && !rule->make && !rule->model && !rule->serial) {
			mango_error(false, WLR_ERROR,
						"Monitor rule "
						"must have at least one of the following "
						"options: name, make, model, serial\n");
			return false;
		}

		config->monitor_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "tag_rule") == 0) {
		config->tag_rules =
			realloc(config->tag_rules,
					(config->tag_rules_count + 1) * sizeof(ConfigTagRule));
		if (!config->tag_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for tag rules\n");
			return false;
		}

		ConfigTagRule *rule = &config->tag_rules[config->tag_rules_count];
		memset(rule, 0, sizeof(ConfigTagRule));

		// Sets default values.
		rule->id = 0;
		rule->id_wildcard = false;
		rule->layout_name = NULL;
		rule->monitor_name = NULL;
		rule->monitor_make = NULL;
		rule->monitor_model = NULL;
		rule->monitor_serial = NULL;
		rule->nmaster = 0;
		rule->mfact = 0.0f;
		rule->no_render_border = 0;
		rule->open_as_floating = 0;
		rule->no_hide = 0;
		rule->scroller_default_proportion = 0.0f;
		rule->scroller_default_proportion_single = 0.0f;
		rule->scroller_ignore_proportion_single = -1;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "id") == 0) {
					if (strcmp(val, "*") == 0) {
						rule->id_wildcard = true;
						rule->id = 0;
					} else {
						rule->id_wildcard = false;
						rule->id = CLAMP_INT(atoi(val), 0, LENGTH(tags));
					}
				} else if (strcmp(key, "layout_name") == 0) {
					rule->layout_name = strdup(val);
				} else if (strcmp(key, "monitor_name") == 0) {
					rule->monitor_name = strdup(val);
				} else if (strcmp(key, "monitor_make") == 0) {
					rule->monitor_make = strdup(val);
				} else if (strcmp(key, "monitor_model") == 0) {
					rule->monitor_model = strdup(val);
				} else if (strcmp(key, "monitor_serial") == 0) {
					rule->monitor_serial = strdup(val);
				} else if (strcmp(key, "no_render_border") == 0) {
					rule->no_render_border = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "open_as_floating") == 0) {
					rule->open_as_floating = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "no_hide") == 0) {
					rule->no_hide = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "master_count") == 0) {
					rule->nmaster = CLAMP_INT(atoi(val), 1, 99);
				} else if (strcmp(key, "master_factor") == 0) {
					rule->mfact = CLAMP_FLOAT(atof(val), 0.1f, 0.9f);
				} else if (strcmp(key, "scroller_default_proportion") == 0) {
					rule->scroller_default_proportion =
						CLAMP_FLOAT(atof(val), 0.0f, 1.0f);
				} else if (strcmp(key, "scroller_default_proportion_single") ==
						   0) {
					rule->scroller_default_proportion_single =
						CLAMP_FLOAT(atof(val), 0.0f, 1.0f);
				} else if (strcmp(key, "scroller_ignore_proportion_single") ==
						   0) {
					rule->scroller_ignore_proportion_single =
						CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"tag rule "
								"option:\033[1m\033[31m%s\033[0m\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		config->tag_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "layer_rule") == 0) {
		config->layer_rules =
			realloc(config->layer_rules,
					(config->layer_rules_count + 1) * sizeof(ConfigLayerRule));
		if (!config->layer_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for layer rules\n");
			return false;
		}

		ConfigLayerRule *rule = &config->layer_rules[config->layer_rules_count];
		memset(rule, 0, sizeof(ConfigLayerRule));

		// Sets default values.
		rule->layer_name = NULL;
		rule->animation_type_open = ANIM_TYPE_UNSET;
		rule->animation_type_close = ANIM_TYPE_UNSET;
		rule->shield_when_capture = 0;
		rule->no_blur = 0;
		rule->no_animation = 0;
		rule->no_shadow = 0;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "layer_name") == 0) {
					rule->layer_name = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = animation_type_from_string(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close =
						animation_type_from_string(val);
				} else if (strcmp(key, "shield_when_capture") == 0) {
					rule->shield_when_capture = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "no_blur") == 0) {
					rule->no_blur = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "no_animation") == 0) {
					rule->no_animation = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "no_shadow") == 0) {
					rule->no_shadow = CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"layer rule "
								"option:\033[1m\033[31m%s\033[0m\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		// Uses the default value when no layout name is given.
		if (rule->layer_name == NULL) {
			rule->layer_name = strdup("default");
		}

		config->layer_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "window_rule") == 0 ||
			   strcmp(key, "window_rule_once") == 0) {
		config->window_rules =
			realloc(config->window_rules,
					(config->window_rules_count + 1) * sizeof(ConfigWinRule));
		if (!config->window_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for window rules\n");
			return false;
		}

		ConfigWinRule *rule = &config->window_rules[config->window_rules_count];
		memset(rule, 0, sizeof(ConfigWinRule));

		// int32_t rule value, relay to a client property

		if (strcmp(key, "window_rule_once") == 0) {
			rule->is_once = 1;
			rule->is_once_applied = 0;
		} else {
			rule->is_once = 0;
			rule->is_once_applied = 0;
		}

		rule->isfloating = -1;
		rule->isfullscreen = -1;
		rule->isfakefullscreen = -1;
		rule->no_border = -1;
		rule->no_shadow = -1;
		rule->no_radius = -1;
		rule->no_animation = -1;
		rule->isopensilent = -1;
		rule->istagsilent = -1;
		rule->isnamedscratchpad = -1;
		rule->isunglobal = -1;
		rule->isglobal = -1;
		rule->isoverlay = -1;
		rule->shield_when_capture = -1;
		rule->allow_shortcuts_inhibit = -1;
		rule->ignore_maximize = -1;
		rule->ignore_minimize = -1;
		rule->no_size_hint = -1;
		rule->idleinhibit_when_focus = -1;
		rule->vrr_only_fullscreen = -1;
		rule->force_render = -1;
		rule->activation_bypass = -1;
		rule->isterm = -1;
		rule->allow_csd = -1;
		rule->force_fakemaximize = -1;
		rule->force_tiled_state = -1;
		rule->force_tearing = -1;
		rule->no_swallow = -1;
		rule->confine_pointer = -1;
		rule->no_blur = -1;
		rule->no_focus = -1;
		rule->no_fade_in = -1;
		rule->no_fade_out = -1;
		rule->no_force_center = -1;

		// string rule value, relay to a client property
		rule->animation_type_open = ANIM_TYPE_UNSET;
		rule->animation_type_close = ANIM_TYPE_UNSET;

		// float rule value, relay to a client property
		rule->focused_opacity = 0;
		rule->unfocused_opacity = 0;
		rule->scroller_proportion_single = 0.0f;
		rule->scroller_proportion = 0;

		// special rule value,not directly set to client property
		rule->tags = 0;
		rule->offsetx = 0;
		rule->offsety = 0;
		rule->width = 0;
		rule->height = 0;
		rule->monitor = NULL;
		rule->id = NULL;
		rule->title = NULL;

		rule->globalkeybinding = (KeyBinding){0};

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "is_floating") == 0) {
					rule->isfloating = atoi(val);
				} else if (strcmp(key, "title") == 0) {
					rule->title = strdup(val);
				} else if (strcmp(key, "app_id") == 0) {
					rule->id = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = animation_type_from_string(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close =
						animation_type_from_string(val);
				} else if (strcmp(key, "tags") == 0) {
					rule->tags = parse_tag_mask(val);
				} else if (strcmp(key, "monitor") == 0) {
					rule->monitor = strdup(val);
				} else if (strcmp(key, "offset_x") == 0) {
					rule->offsetx = atoi(val);
				} else if (strcmp(key, "offset_y") == 0) {
					rule->offsety = atoi(val);
				} else if (strcmp(key, "no_focus") == 0) {
					rule->no_focus = atoi(val);
				} else if (strcmp(key, "no_fade_in") == 0) {
					rule->no_fade_in = atoi(val);
				} else if (strcmp(key, "no_fade_out") == 0) {
					rule->no_fade_out = atoi(val);
				} else if (strcmp(key, "no_force_center") == 0) {
					rule->no_force_center = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = atof(val);
				} else if (strcmp(key, "height") == 0) {
					rule->height = atof(val);
				} else if (strcmp(key, "no_border") == 0) {
					rule->no_border = atoi(val);
				} else if (strcmp(key, "no_shadow") == 0) {
					rule->no_shadow = atoi(val);
				} else if (strcmp(key, "no_radius") == 0) {
					rule->no_radius = atoi(val);
				} else if (strcmp(key, "no_animation") == 0) {
					rule->no_animation = atoi(val);
				} else if (strcmp(key, "is_open_silent") == 0) {
					rule->isopensilent = atoi(val);
				} else if (strcmp(key, "is_tag_silent") == 0) {
					rule->istagsilent = atoi(val);
				} else if (strcmp(key, "is_named_scratchpad") == 0) {
					rule->isnamedscratchpad = atoi(val);
				} else if (strcmp(key, "is_unmanaged_global") == 0) {
					rule->isunglobal = atoi(val);
				} else if (strcmp(key, "is_global") == 0) {
					rule->isglobal = atoi(val);
				} else if (strcmp(key, "scroller_proportion_single") == 0) {
					rule->scroller_proportion_single = atof(val);
				} else if (strcmp(key, "unfocused_opacity") == 0) {
					rule->unfocused_opacity = atof(val);
				} else if (strcmp(key, "focused_opacity") == 0) {
					rule->focused_opacity = atof(val);
				} else if (strcmp(key, "is_overlay") == 0) {
					rule->isoverlay = atoi(val);
				} else if (strcmp(key, "shield_when_capture") == 0) {
					rule->shield_when_capture = atoi(val);
				} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
					rule->allow_shortcuts_inhibit = atoi(val);
				} else if (strcmp(key, "ignore_maximize") == 0) {
					rule->ignore_maximize = atoi(val);
				} else if (strcmp(key, "ignore_minimize") == 0) {
					rule->ignore_minimize = atoi(val);
				} else if (strcmp(key, "no_size_hint") == 0) {
					rule->no_size_hint = atoi(val);
				} else if (strcmp(key, "idle_inhibit_when_focus") == 0) {
					rule->idleinhibit_when_focus = atoi(val);
				} else if (strcmp(key, "vrr_only_fullscreen") == 0) {
					rule->vrr_only_fullscreen = atoi(val);
				} else if (strcmp(key, "force_render") == 0) {
					rule->force_render = atoi(val);
				} else if (strcmp(key, "activation_bypass") == 0) {
					rule->activation_bypass = atoi(val);
				} else if (strcmp(key, "is_term") == 0) {
					rule->isterm = atoi(val);
				} else if (strcmp(key, "allow_csd") == 0) {
					rule->allow_csd = atoi(val);
				} else if (strcmp(key, "force_fake_maximize") == 0) {
					rule->force_fakemaximize = atoi(val);
				} else if (strcmp(key, "force_tiled_state") == 0) {
					rule->force_tiled_state = atoi(val);
				} else if (strcmp(key, "force_tearing") == 0) {
					rule->force_tearing = atoi(val);
				} else if (strcmp(key, "no_swallow") == 0) {
					rule->no_swallow = atoi(val);
				} else if (strcmp(key, "confine_pointer") == 0) {
					rule->confine_pointer = atoi(val);
				} else if (strcmp(key, "no_blur") == 0) {
					rule->no_blur = atoi(val);
				} else if (strcmp(key, "scroller_proportion") == 0) {
					rule->scroller_proportion = atof(val);
				} else if (strcmp(key, "is_fullscreen") == 0) {
					rule->isfullscreen = atoi(val);
				} else if (strcmp(key, "is_fake_fullscreen") == 0) {
					rule->isfakefullscreen = atoi(val);
				} else if (strcmp(key, "global_key_binding") == 0) {
					char mod_str[256], keysym_str[256];
					sscanf(val, "%255[^-]-%255[a-zA-Z]", mod_str, keysym_str);
					trim_whitespace(mod_str);
					trim_whitespace(keysym_str);
					rule->globalkeybinding.mod = parse_mod(mod_str);
					rule->globalkeybinding.keysymcode =
						parse_key(keysym_str, false);
					if (rule->globalkeybinding.mod == UINT32_MAX) {
						return false;
					}
					if (rule->globalkeybinding.keysymcode.type ==
							KEY_TYPE_SYM &&
						rule->globalkeybinding.keysymcode.keysym ==
							XKB_KEY_NoSymbol) {
						return false;
					}
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown "
								"window rule "
								"option:\033[1m\033[31m%s\033[0m\n",
								key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}
		config->window_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "device_rule") == 0) {
		config->device_rules =
			realloc(config->device_rules, (config->device_rules_count + 1) *
											  sizeof(ConfigDeviceRule));
		if (!config->device_rules) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for device rules\n");
			return false;
		}

		ConfigDeviceRule *rule =
			&config->device_rules[config->device_rules_count];
		memset(rule, 0, sizeof(ConfigDeviceRule));

		// Defaults: -1 / UINT32_MAX / empty string mean unset and fall back to
		// the global config.
		rule->repeat_rate = -1;
		rule->repeat_delay = -1;
		rule->natural_scrolling = -1;
		rule->accel_profile = -1;
		rule->left_handed = -1;
		rule->middle_button_emulation = -1;
		rule->scroll_method = UINT32_MAX;
		rule->scroll_button = UINT32_MAX;
		rule->click_method = UINT32_MAX;
		rule->send_events_mode = UINT32_MAX;
		rule->tap_to_click = -1;
		rule->tap_and_drag = -1;
		rule->drag_lock = -1;
		rule->button_map = UINT32_MAX;
		rule->disable_while_typing = -1;
		rule->map_focus_monitor = -1;
		rule->accel_speed = NAN;

		bool parse_error = false;
		char *token = strtok(value, ",");
		while (token != NULL) {
			char *colon = strchr(token, ':');
			if (colon != NULL) {
				*colon = '\0';
				char *key = token;
				char *val = colon + 1;

				trim_whitespace(key);
				trim_whitespace(val);

				if (strcmp(key, "name") == 0) {
					rule->name = strdup(val);
				} else if (strcmp(key, "type") == 0) {
					/* "touchpad" was the historical name of this device
					 * type; keep it as a deprecated alias so existing
					 * rules are not silently dropped. */
					const char *resolved = val;
					if (strcmp(val, "touchpad") == 0) {
						mango_error(false, WLR_INFO,
									"device rule type "
									"\033[1;36mtouchpad\033[0m is "
									"deprecated, use \033[1;36mtrackpad\033[0m "
									"instead\n");
						resolved = "trackpad";
					}
					if (strcmp(resolved, "keyboard") == 0 ||
						strcmp(resolved, "pointer") == 0 ||
						strcmp(resolved, "trackpad") == 0 ||
						strcmp(resolved, "touch") == 0 ||
						strcmp(resolved, "switch") == 0 ||
						strcmp(resolved, "tablet") == 0 ||
						strcmp(resolved, "pad") == 0) {
						snprintf(rule->type, sizeof(rule->type), "%s",
								 resolved);
					} else {
						mango_error(false, WLR_ERROR,
									"Invalid device rule type: "
									"\033[1m\033[31m%s\033[0m (expected one of "
									"keyboard, pointer, trackpad, touch, "
									"switch, tablet, pad)\n",
									val);
						parse_error = true;
					}
				} else if (strcmp(key, "repeat_rate") == 0) {
					rule->repeat_rate = CLAMP_INT(atoi(val), 0, 1000);
				} else if (strcmp(key, "repeat_delay") == 0) {
					rule->repeat_delay = CLAMP_INT(atoi(val), 0, 10000);
				} else if (strcmp(key, "kb_rules") == 0) {
					snprintf(rule->kb_rules, sizeof(rule->kb_rules), "%s", val);
				} else if (strcmp(key, "kb_model") == 0) {
					snprintf(rule->kb_model, sizeof(rule->kb_model), "%s", val);
				} else if (strcmp(key, "kb_layout") == 0) {
					snprintf(rule->kb_layout, sizeof(rule->kb_layout), "%s",
							 val);
				} else if (strcmp(key, "kb_variant") == 0) {
					snprintf(rule->kb_variant, sizeof(rule->kb_variant), "%s",
							 val);
				} else if (strcmp(key, "kb_options") == 0) {
					snprintf(rule->kb_options, sizeof(rule->kb_options), "%s",
							 val);
				} else if (strcmp(key, "natural_scrolling") == 0) {
					rule->natural_scrolling = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "accel_profile") == 0) {
					rule->accel_profile = CLAMP_INT(atoi(val), 0, 2);
				} else if (strcmp(key, "accel_speed") == 0) {
					rule->accel_speed = CLAMP_FLOAT(atof(val), -1.0, 1.0);
				} else if (strcmp(key, "left_handed") == 0) {
					rule->left_handed = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "middle_button_emulation") == 0) {
					rule->middle_button_emulation = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "scroll_method") == 0) {
					rule->scroll_method = (uint32_t)atoi(val);
				} else if (strcmp(key, "scroll_button") == 0) {
					rule->scroll_button = (uint32_t)atoi(val);
				} else if (strcmp(key, "click_method") == 0) {
					rule->click_method = (uint32_t)atoi(val);
				} else if (strcmp(key, "send_events_mode") == 0) {
					rule->send_events_mode = (uint32_t)atoi(val);
				} else if (strcmp(key, "tap_to_click") == 0) {
					rule->tap_to_click = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "tap_and_drag") == 0) {
					rule->tap_and_drag = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "drag_lock") == 0) {
					rule->drag_lock = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "button_map") == 0) {
					rule->button_map = (uint32_t)atoi(val);
				} else if (strcmp(key, "disable_while_typing") == 0) {
					rule->disable_while_typing = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "monitor") == 0) {
					snprintf(rule->monitor, sizeof(rule->monitor), "%s", val);
				} else if (strcmp(key, "map_focus_monitor") == 0) {
					rule->map_focus_monitor = CLAMP_INT(atoi(val), 0, 1);
				} else {
					mango_error(false, WLR_ERROR,
								"Unknown device rule option: "
								"\033[1m\033[31m%s\033[0m\n",
								key);
					parse_error = true;
				}
			} else {
				mango_error(
					false, WLR_ERROR,
					"Invalid device rule format: \033[1m\033[31m%s\033[0m\n",
					token);
				parse_error = true;
			}
			token = strtok(NULL, ",");
		}
		config->device_rules_count++;
		return !parse_error;
	} else if (strncmp(key, "env", 3) == 0) {

		char env_type[256], env_value[256];
		if (sscanf(value, "%255[^,],%255[^\n]", env_type, env_value) < 2) {
			mango_error(false, WLR_ERROR,
						"Invalid bind format: "
						"\033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
		trim_whitespace(env_type);
		trim_whitespace(env_value);

		ConfigEnv *env = calloc(1, sizeof(ConfigEnv));

		const char *needle = "~/";

		if (strstr(env_value, needle)) {
			const char *home = getenv("HOME");

			size_t rlen = 1; // replace only ~

			if (!home) {
				free(env);
				mango_error(false, WLR_ERROR,
							"HOME environment "
							"variable not set.\n");
				return false;
			}

			size_t hlen = strlen(home);
			size_t len = strlen(env_value) + 1;

			for (char *p = strstr(env_value, needle); p;
				 p = strstr(p + rlen, needle))
				len += hlen - rlen;

			env->value = malloc(len);
			if (!env->value) {
				free(env);
				mango_error(false, WLR_ERROR,
							"Failed to "
							"allocate memory while expanding $HOME\n");
				return false;
			}

			char *substr;
			char *src = env_value;
			char *dst = env->value;

			while ((substr = strstr(src, needle))) {
				size_t n = substr - src;
				memcpy(dst, src, n);
				dst += n;
				memcpy(dst, home, hlen);
				dst += hlen;
				src = substr + rlen;
			}

			strcpy(dst, src);
		} else {
			env->value = strdup(env_value);
		}

		env->type = strdup(env_type);
		ConfigEnv **new_env = realloc(config->env, (config->env_count + 1) *
													   sizeof(*config->env));
		if (!new_env) {
			free(env->type);
			free(env->value);
			free(env);
			mango_error(false, WLR_ERROR,
						"Failed to "
						"allocate memory for env\n");
			return false;
		}
		config->env = new_env;

		config->env[config->env_count] = env;
		config->env_count++;

	} else if (strcmp(key, "var") == 0) {
		char *comma = strchr(value, ',');
		if (!comma) {
			mango_error(false, WLR_ERROR,
						"Invalid variable format (expected "
						"\033[1mname,value\033[0m): \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
		*comma = '\0';
		char *name = value;
		char *var_value = comma + 1;
		trim_whitespace(name);
		trim_whitespace(var_value);

		if (!is_valid_var_name(name)) {
			mango_error(false, WLR_ERROR,
						"Invalid variable name (allowed: "
						"[A-Za-z_][A-Za-z0-9_]*): \033[1m\033[31m%s\033[0m\n",
						name);
			return false;
		}
		set_config_var(name, var_value);

	} else if (strncmp(key, "exec", 9) == 0) {
		char **new_exec =
			realloc(config->exec, (config->exec_count + 1) * sizeof(char *));
		if (!new_exec) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for exec\n");
			return false;
		}
		config->exec = new_exec;

		config->exec[config->exec_count] = strdup(value);
		if (!config->exec[config->exec_count]) {
			mango_error(false, WLR_ERROR,
						"Failed to "
						"duplicate exec string\n");
			return false;
		}

		config->exec_count++;

	} else if (strncmp(key, "exec_once", 9) == 0) {

		char **new_exec_once = realloc(
			config->exec_once, (config->exec_once_count + 1) * sizeof(char *));
		if (!new_exec_once) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for exec_once\n");
			return false;
		}
		config->exec_once = new_exec_once;

		config->exec_once[config->exec_once_count] = strdup(value);
		if (!config->exec_once[config->exec_once_count]) {
			mango_error(false, WLR_ERROR,
						"Failed to duplicate "
						"exec_once string\n");
			return false;
		}

		config->exec_once_count++;

	} else if (regex_match("^bind[s|l|r|p|c]*$", key)) {
		config->key_bindings =
			realloc(config->key_bindings,
					(config->key_bindings_count + 1) * sizeof(KeyBinding));
		if (!config->key_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for key bindings\n");
			return false;
		}

		KeyBinding *binding = &config->key_bindings[config->key_bindings_count];
		memset(binding, 0, sizeof(KeyBinding));
		binding->line_number = line_number;
		binding->file_index = config_current_file_index();

		char mod_str[256], keysym_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, keysym_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid bind format: "
						"\033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
		trim_whitespace(mod_str);
		trim_whitespace(keysym_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		strcpy(binding->mode, config->keymode);
		if (strcmp(binding->mode, "common") == 0) {
			binding->iscommonmode = true;
			binding->isdefaultmode = false;
		} else if (strcmp(binding->mode, "default") == 0) {
			binding->isdefaultmode = true;
			binding->iscommonmode = false;
		} else {
			binding->isdefaultmode = false;
			binding->iscommonmode = false;
		}

		parse_bind_flags(key, binding);
		binding->keysymcode =
			parse_key(keysym_str, binding->keysymcode.type == KEY_TYPE_SYM);
		binding->mod = parse_mod(mod_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);
		if (!binding->func || binding->mod == UINT32_MAX ||
			(binding->keysymcode.type == KEY_TYPE_SYM &&
			 binding->keysymcode.keysym == XKB_KEY_NoSymbol)) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in bind: "
							"\033[1m\033[31m%s\033[0m\n",
							func_name);
			return false;
		} else {
			config->key_bindings_count++;
		}

	} else if (strncmp(key, "mousebind", 9) == 0) {
		config->mouse_bindings =
			realloc(config->mouse_bindings,
					(config->mouse_bindings_count + 1) * sizeof(MouseBinding));
		if (!config->mouse_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for mouse bindings\n");
			return false;
		}

		MouseBinding *binding =
			&config->mouse_bindings[config->mouse_bindings_count];
		memset(binding, 0, sizeof(MouseBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = config_current_file_index();

		char mod_str[256], button_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, button_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid mousebind "
						"format: "
						"\033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
		trim_whitespace(mod_str);
		trim_whitespace(button_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->button = parse_button(button_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;

		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);
		if (!binding->func || binding->mod == UINT32_MAX ||
			binding->button == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}

			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in "
							"mousebind: \033[1m\033[31m%s\033[0m\n",
							func_name);
			return false;
		} else {
			config->mouse_bindings_count++;
		}
	} else if (strncmp(key, "axisbind", 8) == 0) {
		config->axis_bindings =
			realloc(config->axis_bindings,
					(config->axis_bindings_count + 1) * sizeof(AxisBinding));
		if (!config->axis_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for axis bindings\n");
			return false;
		}

		AxisBinding *binding =
			&config->axis_bindings[config->axis_bindings_count];
		memset(binding, 0, sizeof(AxisBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = config_current_file_index();

		char mod_str[256], dir_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^\n]",
				   mod_str, dir_str, func_name, arg_value, arg_value2,
				   arg_value3, arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid axisbind "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}

		trim_whitespace(mod_str);
		trim_whitespace(dir_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->dir = parse_direction(dir_str);
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func || binding->mod == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in "
							"axisbind: \033[1m\033[31m%s\033[0m\n",
							func_name);
			return false;
		} else {
			config->axis_bindings_count++;
		}

	} else if (strncmp(key, "switchbind", 10) == 0) {
		SwitchBinding *new_switch_bindings = realloc(
			config->switch_bindings,
			(config->switch_bindings_count + 1) * sizeof(SwitchBinding));
		if (!new_switch_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for switch bindings\n");
			return false;
		}
		config->switch_bindings = new_switch_bindings;

		SwitchBinding *binding =
			&config->switch_bindings[config->switch_bindings_count];
		memset(binding, 0, sizeof(SwitchBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = config_current_file_index();

		char fold_str[256], func_name[256],
			arg_value[256] = "0\0", arg_value2[256] = "0\0",
			arg_value3[256] = "0\0", arg_value4[256] = "0\0",
			arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^\n]",
				   fold_str, func_name, arg_value, arg_value2, arg_value3,
				   arg_value4, arg_value5) < 3) {
			mango_error(false, WLR_ERROR,
						"Invalid switchbind "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}
		trim_whitespace(fold_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->fold = parse_fold_state(fold_str);
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}

			mango_error(false, WLR_ERROR,
						"Unknown dispatch in "
						"switchbind: "
						"\033[1m\033[31m%s\033[0m\n",
						func_name);
			return false;
		} else {
			config->switch_bindings_count++;
		}

	} else if (strncmp(key, "gesturebind", 11) == 0) {
		GestureBinding *new_gesture_bindings = realloc(
			config->gesture_bindings,
			(config->gesture_bindings_count + 1) * sizeof(GestureBinding));
		if (!new_gesture_bindings) {
			mango_error(false, WLR_ERROR,
						"Failed to allocate "
						"memory for axis gesturebind\n");
			return false;
		}
		config->gesture_bindings = new_gesture_bindings;

		GestureBinding *binding =
			&config->gesture_bindings[config->gesture_bindings_count];
		memset(binding, 0, sizeof(GestureBinding));
		set_binding_keymode(config, binding->mode, &binding->iscommonmode,
							&binding->isdefaultmode);
		binding->line_number = line_number;
		binding->file_index = config_current_file_index();

		char mod_str[256], motion_str[256], fingers_count_str[256],
			func_name[256], arg_value[256] = "0\0", arg_value2[256] = "0\0",
							arg_value3[256] = "0\0", arg_value4[256] = "0\0",
							arg_value5[256] = "0\0";
		if (sscanf(value,
				   "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^"
				   ",],%255["
				   "^,],%255[^,],%255[^\n]",
				   mod_str, motion_str, fingers_count_str, func_name, arg_value,
				   arg_value2, arg_value3, arg_value4, arg_value5) < 4) {
			mango_error(false, WLR_ERROR,
						"Invalid gesturebind "
						"format: \033[1m\033[31m%s\033[0m\n",
						value);
			return false;
		}

		trim_whitespace(mod_str);
		trim_whitespace(motion_str);
		trim_whitespace(fingers_count_str);
		trim_whitespace(func_name);
		trim_whitespace(arg_value);
		trim_whitespace(arg_value2);
		trim_whitespace(arg_value3);
		trim_whitespace(arg_value4);
		trim_whitespace(arg_value5);

		binding->mod = parse_mod(mod_str);
		binding->motion = parse_direction(motion_str);
		binding->fingers_count = atoi(fingers_count_str);
		binding->arg.i = 0;
		binding->arg.i2 = 0;
		binding->arg.f = 0.0f;
		binding->arg.f2 = 0.0f;
		binding->arg.ui = 0;
		binding->arg.ui2 = 0;
		binding->arg.v = NULL;
		binding->arg.v2 = NULL;
		binding->arg.v3 = NULL;
		binding->arg.tc = NULL;
		binding->func =
			parse_func_name(func_name, &binding->arg, arg_value, arg_value2,
							arg_value3, arg_value4, arg_value5);

		if (!binding->func || binding->mod == UINT32_MAX) {
			if (binding->arg.v) {
				free(binding->arg.v);
				binding->arg.v = NULL;
			}
			if (binding->arg.v2) {
				free(binding->arg.v2);
				binding->arg.v2 = NULL;
			}
			if (binding->arg.v3) {
				free(binding->arg.v3);
				binding->arg.v3 = NULL;
			}
			if (!binding->func)
				mango_error(false, WLR_ERROR,
							"Unknown "
							"dispatch in "
							"axisbind: \033[1m\033[31m%s\033[0m\n",
							func_name);
			return false;
		} else {
			config->gesture_bindings_count++;
		}

	} else if (strncmp(key, "source_optional", 15) == 0) {
		parse_config_file(config, value, false);
	} else if (strncmp(key, "source", 6) == 0) {
		parse_config_file(config, value, true);
	} else {
		mango_error(false, WLR_ERROR,
					"Unknown keyword: "
					"\033[1m\033[31m%s\033[0m\n",
					key);
		return false;
	}

	return true;
}

bool apply_option_expanded(Config *config, char *key, char *value,
						   int line_number) {
	char *expanded = expand_config_variables(value);
	bool ok =
		parse_option(config, key, expanded ? expanded : value, line_number);
	free(expanded);
	return ok;
}

void set_value_default() {
	config.animations = 1;
	config.layer_animations = 0;
	config.animation_type_open = ANIM_TYPE_UNSET;
	config.animation_type_close = ANIM_TYPE_UNSET;
	config.layer_animation_type_open = ANIM_TYPE_UNSET;
	config.layer_animation_type_close = ANIM_TYPE_UNSET;
	config.animation_fade_in = 1;
	config.animation_fade_out = 1;
	config.tag_animation_direction = HORIZONTAL;
	config.zoom_initial_ratio = 0.4f;
	config.zoom_end_ratio = 0.8f;
	config.fadein_begin_opacity = 0.5f;
	config.fadeout_begin_opacity = 0.5f;
	config.animation_duration_move = 500;
	config.animation_duration_open = 400;
	config.animation_duration_tag = 300;
	config.animation_duration_close = 300;
	config.animation_duration_focus = 0;

	config.axis_bind_apply_timeout = 100;
	config.focus_on_activate = 1;
	config.new_is_master = 1;
	config.default_mfact = 0.55f;
	config.default_nmaster = 1;
	config.tag_num = 9;
	config.tag_gather = 0;
	config.center_master_overspread = 0;
	config.center_when_single_stack = 1;

	config.dwindle_vsplit = 1;
	config.dwindle_hsplit = 1;
	config.dwindle_preserve_split = 0;
	config.dwindle_smart_split = 0;
	config.dwindle_smart_resize = 0;
	config.dwindle_drop_simple_split = 1;
	config.dwindle_manual_split = 0;
	config.dwindle_split_ratio = 0.5f;

	config.log_level = WLR_ERROR;
	config.numlockon = 0;
	config.capslock = 0;
	config.hotarea_size = 10;
	config.hotarea_corner = BOTTOM_LEFT;
	config.enable_hotarea = 0;
	config.hotarea_disable_on_fullscreen = 1;
	config.smartgaps = 0;
	config.monocle_tab_mode = 1;
	config.deck_tab_mode = 1;
	config.sloppyfocus = 1;
	config.gappih = 5;
	config.gappiv = 5;
	config.gappoh = 10;
	config.gappov = 10;
	config.scratchpad_width_ratio = 0.8f;
	config.scratchpad_height_ratio = 0.9f;
	config.special_dim = 0.5f;
	config.special_gappih = 10;
	config.special_gappiv = 10;
	config.special_gappoh = 20;
	config.special_gappov = 20;

	config.scroller_structs = 20;
	config.scroller_default_proportion = 0.9f;
	config.scroller_default_proportion_single = 1.0f;
	config.scroller_ignore_proportion_single = 1;
	config.scroller_focus_center = 0;
	config.scroller_prefer_center = 0;
	config.scroller_prefer_overspread = 1;
	config.edge_scroller_pointer_focus = 1;
	config.edge_scroller_focus_allow_speed = 0.0f;
	config.focus_cross_monitor = 0;
	config.focusdir_only_zone_overlap = 1;
	config.exchange_cross_monitor = 0;
	config.scratchpad_cross_monitor = 0;
	config.focus_cross_tag = 0;
	config.axis_scroll_factor = 1.0;
	config.trackpad_scroll_factor = 1.0;
	config.view_current_to_back = 0;
	config.single_scratchpad = 1;
	config.xwayland_persistence = 1;
	config.xwayland_ignore_scale = 0;
	config.syncobj_enable = 1;
	config.tag_carousel = 0;
	config.drag_tile_refresh_interval = 8.0f;
	config.drag_floating_refresh_interval = 8.0f;
	config.allow_tearing = TEARING_DISABLED;
	config.hdr_depth = MANGO_RENDER_BIT_DEPTH_10;
	config.allow_shortcuts_inhibit = SHORTCUTS_INHIBIT_ENABLE;
	config.disable_middle_paste = 0;
	config.allow_lock_transparent = 0;
	config.auto_reload_config = 1;
	config.no_border_when_single = 0;
	config.no_radius_when_single = 0;
	config.snap_distance = 30;
	config.drag_tile_to_tile = 1;
	config.drag_tile_small = 1;
	config.enable_floating_snap = 0;
	config.float_full_to_top = 0;
	config.swipe_min_threshold = 1;
	config.gesture_live = 1;
	config.gesture_swipe_distance = 300;
	config.gesture_swipe_cancel_ratio = 0.5;
	config.gesture_swipe_min_speed_to_force = 10;

	config.idleinhibit_ignore_visible = 0;
	config.idleinhibit_when_fullscreen = 0;

	config.borderpx = 4;
	config.group_bar_height = 25;
	config.tab_bar_height = 25;
	config.overviewgappi = 5;
	config.overviewgappo = 30;
	config.overcircle_center_ratio = 0.5f;
	config.cursor_hide_timeout = 0;
	config.cursor_hide_on_keypress = 0;

	config.warpcursor = 1;
	config.drag_corner = 3;
	config.drag_warp_cursor = 1;

	config.repeat_rate = 25;
	config.repeat_delay = 600;

	config.disable_trackpad = 0;
	config.touch_enable = 1;
	config.touch_enable_mouse_emulation = 0;
	config.tap_to_click = 1;
	config.tap_and_drag = 1;
	config.drag_lock = 1;
	config.mouse_natural_scrolling = 0;
	config.cursor_size = 24;
	config.trackpad_natural_scrolling = 0;
	config.mouse_accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	config.mouse_accel_speed = 0.0;
	config.trackpad_accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	config.trackpad_accel_speed = 0.0;
	config.send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	config.button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
	config.mouse_left_handed = 0;
	config.mouse_middle_button_emulation = 0;
	config.mouse_scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	config.mouse_scroll_button = 274;
	config.mouse_click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	config.mouse_send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	config.trackpad_left_handed = 0;
	config.trackpad_middle_button_emulation = 0;
	config.trackpad_disable_while_typing = 1;
	config.trackpad_scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	config.trackpad_scroll_button = 274;
	config.trackpad_click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	config.trackpad_send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

	config.blur = 0;
	config.blur_layer = 0;
	config.blur_optimized = 1;
	config.border_radius = 0;
	config.blur_params.num_passes = 1;
	config.blur_params.radius = 5;
	config.blur_params.noise = 0.02f;
	config.blur_params.brightness = 0.9f;
	config.blur_params.contrast = 0.9f;
	config.blur_params.saturation = 1.2f;
	config.shadows = 0;
	config.shadow_only_floating = 1;
	config.layer_shadows = 0;
	config.shadows_size = 10;
	config.shadows_blur = 15.0f;
	config.shadows_position_x = 0;
	config.shadows_position_y = 0;
	config.focused_opacity = 1.0f;
	config.unfocused_opacity = 1.0f;

	config.shadowscolor[0] = 0.0f;
	config.shadowscolor[1] = 0.0f;
	config.shadowscolor[2] = 0.0f;
	config.shadowscolor[3] = 1.0f;

	config.dim_enable = 0;
	config.dim_focused_color[0] = 0.0f;
	config.dim_focused_color[1] = 0.0f;
	config.dim_focused_color[2] = 0.0f;
	config.dim_focused_color[3] = 0.0f;
	config.dim_unfocused_color[0] = 0.0f;
	config.dim_unfocused_color[1] = 0.0f;
	config.dim_unfocused_color[2] = 0.0f;
	config.dim_unfocused_color[3] = 0x55 / 255.0f;

	config.animation_curve_move[0] = 0.46;
	config.animation_curve_move[1] = 1.0;
	config.animation_curve_move[2] = 0.29;
	config.animation_curve_move[3] = 0.99;
	config.animation_curve_open[0] = 0.46;
	config.animation_curve_open[1] = 1.0;
	config.animation_curve_open[2] = 0.29;
	config.animation_curve_open[3] = 0.99;
	config.animation_curve_tag[0] = 0.46;
	config.animation_curve_tag[1] = 1.0;
	config.animation_curve_tag[2] = 0.29;
	config.animation_curve_tag[3] = 0.99;
	config.animation_curve_close[0] = 0.46;
	config.animation_curve_close[1] = 1.0;
	config.animation_curve_close[2] = 0.29;
	config.animation_curve_close[3] = 0.99;
	config.animation_curve_focus[0] = 0.46;
	config.animation_curve_focus[1] = 1.0;
	config.animation_curve_focus[2] = 0.29;
	config.animation_curve_focus[3] = 0.99;
	config.animation_curve_opafadein[0] = 0.46;
	config.animation_curve_opafadein[1] = 1.0;
	config.animation_curve_opafadein[2] = 0.29;
	config.animation_curve_opafadein[3] = 0.99;
	config.animation_curve_opafadeout[0] = 0.5;
	config.animation_curve_opafadeout[1] = 0.5;
	config.animation_curve_opafadeout[2] = 0.5;
	config.animation_curve_opafadeout[3] = 0.5;

	config.groupbardata.fg_color[0] = 0xc0 / 255.0f;
	config.groupbardata.fg_color[1] = 0xca / 255.0f;
	config.groupbardata.fg_color[2] = 0xf5 / 255.0f;
	config.groupbardata.fg_color[3] = 1.0f;
	config.groupbardata.bg_color[0] = 0x1a / 255.0f;
	config.groupbardata.bg_color[1] = 0x1b / 255.0f;
	config.groupbardata.bg_color[2] = 0x26 / 255.0f;
	config.groupbardata.bg_color[3] = 1.0f;
	config.groupbardata.focus_fg_color[0] = 0x1a / 255.0f;
	config.groupbardata.focus_fg_color[1] = 0x1b / 255.0f;
	config.groupbardata.focus_fg_color[2] = 0x26 / 255.0f;
	config.groupbardata.focus_fg_color[3] = 1.0f;
	config.groupbardata.focus_bg_color[0] = 0x9e / 255.0f;
	config.groupbardata.focus_bg_color[1] = 0xce / 255.0f;
	config.groupbardata.focus_bg_color[2] = 0x6a / 255.0f;
	config.groupbardata.focus_bg_color[3] = 1.0f;
	config.groupbardata.border_color[0] = 0x3b / 255.0f;
	config.groupbardata.border_color[1] = 0x42 / 255.0f;
	config.groupbardata.border_color[2] = 0x61 / 255.0f;
	config.groupbardata.border_color[3] = 1.0f;
	config.groupbardata.border_width = 4;
	config.groupbardata.corner_radius = 5;
	config.groupbardata.padding_x = 0;
	config.groupbardata.padding_y = 0;

	config.tabbardata.fg_color[0] = 0xc0 / 255.0f;
	config.tabbardata.fg_color[1] = 0xca / 255.0f;
	config.tabbardata.fg_color[2] = 0xf5 / 255.0f;
	config.tabbardata.fg_color[3] = 1.0f;
	config.tabbardata.bg_color[0] = 0x1a / 255.0f;
	config.tabbardata.bg_color[1] = 0x1b / 255.0f;
	config.tabbardata.bg_color[2] = 0x26 / 255.0f;
	config.tabbardata.bg_color[3] = 1.0f;
	config.tabbardata.focus_fg_color[0] = 0x1a / 255.0f;
	config.tabbardata.focus_fg_color[1] = 0x1b / 255.0f;
	config.tabbardata.focus_fg_color[2] = 0x26 / 255.0f;
	config.tabbardata.focus_fg_color[3] = 1.0f;
	config.tabbardata.focus_bg_color[0] = 0x7a / 255.0f;
	config.tabbardata.focus_bg_color[1] = 0xa2 / 255.0f;
	config.tabbardata.focus_bg_color[2] = 0xf7 / 255.0f;
	config.tabbardata.focus_bg_color[3] = 1.0f;
	config.tabbardata.border_color[0] = 0x3b / 255.0f;
	config.tabbardata.border_color[1] = 0x42 / 255.0f;
	config.tabbardata.border_color[2] = 0x61 / 255.0f;
	config.tabbardata.border_color[3] = 1.0f;
	config.tabbardata.border_width = 4;
	config.tabbardata.corner_radius = 5;
	config.tabbardata.padding_x = 0;
	config.tabbardata.padding_y = 0;

	config.jumplabeldata.fg_color[0] = 0xc4 / 255.0f;
	config.jumplabeldata.fg_color[1] = 0x93 / 255.0f;
	config.jumplabeldata.fg_color[2] = 0x9d / 255.0f;
	config.jumplabeldata.fg_color[3] = 1.0f;
	config.jumplabeldata.bg_color[0] = 0x32 / 255.0f;
	config.jumplabeldata.bg_color[1] = 0x32 / 255.0f;
	config.jumplabeldata.bg_color[2] = 0x32 / 255.0f;
	config.jumplabeldata.bg_color[3] = 1.0f;
	config.jumplabeldata.focus_fg_color[0] = 0xed / 255.0f;
	config.jumplabeldata.focus_fg_color[1] = 0xa6 / 255.0f;
	config.jumplabeldata.focus_fg_color[2] = 0xb4 / 255.0f;
	config.jumplabeldata.focus_fg_color[3] = 1.0f;
	config.jumplabeldata.focus_bg_color[0] = 0x4e / 255.0f;
	config.jumplabeldata.focus_bg_color[1] = 0x45 / 255.0f;
	config.jumplabeldata.focus_bg_color[2] = 0x3c / 255.0f;
	config.jumplabeldata.focus_bg_color[3] = 1.0f;
	config.jumplabeldata.border_color[0] = 0x8b / 255.0f;
	config.jumplabeldata.border_color[1] = 0xaa / 255.0f;
	config.jumplabeldata.border_color[2] = 0x9b / 255.0f;
	config.jumplabeldata.border_color[3] = 1.0f;
	config.jumplabeldata.border_width = 4;
	config.jumplabeldata.corner_radius = 5;
	config.jumplabeldata.padding_x = 10;
	config.jumplabeldata.padding_y = 10;

	config.rootcolor[0] = 0x32 / 255.0f;
	config.rootcolor[1] = 0x32 / 255.0f;
	config.rootcolor[2] = 0x32 / 255.0f;
	config.rootcolor[3] = 1.0f;
	config.bordercolor[0] = 0x44 / 255.0f;
	config.bordercolor[1] = 0x44 / 255.0f;
	config.bordercolor[2] = 0x44 / 255.0f;
	config.bordercolor[3] = 1.0f;
	config.dropcolor[0] = 0xd5 / 255.0f;
	config.dropcolor[1] = 0x89 / 255.0f;
	config.dropcolor[2] = 0x9d / 255.0f;
	config.dropcolor[3] = 0.5f;
	config.splitcolor[0] = 0xeb / 255.0f;
	config.splitcolor[1] = 0x44 / 255.0f;
	config.splitcolor[2] = 0x1e / 255.0f;
	config.splitcolor[3] = 1.0f;
	config.focuscolor[0] = 0xc6 / 255.0f;
	config.focuscolor[1] = 0x6b / 255.0f;
	config.focuscolor[2] = 0x25 / 255.0f;
	config.focuscolor[3] = 1.0f;
	config.maximizescreencolor[0] = 0x89 / 255.0f;
	config.maximizescreencolor[1] = 0xaa / 255.0f;
	config.maximizescreencolor[2] = 0x61 / 255.0f;
	config.maximizescreencolor[3] = 1.0f;
	config.urgentcolor[0] = 0xad / 255.0f;
	config.urgentcolor[1] = 0x40 / 255.0f;
	config.urgentcolor[2] = 0x1f / 255.0f;
	config.urgentcolor[3] = 1.0f;
	config.scratchpadcolor[0] = 0x51 / 255.0f;
	config.scratchpadcolor[1] = 0x6c / 255.0f;
	config.scratchpadcolor[2] = 0x93 / 255.0f;
	config.scratchpadcolor[3] = 1.0f;
	config.globalcolor[0] = 0xb1 / 255.0f;
	config.globalcolor[1] = 0x53 / 255.0f;
	config.globalcolor[2] = 0xa7 / 255.0f;
	config.globalcolor[3] = 1.0f;
	config.overlaycolor[0] = 0x14 / 255.0f;
	config.overlaycolor[1] = 0xa5 / 255.0f;
	config.overlaycolor[2] = 0x7c / 255.0f;
	config.overlaycolor[3] = 1.0f;
}

void override_config(void) {
	config.animations = CLAMP_INT(config.animations, 0, 1);
	config.layer_animations = CLAMP_INT(config.layer_animations, 0, 1);
	config.tag_animation_direction =
		CLAMP_INT(config.tag_animation_direction, 0, 1);
	config.animation_fade_in = CLAMP_INT(config.animation_fade_in, 0, 1);
	config.animation_fade_out = CLAMP_INT(config.animation_fade_out, 0, 1);
	config.zoom_initial_ratio =
		CLAMP_FLOAT(config.zoom_initial_ratio, 0.1f, 1.0f);
	config.zoom_end_ratio = CLAMP_FLOAT(config.zoom_end_ratio, 0.1f, 1.0f);
	config.fadein_begin_opacity =
		CLAMP_FLOAT(config.fadein_begin_opacity, 0.0f, 1.0f);
	config.fadeout_begin_opacity =
		CLAMP_FLOAT(config.fadeout_begin_opacity, 0.0f, 1.0f);
	config.animation_duration_move =
		CLAMP_INT(config.animation_duration_move, 0, 50000);
	config.animation_duration_open =
		CLAMP_INT(config.animation_duration_open, 0, 50000);
	config.animation_duration_tag =
		CLAMP_INT(config.animation_duration_tag, 0, 50000);
	config.animation_duration_close =
		CLAMP_INT(config.animation_duration_close, 0, 50000);
	config.animation_duration_focus =
		CLAMP_INT(config.animation_duration_focus, 0, 50000);
	config.scroller_default_proportion =
		CLAMP_FLOAT(config.scroller_default_proportion, 0.1f, 1.0f);
	config.scroller_default_proportion_single =
		CLAMP_FLOAT(config.scroller_default_proportion_single, 0.1f, 1.0f);
	config.scroller_ignore_proportion_single =
		CLAMP_INT(config.scroller_ignore_proportion_single, 0, 1);
	config.scroller_focus_center =
		CLAMP_INT(config.scroller_focus_center, 0, 1);
	config.scroller_prefer_center =
		CLAMP_INT(config.scroller_prefer_center, 0, 1);
	config.scroller_prefer_overspread =
		CLAMP_INT(config.scroller_prefer_overspread, 0, 1);
	config.edge_scroller_pointer_focus =
		CLAMP_INT(config.edge_scroller_pointer_focus, 0, 1);
	config.edge_scroller_focus_allow_speed =
		CLAMP_FLOAT(config.edge_scroller_focus_allow_speed, 0.0f, 1000.0f);
	config.scroller_structs = CLAMP_INT(config.scroller_structs, 0, 1000);
	config.default_mfact = CLAMP_FLOAT(config.default_mfact, 0.1f, 0.9f);
	config.default_nmaster = CLAMP_INT(config.default_nmaster, 1, 1000);
	config.tag_num = CLAMP_INT(config.tag_num, 1, tag_num_MAX);
	config.tag_gather = CLAMP_INT(config.tag_gather, 0, 1);
	config.center_master_overspread =
		CLAMP_INT(config.center_master_overspread, 0, 1);
	config.center_when_single_stack =
		CLAMP_INT(config.center_when_single_stack, 0, 1);
	config.new_is_master = CLAMP_INT(config.new_is_master, 0, 1);
	config.dwindle_vsplit = CLAMP_INT(config.dwindle_vsplit, 0, 2);
	config.dwindle_hsplit = CLAMP_INT(config.dwindle_hsplit, 0, 2);
	config.dwindle_preserve_split =
		CLAMP_INT(config.dwindle_preserve_split, 0, 1);
	config.dwindle_smart_split = CLAMP_INT(config.dwindle_smart_split, 0, 1);
	config.dwindle_smart_resize = CLAMP_INT(config.dwindle_smart_resize, 0, 1);
	config.dwindle_drop_simple_split =
		CLAMP_INT(config.dwindle_drop_simple_split, 0, 1);
	config.dwindle_manual_split = CLAMP_INT(config.dwindle_manual_split, 0, 1);
	config.dwindle_split_ratio =
		CLAMP_FLOAT(config.dwindle_split_ratio, 0.05f, 0.95f);
	config.hotarea_size = CLAMP_INT(config.hotarea_size, 1, 1000);
	config.hotarea_corner = CLAMP_INT(config.hotarea_corner, 0, 3);
	config.enable_hotarea = CLAMP_INT(config.enable_hotarea, 0, 1);
	config.hotarea_disable_on_fullscreen =
		CLAMP_INT(config.hotarea_disable_on_fullscreen, 0, 1);
	config.overviewgappi = CLAMP_INT(config.overviewgappi, 0, 1000);
	config.overviewgappo = CLAMP_INT(config.overviewgappo, 0, 1000);
	config.overcircle_center_ratio =
		CLAMP_FLOAT(config.overcircle_center_ratio, 0.1f, 0.9f);
	config.xwayland_persistence = CLAMP_INT(config.xwayland_persistence, 0, 1);
	config.xwayland_ignore_scale =
		CLAMP_INT(config.xwayland_ignore_scale, 0, 1);
	config.syncobj_enable = CLAMP_INT(config.syncobj_enable, 0, 1);
	config.drag_tile_refresh_interval =
		CLAMP_FLOAT(config.drag_tile_refresh_interval, 1.0f, 16.0f);
	config.drag_floating_refresh_interval =
		CLAMP_FLOAT(config.drag_floating_refresh_interval, 0.0f, 1000.0f);
	config.drag_tile_to_tile = CLAMP_INT(config.drag_tile_to_tile, 0, 1);
	config.drag_tile_small = CLAMP_INT(config.drag_tile_small, 0, 1);
	config.allow_tearing = CLAMP_INT(config.allow_tearing, 0, 2);
	config.hdr_depth = CLAMP_INT(config.hdr_depth, 0, 2);
	config.allow_shortcuts_inhibit =
		CLAMP_INT(config.allow_shortcuts_inhibit, 0, 1);
	config.disable_middle_paste = CLAMP_INT(config.disable_middle_paste, 0, 1);
	config.allow_lock_transparent =
		CLAMP_INT(config.allow_lock_transparent, 0, 1);
	config.auto_reload_config = CLAMP_INT(config.auto_reload_config, 0, 1);
	config.axis_bind_apply_timeout =
		CLAMP_INT(config.axis_bind_apply_timeout, 0, 1000);
	config.focus_on_activate = CLAMP_INT(config.focus_on_activate, 0, 1);
	config.idleinhibit_ignore_visible =
		CLAMP_INT(config.idleinhibit_ignore_visible, 0, 1);
	config.idleinhibit_when_fullscreen =
		CLAMP_INT(config.idleinhibit_when_fullscreen, 0, 1);
	config.sloppyfocus = CLAMP_INT(config.sloppyfocus, 0, 1);
	config.warpcursor = CLAMP_INT(config.warpcursor, 0, 1);
	config.drag_corner = CLAMP_INT(config.drag_corner, 0, 4);
	config.drag_warp_cursor = CLAMP_INT(config.drag_warp_cursor, 0, 1);
	config.focus_cross_monitor = CLAMP_INT(config.focus_cross_monitor, 0, 1);
	config.focusdir_only_zone_overlap =
		CLAMP_INT(config.focusdir_only_zone_overlap, 0, 1);
	config.exchange_cross_monitor =
		CLAMP_INT(config.exchange_cross_monitor, 0, 1);
	config.scratchpad_cross_monitor =
		CLAMP_INT(config.scratchpad_cross_monitor, 0, 1);
	config.focus_cross_tag = CLAMP_INT(config.focus_cross_tag, 0, 1);
	config.view_current_to_back = CLAMP_INT(config.view_current_to_back, 0, 1);
	config.enable_floating_snap = CLAMP_INT(config.enable_floating_snap, 0, 1);
	config.float_full_to_top = CLAMP_INT(config.float_full_to_top, 0, 1);
	config.snap_distance = CLAMP_INT(config.snap_distance, 0, 99999);
	config.cursor_size = CLAMP_INT(config.cursor_size, 4, 512);
	config.no_border_when_single =
		CLAMP_INT(config.no_border_when_single, 0, 1);
	config.no_radius_when_single =
		CLAMP_INT(config.no_radius_when_single, 0, 1);
	config.cursor_hide_timeout =
		CLAMP_INT(config.cursor_hide_timeout, 0, 36000);
	config.cursor_hide_on_keypress =
		CLAMP_INT(config.cursor_hide_on_keypress, 0, 1);
	config.single_scratchpad = CLAMP_INT(config.single_scratchpad, 0, 1);
	config.repeat_rate = CLAMP_INT(config.repeat_rate, 1, 1000);
	config.repeat_delay = CLAMP_INT(config.repeat_delay, 1, 20000);
	config.numlockon = CLAMP_INT(config.numlockon, 0, 1);
	config.disable_trackpad = CLAMP_INT(config.disable_trackpad, 0, 1);
	config.touch_enable = CLAMP_INT(config.touch_enable, 0, 1);
	config.touch_enable_mouse_emulation =
		CLAMP_INT(config.touch_enable_mouse_emulation, 0, 1);
	config.tap_to_click = CLAMP_INT(config.tap_to_click, 0, 1);
	config.tap_and_drag = CLAMP_INT(config.tap_and_drag, 0, 1);
	config.drag_lock = CLAMP_INT(config.drag_lock, 0, 1);
	config.trackpad_natural_scrolling =
		CLAMP_INT(config.trackpad_natural_scrolling, 0, 1);
	config.swipe_min_threshold = CLAMP_INT(config.swipe_min_threshold, 1, 1000);
	config.gesture_live = CLAMP_INT(config.gesture_live, 0, 1);
	config.gesture_swipe_distance =
		CLAMP_INT(config.gesture_swipe_distance, 32, 4096);
	if (config.gesture_swipe_cancel_ratio < 0.05)
		config.gesture_swipe_cancel_ratio = 0.05;
	if (config.gesture_swipe_cancel_ratio > 0.95)
		config.gesture_swipe_cancel_ratio = 0.95;
	if (config.gesture_swipe_min_speed_to_force < 0)
		config.gesture_swipe_min_speed_to_force = 0;
	config.mouse_natural_scrolling =
		CLAMP_INT(config.mouse_natural_scrolling, 0, 1);
	config.mouse_accel_profile = CLAMP_INT(config.mouse_accel_profile, 0, 2);
	config.mouse_accel_speed =
		CLAMP_FLOAT(config.mouse_accel_speed, -1.0f, 1.0f);
	config.trackpad_accel_profile =
		CLAMP_INT(config.trackpad_accel_profile, 0, 2);
	config.trackpad_accel_speed =
		CLAMP_FLOAT(config.trackpad_accel_speed, -1.0f, 1.0f);
	config.send_events_mode = CLAMP_INT(config.send_events_mode, 0, 2);
	config.button_map = CLAMP_INT(config.button_map, 0, 1);
	config.mouse_left_handed = CLAMP_INT(config.mouse_left_handed, 0, 1);
	config.mouse_middle_button_emulation =
		CLAMP_INT(config.mouse_middle_button_emulation, 0, 1);
	config.mouse_scroll_method = CLAMP_INT(config.mouse_scroll_method, 0, 4);
	config.mouse_scroll_button =
		CLAMP_INT(config.mouse_scroll_button, 272, 279);
	config.mouse_click_method = CLAMP_INT(config.mouse_click_method, 0, 2);
	config.mouse_send_events_mode =
		CLAMP_INT(config.mouse_send_events_mode, 0, 2);
	config.trackpad_left_handed = CLAMP_INT(config.trackpad_left_handed, 0, 1);
	config.trackpad_middle_button_emulation =
		CLAMP_INT(config.trackpad_middle_button_emulation, 0, 1);
	config.trackpad_disable_while_typing =
		CLAMP_INT(config.trackpad_disable_while_typing, 0, 1);
	config.trackpad_scroll_method =
		CLAMP_INT(config.trackpad_scroll_method, 0, 4);
	config.trackpad_scroll_button =
		CLAMP_INT(config.trackpad_scroll_button, 272, 279);
	config.trackpad_click_method =
		CLAMP_INT(config.trackpad_click_method, 0, 2);
	config.trackpad_send_events_mode =
		CLAMP_INT(config.trackpad_send_events_mode, 0, 2);
	config.axis_scroll_factor =
		CLAMP_FLOAT(config.axis_scroll_factor, 0.1f, 10.0f);
	config.trackpad_scroll_factor =
		CLAMP_FLOAT(config.trackpad_scroll_factor, 0.1f, 10.0f);
	config.gappih = CLAMP_INT(config.gappih, 0, 1000);
	config.gappiv = CLAMP_INT(config.gappiv, 0, 1000);
	config.gappoh = CLAMP_INT(config.gappoh, 0, 1000);
	config.gappov = CLAMP_INT(config.gappov, 0, 1000);
	config.scratchpad_width_ratio =
		CLAMP_FLOAT(config.scratchpad_width_ratio, 0.1f, 1.0f);
	config.scratchpad_height_ratio =
		CLAMP_FLOAT(config.scratchpad_height_ratio, 0.1f, 1.0f);
	config.special_dim = CLAMP_FLOAT(config.special_dim, 0.0f, 1.0f);
	config.special_gappih = CLAMP_INT(config.special_gappih, 0, 1000);
	config.special_gappiv = CLAMP_INT(config.special_gappiv, 0, 1000);
	config.special_gappoh = CLAMP_INT(config.special_gappoh, 0, 1000);
	config.special_gappov = CLAMP_INT(config.special_gappov, 0, 1000);
	config.borderpx = CLAMP_INT(config.borderpx, 0, 200);
	config.group_bar_height = CLAMP_INT(config.group_bar_height, 0, 500);
	config.tab_bar_height = CLAMP_INT(config.tab_bar_height, 0, 500);
	config.smartgaps = CLAMP_INT(config.smartgaps, 0, 1);
	config.blur = CLAMP_INT(config.blur, 0, 1);
	config.blur_layer = CLAMP_INT(config.blur_layer, 0, 1);
	config.blur_optimized = CLAMP_INT(config.blur_optimized, 0, 1);
	config.border_radius = CLAMP_INT(config.border_radius, 0, 100);
	config.blur_params.num_passes =
		CLAMP_INT(config.blur_params.num_passes, 0, 10);
	config.blur_params.radius = CLAMP_INT(config.blur_params.radius, 0, 100);
	config.blur_params.noise = CLAMP_FLOAT(config.blur_params.noise, 0, 1);
	config.blur_params.brightness =
		CLAMP_FLOAT(config.blur_params.brightness, 0, 1);
	config.blur_params.contrast =
		CLAMP_FLOAT(config.blur_params.contrast, 0, 1);
	config.blur_params.saturation =
		CLAMP_FLOAT(config.blur_params.saturation, 0, 1);
	config.shadows = CLAMP_INT(config.shadows, 0, 1);
	config.shadow_only_floating = CLAMP_INT(config.shadow_only_floating, 0, 1);
	config.layer_shadows = CLAMP_INT(config.layer_shadows, 0, 1);
	config.shadows_size = CLAMP_INT(config.shadows_size, 0, 100);
	config.shadows_blur = CLAMP_INT(config.shadows_blur, 0, 100);
	config.shadows_position_x =
		CLAMP_INT(config.shadows_position_x, -1000, 1000);
	config.shadows_position_y =
		CLAMP_INT(config.shadows_position_y, -1000, 1000);
	config.focused_opacity = CLAMP_FLOAT(config.focused_opacity, 0.0f, 1.0f);
	config.unfocused_opacity =
		CLAMP_FLOAT(config.unfocused_opacity, 0.0f, 1.0f);
	config.dim_enable = CLAMP_INT(config.dim_enable, 0, 1);

	config.groupbardata.border_width =
		CLAMP_INT(config.groupbardata.border_width, 0, 100);
	config.groupbardata.corner_radius =
		CLAMP_INT(config.groupbardata.corner_radius, 0, 100);
	config.groupbardata.padding_x =
		CLAMP_INT(config.groupbardata.padding_x, 0, 100);
	config.groupbardata.padding_y =
		CLAMP_INT(config.groupbardata.padding_y, 0, 100);

	config.tabbardata.border_width =
		CLAMP_INT(config.tabbardata.border_width, 0, 100);
	config.tabbardata.corner_radius =
		CLAMP_INT(config.tabbardata.corner_radius, 0, 100);
	config.tabbardata.padding_x =
		CLAMP_INT(config.tabbardata.padding_x, 0, 100);
	config.tabbardata.padding_y =
		CLAMP_INT(config.tabbardata.padding_y, 0, 100);

	config.jumplabeldata.border_width =
		CLAMP_INT(config.jumplabeldata.border_width, 0, 100);
	config.jumplabeldata.corner_radius =
		CLAMP_INT(config.jumplabeldata.corner_radius, 0, 100);
	config.jumplabeldata.padding_x =
		CLAMP_INT(config.jumplabeldata.padding_x, 0, 100);
	config.jumplabeldata.padding_y =
		CLAMP_INT(config.jumplabeldata.padding_y, 0, 100);

	update_global_var();
}

void update_global_var(void) {
	server.tagmask = ((uint32_t)1 << config.tag_num) - 1;
}
