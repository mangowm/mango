#include "mango/config/parse.h"

#include <stdlib.h>
#include <string.h>

#include "mango/common/log.h"
#include "mango/common/server.h"

void free_circle_layout(Config *config) {
	if (config->circle_layout) {
		// Frees each string.
		for (int32_t i = 0; i < config->circle_layout_count; i++) {
			if (config->circle_layout[i]) {
				free(config->circle_layout[i]);	 // Frees a single string.
				config->circle_layout[i] = NULL; // Prevents a dangling pointer.
			}
		}
		// Frees the circle_layout array itself.
		free(config->circle_layout);
		config->circle_layout = NULL; // Prevents a dangling pointer.
	}
	config->circle_layout_count = 0; // Resets the count.
}

void free_baked_points(void) {
	if (server.baked_points_move) {
		free(server.baked_points_move);
		server.baked_points_move = NULL;
	}
	if (server.baked_points_open) {
		free(server.baked_points_open);
		server.baked_points_open = NULL;
	}
	if (server.baked_points_close) {
		free(server.baked_points_close);
		server.baked_points_close = NULL;
	}
	if (server.baked_points_tag) {
		free(server.baked_points_tag);
		server.baked_points_tag = NULL;
	}
	if (server.baked_points_focus) {
		free(server.baked_points_focus);
		server.baked_points_focus = NULL;
	}
	if (server.baked_points_opafadein) {
		free(server.baked_points_opafadein);
		server.baked_points_opafadein = NULL;
	}
	if (server.baked_points_opafadeout) {
		free(server.baked_points_opafadeout);
		server.baked_points_opafadeout = NULL;
	}
}

