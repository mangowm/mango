#include <ctype.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

// 整数版本 - 截断小数部分
#define CLAMP_INT(x, min, max)                                                 \
	((int32_t)(x) < (int32_t)(min)                                             \
		 ? (int32_t)(min)                                                      \
		 : ((int32_t)(x) > (int32_t)(max) ? (int32_t)(max) : (int32_t)(x)))

// 浮点数版本 - 保留小数部分
#define CLAMP_FLOAT(x, min, max)                                               \
	((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

enum { NUM_TYPE_MINUS, NUM_TYPE_PLUS, NUM_TYPE_DEFAULT };

enum { KEY_TYPE_CODE, KEY_TYPE_SYM };

typedef struct {
	uint32_t keycode1;
	uint32_t keycode2;
	uint32_t keycode3;
} MultiKeycode;

typedef struct {
	xkb_keysym_t keysym;
	MultiKeycode keycode;
	int32_t type;
} KeySymCode;

typedef struct {
	uint32_t mod;
	KeySymCode keysymcode;
	int32_t (*func)(const Arg *);
	Arg arg;
	char mode[28];
	bool iscommonmode;
	bool isdefaultmode;
	bool islockapply;
	bool isreleaseapply;
	bool ispassapply;
} KeyBinding;

typedef struct {
	char *type;
	char *value;
} ConfigEnv;

typedef struct {
	const char *id;
	const char *title;
	uint32_t tags;
	int32_t isfloating;
	int32_t isfullscreen;
	int32_t isfakefullscreen;
	float scroller_proportion;
	const char *animation_type_open;
	const char *animation_type_close;
	const char *layer_animation_type_open;
	const char *layer_animation_type_close;
	int32_t isnoborder;
	int32_t isnoshadow;
	int32_t isnoradius;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t isnamedscratchpad;
	int32_t isunglobal;
	int32_t isglobal;
	int32_t isoverlay;
	int32_t allow_shortcuts_inhibit;
	int32_t ignore_maximize;
	int32_t ignore_minimize;
	int32_t isnosizehint;
	int32_t atstartup;
	const char *monitor;
	int32_t offsetx;
	int32_t offsety;
	int32_t width;
	int32_t height;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isterm;
	int32_t allow_csd;
	int32_t force_maximize;
	int32_t force_tiled_state;
	int32_t force_tearing;
	int32_t noswallow;
	int32_t noblur;
	float focused_opacity;
	float unfocused_opacity;
	float scroller_proportion_single;
	uint32_t passmod;
	xkb_keysym_t keysym;
	KeyBinding globalkeybinding;
} ConfigWinRule;

typedef struct {
	const char *name;			 // Monitor name
	char *make, *model, *serial; // may be NULL
	int32_t rr;					 // Rotate and flip (assume integer)
	float scale;				 // Monitor scale factor
	int32_t x, y;				 // Monitor position
	int32_t width, height;		 // Monitor resolution
	float refresh;				 // Refresh rate
	int32_t vrr;				 // variable refresh rate
} ConfigMonitorRule;

// 修改后的宏定义
#define CHVT(n)                                                                \
	{                                                                          \
		WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,                                  \
			{.keysym = XKB_KEY_XF86Switch_VT_##n, .type = KEY_TYPE_SYM}, chvt, \
		{                                                                      \
			.ui = (n)                                                          \
		}                                                                      \
	}

// 默认按键绑定数组
KeyBinding default_key_bindings[] = {CHVT(1), CHVT(2),	CHVT(3),  CHVT(4),
									 CHVT(5), CHVT(6),	CHVT(7),  CHVT(8),
									 CHVT(9), CHVT(10), CHVT(11), CHVT(12)};

typedef struct {
	uint32_t mod;
	uint32_t button;
	int32_t (*func)(const Arg *);
	Arg arg;
} MouseBinding;

typedef struct {
	uint32_t mod;
	uint32_t dir;
	int32_t (*func)(const Arg *);
	Arg arg;
} AxisBinding;

typedef struct {
	uint32_t fold;
	int32_t (*func)(const Arg *);
	Arg arg;
} SwitchBinding;

typedef struct {
	uint32_t mod;
	uint32_t motion;
	uint32_t fingers_count;
	int32_t (*func)(const Arg *);
	Arg arg;
} GestureBinding;

typedef struct {
	int32_t id;
	char *layout_name;
	char *monitor_name;
	char *monitor_make;
	char *monitor_model;
	char *monitor_serial;
	float mfact;
	int32_t nmaster;
	int32_t no_render_border;
	int32_t no_hide;
} ConfigTagRule;

typedef struct {
	char *layer_name; // 布局名称
	char *animation_type_open;
	char *animation_type_close;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
} ConfigLayerRule;

typedef struct {
	int32_t animations;
	int32_t layer_animations;
	char animation_type_open[10];
	char animation_type_close[10];
	char layer_animation_type_open[10];
	char layer_animation_type_close[10];
	int32_t animation_fade_in;
	int32_t animation_fade_out;
	int32_t tag_animation_direction;
	float zoom_initial_ratio;
	float zoom_end_ratio;
	float fadein_begin_opacity;
	float fadeout_begin_opacity;
	uint32_t animation_duration_move;
	uint32_t animation_duration_open;
	uint32_t animation_duration_tag;
	uint32_t animation_duration_close;
	uint32_t animation_duration_focus;
	double animation_curve_move[4];
	double animation_curve_open[4];
	double animation_curve_tag[4];
	double animation_curve_close[4];
	double animation_curve_focus[4];
	double animation_curve_opafadein[4];
	double animation_curve_opafadeout[4];

	int32_t scroller_structs;
	float scroller_default_proportion;
	float scroller_default_proportion_single;
	int32_t scroller_ignore_proportion_single;
	int32_t scroller_focus_center;
	int32_t scroller_prefer_center;
	int32_t scroller_prefer_overspread;
	int32_t edge_scroller_pointer_focus;
	int32_t focus_cross_monitor;
	int32_t exchange_cross_monitor;
	int32_t scratchpad_cross_monitor;
	int32_t focus_cross_tag;
	int32_t view_current_to_back;
	int32_t no_border_when_single;
	int32_t no_radius_when_single;
	int32_t snap_distance;
	int32_t enable_floating_snap;
	int32_t drag_tile_to_tile;
	uint32_t swipe_min_threshold;
	float focused_opacity;
	float unfocused_opacity;
	float *scroller_proportion_preset;
	int32_t scroller_proportion_preset_count;

	char **circle_layout;
	int32_t circle_layout_count;

	uint32_t new_is_master;
	float default_mfact;
	uint32_t default_nmaster;
	int32_t center_master_overspread;
	int32_t center_when_single_stack;

	uint32_t hotarea_size;
	uint32_t hotarea_corner;
	uint32_t enable_hotarea;
	uint32_t ov_tab_mode;
	int32_t overviewgappi;
	int32_t overviewgappo;
	uint32_t cursor_hide_timeout;

	uint32_t axis_bind_apply_timeout;
	uint32_t focus_on_activate;
	int32_t idleinhibit_ignore_visible;
	int32_t sloppyfocus;
	int32_t warpcursor;
	int32_t drag_corner;
	int32_t drag_warp_cursor;

	/* keyboard */
	int32_t repeat_rate;
	int32_t repeat_delay;
	uint32_t numlockon;

	/* Trackpad */
	int32_t disable_trackpad;
	int32_t tap_to_click;
	int32_t tap_and_drag;
	int32_t drag_lock;
	int32_t mouse_natural_scrolling;
	int32_t trackpad_natural_scrolling;
	int32_t disable_while_typing;
	int32_t left_handed;
	int32_t middle_button_emulation;
	uint32_t accel_profile;
	double accel_speed;
	uint32_t scroll_method;
	uint32_t scroll_button;
	uint32_t click_method;
	uint32_t send_events_mode;
	uint32_t button_map;

	double axis_scroll_factor;

	int32_t blur;
	int32_t blur_layer;
	int32_t blur_optimized;
	int32_t border_radius;
	struct blur_data blur_params;
	int32_t shadows;
	int32_t shadow_only_floating;
	int32_t layer_shadows;
	uint32_t shadows_size;
	float shadows_blur;
	int32_t shadows_position_x;
	int32_t shadows_position_y;
	float shadowscolor[4];

	int32_t smartgaps;
	uint32_t gappih;
	uint32_t gappiv;
	uint32_t gappoh;
	uint32_t gappov;
	uint32_t borderpx;
	float scratchpad_width_ratio;
	float scratchpad_height_ratio;
	float rootcolor[4];
	float bordercolor[4];
	float focuscolor[4];
	float maximizescreencolor[4];
	float urgentcolor[4];
	float scratchpadcolor[4];
	float globalcolor[4];
	float overlaycolor[4];

	char autostart[3][256];

	ConfigTagRule *tag_rules; // 动态数组
	int32_t tag_rules_count;  // 数量

	ConfigLayerRule *layer_rules; // 动态数组
	int32_t layer_rules_count;	  // 数量

	ConfigWinRule *window_rules;
	int32_t window_rules_count;

	ConfigMonitorRule *monitor_rules; // 动态数组
	int32_t monitor_rules_count;	  // 条数

	KeyBinding *key_bindings;
	int32_t key_bindings_count;

	MouseBinding *mouse_bindings;
	int32_t mouse_bindings_count;

	AxisBinding *axis_bindings;
	int32_t axis_bindings_count;

	SwitchBinding *switch_bindings;
	int32_t switch_bindings_count;

	GestureBinding *gesture_bindings;
	int32_t gesture_bindings_count;

	ConfigEnv **env;
	int32_t env_count;

	char **exec;
	int32_t exec_count;

	char **exec_once;
	int32_t exec_once_count;

	char *cursor_theme;
	uint32_t cursor_size;

	int32_t single_scratchpad;
	int32_t xwayland_persistence;
	int32_t syncobj_enable;
	int32_t allow_tearing;
	int32_t allow_shortcuts_inhibit;
	int32_t allow_lock_transparent;

	struct xkb_rule_names xkb_rules;

	char keymode[28];

	struct xkb_context *ctx;
	struct xkb_keymap *keymap;
} Config;

typedef int32_t (*FuncType)(const Arg *);
Config config;

bool parse_config_file(Config *config, const char *file_path, bool must_exist);

// Helper function to trim whitespace from start and end of a string
void trim_whitespace(char *str) {
	if (str == NULL || *str == '\0')
		return;

	// Trim leading space
	char *start = str;
	while (isspace((unsigned char)*start)) {
		start++;
	}

	// Trim trailing space
	char *end = str + strlen(str) - 1;
	while (end > start && isspace((unsigned char)*end)) {
		end--;
	}

	// Null-terminate the trimmed string
	*(end + 1) = '\0';

	// Move the trimmed part to the beginning if needed
	if (start != str) {
		memmove(str, start, end - start + 2); // +2 to include null terminator
	}
}

int32_t parse_double_array(const char *input, double *output,
						   int32_t max_count) {
	char *dup = strdup(input);
	char *token;
	int32_t count = 0;

	// 先清空整个数组
	memset(output, 0, max_count * sizeof(double));

	token = strtok(dup, ",");
	while (token != NULL && count < max_count) {
		trim_whitespace(token);
		char *endptr;
		double val = strtod(token, &endptr);
		if (endptr == token || *endptr != '\0') {
			fprintf(
				stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Invalid number in array: %s\n",
				token);
			free(dup);
			return -1;
		}
		output[count] = val; // 赋值到当前count位置
		count++;			 // 然后才自增
		token = strtok(NULL, ",");
	}

	free(dup);
	return count;
}

// 清理字符串中的不可见字符（包括 \r, \n, 空格等）
char *sanitize_string(char *str) {
	// 去除首部不可见字符
	while (*str != '\0' && !isprint((unsigned char)*str))
		str++;
	// 去除尾部不可见字符
	char *end = str + strlen(str) - 1;
	while (end > str && !isprint((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return str;
}

// 解析bind组合字符串
void parse_bind_flags(const char *str, KeyBinding *kb) {

	// 检查是否以"bind"开头
	if (strncmp(str, "bind", 4) != 0) {
		return;
	}

	const char *suffix = str + 4; // 跳过"bind"

	// 遍历后缀字符
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
		default:
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Unknown bind flag: %c\n",
					suffix[i]);
			break;
		}
	}
}

int32_t parse_circle_direction(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "next") == 0) {
		return NEXT;
	} else {
		return PREV;
	}
}

int32_t parse_direction(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "up") == 0) {
		return UP;
	} else if (strcmp(lowerStr, "down") == 0) {
		return DOWN;
	} else if (strcmp(lowerStr, "left") == 0) {
		return LEFT;
	} else if (strcmp(lowerStr, "right") == 0) {
		return RIGHT;
	} else {
		return UNDIR;
	}
}

