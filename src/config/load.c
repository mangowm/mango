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

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

Config config;

const char default_jump_labels[] = "HJKLASDFGQWERTYUIOPZXCVBNM";

static char **file_paths = NULL;
static int file_paths_count = 0;
static int current_file_index = -1;

#define CHVT(n)                                                                \
	{                                                                          \
		WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,                                  \
			{.keysym = XKB_KEY_XF86Switch_VT_##n, .type = KEY_TYPE_SYM},       \
			change_vt, {                                                       \
			.ui = (n)                                                          \
		}                                                                      \
	}

static KeyBinding default_key_bindings[] = {
	CHVT(1), CHVT(2), CHVT(3), CHVT(4),	 CHVT(5),  CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12)};

static bool parse_conf_stream(Config *config, FILE *file,
							  const char *full_path);
static ConfigFileFormat detect_config_format(const char *full_path, FILE *file);
static bool resolve_config_path(const char *file_path, char *full_path,
								size_t size);
static void resolve_keybinding_layout(struct xkb_keymap *keymap,
									  KeyBinding *binding);
static void resolve_bindings_to_configured_layouts(Config *config);

bool parse_config_line(Config *config, const char *line, int line_number) {
	char processed_line[512];
	strncpy(processed_line, line, sizeof(processed_line) - 1);
	processed_line[sizeof(processed_line) - 1] = '\0';

	remove_comment(processed_line);

	char key[256], value[256];
	if (sscanf(processed_line, "%255[^=]=%255[^\n]", key, value) != 2) {
		mango_error(false, WLR_ERROR,
					"Invalid line format: \033[1m\033[31m%s\033[0m\n", line);
		return false;
	}

	char *equals = strchr(processed_line, '=');
	if (equals && strcspn(equals + 1, "\n") >= sizeof(value)) {
		mango_error(false, WLR_ERROR,
					"Configuration value exceeds %zu bytes: "
					"\033[1m\033[31m%s\033[0m\n",
					sizeof(value) - 1, line);
		return false;
	}

	trim_whitespace(key);
	trim_whitespace(value);
	strip_quotes(value);

	return apply_option_expanded(config, key, value, line_number);
}

static bool parse_conf_stream(Config *config, FILE *file,
							  const char *full_path) {
	char line[512];
	bool parse_correct = true;
	uint32_t line_count = 0;

	while (fgets(line, sizeof(line), file)) {
		line_count++;
		if (line[0] == '#' || line[0] == '\n')
			continue;

		if (!parse_config_line(config, line, line_count)) {
			parse_correct = false;
			report_config_line_error(full_path, line_count, line);
		}
	}

	return parse_correct;
}

static ConfigFileFormat detect_config_format(const char *full_path,
											 FILE *file) {
	const char *dot = strrchr(full_path, '.');
	if (dot != NULL && strcasecmp(dot, ".toml") == 0)
		return CONFIG_FORMAT_TOML;
	if (dot != NULL && strcasecmp(dot, ".conf") == 0)
		return CONFIG_FORMAT_CONF;

	int c;
	while ((c = fgetc(file)) != EOF) {
		if (c == '#') {
			while ((c = fgetc(file)) != EOF && c != '\n')
				;
			continue;
		}
		if (isspace(c))
			continue;
		ungetc(c, file);
		return c == '[' ? CONFIG_FORMAT_TOML : CONFIG_FORMAT_CONF;
	}
	return CONFIG_FORMAT_CONF;
}

static bool resolve_config_path(const char *file_path, char *full_path,
								size_t size) {
	if (file_path[0] == '.' && file_path[1] == '/') {
		const char *rel = file_path + 2;
		if (server.cli_config_path[0]) {
			char *config_path = strdup(server.cli_config_path);
			char *config_dir = dirname(config_path);
			snprintf(full_path, size, "%s/%s", config_dir, rel);
			free(config_path);
		} else {
			const char *confdir = getenv("XDG_CONFIG_HOME");
			if (!confdir) {
				confdir = getenv("HOME");
				if (!confdir) {
					mango_error(false, WLR_ERROR,
								"HOME environment variable not set.\n");
					return false;
				}
				snprintf(full_path, size, "%s/.config/mango/%s", confdir, rel);
			} else {
				snprintf(full_path, size, "%s/mango/%s", confdir, rel);
			}
		}
	} else if (file_path[0] == '~' &&
			   (file_path[1] == '/' || file_path[1] == '\0')) {
		const char *home = getenv("HOME");
		if (!home) {
			mango_error(false, WLR_ERROR,
						"HOME environment variable not set.\n");
			return false;
		}
		snprintf(full_path, size, "%s%s", home, file_path + 1);
	} else {
		snprintf(full_path, size, "%s", file_path);
	}
	return true;
}

