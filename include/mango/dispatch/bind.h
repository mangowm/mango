#ifndef __BIND_H__
#define __BIND_H__ 1

#include "mango/common/types.h"
#include <stdint.h>

/* The generic argument carried by a binding action. */
typedef struct Arg {
	int32_t i;
	int32_t i2;
	float f;
	float f2;
	char *v;
	char *v2;
	char *v3;
	uint32_t ui;
	uint32_t ui2;
	Client *tc;
} Arg;

enum { PREV, NEXT };
enum { MON_PREV = -1, MON_NEXT = -2 };
enum { FORCE, UNFORCE };
enum {
	OVERCIRCLE_PREV = PREV,
	OVERCIRCLE_NEXT = NEXT,
	OVERCIRCLE_CURRENT_PREV,
	OVERCIRCLE_CURRENT_NEXT,
};

int32_t minimize_window(const Arg *arg);
int32_t restore_minimized(const Arg *arg);
int32_t toggle_scratchpad(const Arg *arg);
int32_t focus_direction(const Arg *arg);
int32_t focus_window_or_workspace(const Arg *arg);
int32_t group_join(const Arg *arg);
int32_t group_leave(const Arg *arg);
int32_t toggle_overview(const Arg *arg);
int32_t enter_overview(const Arg *arg);
int32_t leave_overview(const Arg *arg);
int32_t switcher(const Arg *arg);
int32_t toggle_hdr(const Arg *arg);
int32_t toggle_jump(const Arg *arg);
int32_t set_proportion(const Arg *arg);
int32_t switch_proportion_preset(const Arg *arg);
int32_t zoom(const Arg *arg);
int32_t tag_silent(const Arg *arg);
int32_t tag_to_left(const Arg *arg);
int32_t tag_to_right(const Arg *arg);
int32_t tag_cross_monitor(const Arg *arg);
int32_t view_to_left(const Arg *arg);
int32_t view_to_right(const Arg *arg);
int32_t view_insert(const Arg *arg);
int32_t view_to_left_have_client(const Arg *arg);
int32_t view_to_right_have_client(const Arg *arg);
int32_t viewprev_have_client(const Arg *arg);
int32_t viewnext_have_client(const Arg *arg);
int32_t view_cross_monitor(const Arg *arg);
int32_t toggle_floating(const Arg *arg);
int32_t toggle_fullscreen(const Arg *arg);
int32_t toggle_maximize_screen(const Arg *arg);
int32_t toggle_gaps(const Arg *arg);
int32_t tag_monitor(const Arg *arg);
int32_t spawn(const Arg *arg);
int32_t spawn_shell(const Arg *arg);
int32_t spawn_on_empty(const Arg *arg);
int32_t set_key_mode(const Arg *arg);
int32_t switch_keyboard_layout(const Arg *arg);
int32_t set_layout(const Arg *arg);
int32_t switch_layout(const Arg *arg);
int32_t set_master_factor(const Arg *arg);
int32_t quit(const Arg *arg);
int32_t move_resize(const Arg *arg);
int32_t exchange_client(const Arg *arg);
int32_t move_client(const Arg *arg);
int32_t exchange_stack_client(const Arg *arg);
int32_t kill_client(const Arg *arg);
int32_t toggle_global(const Arg *arg);
int32_t inc_nmaster(const Arg *arg);
int32_t focus_monitor(const Arg *arg);
int32_t focus_stack(const Arg *arg);
int32_t over_circle(const Arg *arg);
int32_t group_focus(const Arg *arg);
int32_t change_vt(const Arg *arg);
int32_t reload_config(const Arg *arg);
int32_t load_config_file(const Arg *arg);
int32_t smart_move_window(const Arg *arg);
int32_t smart_resize_window(const Arg *arg);
int32_t center_window(const Arg *arg);
int32_t bind_to_view(const Arg *arg);
int32_t toggle_tag(const Arg *arg);
int32_t toggle_view(const Arg *arg);
int32_t tag(const Arg *arg);
int32_t combo_view(const Arg *arg);
int32_t increase_gaps(const Arg *arg);
int32_t increase_inner_gap(const Arg *arg);
int32_t increase_inner_horizontal_gap(const Arg *arg);
int32_t increase_inner_vertical_gap(const Arg *arg);
int32_t increase_outer_gap(const Arg *arg);
int32_t increase_outer_horizontal_gap(const Arg *arg);
int32_t increase_outer_vertical_gap(const Arg *arg);
int32_t reset_gaps(const Arg *arg);
int32_t toggle_fake_fullscreen(const Arg *arg);
int32_t toggle_overlay(const Arg *arg);
int32_t move_window(const Arg *arg);
int32_t resize_window(const Arg *arg);
int32_t toggle_named_scratchpad(const Arg *arg);
int32_t toggle_render_border(const Arg *arg);
int32_t create_virtual_output(const Arg *arg);
int32_t destroy_all_virtual_output(const Arg *arg);
int32_t focus_last(const Arg *arg);
int32_t toggle_trackpad_enable(const Arg *arg);
int32_t setoption(const Arg *arg);
int32_t disable_monitor(const Arg *arg);
int32_t enable_monitor(const Arg *arg);
int32_t toggle_monitor(const Arg *arg);
int32_t sleep_monitor(const Arg *arg);
int32_t wakeup_monitor(const Arg *arg);
int32_t sleep_toggle_monitor(const Arg *arg);
int32_t scroller_stack(const Arg *arg);
int32_t toggle_all_floating(const Arg *arg);
int32_t dwindle_toggle_split_direction(const Arg *arg);
int32_t dwindle_split_horizontal(const Arg *arg);
int32_t dwindle_split_vertical(const Arg *arg);
int32_t dwindle_toggle_current_split(const Arg *arg);
int32_t focus_by_id(const Arg *arg);

int32_t toggle_special_tag(const Arg *arg);
int32_t tag_special_tag(const Arg *arg);
void toggle_special_tag_mon(Monitor *m);
int32_t tag_special_silent(const Arg *arg);
#endif