int32_t parse_fold_state(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// 根据转换后的小写字符串返回对应的枚举值
	if (strcmp(lowerStr, "fold") == 0) {
		return FOLD;
	} else if (strcmp(lowerStr, "unfold") == 0) {
		return UNFOLD;
	} else {
		return INVALIDFOLD;
	}
}
int64_t parse_color(const char *hex_str) {
	char *endptr;
	int64_t hex_num = strtol(hex_str, &endptr, 16);
	if (*endptr != '\0') {
		return -1;
	}
	return hex_num;
}

// 辅助函数：检查字符串是否以指定的前缀开头（忽略大小写）
static bool starts_with_ignore_case(const char *str, const char *prefix) {
	while (*prefix) {
		if (tolower(*str) != tolower(*prefix)) {
			return false;
		}
		str++;
		prefix++;
	}
	return true;
}

static char *combine_args_until_empty(char *values[], int count) {
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
	size_t total_len = 0;
	for (int i = 0; i < first_empty; i++) {
		total_len += strlen(values[i]);
	}
	// 	plus the number of commas (first_empty-1 commas)
	total_len += (first_empty - 1);

	// 	allocate memory and concatenate
	char *combined = malloc(total_len + 1);
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

uint32_t parse_mod(const char *mod_str) {
	if (!mod_str || !*mod_str) {
		return UINT32_MAX;
	}

	uint32_t mod = 0;
	char input_copy[256];
	char *token;
	char *saveptr = NULL;
	bool match_success = false;

	// 复制并转换为小写
	strncpy(input_copy, mod_str, sizeof(input_copy) - 1);
	input_copy[sizeof(input_copy) - 1] = '\0';
	for (char *p = input_copy; *p; p++) {
		*p = tolower(*p);
	}

	// 分割处理每个部分
	token = strtok_r(input_copy, "+", &saveptr);
	while (token != NULL) {
		// 去除空白
		while (*token == ' ' || *token == '\t')
			token++;

		if (strncmp(token, "code:", 5) == 0) {
			// 处理 code: 形式
			char *endptr;
			long keycode = strtol(token + 5, &endptr, 10);
			if (endptr != token + 5 && (*endptr == '\0' || *endptr == ' ')) {
				switch (keycode) {
				case 133:
				case 134:
					mod |= WLR_MODIFIER_LOGO;
					break;
				case 37:
				case 105:
					mod |= WLR_MODIFIER_CTRL;
					break;
				case 50:
				case 62:
					mod |= WLR_MODIFIER_SHIFT;
					break;
				case 64:
				case 108:
					mod |= WLR_MODIFIER_ALT;
					break;
				default:
					fprintf(stderr,
							"unknown modifier keycode: \033[1m\033[31m%s\n",
							token);
					break;
				}
			}
		} else {
			// 完整的 modifier 检查（保留原始所有检查项）
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
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Unknown modifier: "
				"\033[1m\033[31m%s\n",
				mod_str);
	}

	return mod;
}

// 定义辅助函数：在 keymap 中查找 keysym 对应的多个 keycode
static int32_t find_keycodes_for_keysym(struct xkb_keymap *keymap,
										xkb_keysym_t sym,
										MultiKeycode *multi_kc) {
	xkb_keycode_t min_keycode = xkb_keymap_min_keycode(keymap);
	xkb_keycode_t max_keycode = xkb_keymap_max_keycode(keymap);

	multi_kc->keycode1 = 0;
	multi_kc->keycode2 = 0;
	multi_kc->keycode3 = 0;

	int32_t found_count = 0;

	for (xkb_keycode_t keycode = min_keycode;
		 keycode <= max_keycode && found_count < 3; keycode++) {
		// 使用布局0和层级0
		const xkb_keysym_t *syms;
		int32_t num_syms =
			xkb_keymap_key_get_syms_by_level(keymap, keycode, 0, 0, &syms);

		for (int32_t i = 0; i < num_syms; i++) {
			if (syms[i] == sym) {
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
				break;
			}
		}
	}

	return found_count;
}

void cleanup_config_keymap(void) {
	if (config.keymap != NULL) {
		xkb_keymap_unref(config.keymap);
		config.keymap = NULL;
	}
	if (config.ctx != NULL) {
		xkb_context_unref(config.ctx);
		config.ctx = NULL;
	}
}

void create_config_keymap(void) {
	// 初始化 xkb 上下文和 keymap

	if (config.ctx == NULL) {
		config.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	}

	if (config.keymap == NULL) {
		config.keymap = xkb_keymap_new_from_names(
			config.ctx, &xkb_fallback_rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}
}

KeySymCode parse_key(const char *key_str, bool isbindsym) {
	KeySymCode kc = {0}; // 初始化为0

	if (config.keymap == NULL || config.ctx == NULL) {
		// 处理错误
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		return kc;
	}

	// 处理 code: 前缀的情况
	if (strncmp(key_str, "code:", 5) == 0) {
		char *endptr;
		xkb_keycode_t keycode = (xkb_keycode_t)strtol(key_str + 5, &endptr, 10);
		kc.type = KEY_TYPE_CODE;
		kc.keycode.keycode1 = keycode; // 只设置第一个
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
		// 尝试找到对应的多个 keycode
		int32_t found_count =
			find_keycodes_for_keysym(config.keymap, sym, &kc.keycode);
		if (found_count > 0) {
			kc.type = KEY_TYPE_CODE;
			kc.keysym = sym; // 仍然保存 keysym 供参考
		} else {
			kc.type = KEY_TYPE_SYM;
			kc.keysym = sym;
			// keycode 字段保持为0
		}
	} else {
		// 无法解析的键名
		kc.type = KEY_TYPE_SYM;
		kc.keysym = XKB_KEY_NoSymbol;
		fprintf(
			stderr,
			"\033[1m\033[31m[ERROR]:\033[33m Unknown key: \033[1m\033[31m%s\n",
			key_str);
		// keycode 字段保持为0
	}

	return kc;
}

uint32_t parse_button(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // 确保字符串正确终止

	// 根据转换后的小写字符串返回对应的按钮编号
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
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Unknown button: "
				"\033[1m\033[31m%s\n",
				str);
		return UINT32_MAX;
	}
}

int32_t parse_mouse_action(const char *str) {
	// 将输入字符串转换为小写
	char lowerStr[20];
	int32_t i = 0;
	while (str[i] && i < 19) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0'; // 确保字符串正确终止

	// 根据转换后的小写字符串返回对应的按钮编号
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

void convert_hex_to_rgba(float *color, uint32_t hex) {
	color[0] = ((hex >> 24) & 0xFF) / 255.0f;
	color[1] = ((hex >> 16) & 0xFF) / 255.0f;
	color[2] = ((hex >> 8) & 0xFF) / 255.0f;
	color[3] = (hex & 0xFF) / 255.0f;
}

uint32_t parse_num_type(char *str) {
	switch (str[0]) {
	case '-':
		return NUM_TYPE_MINUS;
	case '+':
		return NUM_TYPE_PLUS;
	default:
		return NUM_TYPE_DEFAULT;
	}
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
		func = focusstack;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "focusdir") == 0) {
		func = focusdir;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "incnmaster") == 0) {
		func = incnmaster;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "setmfact") == 0) {
		func = setmfact;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "zoom") == 0) {
		func = zoom;
	} else if (strcmp(func_name, "exchange_client") == 0) {
		func = exchange_client;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "exchange_stack_client") == 0) {
		func = exchange_stack_client;
		(*arg).i = parse_circle_direction(arg_value);
	} else if (strcmp(func_name, "toggleglobal") == 0) {
		func = toggleglobal;
	} else if (strcmp(func_name, "toggleoverview") == 0) {
		func = toggleoverview;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "set_proportion") == 0) {
		func = set_proportion;
		(*arg).f = atof(arg_value);
	} else if (strcmp(func_name, "switch_proportion_preset") == 0) {
		func = switch_proportion_preset;
	} else if (strcmp(func_name, "viewtoleft") == 0) {
		func = viewtoleft;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright") == 0) {
		func = viewtoright;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "tagsilent") == 0) {
		func = tagsilent;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "tagtoleft") == 0) {
		func = tagtoleft;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "tagtoright") == 0) {
		func = tagtoright;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "killclient") == 0) {
		func = killclient;
	} else if (strcmp(func_name, "centerwin") == 0) {
		func = centerwin;
	} else if (strcmp(func_name, "focuslast") == 0) {
		func = focuslast;
	} else if (strcmp(func_name, "toggle_trackpad_enable") == 0) {
		func = toggle_trackpad_enable;
	} else if (strcmp(func_name, "setoption") == 0) {
		func = setoption;

		(*arg).v = strdup(arg_value);

		// 收集需要拼接的参数
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

		// 处理拼接
		if (param_index == 0) {
			(*arg).v2 = strdup("");
		} else {
			// 计算总长度
			size_t len = 0;
			for (int32_t i = 0; i < param_index; i++) {
				len += strlen(non_empty_params[i]);
			}
			len += (param_index - 1) + 1; // 逗号数 + null终止符

			char *temp = malloc(len);
			if (temp) {
				char *cursor = temp;
				for (int32_t i = 0; i < param_index; i++) {
					if (i > 0) {
						*cursor++ = ',';
					}
					size_t param_len = strlen(non_empty_params[i]);
					memcpy(cursor, non_empty_params[i], param_len);
					cursor += param_len;
				}
				*cursor = '\0';
				(*arg).v2 = temp;
			}
		}
	} else if (strcmp(func_name, "setkeymode") == 0) {
		func = setkeymode;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_keyboard_layout") == 0) {
		func = switch_keyboard_layout;
		(*arg).i = CLAMP_INT(atoi(arg_value), 0, 100);
	} else if (strcmp(func_name, "setlayout") == 0) {
		func = setlayout;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "switch_layout") == 0) {
		func = switch_layout;
	} else if (strcmp(func_name, "togglefloating") == 0) {
		func = togglefloating;
	} else if (strcmp(func_name, "togglefullscreen") == 0) {
		func = togglefullscreen;
	} else if (strcmp(func_name, "togglefakefullscreen") == 0) {
		func = togglefakefullscreen;
	} else if (strcmp(func_name, "toggleoverlay") == 0) {
		func = toggleoverlay;
	} else if (strcmp(func_name, "minimized") == 0) {
		func = minimized;
	} else if (strcmp(func_name, "restore_minimized") == 0) {
		func = restore_minimized;
	} else if (strcmp(func_name, "toggle_scratchpad") == 0) {
		func = toggle_scratchpad;
	} else if (strcmp(func_name, "toggle_render_border") == 0) {
		func = toggle_render_border;
	} else if (strcmp(func_name, "focusmon") == 0) {
		func = focusmon;
		(*arg).i = parse_direction(arg_value);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		}
	} else if (strcmp(func_name, "tagmon") == 0) {
		func = tagmon;
		(*arg).i = parse_direction(arg_value);
		(*arg).i2 = atoi(arg_value2);
		if ((*arg).i == UNDIR) {
			(*arg).v = strdup(arg_value);
		};
	} else if (strcmp(func_name, "incgaps") == 0) {
		func = incgaps;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "togglegaps") == 0) {
		func = togglegaps;
	} else if (strcmp(func_name, "chvt") == 0) {
		func = chvt;
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
		(*arg).v = strdup(arg_value); // 注意：之后需要释放这个内存
		(*arg).ui = 1 << (atoi(arg_value2) - 1);
	} else if (strcmp(func_name, "quit") == 0) {
		func = quit;
	} else if (strcmp(func_name, "create_virtual_output") == 0) {
		func = create_virtual_output;
	} else if (strcmp(func_name, "destroy_all_virtual_output") == 0) {
		func = destroy_all_virtual_output;
	} else if (strcmp(func_name, "moveresize") == 0) {
		func = moveresize;
		(*arg).ui = parse_mouse_action(arg_value);
	} else if (strcmp(func_name, "togglemaximizescreen") == 0) {
		func = togglemaximizescreen;
	} else if (strcmp(func_name, "viewtoleft_have_client") == 0) {
		func = viewtoleft_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "viewtoright_have_client") == 0) {
		func = viewtoright_have_client;
		(*arg).i = atoi(arg_value);
	} else if (strcmp(func_name, "reload_config") == 0) {
		func = reload_config;
	} else if (strcmp(func_name, "tag") == 0) {
		func = tag;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "view") == 0) {
		func = bind_to_view;

		uint32_t mask = 0;
		char *token;
		char *arg_copy = strdup(arg_value);

		if (arg_copy != NULL) {
			char *saveptr = NULL;
			token = strtok_r(arg_copy, "|", &saveptr);

			while (token != NULL) {
				int32_t num = atoi(token);
				if (num > 0 && num <= LENGTH(tags)) {
					mask |= (1 << (num - 1));
				}
				token = strtok_r(NULL, "|", &saveptr);
			}

			free(arg_copy);
		}

		if (mask) {
			(*arg).ui = mask;
		} else {
			(*arg).ui = atoi(arg_value);
		}
		(*arg).i = atoi(arg_value2);
	} else if (strcmp(func_name, "viewcrossmon") == 0) {
		func = viewcrossmon;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "tagcrossmon") == 0) {
		func = tagcrossmon;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
		(*arg).v = strdup(arg_value2);
	} else if (strcmp(func_name, "toggletag") == 0) {
		func = toggletag;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "toggleview") == 0) {
		func = toggleview;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "comboview") == 0) {
		func = comboview;
		(*arg).ui = 1 << (atoi(arg_value) - 1);
	} else if (strcmp(func_name, "smartmovewin") == 0) {
		func = smartmovewin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "smartresizewin") == 0) {
		func = smartresizewin;
		(*arg).i = parse_direction(arg_value);
	} else if (strcmp(func_name, "resizewin") == 0) {
		func = resizewin;
		(*arg).ui = parse_num_type(arg_value);
		(*arg).ui2 = parse_num_type(arg_value2);
		(*arg).i = (*arg).ui == NUM_TYPE_DEFAULT ? atoi(arg_value)
												 : atoi(arg_value + 1);
		(*arg).i2 = (*arg).ui2 == NUM_TYPE_DEFAULT ? atoi(arg_value2)
												   : atoi(arg_value2 + 1);
	} else if (strcmp(func_name, "movewin") == 0) {
		func = movewin;
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
	} else if (strcmp(func_name, "disable_monitor") == 0) {
		func = disable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "enable_monitor") == 0) {
		func = enable_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "toggle_monitor") == 0) {
		func = toggle_monitor;
		(*arg).v = strdup(arg_value);
	} else if (strcmp(func_name, "scroller_stack") == 0) {
		func = scroller_stack;
		(*arg).i = parse_direction(arg_value);
	} else {
		return NULL;
	}
	return func;
}