bool parse_config_file(Config *config, const char *file_path, bool must_exist) {
	char full_path[1024];

	if (!resolve_config_path(file_path, full_path, sizeof(full_path)))
		return false;

	FILE *file = fopen(full_path, "r");

	// Saves the current file index to restore after recursion.
	int saved_file_index = current_file_index;

	// Adds the file path to the global list.
	file_paths = realloc(file_paths, (file_paths_count + 1) * sizeof(char *));
	file_paths[file_paths_count] =
		strdup(full_path); // Needs strdup for independent memory.
	current_file_index = file_paths_count;
	file_paths_count++;

	if (!file) {
		current_file_index = saved_file_index;
		if (must_exist) {
			mango_error(false, WLR_ERROR,
						"Failed to open "
						"config file: \033[1m\033[31m%s\033[0m\n",
						file_path);
			return false;
		} else {
			return true;
		}
	}

	ConfigFileFormat format = detect_config_format(full_path, file);
	bool parse_correct = (format == CONFIG_FORMAT_TOML)
							 ? parse_toml_stream(config, file, full_path)
							 : parse_conf_stream(config, file, full_path);

	fclose(file);

	current_file_index = saved_file_index;
	return parse_correct;
}

static void resolve_keybinding_layout(struct xkb_keymap *keymap,
									  KeyBinding *binding) {
	if (binding->keysymcode.keysym == XKB_KEY_NoSymbol)
		return;

	if (binding->keysymcode.type != KEY_TYPE_CODE &&
		!binding->keysymcode.unresolved) {
		return;
	}

	MultiKeycode keycode = {0};

	if (find_keycodes_for_keysym(keymap, binding->keysymcode.keysym, &keycode) >
		0) {
		binding->keysymcode.keycode = keycode;
		binding->keysymcode.type = KEY_TYPE_CODE;
		binding->keysymcode.unresolved = false;
		return;
	}

	if (!binding->keysymcode.unresolved)
		return;

	char name[64] = {0};

	xkb_keysym_get_name(binding->keysymcode.keysym, name, sizeof(name));
	mango_error(false, WLR_ERROR,
				"Key '\033[1m\033[31m%s\033[0m' has no keycode in the "
				"configured layouts; it is matched by keysym, which depends "
				"on the active layout\n",
				name);
}