void free_config(void) {
	// Frees memory.
	int32_t i;

	// Frees window_rules.
	if (config.window_rules) {
		for (int32_t i = 0; i < config.window_rules_count; i++) {
			ConfigWinRule *rule = &config.window_rules[i];
			if (rule->id)
				free((void *)rule->id);
			if (rule->title)
				free((void *)rule->title);
			if (rule->monitor)
				free((void *)rule->monitor);
			rule->id = NULL;
			rule->title = NULL;
			rule->animation_type_open = ANIM_TYPE_UNSET;
			rule->animation_type_close = ANIM_TYPE_UNSET;
			rule->monitor = NULL;
			// Frees arg.v of globalkeybinding if dynamically allocated.
			if (rule->globalkeybinding.arg.v) {
				free((void *)rule->globalkeybinding.arg.v);
			}
		}
		free(config.window_rules);
		config.window_rules = NULL;
		config.window_rules_count = 0;
	}

	// Frees device_rules.
	if (config.device_rules) {
		for (i = 0; i < config.device_rules_count; i++) {
			if (config.device_rules[i].name)
				free(config.device_rules[i].name);
		}
		free(config.device_rules);
		config.device_rules = NULL;
		config.device_rules_count = 0;
	}

	// Frees key_bindings.
	if (config.key_bindings) {
		for (i = 0; i < config.key_bindings_count; i++) {
			if (config.key_bindings[i].arg.v) {
				free((void *)config.key_bindings[i].arg.v);
				config.key_bindings[i].arg.v = NULL;
			}
			if (config.key_bindings[i].arg.v2) {
				free((void *)config.key_bindings[i].arg.v2);
				config.key_bindings[i].arg.v2 = NULL;
			}
			if (config.key_bindings[i].arg.v3) {
				free((void *)config.key_bindings[i].arg.v3);
				config.key_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.key_bindings);
		config.key_bindings = NULL;
		config.key_bindings_count = 0;
	}

	// Frees mouse_bindings.
	if (config.mouse_bindings) {
		for (i = 0; i < config.mouse_bindings_count; i++) {
			if (config.mouse_bindings[i].arg.v) {
				free((void *)config.mouse_bindings[i].arg.v);
				config.mouse_bindings[i].arg.v = NULL;
			}
			if (config.mouse_bindings[i].arg.v2) {
				free((void *)config.mouse_bindings[i].arg.v2);
				config.mouse_bindings[i].arg.v2 = NULL;
			}
			if (config.mouse_bindings[i].arg.v3) {
				free((void *)config.mouse_bindings[i].arg.v3);
				config.mouse_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.mouse_bindings);
		config.mouse_bindings = NULL;
		config.mouse_bindings_count = 0;
	}

	// Frees axis_bindings.
	if (config.axis_bindings) {
		for (i = 0; i < config.axis_bindings_count; i++) {
			if (config.axis_bindings[i].arg.v) {
				free((void *)config.axis_bindings[i].arg.v);
				config.axis_bindings[i].arg.v = NULL;
			}
			if (config.axis_bindings[i].arg.v2) {
				free((void *)config.axis_bindings[i].arg.v2);
				config.axis_bindings[i].arg.v2 = NULL;
			}
			if (config.axis_bindings[i].arg.v3) {
				free((void *)config.axis_bindings[i].arg.v3);
				config.axis_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.axis_bindings);
		config.axis_bindings = NULL;
		config.axis_bindings_count = 0;
	}

	// Frees switch_bindings.
	if (config.switch_bindings) {
		for (i = 0; i < config.switch_bindings_count; i++) {
			if (config.switch_bindings[i].arg.v) {
				free((void *)config.switch_bindings[i].arg.v);
				config.switch_bindings[i].arg.v = NULL;
			}
			if (config.switch_bindings[i].arg.v2) {
				free((void *)config.switch_bindings[i].arg.v2);
				config.switch_bindings[i].arg.v2 = NULL;
			}
			if (config.switch_bindings[i].arg.v3) {
				free((void *)config.switch_bindings[i].arg.v3);
				config.switch_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.switch_bindings);
		config.switch_bindings = NULL;
		config.switch_bindings_count = 0;
	}

	// Frees gesture_bindings.
	if (config.gesture_bindings) {
		for (i = 0; i < config.gesture_bindings_count; i++) {
			if (config.gesture_bindings[i].arg.v) {
				free((void *)config.gesture_bindings[i].arg.v);
				config.gesture_bindings[i].arg.v = NULL;
			}
			if (config.gesture_bindings[i].arg.v2) {
				free((void *)config.gesture_bindings[i].arg.v2);
				config.gesture_bindings[i].arg.v2 = NULL;
			}
			if (config.gesture_bindings[i].arg.v3) {
				free((void *)config.gesture_bindings[i].arg.v3);
				config.gesture_bindings[i].arg.v3 = NULL;
			}
		}
		free(config.gesture_bindings);
		config.gesture_bindings = NULL;
		config.gesture_bindings_count = 0;
	}

	// Frees tag_rules.
	if (config.tag_rules) {
		for (int32_t i = 0; i < config.tag_rules_count; i++) {
			if (config.tag_rules[i].layout_name)
				free((void *)config.tag_rules[i].layout_name);
			if (config.tag_rules[i].monitor_name)
				free((void *)config.tag_rules[i].monitor_name);
			if (config.tag_rules[i].monitor_make)
				free((void *)config.tag_rules[i].monitor_make);
			if (config.tag_rules[i].monitor_model)
				free((void *)config.tag_rules[i].monitor_model);
			if (config.tag_rules[i].monitor_serial)
				free((void *)config.tag_rules[i].monitor_serial);
		}
		free(config.tag_rules);
		config.tag_rules = NULL;
		config.tag_rules_count = 0;
	}

	// Frees monitor_rules.
	if (config.monitor_rules) {
		for (int32_t i = 0; i < config.monitor_rules_count; i++) {
			if (config.monitor_rules[i].name)
				free((void *)config.monitor_rules[i].name);
			if (config.monitor_rules[i].make)
				free((void *)config.monitor_rules[i].make);
			if (config.monitor_rules[i].model)
				free((void *)config.monitor_rules[i].model);
			if (config.monitor_rules[i].serial)
				free((void *)config.monitor_rules[i].serial);
			if (config.monitor_rules[i].icc)
				free((void *)config.monitor_rules[i].icc);
		}
		free(config.monitor_rules);
		config.monitor_rules = NULL;
		config.monitor_rules_count = 0;
	}

	// Frees layer_rules.
	if (config.layer_rules) {
		for (int32_t i = 0; i < config.layer_rules_count; i++) {
			if (config.layer_rules[i].layer_name)
				free((void *)config.layer_rules[i].layer_name);
			config.layer_rules[i].animation_type_open = ANIM_TYPE_UNSET;
			config.layer_rules[i].animation_type_close = ANIM_TYPE_UNSET;
		}
		free(config.layer_rules);
		config.layer_rules = NULL;
		config.layer_rules_count = 0;
	}

	// Frees env.
	if (config.env) {
		for (int32_t i = 0; i < config.env_count; i++) {
			if (config.env[i]->type) {
				free((void *)config.env[i]->type);
			}
			if (config.env[i]->value) {
				free((void *)config.env[i]->value);
			}
			free(config.env[i]);
		}
		free(config.env);
		config.env = NULL;
		config.env_count = 0;
	}

	if (config.vars) {
		for (i = 0; i < config.vars_count; i++) {
			if (config.vars[i]) {
				free(config.vars[i]->name);
				free(config.vars[i]->value);
				free(config.vars[i]);
			}
		}
		free(config.vars);
		config.vars = NULL;
		config.vars_count = 0;
	}

	// Frees exec.
	if (config.exec) {
		for (i = 0; i < config.exec_count; i++) {
			free(config.exec[i]);
		}
		free(config.exec);
		config.exec = NULL;
		config.exec_count = 0;
	}

	// Frees exec_once.
	if (config.exec_once) {
		for (i = 0; i < config.exec_once_count; i++) {
			free(config.exec_once[i]);
		}
		free(config.exec_once);
		config.exec_once = NULL;
		config.exec_once_count = 0;
	}

	// Frees scroller_proportion_preset.
	if (config.scroller_proportion_preset) {
		free(config.scroller_proportion_preset);
		config.scroller_proportion_preset = NULL;
		config.scroller_proportion_preset_count = 0;
	}

	if (config.cursor_theme) {
		free(config.cursor_theme);
		config.cursor_theme = NULL;
	}

	if (config.jumplabeldata.font_desc) {
		free((void *)config.jumplabeldata.font_desc);
		config.jumplabeldata.font_desc = NULL;
	}

	if (config.groupbardata.font_desc) {
		free((void *)config.groupbardata.font_desc);
		config.groupbardata.font_desc = NULL;
	}

	if (config.tabbardata.font_desc) {
		free((void *)config.tabbardata.font_desc);
		config.tabbardata.font_desc = NULL;
	}

	if (config.jump_labels) {
		free(config.jump_labels);
		config.jump_labels = NULL;
	}

	// Frees circle_layout.
	free_circle_layout(&config);

	// Frees animation resources.
	free_baked_points();

	// Cleans up the keymap used for key parsing.
	cleanup_config_keymap();
}