void set_env() {
	for (int32_t i = 0; i < config.env_count; i++) {
		setenv(config.env[i]->type, config.env[i]->value, 1);
	}
}

void run_exec() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_count; i++) {
		arg.v = config.exec[i];
		spawn_shell(&arg);
	}
}

void run_exec_once() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_once_count; i++) {
		arg.v = config.exec_once[i];
		spawn_shell(&arg);
	}
}

bool parse_option(Config *config, char *key, char *value) {
	if (strcmp(key, "keymode") == 0) {
		snprintf(config->keymode, sizeof(config->keymode), "%.27s", value);
	} else if (strcmp(key, "animations") == 0) {
		config->animations = atoi(value);
	} else if (strcmp(key, "layer_animations") == 0) {
		config->layer_animations = atoi(value);
	} else if (strcmp(key, "animation_type_open") == 0) {
		snprintf(config->animation_type_open,
				 sizeof(config->animation_type_open), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "animation_type_close") == 0) {
		snprintf(config->animation_type_close,
				 sizeof(config->animation_type_close), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "layer_animation_type_open") == 0) {
		snprintf(config->layer_animation_type_open,
				 sizeof(config->layer_animation_type_open), "%.9s",
				 value); // string limit to 9 char
	} else if (strcmp(key, "layer_animation_type_close") == 0) {
		snprintf(config->layer_animation_type_close,
				 sizeof(config->layer_animation_type_close), "%.9s",
				 value); // string limit to 9 char
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
	} else if (strcmp(key, "fadein_begin_opacity") == 0) {
		config->fadein_begin_opacity = atof(value);
	} else if (strcmp(key, "fadeout_begin_opacity") == 0) {
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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_move: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_open") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_open, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_open: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_tag") == 0) {
		int32_t num = parse_double_array(value, config->animation_curve_tag, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_tag: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_close") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_close, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_close: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_focus") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_focus, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_focus: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opafadein") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadein, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_opafadein: %s\n",
					value);
			return false;
		}
	} else if (strcmp(key, "animation_curve_opafadeout") == 0) {
		int32_t num =
			parse_double_array(value, config->animation_curve_opafadeout, 4);
		if (num != 4) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to parse "
					"animation_curve_opafadeout: %s\n",
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
	} else if (strcmp(key, "focus_cross_monitor") == 0) {
		config->focus_cross_monitor = atoi(value);
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
	} else if (strcmp(key, "syncobj_enable") == 0) {
		config->syncobj_enable = atoi(value);
	} else if (strcmp(key, "allow_tearing") == 0) {
		config->allow_tearing = atoi(value);
	} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
		config->allow_shortcuts_inhibit = atoi(value);
	} else if (strcmp(key, "allow_lock_transparent") == 0) {
		config->allow_lock_transparent = atoi(value);
	} else if (strcmp(key, "no_border_when_single") == 0) {
		config->no_border_when_single = atoi(value);
	} else if (strcmp(key, "no_radius_when_single") == 0) {
		config->no_radius_when_single = atoi(value);
	} else if (strcmp(key, "snap_distance") == 0) {
		config->snap_distance = atoi(value);
	} else if (strcmp(key, "enable_floating_snap") == 0) {
		config->enable_floating_snap = atoi(value);
	} else if (strcmp(key, "drag_tile_to_tile") == 0) {
		config->drag_tile_to_tile = atoi(value);
	} else if (strcmp(key, "swipe_min_threshold") == 0) {
		config->swipe_min_threshold = atoi(value);
	} else if (strcmp(key, "focused_opacity") == 0) {
		config->focused_opacity = atof(value);
	} else if (strcmp(key, "unfocused_opacity") == 0) {
		config->unfocused_opacity = atof(value);
	} else if (strcmp(key, "xkb_rules_rules") == 0) {
		strncpy(xkb_rules_rules, value, sizeof(xkb_rules_rules) - 1);
		xkb_rules_rules[sizeof(xkb_rules_rules) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_model") == 0) {
		strncpy(xkb_rules_model, value, sizeof(xkb_rules_model) - 1);
		xkb_rules_model[sizeof(xkb_rules_model) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_layout") == 0) {
		strncpy(xkb_rules_layout, value, sizeof(xkb_rules_layout) - 1);
		xkb_rules_layout[sizeof(xkb_rules_layout) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_variant") == 0) {
		strncpy(xkb_rules_variant, value, sizeof(xkb_rules_variant) - 1);
		xkb_rules_variant[sizeof(xkb_rules_variant) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "xkb_rules_options") == 0) {
		strncpy(xkb_rules_options, value, sizeof(xkb_rules_options) - 1);
		xkb_rules_options[sizeof(xkb_rules_options) - 1] =
			'\0'; // 确保字符串以 null 结尾
	} else if (strcmp(key, "scroller_proportion_preset") == 0) {
		// 1. 统计 value 中有多少个逗号，确定需要解析的浮点数个数
		int32_t count = 0; // 初始化为 0
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t float_count = count + 1; // 浮点数的数量是逗号数量加 1

		// 2. 动态分配内存，存储浮点数
		config->scroller_proportion_preset =
			(float *)malloc(float_count * sizeof(float));
		if (!config->scroller_proportion_preset) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Memory "
							"allocation failed\n");
			return false;
		}

		// 3. 解析 value 中的浮点数
		char *value_copy =
			strdup(value); // 复制 value，因为 strtok 会修改原字符串
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		float value_set;

		while (token != NULL && i < float_count) {
			if (sscanf(token, "%f", &value_set) != 1) {
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Invalid float "
						"value in "
						"scroller_proportion_preset: %s\n",
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

		// 4. 检查解析的浮点数数量是否匹配
		if (i != float_count) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid "
					"scroller_proportion_preset format: %s\n",
					value);
			free(value_copy);
			free(config->scroller_proportion_preset);  // 释放已分配的内存
			config->scroller_proportion_preset = NULL; // 防止野指针
			config->scroller_proportion_preset_count = 0;
			return false;
		}
		config->scroller_proportion_preset_count = float_count;

		// 5. 释放临时复制的字符串
		free(value_copy);
	} else if (strcmp(key, "circle_layout") == 0) {
		// 1. 统计 value 中有多少个逗号，确定需要解析的字符串个数
		int32_t count = 0; // 初始化为 0
		for (const char *p = value; *p; p++) {
			if (*p == ',')
				count++;
		}
		int32_t string_count = count + 1; // 字符串的数量是逗号数量加 1

		// 2. 动态分配内存，存储字符串指针
		config->circle_layout = (char **)malloc(string_count * sizeof(char *));
		memset(config->circle_layout, 0, string_count * sizeof(char *));
		if (!config->circle_layout) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Memory "
							"allocation failed\n");
			return false;
		}

		// 3. 解析 value 中的字符串
		char *value_copy =
			strdup(value); // 复制 value，因为 strtok 会修改原字符串
		char *token = strtok(value_copy, ",");
		int32_t i = 0;
		char *cleaned_token;
		while (token != NULL && i < string_count) {
			// 为每个字符串分配内存并复制内容
			cleaned_token = sanitize_string(token);
			config->circle_layout[i] = strdup(cleaned_token);
			if (!config->circle_layout[i]) {
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Memory allocation "
						"failed for "
						"string: %s\n",
						token);
				// 释放之前分配的内存
				for (int32_t j = 0; j < i; j++) {
					free(config->circle_layout[j]);
				}
				free(config->circle_layout);
				free(value_copy);
				config->circle_layout = NULL; // 防止野指针
				config->circle_layout_count = 0;
				return false;
			}
			token = strtok(NULL, ",");
			i++;
		}

		// 4. 检查解析的字符串数量是否匹配
		if (i != string_count) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid circle_layout "
					"format: %s\n",
					value);
			// 释放之前分配的内存
			for (int32_t j = 0; j < i; j++) {
				free(config->circle_layout[j]);
			}
			free(config->circle_layout);
			free(value_copy);
			config->circle_layout = NULL; // 防止野指针
			config->circle_layout_count = 0;
			return false;
		}
		config->circle_layout_count = string_count;

		// 5. 释放临时复制的字符串
		free(value_copy);
	} else if (strcmp(key, "new_is_master") == 0) {
		config->new_is_master = atoi(value);
	} else if (strcmp(key, "default_mfact") == 0) {
		config->default_mfact = atof(value);
	} else if (strcmp(key, "default_nmaster") == 0) {
		config->default_nmaster = atoi(value);
	} else if (strcmp(key, "center_master_overspread") == 0) {
		config->center_master_overspread = atoi(value);
	} else if (strcmp(key, "center_when_single_stack") == 0) {
		config->center_when_single_stack = atoi(value);
	} else if (strcmp(key, "hotarea_size") == 0) {
		config->hotarea_size = atoi(value);
	} else if (strcmp(key, "hotarea_corner") == 0) {
		config->hotarea_corner = atoi(value);
	} else if (strcmp(key, "enable_hotarea") == 0) {
		config->enable_hotarea = atoi(value);
	} else if (strcmp(key, "ov_tab_mode") == 0) {
		config->ov_tab_mode = atoi(value);
	} else if (strcmp(key, "overviewgappi") == 0) {
		config->overviewgappi = atoi(value);
	} else if (strcmp(key, "overviewgappo") == 0) {
		config->overviewgappo = atoi(value);
	} else if (strcmp(key, "cursor_hide_timeout") == 0) {
		config->cursor_hide_timeout = atoi(value);
	} else if (strcmp(key, "axis_bind_apply_timeout") == 0) {
		config->axis_bind_apply_timeout = atoi(value);
	} else if (strcmp(key, "focus_on_activate") == 0) {
		config->focus_on_activate = atoi(value);
	} else if (strcmp(key, "numlockon") == 0) {
		config->numlockon = atoi(value);
	} else if (strcmp(key, "idleinhibit_ignore_visible") == 0) {
		config->idleinhibit_ignore_visible = atoi(value);
	} else if (strcmp(key, "sloppyfocus") == 0) {
		config->sloppyfocus = atoi(value);
	} else if (strcmp(key, "warpcursor") == 0) {
		config->warpcursor = atoi(value);
	} else if (strcmp(key, "drag_corner") == 0) {
		config->drag_corner = atoi(value);
	} else if (strcmp(key, "drag_warp_cursor") == 0) {
		config->drag_warp_cursor = atoi(value);
	} else if (strcmp(key, "smartgaps") == 0) {
		config->smartgaps = atoi(value);
	} else if (strcmp(key, "repeat_rate") == 0) {
		config->repeat_rate = atoi(value);
	} else if (strcmp(key, "repeat_delay") == 0) {
		config->repeat_delay = atoi(value);
	} else if (strcmp(key, "disable_trackpad") == 0) {
		config->disable_trackpad = atoi(value);
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
		config->cursor_theme = strdup(value);
	} else if (strcmp(key, "disable_while_typing") == 0) {
		config->disable_while_typing = atoi(value);
	} else if (strcmp(key, "left_handed") == 0) {
		config->left_handed = atoi(value);
	} else if (strcmp(key, "middle_button_emulation") == 0) {
		config->middle_button_emulation = atoi(value);
	} else if (strcmp(key, "accel_profile") == 0) {
		config->accel_profile = atoi(value);
	} else if (strcmp(key, "accel_speed") == 0) {
		config->accel_speed = atof(value);
	} else if (strcmp(key, "scroll_method") == 0) {
		config->scroll_method = atoi(value);
	} else if (strcmp(key, "scroll_button") == 0) {
		config->scroll_button = atoi(value);
	} else if (strcmp(key, "click_method") == 0) {
		config->click_method = atoi(value);
	} else if (strcmp(key, "send_events_mode") == 0) {
		config->send_events_mode = atoi(value);
	} else if (strcmp(key, "button_map") == 0) {
		config->button_map = atoi(value);
	} else if (strcmp(key, "axis_scroll_factor") == 0) {
		config->axis_scroll_factor = atof(value);
	} else if (strcmp(key, "gappih") == 0) {
		config->gappih = atoi(value);
	} else if (strcmp(key, "gappiv") == 0) {
		config->gappiv = atoi(value);
	} else if (strcmp(key, "gappoh") == 0) {
		config->gappoh = atoi(value);
	} else if (strcmp(key, "gappov") == 0) {
		config->gappov = atoi(value);
	} else if (strcmp(key, "scratchpad_width_ratio") == 0) {
		config->scratchpad_width_ratio = atof(value);
	} else if (strcmp(key, "scratchpad_height_ratio") == 0) {
		config->scratchpad_height_ratio = atof(value);
	} else if (strcmp(key, "borderpx") == 0) {
		config->borderpx = atoi(value);
	} else if (strcmp(key, "rootcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid rootcolor "
					"format: "
					"%s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->rootcolor, color);
		}

	} else if (strcmp(key, "shadowscolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid shadowscolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->shadowscolor, color);
		}
	} else if (strcmp(key, "bordercolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid bordercolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->bordercolor, color);
		}
	} else if (strcmp(key, "focuscolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid focuscolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->focuscolor, color);
		}
	} else if (strcmp(key, "maximizescreencolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid "
					"maximizescreencolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->maximizescreencolor, color);
		}
	} else if (strcmp(key, "urgentcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid urgentcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->urgentcolor, color);
		}
	} else if (strcmp(key, "scratchpadcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid "
					"scratchpadcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->scratchpadcolor, color);
		}
	} else if (strcmp(key, "globalcolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid globalcolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->globalcolor, color);
		}
	} else if (strcmp(key, "overlaycolor") == 0) {
		int64_t color = parse_color(value);
		if (color == -1) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid overlaycolor "
					"format: %s\n",
					value);
			return false;
		} else {
			convert_hex_to_rgba(config->overlaycolor, color);
		}
	} else if (strcmp(key, "monitorrule") == 0) {
		config->monitor_rules =
			realloc(config->monitor_rules, (config->monitor_rules_count + 1) *
											   sizeof(ConfigMonitorRule));
		if (!config->monitor_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for monitor rules\n");
			return false;
		}

		ConfigMonitorRule *rule =
			&config->monitor_rules[config->monitor_rules_count];
		memset(rule, 0, sizeof(ConfigMonitorRule));

		// 设置默认值
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
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"monitor rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		if (!rule->name && !rule->make && !rule->model && !rule->serial) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Monitor rule "
							"must have at least one of the following "
							"options: name, make, model, serial\n");
			return false;
		}

		config->monitor_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "tagrule") == 0) {
		config->tag_rules =
			realloc(config->tag_rules,
					(config->tag_rules_count + 1) * sizeof(ConfigTagRule));
		if (!config->tag_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for tag rules\n");
			return false;
		}

		ConfigTagRule *rule = &config->tag_rules[config->tag_rules_count];
		memset(rule, 0, sizeof(ConfigTagRule));

		// 设置默认值
		rule->id = 0;
		rule->layout_name = NULL;
		rule->monitor_name = NULL;
		rule->monitor_make = NULL;
		rule->monitor_model = NULL;
		rule->monitor_serial = NULL;
		rule->nmaster = 0;
		rule->mfact = 0.0f;
		rule->no_render_border = 0;
		rule->no_hide = 0;

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
					rule->id = CLAMP_INT(atoi(val), 0, LENGTH(tags));
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
				} else if (strcmp(key, "no_hide") == 0) {
					rule->no_hide = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "nmaster") == 0) {
					rule->nmaster = CLAMP_INT(atoi(val), 1, 99);
				} else if (strcmp(key, "mfact") == 0) {
					rule->mfact = CLAMP_FLOAT(atof(val), 0.1f, 0.9f);
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"tag rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		config->tag_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "layerrule") == 0) {
		config->layer_rules =
			realloc(config->layer_rules,
					(config->layer_rules_count + 1) * sizeof(ConfigLayerRule));
		if (!config->layer_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for layer rules\n");
			return false;
		}

		ConfigLayerRule *rule = &config->layer_rules[config->layer_rules_count];
		memset(rule, 0, sizeof(ConfigLayerRule));

		// 设置默认值
		rule->layer_name = NULL;
		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;
		rule->noblur = 0;
		rule->noanim = 0;
		rule->noshadow = 0;

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
					rule->animation_type_open = strdup(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close = strdup(val);
				} else if (strcmp(key, "noblur") == 0) {
					rule->noblur = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noanim") == 0) {
					rule->noanim = CLAMP_INT(atoi(val), 0, 1);
				} else if (strcmp(key, "noshadow") == 0) {
					rule->noshadow = CLAMP_INT(atoi(val), 0, 1);
				} else {
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"layer rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}

		// 如果没有指定布局名称，则使用默认值
		if (rule->layer_name == NULL) {
			rule->layer_name = strdup("default");
		}

		config->layer_rules_count++;
		return !parse_error;
	} else if (strcmp(key, "windowrule") == 0) {
		config->window_rules =
			realloc(config->window_rules,
					(config->window_rules_count + 1) * sizeof(ConfigWinRule));
		if (!config->window_rules) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for window rules\n");
			return false;
		}

		ConfigWinRule *rule = &config->window_rules[config->window_rules_count];
		memset(rule, 0, sizeof(ConfigWinRule));

		// int32_t rule value, relay to a client property
		rule->isfloating = -1;
		rule->isfullscreen = -1;
		rule->isfakefullscreen = -1;
		rule->isnoborder = -1;
		rule->isnoshadow = -1;
		rule->isnoradius = -1;
		rule->isnoanimation = -1;
		rule->isopensilent = -1;
		rule->istagsilent = -1;
		rule->isnamedscratchpad = -1;
		rule->isunglobal = -1;
		rule->isglobal = -1;
		rule->isoverlay = -1;
		rule->allow_shortcuts_inhibit = -1;
		rule->ignore_maximize = -1;
		rule->ignore_minimize = -1;
		rule->isnosizehint = -1;
		rule->isterm = -1;
		rule->atstartup = -1;
		rule->allow_csd = -1;
		rule->force_maximize = -1;
		rule->force_tiled_state = -1;
		rule->force_tearing = -1;
		rule->noswallow = -1;
		rule->noblur = -1;
		rule->nofocus = -1;
		rule->nofadein = -1;
		rule->nofadeout = -1;
		rule->no_force_center = -1;

		// string rule value, relay to a client property
		rule->animation_type_open = NULL;
		rule->animation_type_close = NULL;

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

				if (strcmp(key, "isfloating") == 0) {
					rule->isfloating = atoi(val);
				} else if (strcmp(key, "title") == 0) {
					rule->title = strdup(val);
				} else if (strcmp(key, "appid") == 0) {
					rule->id = strdup(val);
				} else if (strcmp(key, "animation_type_open") == 0) {
					rule->animation_type_open = strdup(val);
				} else if (strcmp(key, "animation_type_close") == 0) {
					rule->animation_type_close = strdup(val);
				} else if (strcmp(key, "tags") == 0) {
					rule->tags = 1 << (atoi(val) - 1);
				} else if (strcmp(key, "monitor") == 0) {
					rule->monitor = strdup(val);
				} else if (strcmp(key, "offsetx") == 0) {
					rule->offsetx = atoi(val);
				} else if (strcmp(key, "offsety") == 0) {
					rule->offsety = atoi(val);
				} else if (strcmp(key, "nofocus") == 0) {
					rule->nofocus = atoi(val);
				} else if (strcmp(key, "nofadein") == 0) {
					rule->nofadein = atoi(val);
				} else if (strcmp(key, "nofadeout") == 0) {
					rule->nofadeout = atoi(val);
				} else if (strcmp(key, "no_force_center") == 0) {
					rule->no_force_center = atoi(val);
				} else if (strcmp(key, "width") == 0) {
					rule->width = atoi(val);
				} else if (strcmp(key, "height") == 0) {
					rule->height = atoi(val);
				} else if (strcmp(key, "isnoborder") == 0) {
					rule->isnoborder = atoi(val);
				} else if (strcmp(key, "isnoshadow") == 0) {
					rule->isnoshadow = atoi(val);
				} else if (strcmp(key, "isnoradius") == 0) {
					rule->isnoradius = atoi(val);
				} else if (strcmp(key, "isnoanimation") == 0) {
					rule->isnoanimation = atoi(val);
				} else if (strcmp(key, "isopensilent") == 0) {
					rule->isopensilent = atoi(val);
				} else if (strcmp(key, "istagsilent") == 0) {
					rule->istagsilent = atoi(val);
				} else if (strcmp(key, "isnamedscratchpad") == 0) {
					rule->isnamedscratchpad = atoi(val);
				} else if (strcmp(key, "isunglobal") == 0) {
					rule->isunglobal = atoi(val);
				} else if (strcmp(key, "isglobal") == 0) {
					rule->isglobal = atoi(val);
				} else if (strcmp(key, "scroller_proportion_single") == 0) {
					rule->scroller_proportion_single = atof(val);
				} else if (strcmp(key, "unfocused_opacity") == 0) {
					rule->unfocused_opacity = atof(val);
				} else if (strcmp(key, "focused_opacity") == 0) {
					rule->focused_opacity = atof(val);
				} else if (strcmp(key, "isoverlay") == 0) {
					rule->isoverlay = atoi(val);
				} else if (strcmp(key, "allow_shortcuts_inhibit") == 0) {
					rule->allow_shortcuts_inhibit = atoi(val);
				} else if (strcmp(key, "ignore_maximize") == 0) {
					rule->ignore_maximize = atoi(val);
				} else if (strcmp(key, "ignore_minimize") == 0) {
					rule->ignore_minimize = atoi(val);
				} else if (strcmp(key, "isnosizehint") == 0) {
					rule->isnosizehint = atoi(val);
				} else if (strcmp(key, "isterm") == 0) {
					rule->isterm = atoi(val);
				} else if (strcmp(key, "allow_csd") == 0) {
					rule->allow_csd = atoi(val);
				} else if (strcmp(key, "force_maximize") == 0) {
					rule->force_maximize = atoi(val);
				} else if (strcmp(key, "force_tiled_state") == 0) {
					rule->force_tiled_state = atoi(val);
				} else if (strcmp(key, "force_tearing") == 0) {
					rule->force_tearing = atoi(val);
				} else if (strcmp(key, "noswallow") == 0) {
					rule->noswallow = atoi(val);
				} else if (strcmp(key, "noblur") == 0) {
					rule->noblur = atoi(val);
				} else if (strcmp(key, "scroller_proportion") == 0) {
					rule->scroller_proportion = atof(val);
				} else if (strcmp(key, "isfullscreen") == 0) {
					rule->isfullscreen = atoi(val);
				} else if (strcmp(key, "isfakefullscreen") == 0) {
					rule->isfakefullscreen = atoi(val);
				} else if (strcmp(key, "atstartup") == 0) {
					rule->atstartup = atoi(val);
				} else if (strcmp(key, "globalkeybinding") == 0) {
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
					fprintf(stderr,
							"\033[1m\033[31m[ERROR]:\033[33m Unknown "
							"window rule "
							"option:\033[1m\033[31m %s\n",
							key);
					parse_error = true;
				}
			}
			token = strtok(NULL, ",");
		}
		config->window_rules_count++;
		return !parse_error;
	} else if (strncmp(key, "env", 3) == 0) {

		char env_type[256], env_value[256];
		if (sscanf(value, "%255[^,],%255[^\n]", env_type, env_value) < 2) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid bind format: "
					"\033[1m\033[31m%s\n",
					value);
			return false;
		}
		trim_whitespace(env_type);
		trim_whitespace(env_value);

		ConfigEnv *env = calloc(1, sizeof(ConfigEnv));
		env->type = strdup(env_type);
		env->value = strdup(env_value);

		config->env =
			realloc(config->env, (config->env_count + 1) * sizeof(ConfigEnv));
		if (!config->env) {
			free(env->type);
			free(env->value);
			free(env);
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Failed to "
							"allocate memory for env\n");
			return false;
		}

		config->env[config->env_count] = env;
		config->env_count++;

	} else if (strncmp(key, "exec", 9) == 0) {
		char **new_exec =
			realloc(config->exec, (config->exec_count + 1) * sizeof(char *));
		if (!new_exec) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for exec\n");
			return false;
		}
		config->exec = new_exec;

		config->exec[config->exec_count] = strdup(value);
		if (!config->exec[config->exec_count]) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m Failed to "
							"duplicate exec string\n");
			return false;
		}

		config->exec_count++;

	} else if (strncmp(key, "exec-once", 9) == 0) {

		char **new_exec_once = realloc(
			config->exec_once, (config->exec_once_count + 1) * sizeof(char *));
		if (!new_exec_once) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for exec_once\n");
			return false;
		}
		config->exec_once = new_exec_once;

		config->exec_once[config->exec_once_count] = strdup(value);
		if (!config->exec_once[config->exec_once_count]) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to duplicate "
					"exec_once string\n");
			return false;
		}

		config->exec_once_count++;

	} else if (regex_match("^bind[s|l|r|p]*$", key)) {
		config->key_bindings =
			realloc(config->key_bindings,
					(config->key_bindings_count + 1) * sizeof(KeyBinding));
		if (!config->key_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for key bindings\n");
			return false;
		}

		KeyBinding *binding = &config->key_bindings[config->key_bindings_count];
		memset(binding, 0, sizeof(KeyBinding));

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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid bind format: "
					"\033[1m\033[31m%s\n",
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
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in bind: "
						"\033[1m\033[31m%s\n",
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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for mouse bindings\n");
			return false;
		}

		MouseBinding *binding =
			&config->mouse_bindings[config->mouse_bindings_count];
		memset(binding, 0, sizeof(MouseBinding));

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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid mousebind "
					"format: "
					"%s\n",
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

		// TODO: remove this in next version
		if (binding->mod == 0 &&
			(binding->button == BTN_LEFT || binding->button == BTN_RIGHT)) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m \033[31m%s\033[33m can't "
					"bind to \033[31m%s\033[33m mod key\n",
					button_str, mod_str);
			return false;
		}

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
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in "
						"mousebind: \033[1m\033[31m%s\n",
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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for axis bindings\n");
			return false;
		}

		AxisBinding *binding =
			&config->axis_bindings[config->axis_bindings_count];
		memset(binding, 0, sizeof(AxisBinding));

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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid axisbind "
					"format: %s\n",
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
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in "
						"axisbind: \033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->axis_bindings_count++;
		}

	} else if (strncmp(key, "switchbind", 10) == 0) {
		config->switch_bindings = realloc(config->switch_bindings,
										  (config->switch_bindings_count + 1) *
											  sizeof(SwitchBinding));
		if (!config->switch_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for switch bindings\n");
			return false;
		}

		SwitchBinding *binding =
			&config->switch_bindings[config->switch_bindings_count];
		memset(binding, 0, sizeof(SwitchBinding));

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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid switchbind "
					"format: %s\n",
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

			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Unknown dispatch in "
					"switchbind: "
					"\033[1m\033[31m%s\n",
					func_name);
			return false;
		} else {
			config->switch_bindings_count++;
		}

	} else if (strncmp(key, "gesturebind", 11) == 0) {
		config->gesture_bindings = realloc(
			config->gesture_bindings,
			(config->gesture_bindings_count + 1) * sizeof(GestureBinding));
		if (!config->gesture_bindings) {
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Failed to allocate "
					"memory for axis gesturebind\n");
			return false;
		}

		GestureBinding *binding =
			&config->gesture_bindings[config->gesture_bindings_count];
		memset(binding, 0, sizeof(GestureBinding));

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
			fprintf(stderr,
					"\033[1m\033[31m[ERROR]:\033[33m Invalid gesturebind "
					"format: %s\n",
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
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m Unknown "
						"dispatch in "
						"axisbind: \033[1m\033[31m%s\n",
						func_name);
			return false;
		} else {
			config->gesture_bindings_count++;
		}

	} else if (strncmp(key, "source-optional", 15) == 0) {
		parse_config_file(config, value, false);
	} else if (strncmp(key, "source", 6) == 0) {
		parse_config_file(config, value, true);
	} else {
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Unknown keyword: "
				"\033[1m\033[31m%s\n",
				key);
		return false;
	}

	return true;
}