static void resolve_bindings_to_configured_layouts(Config *config) {
	if (config->keymap != NULL) {
		xkb_keymap_unref(config->keymap);
		config->keymap = NULL;
	}

	if (config->ctx != NULL) {
		config->keymap = xkb_keymap_new_from_names(
			config->ctx, &config->xkb_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}

	if (config->keymap == NULL) {
		mango_error(false, WLR_ERROR,
					"Invalid xkb_rules_* layout; key names are resolved with "
					"the us layout\n");

		if (config->ctx != NULL) {
			config->keymap = xkb_keymap_new_from_names(
				config->ctx, &xkb_fallback_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
		}
	}

	for (int32_t i = 0; i < config->key_bindings_count; i++) {
		resolve_keybinding_layout(config->keymap, &config->key_bindings[i]);
	}

	for (int32_t i = 0; i < config->window_rules_count; i++) {
		ConfigWinRule *rule = &config->window_rules[i];

		if (rule->globalkeybinding.mod != 0) {
			resolve_keybinding_layout(config->keymap, &rule->globalkeybinding);
		}
	}
}

void set_default_key_bindings(Config *config) {
	KeyBinding *b = NULL;

	// Computes the default key binding count.
	size_t default_key_bindings_count =
		sizeof(default_key_bindings) / sizeof(KeyBinding);

	// Reallocates memory to hold the new default bindings.
	config->key_bindings =
		realloc(config->key_bindings,
				(config->key_bindings_count + default_key_bindings_count) *
					sizeof(KeyBinding));
	if (!config->key_bindings) {
		return;
	}

	// Copies the default bindings into the config key binding array.
	for (size_t i = 0; i < default_key_bindings_count; i++) {
		config->key_bindings[config->key_bindings_count + i] =
			default_key_bindings[i];
		b = &config->key_bindings[config->key_bindings_count + i];
		b->iscommonmode = true;
		b->islockapply = true;
		b->line_number = 0;
		strcpy(b->mode, "common");
	}

	// Updates the total binding count.
	config->key_bindings_count += default_key_bindings_count;
}

bool parse_config(void) {
	char filename[1024];

	config_error_store_begin();

	if (file_paths) {
		for (int i = 0; i < file_paths_count; i++) {
			free(file_paths[i]);
		}
		free(file_paths);
		file_paths = NULL;
		file_paths_count = 0;
	}

	free_config();

	memset(&config, 0, sizeof(config));

	// Points the xkb_rules pointers back to the static arrays.
	config.xkb_rules.layout = config.xkb_rules_layout;
	config.xkb_rules.variant = config.xkb_rules_variant;
	config.xkb_rules.options = config.xkb_rules_options;
	config.xkb_rules.rules = config.xkb_rules_rules;
	config.xkb_rules.model = config.xkb_rules_model;

	// Initializes dynamic array pointers to NULL to avoid dangling pointers.
	config.window_rules = NULL;
	config.window_rules_count = 0;
	config.monitor_rules = NULL;
	config.monitor_rules_count = 0;
	config.device_rules = NULL;
	config.device_rules_count = 0;
	config.key_bindings = NULL;
	config.key_bindings_count = 0;
	config.mouse_bindings = NULL;
	config.mouse_bindings_count = 0;
	config.axis_bindings = NULL;
	config.axis_bindings_count = 0;
	config.switch_bindings = NULL;
	config.switch_bindings_count = 0;
	config.gesture_bindings = NULL;
	config.gesture_bindings_count = 0;
	config.env = NULL;
	config.env_count = 0;
	config.vars = NULL;
	config.vars_count = 0;
	config.exec = NULL;
	config.exec_count = 0;
	config.exec_once = NULL;
	config.exec_once_count = 0;
	config.scroller_proportion_preset = NULL;
	config.scroller_proportion_preset_count = 0;
	config.circle_layout = NULL;
	config.circle_layout_count = 0;
	config.tag_rules = NULL;
	config.tag_rules_count = 0;
	config.cursor_theme = NULL;
	config.jumplabeldata.font_desc = NULL;
	config.groupbardata.font_desc = NULL;
	config.jump_labels = NULL;
	strcpy(config.keymode, "default");

	create_config_keymap();

	if (server.cli_config_path[0]) {
		snprintf(filename, sizeof(filename), "%s", server.cli_config_path);
	} else {
		const char *confdir = getenv("XDG_CONFIG_HOME");
		if (!confdir) {
			confdir = getenv("HOME");

			if (!confdir) {
				// Cannot continue if that fails.
				config_error_store_end();
				return false;
			}

			snprintf(filename, sizeof(filename), "%s/.config/mango/config.conf",
					 confdir);
			if (access(filename, F_OK) != 0) {
				snprintf(filename, sizeof(filename),
						 "%s/.config/mango/config.toml", confdir);
			}

		} else {
			snprintf(filename, sizeof(filename), "%s/mango/config.conf",
					 confdir);
			if (access(filename, F_OK) != 0) {
				snprintf(filename, sizeof(filename), "%s/mango/config.toml",
						 confdir);
			}
		}

		if (access(filename, F_OK) != 0) {
			snprintf(filename, sizeof(filename), "%s/mango/config.conf",
					 SYSCONFDIR);
			if (access(filename, F_OK) != 0) {
				snprintf(filename, sizeof(filename), "%s/mango/config.toml",
						 SYSCONFDIR);
			}
		}
	}

	bool parse_correct = true;
	bool keybindings_conflict = false;
	set_value_default();
	parse_correct = parse_config_file(&config, filename, true);
	resolve_bindings_to_configured_layouts(&config);
	set_default_key_bindings(&config);
	override_config();

	keybindings_conflict = check_key_binding_conflicts(&config);
	keybindings_conflict |= check_mouse_binding_conflicts(&config);
	keybindings_conflict |= check_axis_binding_conflicts(&config);
	keybindings_conflict |= check_switch_binding_conflicts(&config);
	keybindings_conflict |= check_gesture_binding_conflicts(&config);

	bool result = parse_correct && !keybindings_conflict;
	config_error_store_end();
	config_error_nag_update();
	return result;
}

char **config_get_file_paths(int *count) {
	if (count) {
		*count = file_paths_count;
	}
	return file_paths;
}

int config_current_file_index(void) { return current_file_index; }

const char *config_file_path_at(int index) {
	if (index < 0 || index >= file_paths_count)
		return NULL;
	return file_paths[index];
}