bool parse_config_line(Config *config, const char *line) {
	char key[256], value[256];
	if (sscanf(line, "%255[^=]=%255[^\n]", key, value) != 2) {
		fprintf(stderr,
				"\033[1m\033[31m[ERROR]:\033[33m Invalid line format: %s",
				line);
		return false;
	}

	// Then trim each part separately
	trim_whitespace(key);
	trim_whitespace(value);

	return parse_option(config, key, value);
}

bool parse_config_file(Config *config, const char *file_path, bool must_exist) {
	FILE *file;
	char full_path[1024];

	if (file_path[0] == '.' && file_path[1] == '/') {
		// Relative path

		if (cli_config_path) {
			char *config_path = strdup(cli_config_path);
			char *config_dir = dirname(config_path);
			snprintf(full_path, sizeof(full_path), "%s/%s", config_dir,
					 file_path + 1);
			free(config_path);
		} else {
			const char *home = getenv("HOME");
			if (!home) {
				fprintf(stderr,
						"\033[1m\033[31m[ERROR]:\033[33m HOME environment "
						"variable not set.\n");
				return false;
			}
			snprintf(full_path, sizeof(full_path), "%s/.config/mango/%s", home,
					 file_path + 1);
		}
		file = fopen(full_path, "r");

	} else if (file_path[0] == '~' &&
			   (file_path[1] == '/' || file_path[1] == '\0')) {
		// Home directory

		const char *home = getenv("HOME");
		if (!home) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m HOME environment "
							"variable not set.\n");
			return false;
		}
		snprintf(full_path, sizeof(full_path), "%s%s", home, file_path + 1);
		file = fopen(full_path, "r");

	} else {
		// Absolute path
		file = fopen(file_path, "r");
	}

	if (!file) {
		if (must_exist) {
			fprintf(stderr,
					"\033[1;31m\033[1;33m[ERROR]:\033[0m Failed to open "
					"config file: %s\n",
					file_path);
			return false;
		} else {
			return true;
		}
	}

	char line[512];
	bool parse_correct = true;
	bool parse_line_correct = true;
	uint32_t line_count = 0;
	while (fgets(line, sizeof(line), file)) {
		line_count++;
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}
		parse_line_correct = parse_config_line(config, line);
		if (!parse_line_correct) {
			parse_correct = false;
			fprintf(stderr,
					"\033[1;31m╰─\033[1;33m[Index]\033[0m "
					"\033[1;36m%s\033[0m:\033[1;35m%d\033[0m\n"
					"   \033[1;36m╰─\033[0;33m%s\033[0m\n\n",
					file_path, line_count, line);
		}
	}

	fclose(file);
	return parse_correct;
}

void free_circle_layout(Config *config) {
	if (config->circle_layout) {
		// 释放每个字符串
		for (int32_t i = 0; i < config->circle_layout_count; i++) {
			if (config->circle_layout[i]) {
				free(config->circle_layout[i]);	 // 释放单个字符串
				config->circle_layout[i] = NULL; // 防止野指针
			}
		}
		// 释放 circle_layout 数组本身
		free(config->circle_layout);
		config->circle_layout = NULL; // 防止野指针
	}
	config->circle_layout_count = 0; // 重置计数
}

void free_baked_points(void) {
	if (baked_points_move) {
		free(baked_points_move);
		baked_points_move = NULL;
	}
	if (baked_points_open) {
		free(baked_points_open);
		baked_points_open = NULL;
	}
	if (baked_points_close) {
		free(baked_points_close);
		baked_points_close = NULL;
	}
	if (baked_points_tag) {
		free(baked_points_tag);
		baked_points_tag = NULL;
	}
	if (baked_points_focus) {
		free(baked_points_focus);
		baked_points_focus = NULL;
	}
	if (baked_points_opafadein) {
		free(baked_points_opafadein);
		baked_points_opafadein = NULL;
	}
	if (baked_points_opafadeout) {
		free(baked_points_opafadeout);
		baked_points_opafadeout = NULL;
	}
}

void free_config(void) {
	// 释放内存
	int32_t i;

	// 释放 window_rules
	if (config.window_rules) {
		for (int32_t i = 0; i < config.window_rules_count; i++) {
			ConfigWinRule *rule = &config.window_rules[i];
			if (rule->id)
				free((void *)rule->id);
			if (rule->title)
				free((void *)rule->title);
			if (rule->animation_type_open)
				free((void *)rule->animation_type_open);
			if (rule->animation_type_close)
				free((void *)rule->animation_type_close);
			if (rule->monitor)
				free((void *)rule->monitor);
			rule->id = NULL;
			rule->title = NULL;
			rule->animation_type_open = NULL;
			rule->animation_type_close = NULL;
			rule->monitor = NULL;
			// 释放 globalkeybinding 的 arg.v（如果动态分配）
			if (rule->globalkeybinding.arg.v) {
				free((void *)rule->globalkeybinding.arg.v);
			}
		}
		free(config.window_rules);
		config.window_rules = NULL;
		config.window_rules_count = 0;
	}

	// 释放 key_bindings
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

	// 释放 mouse_bindings
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

	// 释放 axis_bindings
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

	// 释放 switch_bindings
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

	// 释放 gesture_bindings
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

	// 释放 tag_rules
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

	// 释放 monitor_rules
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
		}
		free(config.monitor_rules);
		config.monitor_rules = NULL;
		config.monitor_rules_count = 0;
	}

	// 释放 layer_rules
	if (config.layer_rules) {
		for (int32_t i = 0; i < config.layer_rules_count; i++) {
			if (config.layer_rules[i].layer_name)
				free((void *)config.layer_rules[i].layer_name);
			if (config.layer_rules[i].animation_type_open)
				free((void *)config.layer_rules[i].animation_type_open);
			if (config.layer_rules[i].animation_type_close)
				free((void *)config.layer_rules[i].animation_type_close);
		}
		free(config.layer_rules);
		config.layer_rules = NULL;
		config.layer_rules_count = 0;
	}

	// 释放 env
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

	// 释放 exec
	if (config.exec) {
		for (i = 0; i < config.exec_count; i++) {
			free(config.exec[i]);
		}
		free(config.exec);
		config.exec = NULL;
		config.exec_count = 0;
	}

	// 释放 exec_once
	if (config.exec_once) {
		for (i = 0; i < config.exec_once_count; i++) {
			free(config.exec_once[i]);
		}
		free(config.exec_once);
		config.exec_once = NULL;
		config.exec_once_count = 0;
	}

	// 释放 scroller_proportion_preset
	if (config.scroller_proportion_preset) {
		free(config.scroller_proportion_preset);
		config.scroller_proportion_preset = NULL;
		config.scroller_proportion_preset_count = 0;
	}

	if (config.cursor_theme) {
		free(config.cursor_theme);
		config.cursor_theme = NULL;
	}

	// 释放 circle_layout
	free_circle_layout(&config);

	// 释放动画资源
	free_baked_points();

	// 清理解析按键用的keymap
	cleanup_config_keymap();
}

void override_config(void) {
	// 动画启用
	animations = CLAMP_INT(config.animations, 0, 1);
	layer_animations = CLAMP_INT(config.layer_animations, 0, 1);

	// 标签动画方向
	tag_animation_direction = CLAMP_INT(config.tag_animation_direction, 0, 1);

	// 动画淡入淡出设置
	animation_fade_in = CLAMP_INT(config.animation_fade_in, 0, 1);
	animation_fade_out = CLAMP_INT(config.animation_fade_out, 0, 1);
	zoom_initial_ratio = CLAMP_FLOAT(config.zoom_initial_ratio, 0.1f, 1.0f);
	zoom_end_ratio = CLAMP_FLOAT(config.zoom_end_ratio, 0.1f, 1.0f);
	fadein_begin_opacity = CLAMP_FLOAT(config.fadein_begin_opacity, 0.0f, 1.0f);
	fadeout_begin_opacity =
		CLAMP_FLOAT(config.fadeout_begin_opacity, 0.0f, 1.0f);

	// 打开关闭动画类型
	animation_type_open = config.animation_type_open;
	animation_type_close = config.animation_type_close;

	// layer打开关闭动画类型
	layer_animation_type_open = config.layer_animation_type_open;
	layer_animation_type_close = config.layer_animation_type_close;

	// 动画时间限制在合理范围(1-50000ms)
	animation_duration_move =
		CLAMP_INT(config.animation_duration_move, 1, 50000);
	animation_duration_open =
		CLAMP_INT(config.animation_duration_open, 1, 50000);
	animation_duration_tag = CLAMP_INT(config.animation_duration_tag, 1, 50000);
	animation_duration_close =
		CLAMP_INT(config.animation_duration_close, 1, 50000);
	animation_duration_focus =
		CLAMP_INT(config.animation_duration_focus, 1, 50000);

	// 滚动布局设置
	scroller_default_proportion =
		CLAMP_FLOAT(config.scroller_default_proportion, 0.1f, 1.0f);
	scroller_default_proportion_single =
		CLAMP_FLOAT(config.scroller_default_proportion_single, 0.1f, 1.0f);
	scroller_ignore_proportion_single =
		CLAMP_INT(config.scroller_ignore_proportion_single, 0, 1);
	scroller_focus_center = CLAMP_INT(config.scroller_focus_center, 0, 1);
	scroller_prefer_center = CLAMP_INT(config.scroller_prefer_center, 0, 1);
	scroller_prefer_overspread =
		CLAMP_INT(config.scroller_prefer_overspread, 0, 1);
	edge_scroller_pointer_focus =
		CLAMP_INT(config.edge_scroller_pointer_focus, 0, 1);
	scroller_structs = CLAMP_INT(config.scroller_structs, 0, 1000);

	// 主从布局设置
	default_mfact = CLAMP_FLOAT(config.default_mfact, 0.1f, 0.9f);
	default_nmaster = CLAMP_INT(config.default_nmaster, 1, 1000);
	center_master_overspread = CLAMP_INT(config.center_master_overspread, 0, 1);
	center_when_single_stack = CLAMP_INT(config.center_when_single_stack, 0, 1);
	new_is_master = CLAMP_INT(config.new_is_master, 0, 1);

	// 概述模式设置
	hotarea_size = CLAMP_INT(config.hotarea_size, 1, 1000);
	hotarea_corner = CLAMP_INT(config.hotarea_corner, 0, 3);
	enable_hotarea = CLAMP_INT(config.enable_hotarea, 0, 1);
	ov_tab_mode = CLAMP_INT(config.ov_tab_mode, 0, 1);
	overviewgappi = CLAMP_INT(config.overviewgappi, 0, 1000);
	overviewgappo = CLAMP_INT(config.overviewgappo, 0, 1000);

	// 杂项设置
	xwayland_persistence = CLAMP_INT(config.xwayland_persistence, 0, 1);
	syncobj_enable = CLAMP_INT(config.syncobj_enable, 0, 1);
	allow_tearing = CLAMP_INT(config.allow_tearing, 0, 2);
	allow_shortcuts_inhibit = CLAMP_INT(config.allow_shortcuts_inhibit, 0, 1);
	allow_lock_transparent = CLAMP_INT(config.allow_lock_transparent, 0, 1);
	axis_bind_apply_timeout =
		CLAMP_INT(config.axis_bind_apply_timeout, 0, 1000);
	focus_on_activate = CLAMP_INT(config.focus_on_activate, 0, 1);
	idleinhibit_ignore_visible =
		CLAMP_INT(config.idleinhibit_ignore_visible, 0, 1);
	sloppyfocus = CLAMP_INT(config.sloppyfocus, 0, 1);
	warpcursor = CLAMP_INT(config.warpcursor, 0, 1);
	drag_corner = CLAMP_INT(config.drag_corner, 0, 4);
	drag_warp_cursor = CLAMP_INT(config.drag_warp_cursor, 0, 1);
	focus_cross_monitor = CLAMP_INT(config.focus_cross_monitor, 0, 1);
	exchange_cross_monitor = CLAMP_INT(config.exchange_cross_monitor, 0, 1);
	scratchpad_cross_monitor = CLAMP_INT(config.scratchpad_cross_monitor, 0, 1);
	focus_cross_tag = CLAMP_INT(config.focus_cross_tag, 0, 1);
	view_current_to_back = CLAMP_INT(config.view_current_to_back, 0, 1);
	enable_floating_snap = CLAMP_INT(config.enable_floating_snap, 0, 1);
	snap_distance = CLAMP_INT(config.snap_distance, 0, 99999);
	cursor_size = CLAMP_INT(config.cursor_size, 4, 512);
	no_border_when_single = CLAMP_INT(config.no_border_when_single, 0, 1);
	no_radius_when_single = CLAMP_INT(config.no_radius_when_single, 0, 1);
	cursor_hide_timeout =
		CLAMP_INT(config.cursor_hide_timeout, 0, 36000); // 0-10小时
	drag_tile_to_tile = CLAMP_INT(config.drag_tile_to_tile, 0, 1);
	single_scratchpad = CLAMP_INT(config.single_scratchpad, 0, 1);

	// 键盘设置
	repeat_rate = CLAMP_INT(config.repeat_rate, 1, 1000);
	repeat_delay = CLAMP_INT(config.repeat_delay, 1, 20000);
	numlockon = CLAMP_INT(config.numlockon, 0, 1);

	// 触控板设置
	disable_trackpad = CLAMP_INT(config.disable_trackpad, 0, 1);
	tap_to_click = CLAMP_INT(config.tap_to_click, 0, 1);
	tap_and_drag = CLAMP_INT(config.tap_and_drag, 0, 1);
	drag_lock = CLAMP_INT(config.drag_lock, 0, 1);
	trackpad_natural_scrolling =
		CLAMP_INT(config.trackpad_natural_scrolling, 0, 1);
	disable_while_typing = CLAMP_INT(config.disable_while_typing, 0, 1);
	left_handed = CLAMP_INT(config.left_handed, 0, 1);
	middle_button_emulation = CLAMP_INT(config.middle_button_emulation, 0, 1);
	swipe_min_threshold = CLAMP_INT(config.swipe_min_threshold, 1, 1000);

	// 鼠标设置
	mouse_natural_scrolling = CLAMP_INT(config.mouse_natural_scrolling, 0, 1);
	accel_profile = CLAMP_INT(config.accel_profile, 0, 2);
	accel_speed = CLAMP_FLOAT(config.accel_speed, -1.0f, 1.0f);
	scroll_method = CLAMP_INT(config.scroll_method, 0, 4);
	scroll_button = CLAMP_INT(config.scroll_button, 272, 276);
	click_method = CLAMP_INT(config.click_method, 0, 2);
	send_events_mode = CLAMP_INT(config.send_events_mode, 0, 2);
	button_map = CLAMP_INT(config.button_map, 0, 1);
	axis_scroll_factor = CLAMP_FLOAT(config.axis_scroll_factor, 0.1f, 10.0f);

	// 外观设置
	gappih = CLAMP_INT(config.gappih, 0, 1000);
	gappiv = CLAMP_INT(config.gappiv, 0, 1000);
	gappoh = CLAMP_INT(config.gappoh, 0, 1000);
	gappov = CLAMP_INT(config.gappov, 0, 1000);
	scratchpad_width_ratio =
		CLAMP_FLOAT(config.scratchpad_width_ratio, 0.1f, 1.0f);
	scratchpad_height_ratio =
		CLAMP_FLOAT(config.scratchpad_height_ratio, 0.1f, 1.0f);
	borderpx = CLAMP_INT(config.borderpx, 0, 200);
	smartgaps = CLAMP_INT(config.smartgaps, 0, 1);

	blur = CLAMP_INT(config.blur, 0, 1);
	blur_layer = CLAMP_INT(config.blur_layer, 0, 1);
	blur_optimized = CLAMP_INT(config.blur_optimized, 0, 1);
	border_radius = CLAMP_INT(config.border_radius, 0, 100);
	blur_params.num_passes = CLAMP_INT(config.blur_params.num_passes, 0, 10);
	blur_params.radius = CLAMP_INT(config.blur_params.radius, 0, 100);
	blur_params.noise = CLAMP_FLOAT(config.blur_params.noise, 0, 1);
	blur_params.brightness = CLAMP_FLOAT(config.blur_params.brightness, 0, 1);
	blur_params.contrast = CLAMP_FLOAT(config.blur_params.contrast, 0, 1);
	blur_params.saturation = CLAMP_FLOAT(config.blur_params.saturation, 0, 1);
	shadows = CLAMP_INT(config.shadows, 0, 1);
	shadow_only_floating = CLAMP_INT(config.shadow_only_floating, 0, 1);
	layer_shadows = CLAMP_INT(config.layer_shadows, 0, 1);
	shadows_size = CLAMP_INT(config.shadows_size, 0, 100);
	shadows_blur = CLAMP_INT(config.shadows_blur, 0, 100);
	shadows_position_x = CLAMP_INT(config.shadows_position_x, -1000, 1000);
	shadows_position_y = CLAMP_INT(config.shadows_position_y, -1000, 1000);
	focused_opacity = CLAMP_FLOAT(config.focused_opacity, 0.0f, 1.0f);
	unfocused_opacity = CLAMP_FLOAT(config.unfocused_opacity, 0.0f, 1.0f);
	memcpy(shadowscolor, config.shadowscolor, sizeof(shadowscolor));

	// 复制颜色数组
	memcpy(rootcolor, config.rootcolor, sizeof(rootcolor));
	memcpy(bordercolor, config.bordercolor, sizeof(bordercolor));
	memcpy(focuscolor, config.focuscolor, sizeof(focuscolor));
	memcpy(maximizescreencolor, config.maximizescreencolor,
		   sizeof(maximizescreencolor));
	memcpy(urgentcolor, config.urgentcolor, sizeof(urgentcolor));
	memcpy(scratchpadcolor, config.scratchpadcolor, sizeof(scratchpadcolor));
	memcpy(globalcolor, config.globalcolor, sizeof(globalcolor));
	memcpy(overlaycolor, config.overlaycolor, sizeof(overlaycolor));

	// 复制动画曲线
	memcpy(animation_curve_move, config.animation_curve_move,
		   sizeof(animation_curve_move));
	memcpy(animation_curve_open, config.animation_curve_open,
		   sizeof(animation_curve_open));
	memcpy(animation_curve_tag, config.animation_curve_tag,
		   sizeof(animation_curve_tag));
	memcpy(animation_curve_close, config.animation_curve_close,
		   sizeof(animation_curve_close));
	memcpy(animation_curve_focus, config.animation_curve_focus,
		   sizeof(animation_curve_focus));
	memcpy(animation_curve_opafadein, config.animation_curve_opafadein,
		   sizeof(animation_curve_opafadein));
	memcpy(animation_curve_opafadeout, config.animation_curve_opafadeout,
		   sizeof(animation_curve_opafadeout));
}

void set_value_default() {
	/* animaion */
	config.animations = animations;					// 是否启用动画
	config.layer_animations = layer_animations;		// 是否启用layer动画
	config.animation_fade_in = animation_fade_in;	// Enable animation fade in
	config.animation_fade_out = animation_fade_out; // Enable animation fade out
	config.tag_animation_direction = tag_animation_direction; // 标签动画方向
	config.zoom_initial_ratio = zoom_initial_ratio; // 动画起始窗口比例
	config.zoom_end_ratio = zoom_end_ratio;			// 动画结束窗口比例
	config.fadein_begin_opacity =
		fadein_begin_opacity; // Begin opac window ratio for animations
	config.fadeout_begin_opacity = fadeout_begin_opacity;
	config.animation_duration_move =
		animation_duration_move; // Animation move speed
	config.animation_duration_open =
		animation_duration_open; // Animation open speed
	config.animation_duration_tag =
		animation_duration_tag; // Animation tag speed
	config.animation_duration_close =
		animation_duration_close; // Animation tag speed
	config.animation_duration_focus =
		animation_duration_focus; // Animation focus opacity speed

	/* appearance */
	config.axis_bind_apply_timeout =
		axis_bind_apply_timeout; // 滚轮绑定动作的触发的时间间隔
	config.focus_on_activate =
		focus_on_activate;				  // 收到窗口激活请求是否自动跳转聚焦
	config.new_is_master = new_is_master; // 新窗口是否插在头部
	config.default_mfact = default_mfact; // master 窗口比例
	config.default_nmaster = default_nmaster; // 默认master数量
	config.center_master_overspread =
		center_master_overspread; // 中心master时是否铺满
	config.center_when_single_stack =
		center_when_single_stack; // 单个stack时是否居中

	config.numlockon = numlockon; // 是否打开右边小键盘

	config.ov_tab_mode = ov_tab_mode;	// alt tab切换模式
	config.hotarea_size = hotarea_size; // 热区大小,10x10
	config.hotarea_corner = hotarea_corner;
	config.enable_hotarea = enable_hotarea; // 是否启用鼠标热区
	config.smartgaps = smartgaps;	  /* 1 means no outer gap when there is
										 only one window */
	config.sloppyfocus = sloppyfocus; /* focus follows mouse */
	config.gappih = gappih;			  /* horiz inner gap between windows */
	config.gappiv = gappiv;			  /* vert inner gap between windows */
	config.gappoh =
		gappoh; /* horiz outer gap between windows and screen edge */
	config.gappov = gappov; /* vert outer gap between windows and screen edge */
	config.scratchpad_width_ratio = scratchpad_width_ratio;
	config.scratchpad_height_ratio = scratchpad_height_ratio;

	config.scroller_structs = scroller_structs;
	config.scroller_default_proportion = scroller_default_proportion;
	config.scroller_default_proportion_single =
		scroller_default_proportion_single;
	config.scroller_ignore_proportion_single =
		scroller_ignore_proportion_single;
	config.scroller_focus_center = scroller_focus_center;
	config.scroller_prefer_center = scroller_prefer_center;
	config.scroller_prefer_overspread = scroller_prefer_overspread;
	config.edge_scroller_pointer_focus = edge_scroller_pointer_focus;
	config.focus_cross_monitor = focus_cross_monitor;
	config.exchange_cross_monitor = exchange_cross_monitor;
	config.scratchpad_cross_monitor = scratchpad_cross_monitor;
	config.focus_cross_tag = focus_cross_tag;
	config.axis_scroll_factor = axis_scroll_factor;
	config.view_current_to_back = view_current_to_back;
	config.single_scratchpad = single_scratchpad;
	config.xwayland_persistence = xwayland_persistence;
	config.syncobj_enable = syncobj_enable;
	config.allow_tearing = allow_tearing;
	config.allow_shortcuts_inhibit = allow_shortcuts_inhibit;
	config.allow_lock_transparent = allow_lock_transparent;
	config.no_border_when_single = no_border_when_single;
	config.no_radius_when_single = no_radius_when_single;
	config.snap_distance = snap_distance;
	config.drag_tile_to_tile = drag_tile_to_tile;
	config.enable_floating_snap = enable_floating_snap;
	config.swipe_min_threshold = swipe_min_threshold;

	config.idleinhibit_ignore_visible =
		idleinhibit_ignore_visible; /* 1 means idle inhibitors will
									  disable idle tracking even if it's
									  surface isn't visible
									*/

	config.borderpx = borderpx;
	config.overviewgappi = overviewgappi; /* overview时 窗口与边缘 缝隙大小 */
	config.overviewgappo = overviewgappo; /* overview时 窗口与窗口 缝隙大小 */
	config.cursor_hide_timeout = cursor_hide_timeout;

	config.warpcursor = warpcursor; /* Warp cursor to focused client */
	config.drag_corner = drag_corner;
	config.drag_warp_cursor = drag_warp_cursor;

	config.repeat_rate = repeat_rate;
	config.repeat_delay = repeat_delay;

	/* Trackpad */
	config.disable_trackpad = disable_trackpad;
	config.tap_to_click = tap_to_click;
	config.tap_and_drag = tap_and_drag;
	config.drag_lock = drag_lock;
	config.mouse_natural_scrolling = mouse_natural_scrolling;
	config.cursor_size = cursor_size;
	config.trackpad_natural_scrolling = trackpad_natural_scrolling;
	config.disable_while_typing = disable_while_typing;
	config.left_handed = left_handed;
	config.middle_button_emulation = middle_button_emulation;
	config.accel_profile = accel_profile;
	config.accel_speed = accel_speed;
	config.scroll_method = scroll_method;
	config.scroll_button = scroll_button;
	config.click_method = click_method;
	config.send_events_mode = send_events_mode;
	config.button_map = button_map;

	config.blur = blur;
	config.blur_layer = blur_layer;
	config.blur_optimized = blur_optimized;
	config.border_radius = border_radius;
	config.blur_params.num_passes = blur_params_num_passes;
	config.blur_params.radius = blur_params_radius;
	config.blur_params.noise = blur_params_noise;
	config.blur_params.brightness = blur_params_brightness;
	config.blur_params.contrast = blur_params_contrast;
	config.blur_params.saturation = blur_params_saturation;
	config.shadows = shadows;
	config.shadow_only_floating = shadow_only_floating;
	config.layer_shadows = layer_shadows;
	config.shadows_size = shadows_size;
	config.shadows_blur = shadows_blur;
	config.shadows_position_x = shadows_position_x;
	config.shadows_position_y = shadows_position_y;
	config.focused_opacity = focused_opacity;
	config.unfocused_opacity = unfocused_opacity;
	memcpy(config.shadowscolor, shadowscolor, sizeof(shadowscolor));

	memcpy(config.animation_curve_move, animation_curve_move,
		   sizeof(animation_curve_move));
	memcpy(config.animation_curve_open, animation_curve_open,
		   sizeof(animation_curve_open));
	memcpy(config.animation_curve_tag, animation_curve_tag,
		   sizeof(animation_curve_tag));
	memcpy(config.animation_curve_close, animation_curve_close,
		   sizeof(animation_curve_close));
	memcpy(config.animation_curve_focus, animation_curve_focus,
		   sizeof(animation_curve_focus));
	memcpy(config.animation_curve_opafadein, animation_curve_opafadein,
		   sizeof(animation_curve_opafadein));
	memcpy(config.animation_curve_opafadeout, animation_curve_opafadeout,
		   sizeof(animation_curve_opafadeout));

	memcpy(config.rootcolor, rootcolor, sizeof(rootcolor));
	memcpy(config.bordercolor, bordercolor, sizeof(bordercolor));
	memcpy(config.focuscolor, focuscolor, sizeof(focuscolor));
	memcpy(config.maximizescreencolor, maximizescreencolor,
		   sizeof(maximizescreencolor));
	memcpy(config.urgentcolor, urgentcolor, sizeof(urgentcolor));
	memcpy(config.scratchpadcolor, scratchpadcolor, sizeof(scratchpadcolor));
	memcpy(config.globalcolor, globalcolor, sizeof(globalcolor));
	memcpy(config.overlaycolor, overlaycolor, sizeof(overlaycolor));
}

void set_default_key_bindings(Config *config) {
	// 计算默认按键绑定的数量
	size_t default_key_bindings_count =
		sizeof(default_key_bindings) / sizeof(KeyBinding);

	// 重新分配内存以容纳新的默认按键绑定
	config->key_bindings =
		realloc(config->key_bindings,
				(config->key_bindings_count + default_key_bindings_count) *
					sizeof(KeyBinding));
	if (!config->key_bindings) {
		return;
	}

	// 将默认按键绑定复制到配置的按键绑定数组中
	for (size_t i = 0; i < default_key_bindings_count; i++) {
		config->key_bindings[config->key_bindings_count + i] =
			default_key_bindings[i];
		config->key_bindings[config->key_bindings_count + i].iscommonmode =
			true;
		config->key_bindings[config->key_bindings_count + i].islockapply = true;
	}

	// 更新按键绑定的总数
	config->key_bindings_count += default_key_bindings_count;
}

bool parse_config(void) {

	char filename[1024];

	free_config();

	// 重置config结构体，确保所有指针初始化为NULL
	memset(&config, 0, sizeof(config));
	memset(&xkb_rules_rules, 0, sizeof(xkb_rules_rules));
	memset(&xkb_rules_model, 0, sizeof(xkb_rules_model));
	memset(&xkb_rules_layout, 0, sizeof(xkb_rules_layout));
	memset(&xkb_rules_variant, 0, sizeof(xkb_rules_variant));
	memset(&xkb_rules_options, 0, sizeof(xkb_rules_options));

	// 初始化动态数组的指针为NULL，避免野指针
	config.window_rules = NULL;
	config.window_rules_count = 0;
	config.monitor_rules = NULL;
	config.monitor_rules_count = 0;
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
	strcpy(config.keymode, "default");

	create_config_keymap();

	if (cli_config_path) {
		snprintf(filename, sizeof(filename), "%s", cli_config_path);
	} else {
		// 获取当前用户家目录
		const char *homedir = getenv("HOME");
		if (!homedir) {
			// 如果获取失败，则无法继续
			return false;
		}
		// 构建日志文件路径
		snprintf(filename, sizeof(filename), "%s/.config/mango/config.conf",
				 homedir);

		// 检查文件是否存在
		if (access(filename, F_OK) != 0) {
			// 如果文件不存在，则使用 /etc/mango/config.conf
			snprintf(filename, sizeof(filename), "%s/mango/config.conf",
					 SYSCONFDIR);
		}
	}

	bool parse_correct = true;
	set_value_default();
	parse_correct = parse_config_file(&config, filename, true);
	set_default_key_bindings(&config);
	override_config();
	return parse_correct;
}

void reset_blur_params(void) {
	if (blur) {
		Monitor *m = NULL;
		wl_list_for_each(m, &mons, link) {
			if (m->blur != NULL) {
				wlr_scene_node_destroy(&m->blur->node);
			}
			m->blur = wlr_scene_optimized_blur_create(&scene->tree, 0, 0);
			wlr_scene_node_reparent(&m->blur->node, layers[LyrBlur]);
			wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
			wlr_scene_set_blur_data(
				scene, blur_params.num_passes, blur_params.radius,
				blur_params.noise, blur_params.brightness, blur_params.contrast,
				blur_params.saturation);
		}
	} else {
		Monitor *m = NULL;
		wl_list_for_each(m, &mons, link) {

			if (m->blur) {
				wlr_scene_node_destroy(&m->blur->node);
				m->blur = NULL;
			}
		}
	}
}

void reapply_monitor_rules(void) {
	ConfigMonitorRule *mr;
	Monitor *m = NULL;
	int32_t ji, vrr;
	int32_t mx, my;
	struct wlr_output_state state;
	struct wlr_output_mode *internal_mode = NULL;
	wlr_output_state_init(&state);
	bool match_rule = false;

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}

		for (ji = 0; ji < config.monitor_rules_count; ji++) {
			if (config.monitor_rules_count < 1)
				break;

			mr = &config.monitor_rules[ji];

			// 检查是否匹配的变量
			match_rule = true;

			// 检查四个标识字段的匹配
			if (mr->name != NULL) {
				if (!regex_match(mr->name, m->wlr_output->name)) {
					match_rule = false;
				}
			}

			if (mr->make != NULL) {
				if (m->wlr_output->make == NULL ||
					strcmp(mr->make, m->wlr_output->make) != 0) {
					match_rule = false;
				}
			}

			if (mr->model != NULL) {
				if (m->wlr_output->model == NULL ||
					strcmp(mr->model, m->wlr_output->model) != 0) {
					match_rule = false;
				}
			}

			if (mr->serial != NULL) {
				if (m->wlr_output->serial == NULL ||
					strcmp(mr->serial, m->wlr_output->serial) != 0) {
					match_rule = false;
				}
			}

			// 只有当所有指定的标识都匹配时才应用规则
			if (match_rule) {
				mx = mr->x == INT32_MAX ? m->m.x : mr->x;
				my = mr->y == INT32_MAX ? m->m.y : mr->y;
				vrr = mr->vrr >= 0 ? mr->vrr : 0;

				if (mr->width > 0 && mr->height > 0 && mr->refresh > 0) {
					internal_mode = get_nearest_output_mode(
						m->wlr_output, mr->width, mr->height, mr->refresh);
					if (internal_mode) {
						wlr_output_state_set_mode(&state, internal_mode);
					} else if (wlr_output_is_headless(m->wlr_output)) {
						wlr_output_state_set_custom_mode(
							&state, mr->width, mr->height,
							(int32_t)roundf(mr->refresh * 1000));
					}
				}

				if (vrr) {
					enable_adaptive_sync(m, &state);
				} else {
					wlr_output_state_set_adaptive_sync_enabled(&state, false);
				}

				wlr_output_state_set_scale(&state, mr->scale);
				wlr_output_state_set_transform(&state, mr->rr);
				wlr_output_layout_add(output_layout, m->wlr_output, mx, my);
			}
		}

		wlr_output_commit_state(m->wlr_output, &state);
		wlr_output_state_finish(&state);
		updatemons(NULL, NULL);
	}
}

void reapply_cursor_style(void) {
	if (hide_source) {
		wl_event_source_timer_update(hide_source, 0);
		wl_event_source_remove(hide_source);
		hide_source = NULL;
	}

	wlr_cursor_unset_image(cursor);

	wlr_cursor_set_surface(cursor, NULL, 0, 0);

	if (cursor_mgr) {
		wlr_xcursor_manager_destroy(cursor_mgr);
		cursor_mgr = NULL;
	}

	cursor_mgr = wlr_xcursor_manager_create(config.cursor_theme, cursor_size);

	if (cursor_size > 0) {
		char size_str[16];
		snprintf(size_str, sizeof(size_str), "%d", cursor_size);
		setenv("XCURSOR_SIZE", size_str, 1);
	}

	if (config.cursor_theme) {
		setenv("XCURSOR_THEME", config.cursor_theme, 1);
	}

	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		wlr_xcursor_manager_load(cursor_mgr, m->wlr_output->scale);
	}

	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");

	hide_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
										  hidecursor, cursor);
	if (cursor_hidden) {
		wlr_cursor_unset_image(cursor);
	} else {
		wl_event_source_timer_update(hide_source, cursor_hide_timeout * 1000);
	}
}

void reapply_rootbg(void) { wlr_scene_rect_set_color(root_bg, rootcolor); }

void reapply_border(void) {
	Client *c = NULL;

	// reset border width when config change
	wl_list_for_each(c, &clients, link) {
		if (c && !c->iskilling) {
			if (!c->isnoborder && !c->isfullscreen) {
				c->bw = borderpx;
			}
		}
	}
}

void reapply_keyboard(void) {
	InputDevice *id;
	wl_list_for_each(id, &inputdevices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}
		wlr_keyboard_set_repeat_info((struct wlr_keyboard *)id->device_data,
									 repeat_rate, repeat_delay);
	}
}

void reapply_pointer(void) {
	InputDevice *id;
	struct libinput_device *device;
	wl_list_for_each(id, &inputdevices, link) {

		if (id->wlr_device->type != WLR_INPUT_DEVICE_POINTER) {
			continue;
		}

		device = id->libinput_device;
		if (wlr_input_device_is_libinput(id->wlr_device) && device) {
			configure_pointer(device);
		}
	}
}

void reapply_master(void) {

	int32_t i;
	Monitor *m = NULL;
	for (i = 0; i <= LENGTH(tags); i++) {
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			m->pertag->nmasters[i] = default_nmaster;
			m->pertag->mfacts[i] = default_mfact;
			m->gappih = gappih;
			m->gappiv = gappiv;
			m->gappoh = gappoh;
			m->gappov = gappov;
		}
	}
}

void parse_tagrule(Monitor *m) {
	int32_t i, jk;
	ConfigTagRule tr;
	Client *c = NULL;
	bool match_rule = false;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = default_nmaster;
		m->pertag->mfacts[i] = default_mfact;
	}

	for (i = 0; i < config.tag_rules_count; i++) {

		tr = config.tag_rules[i];

		match_rule = true;

		if (tr.monitor_name != NULL) {
			if (!regex_match(tr.monitor_name, m->wlr_output->name)) {
				match_rule = false;
			}
		}

		if (tr.monitor_make != NULL) {
			if (m->wlr_output->make == NULL ||
				strcmp(tr.monitor_make, m->wlr_output->make) != 0) {
				match_rule = false;
			}
		}

		if (tr.monitor_model != NULL) {
			if (m->wlr_output->model == NULL ||
				strcmp(tr.monitor_model, m->wlr_output->model) != 0) {
				match_rule = false;
			}
		}

		if (tr.monitor_serial != NULL) {
			if (m->wlr_output->serial == NULL ||
				strcmp(tr.monitor_serial, m->wlr_output->serial) != 0) {
				match_rule = false;
			}
		}

		if (config.tag_rules_count > 0 && match_rule) {

			for (jk = 0; jk < LENGTH(layouts); jk++) {
				if (tr.layout_name &&
					strcmp(layouts[jk].name, tr.layout_name) == 0) {
					m->pertag->ltidxs[tr.id] = &layouts[jk];
				}
			}

			if (tr.no_hide >= 0)
				m->pertag->no_hide[tr.id] = tr.no_hide;
			if (tr.nmaster >= 1)
				m->pertag->nmasters[tr.id] = tr.nmaster;
			if (tr.mfact > 0.0f)
				m->pertag->mfacts[tr.id] = tr.mfact;
			if (tr.no_render_border >= 0)
				m->pertag->no_render_border[tr.id] = tr.no_render_border;
		}
	}

	for (i = 1; i <= LENGTH(tags); i++) {
		wl_list_for_each(c, &clients, link) {
			if ((c->tags & (1 << (i - 1)) & TAGMASK) && ISTILED(c)) {
				if (m->pertag->mfacts[i] > 0.0f)
					c->master_mfact_per = m->pertag->mfacts[i];
			}
		}
	}
}

void reapply_tagrule(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		parse_tagrule(m);
	}
}

void reset_option(void) {
	init_baked_points();
	handlecursoractivity();
	reset_keyboard_layout();
	reset_blur_params();
	set_env();
	run_exec();

	reapply_cursor_style();
	reapply_border();
	reapply_rootbg();
	reapply_keyboard();
	reapply_pointer();
	reapply_master();

	reapply_tagrule();
	reapply_monitor_rules();

	arrange(selmon, false, false);
}

int32_t reload_config(const Arg *arg) {
	parse_config();
	reset_option();
	printstatus();
	return 1;
}
