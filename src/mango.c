/*
 * See LICENSE file for copyright and license details.
 */
#include "mango.h"
#include "wlr/util/box.h"
/* Mango includes */
#include "animation/common.h"
#include "wlr/util/edges.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <pthread.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/fx/blur_data.h>
#include <scenefx/types/fx/clipped_region.h>
#include <scenefx/types/wlr_scene.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_color_representation_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fixes.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_keyboard_shortcuts_inhibit_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_pointer_gestures_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wordexp.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <X11/Xlib.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#endif
#include "common/log.h"
#include "common/util.h"
#include "draw/text-node.h"

/* macros */
#define MANGO_MAX(A, B) ((A) > (B) ? (A) : (B))
#define MANGO_MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define INSIDEMON(A)                                                           \
	(A->geom.x >= A->mon->m.x && A->geom.y >= A->mon->m.y &&                   \
	 A->geom.x + A->geom.width <= A->mon->m.x + A->mon->m.width &&             \
	 A->geom.y + A->geom.height <= A->mon->m.y + A->mon->m.height)
#define GEOMINSIDEMON(A, M)                                                    \
	(A->x >= M->m.x && A->y >= M->m.y &&                                       \
	 A->x + A->width <= M->m.x + M->m.width &&                                 \
	 A->y + A->height <= M->m.y + M->m.height)
#define ISTILED(A)                                                             \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->ismaximizescreen && !(A)->isfullscreen && !(A)->isunglobal)
#define ISNORMAL(A)                                                            \
	(A && !(A)->isminimized && !(A)->iskilling && !(A)->isunglobal)
#define ISSCROLLTILED(A)                                                       \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->isunglobal)
#define ISFAKETILED(A)                                                         \
	(A && !(A)->isfloating && !(A)->isminimized && !(A)->iskilling &&          \
	 !(A)->isunglobal)
#define VISIBLEON(C, M)                                                        \
	((C) && (M) && (C)->mon == (M) && !(C)->is_logic_hide &&                   \
	 !(C)->isminimized &&                                                      \
	 (((C)->tags & (M)->tagset[(M)->seltags] || (C)->isglobal ||               \
	   (C)->isunglobal)))

#define TAGMATCH(C, M)                                                         \
	((C) && (M) && (C)->mon == (M) && !(C)->is_logic_hide &&                   \
	 !(C)->isminimized && (((C)->tags & (M)->tagset[(M)->seltags])))

#define ISMODEKEYCODE(KEY)                                                     \
	((KEY) == 133 || (KEY) == 37 || (KEY) == 64 || (KEY) == 50 ||              \
	 (KEY) == 134 || (KEY) == 105 || (KEY) == 108 || (KEY) == 62)

#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))

#define TAGMASK (tagmask)
uint32_t tagmask = ((1u << 9) - 1); // 默认 9 个 tag

#define ISFULLSCREEN(A)                                                        \
	((A)->isfullscreen || (A)->ismaximizescreen ||                             \
	 (A)->overview_ismaximizescreenbak || (A)->overview_isfullscreenbak)
#define LISTEN_STATIC(E, H)                                                    \
	do {                                                                       \
		struct wl_listener *_l = ecalloc(1, sizeof(*_l));                      \
		_l->notify = (H);                                                      \
		wl_signal_add((E), _l);                                                \
	} while (0)

#define APPLY_INT_PROP(obj, rule, prop)                                        \
	if (rule->prop >= 0)                                                       \
	obj->prop = rule->prop

#define APPLY_FLOAT_PROP(obj, rule, prop)                                      \
	if (rule->prop > 0.0f)                                                     \
	obj->prop = rule->prop

#define APPLY_STRING_PROP(obj, rule, prop)                                     \
	if (rule->prop != NULL)                                                    \
	obj->prop = rule->prop

#define BAKED_POINTS_COUNT 256

#define IPC_WATCH_ARRANGGE                                                     \
	IPC_WATCH_MONITOR | IPC_WATCH_CLIENT | IPC_WATCH_TAGS |                    \
		IPC_WATCH_ALL_MONITORS | IPC_WATCH_ALL_TAGS | IPC_WATCH_ALL_CLIENTS |  \
		IPC_WATCH_LAST_OPEN_SURFACE | IPC_WATCH_FOCUSING_CLIENT

/* enums */
enum { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };

enum { VERTICAL, HORIZONTAL };
enum { SWIPE_UP, SWIPE_DOWN, SWIPE_LEFT, SWIPE_RIGHT };
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum {
	XDGShell,
	LayerShell,
	X11,
	Snapshot,
	XdgPopup,
	XdgImPopup,
	GroupBar
}; /* client types */
enum { AxisUp, AxisDown, AxisLeft, AxisRight }; // 滚轮滚动的方向
enum {
	LyrBg,
	LyrBlur,
	LyrBottom,
	LyrTile,
	LyrMaximize,
	LyrTop,
	// special workspace layers: above LyrTop; dim at the bottom, then
	// tiled / maximized / floating+fullscreen
	LyrSpecialDim,
	LyrSpecialTile,
	LyrSpecialMaximize,
	LyrSpecialTop,
	LyrFadeOut,
	LyrOverlay,
	LyrIMPopup, // text-input layer
	LyrBlock,
	NUM_LAYERS
}; /* scene layers */

#ifdef XWAYLAND
enum {
	NetWMWindowTypeDialog,
	NetWMWindowTypeSplash,
	NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility,
	NetLast
}; /* EWMH atoms */
#endif
enum { UP, DOWN, LEFT, RIGHT, UNDIR }; /* smartmovewin */
enum { NONE, OPEN, MOVE, CLOSE, TAG, FOCUS, OPAFADEIN, OPAFADEOUT, OVERVIEW };
enum { UNFOLD, FOLD, INVALIDFOLD };
enum { PREV, NEXT };
enum { SW_CURRENT_TAG, SW_ALL_TAG, SW_ALL_MON }; /* switcher 候选范围 */
enum { STATE_UNSPECIFIED = 0, STATE_ENABLED, STATE_DISABLED };
enum { FORCE, UNFORCE };

enum tearing_mode {
	TEARING_DISABLED = 0,
	TEARING_ENABLED,
	TEARING_FULLSCREEN_ONLY,
};

enum seat_config_shortcuts_inhibit {
	SHORTCUTS_INHIBIT_DISABLE,
	SHORTCUTS_INHIBIT_ENABLE,
};

enum ipc_watch_type {
	IPC_WATCH_NONE = 0,
	IPC_WATCH_MONITOR = 1 << 0,
	IPC_WATCH_CLIENT = 1 << 1,
	IPC_WATCH_TAGS = 1 << 2,
	IPC_WATCH_ALL_MONITORS = 1 << 3,
	IPC_WATCH_ALL_TAGS = 1 << 4,
	IPC_WATCH_ALL_CLIENTS = 1 << 5,
	IPC_WATCH_KEYMODE = 1 << 6,
	IPC_WATCH_KB_LAYOUT = 1 << 7,
	IPC_WATCH_LAST_OPEN_SURFACE = 1 << 8,
	IPC_WATCH_FOCUSING_CLIENT = 1 << 9,
	IPC_WATCH_DEVICE = 1 << 10,
};

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
typedef struct Client Client;

struct dvec2 {
	double x, y;
};

struct ivec2 {
	int32_t x, y, width, height;
};

typedef struct {
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

typedef struct {
	char mode[28];
	bool isdefault;
} KeyMode;

typedef struct {
	struct wl_list link;
	struct wlr_input_device *wlr_device;
	struct libinput_device *libinput_device;
	struct wl_listener destroy_listener;
	void *device_data;
	bool standalone;			  /* 命中 devicerule 的键盘，独立于默认键盘组 */
	struct wl_listener key_watch; /* 记录最后触发按键事件的设备 */
} InputDevice;

typedef struct {
	struct wl_list link;
	struct wlr_switch *wlr_switch;
	struct wl_listener toggle;
	InputDevice *input_dev;
} Switch;

struct mango_animation {
	bool should_animate;
	bool running;
	bool tagining;
	bool tagouted;
	bool tagouting;
	bool begin_fade_in;
	bool tag_from_rule;
	bool overining;
	bool overview_enter_anim_set;
	uint32_t time_started;
	uint32_t duration;
	struct wlr_box initial;
	struct wlr_box current;
	int32_t action;
};

struct mango_opacity_animation {
	bool running;
	float current_opacity;
	float target_opacity;
	float initial_opacity;
	uint32_t time_started;
	uint32_t duration;
	float current_border_color[4];
	float target_border_color[4];
	float initial_border_color[4];
};

typedef struct {
	float width_scale;
	float height_scale;
	int32_t width;
	int32_t height;
	struct fx_corner_radii corner_location;
	bool should_scale;
} BufferData;

// overview 卡片 surface 节点：每个 surface（含 subsurface）对应卡片树中
// 的一个 scene_surface 节点，sx/sy 是其相对根 surface 的坐标
struct ov_card_surface {
	Client *c;
	struct wlr_surface *surface;
	struct wlr_scene_surface *scene_surface;
	struct wlr_scene_buffer *buffer;
	int sx, sy; /* 相对根 surface 的坐标 */
	bool is_root;
	struct wl_list link;
	struct wl_listener commit;	/* 提交后重算缩放（提交会重置 dest/source） */
	struct wl_listener destroy; /* surface 销毁时移除节点 */
};

struct Client {
	/* Must keep these three elements in this order */
	uint32_t type; // must at first in struct
	struct wlr_box geom, pending, float_geom, animainit_geom,
		overview_backup_geom, current,
		drag_begin_geom; /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border; /* top, bottom, left, right */
	struct wlr_scene_rect *droparea;
	struct wlr_scene_rect *splitindicator[4];
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_rect *shield;
	struct wlr_scene_blur *blur;
	struct wlr_scene_tree *scene_surface;
	struct wlr_scene_tree *image_capture_tree;
	struct wlr_scene *image_capture_scene;
	struct wlr_ext_image_capture_source_v1 *image_capture_source;
	struct wlr_scene_surface *image_capture_scene_surface;
	struct wlr_scene_tree *overview_scene_surface;
	MangoJumpLabel *jump_label_node;
	MangoGroupBar *group_bar;
	struct wl_list link;
	struct wl_list flink;
	struct wl_list fadeout_link;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener minimize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
	struct wl_listener set_geometry;
	struct wl_listener commmitx11;
	struct wlr_scene_buffer *xwl_root_buffer;
	float xwayland_scale;	 /* X11 坐标相对逻辑坐标的缩放 */
	struct wlr_box xwl_clip; /* XWayland 根 surface 最近一次逻辑裁剪区 */
	bool xwl_clip_active;	 /* 是否处于 source_box 裁剪状态 */
	/* X11 configure 去重：客户端尚未 ack 时 surface->current 不更新，
	 * 多次 arrange 会重复发相同参数的 configure，导致客户端反复重渲染/
	 * 上传。这里记录最近一次请求的物理尺寸/位置，相同参数不再重复发送。 */
	int32_t xwl_req_x, xwl_req_y, xwl_req_w, xwl_req_h;
	bool xwl_req_valid;
#endif
	uint32_t bw;
	uint32_t tags, oldtags, mini_restore_tag;
	bool dirty;
	uint32_t configure_serial;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int32_t isfloating, isurgent, isfullscreen, isfakefullscreen,
		need_float_size_reduce, isminimized, isoverlay, isnosizehint,
		ignore_maximize, ignore_minimize, idleinhibit_when_focus,
		vrr_only_fullscreen, force_render, activation_bypass;
	int32_t ismaximizescreen;
	int32_t overview_backup_bw;
	int32_t fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
		fullscreen_backup_h;
	int32_t overview_isfullscreenbak, overview_ismaximizescreenbak,
		overview_isfloatingbak;

	struct wlr_scene_tree *
		ov_card_tree; /* overview 卡片树（含主 surface + 各 subsurface 节点） */
	struct wl_list ov_card_surfaces; /* struct ov_card_surface 链表 */

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener foreign_minimize_request;
	struct wl_listener foreign_maximize_request;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;

	const char *animation_type_open;
	const char *animation_type_close;
	int32_t is_in_scratchpad;
	int32_t iscustomsize;
	int32_t iscustompos;
	int32_t iscustom_scroller_proportion;
	int32_t iscustom_scroller_proportion_single;
	int32_t is_scratchpad_show;
	int32_t isglobal;
	int32_t isnoborder;
	int32_t isnoshadow;
	int32_t isnoradius;
	int32_t isnoanimation;
	int32_t isopensilent;
	int32_t istagsilent;
	int32_t iskilling;
	int32_t isnamedscratchpad;
	int32_t shield_when_capture;
	bool is_pending_open_animation;
	bool is_restoring_from_ov;
	float scroller_proportion;
	float stack_proportion;
	float old_stack_proportion;
	bool need_output_flush;
	struct mango_animation animation;
	struct mango_opacity_animation opacity_animation;
	int32_t isterm, noswallow;
	int32_t allow_csd;
	int32_t force_fakemaximize;
	int32_t force_tiled_state;
	pid_t pid;
	Client *swallowdby, *swallowing;
	bool is_clip_to_hide;
	bool drag_to_tile;
	bool scratchpad_switching_mon;
	bool fake_no_border;
	int32_t nofocus;
	int32_t nofadein;
	int32_t nofadeout;
	int32_t no_force_center;
	int32_t isunglobal;
	float focused_opacity;
	float unfocused_opacity;
	char oldmonname[128];
	int32_t noblur;
	float blur_opacity;
	struct wlr_ext_foreign_toplevel_handle_v1 *ext_foreign_toplevel;
	double master_mfact_per, master_inner_per, stack_inner_per;
	double old_master_mfact_per, old_master_inner_per, old_stack_inner_per;
	double old_scroller_pproportion;
	bool ismaster;
	bool old_ismaster;
	bool cursor_in_upper_half, cursor_in_left_half;
	bool isleftstack;
	int32_t tearing_hint;
	int32_t force_tearing;
	int32_t allow_shortcuts_inhibit;
	float scroller_proportion_single;
	bool isfocusing;
	char jump_char;
	bool enable_drop_area_draw;
	int32_t drop_direction;
	struct wlr_box drag_tile_float_backup_geom;
	float grid_col_per;
	float grid_row_per;
	float old_grid_col_per;
	float old_grid_row_per;
	int32_t grid_col_idx;
	int32_t grid_row_idx;
	uint32_t id;
	Client *group_prev;
	Client *group_next;
	bool isgroupfocusing;
	bool is_logic_hide;
};

typedef struct {
	struct wlr_keyboard_group *wlr_group;
	struct wlr_keyboard
		*keyboard; /* 实际生效的 wlr_keyboard（group 或独立键盘） */
	struct wlr_keyboard *virtual_keyboard;
	struct wlr_keyboard
		*prev_seat_keyboard; /* 接管 seat 前生效的键盘，销毁时恢复用 */

	int32_t nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods;				 /* invalid if nsyms == 0 */
	uint32_t keycode;
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;

	uint32_t layout_index;
	struct wl_list link; /* standalone_keyboards */
} KeyboardGroup;

typedef struct {
	struct wlr_keyboard_shortcuts_inhibitor_v1 *inhibitor;
	struct wl_listener destroy;
	struct wl_list link;
} KeyboardShortcutsInhibitor;

typedef struct {
	/* Must keep these three elements in this order */
	uint32_t type; // must at first in struct
	struct wlr_box geom, current, pending, animainit_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_rect *shield;
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_blur *blur;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	struct wl_list fadeout_link;
	int32_t mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;

	struct mango_animation animation;
	bool dirty;
	int32_t noblur;
	int32_t noanim;
	int32_t noshadow;
	char *animation_type_open;
	char *animation_type_close;
	bool shield_when_capture;
	bool need_output_flush;
	bool being_unmapped;
} LayerSurface;

typedef struct {
	uint32_t type; // must at first in struct
	struct wlr_xdg_popup *wlr_popup;
	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener reposition;
} Popup;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
	uint32_t id;
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_output_state pending;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wl_event_source *skip_frame_timeout;
	struct wlr_box m;		  /* monitor area, layout-relative */
	struct wlr_box w;		  /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	uint32_t seltags;
	uint32_t tagset[2];
	bool skiping_frame;
	uint32_t resizing_count_pending;
	uint32_t resizing_count_current;

	int32_t gappih; /* horizontal gap between windows */
	int32_t gappiv; /* vertical gap between windows */
	int32_t gappoh; /* horizontal outer gaps */
	int32_t gappov; /* vertical outer gaps */
	// special workspace gaps, per monitor
	int32_t special_gappih;
	int32_t special_gappiv;
	int32_t special_gappoh;
	int32_t special_gappov;
	Pertag *pertag;
	uint32_t ovbk_current_tagset;
	uint32_t ovbk_prev_tagset;
	Client *sel, *prevsel;
	int32_t isoverview;
	int32_t is_jump_mode;
	int32_t is_in_hotarea;
	int32_t ov_normal_mode; /* 热区进入时使用普通网格布局 */
	int32_t ov_tab_layout;	/* overcircle 进入时使用居中 tab 布局 */
	int32_t only_sleep;
	bool special_empty_view; // user intentionally opened the empty special view
	uint32_t visible_clients;
	uint32_t visible_tiling_clients;
	uint32_t visible_scroll_tiling_clients;
	uint32_t visible_fake_tiling_clients;
	uint32_t hide_clients;
	struct wlr_scene_optimized_blur *blur;
	struct wlr_scene_rect *special_dim_rect;
	char last_open_surface[256];
	struct wlr_ext_workspace_group_handle_v1 *ext_group;
	bool iscleanuping;
	int8_t carousel_anim_dir;
	bool vrr_global_enable;
	bool is_vrr_enabling;
	bool hdr_enable;
	bool prefer_disable;
	bool is_hdr_enabling;
	// Mastering display metadata, in cd/m². 0 = unset, see output_enable_hdr().
	float hdr_min_lum;
	float hdr_max_lum;
	float hdr_max_avg_lum;
	// Bypass the EDID-derived capability checks (DisplayID-only panels).
	bool hdr_force;
	struct wlr_color_transform *icc_transform; /* 从 icc 加载的 ICC 变换 */
	char icc_path[PATH_MAX];
};

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

struct capture_session_tracker {
	struct wl_listener session_destroy;
	struct wlr_ext_image_copy_capture_session_v1 *session;
};

typedef struct DwindleNode DwindleNode;
struct DwindleNode {
	bool is_split;
	bool split_h;
	bool split_locked;
	bool custom_leaf_split_h;
	float ratio;
	float drag_init_ratio;
	int32_t container_x;
	int32_t container_y;
	int32_t container_w;
	int32_t container_h;
	DwindleNode *parent;
	DwindleNode *first;
	DwindleNode *second;
	Client *client;
};

struct ScrollerStackNode {
	Client *client;
	float scroller_proportion;
	float stack_proportion;
	float scroller_proportion_single;

	struct ScrollerStackNode *next_in_stack;
	struct ScrollerStackNode *prev_in_stack;
	struct ScrollerStackNode *all_next;
};

struct TagScrollerState {
	struct ScrollerStackNode *all_first; /* 所有节点的单链表头 */
	int count;
};

typedef struct {
	uint32_t type; // must at first in struct
	int32_t orig_width;
	int32_t orig_height;
	bool is_subsurface;
	struct wl_listener destroy;
} SnapshotMetadata;

/* function declarations */
void applybounds(
	Client *c,
	struct wlr_box *bbox);	// 设置边界规则,能让一些窗口拥有比较适合的大小
void applyrules(Client *c); // 窗口规则应用,应用config.h中定义的窗口规则
void apply_window_snap(Client *c);
void arrange(Monitor *m, bool want_animation,
			 bool from_view); // 布局函数,让窗口俺平铺规则移动和重置大小
void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int32_t exclusive);
void arrangelayers(Monitor *m);
void handle_print_status(struct wl_listener *listener, void *data);
void axisnotify(struct wl_listener *listener,
				void *data); // 滚轮事件处理
void buttonpress(struct wl_listener *listener,
				 void *data); // 鼠标按键事件处理
bool handle_buttonpress(struct wlr_pointer_button_event *event);
int32_t ongesture(struct wlr_pointer_swipe_end_event *event);
void swipe_begin(struct wl_listener *listener, void *data);
void swipe_update(struct wl_listener *listener, void *data);
void swipe_end(struct wl_listener *listener, void *data);
void pinch_begin(struct wl_listener *listener, void *data);
void pinch_update(struct wl_listener *listener, void *data);
void pinch_end(struct wl_listener *listener, void *data);
void hold_begin(struct wl_listener *listener, void *data);
void hold_end(struct wl_listener *listener, void *data);
void checkidleinhibitor(struct wlr_surface *exclude);
void cleanupmon(struct wl_listener *listener, void *data); // 退出清理
void closemon(Monitor *m);
void toggle_hotarea(int32_t x_root, int32_t y_root); // 触发热区
void maplayersurfacenotify(struct wl_listener *listener, void *data);
void commitlayersurfacenotify(struct wl_listener *listener, void *data);
void commitnotify(struct wl_listener *listener, void *data);
void createdecoration(struct wl_listener *listener, void *data);
void createidleinhibitor(struct wl_listener *listener, void *data);
void createkeyboard(struct wlr_keyboard *keyboard);
void requestmonstate(struct wl_listener *listener, void *data);
void createlayersurface(struct wl_listener *listener, void *data);
void createlocksurface(struct wl_listener *listener, void *data);
void createmon(struct wl_listener *listener, void *data);
void createpointer(struct wlr_pointer *pointer);
void configure_pointer(struct wlr_input_device *wlr_device,
					   struct libinput_device *device);
void destroyinputdevice(struct wl_listener *listener, void *data);
void createswitch(struct wlr_switch *switch_device);
void switch_toggle(struct wl_listener *listener, void *data);
void createpointerconstraint(struct wl_listener *listener, void *data);
void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
void commitpopup(struct wl_listener *listener, void *data);
void cursorwarptohint(void);
void destroydecoration(struct wl_listener *listener, void *data);
void destroydragicon(struct wl_listener *listener, void *data);
void destroyidleinhibitor(struct wl_listener *listener, void *data);
void destroylayernodenotify(struct wl_listener *listener, void *data);
void destroylock(SessionLock *lock, int32_t unlocked);
void destroylocksurface(struct wl_listener *listener, void *data);
void destroynotify(struct wl_listener *listener, void *data);
void destroypointerconstraint(struct wl_listener *listener, void *data);
void destroysessionlock(struct wl_listener *listener, void *data);
void destroykeyboardgroup(struct wl_listener *listener, void *data);
Monitor *dirtomon(enum wlr_direction dir);
void setcursorshape(struct wl_listener *listener, void *data);

void focuslayer(LayerSurface *l);
void focusclient(Client *c, int32_t lift);

void setborder_color(Client *c);
Client *focustop(Monitor *m);
void fullscreennotify(struct wl_listener *listener, void *data);
void gpureset(struct wl_listener *listener, void *data);

int32_t keyrepeat(void *data);

void inputdevice(struct wl_listener *listener, void *data);
int32_t keybinding(uint32_t state, bool locked, uint32_t mods, xkb_keysym_t sym,
				   uint32_t keycode);
void keypress(struct wl_listener *listener, void *data);
void keypressmod(struct wl_listener *listener, void *data);
bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, uint32_t mods,
					xkb_keysym_t keysym, uint32_t keycode);
void locksession(struct wl_listener *listener, void *data);
void mapnotify(struct wl_listener *listener, void *data);
void maximizenotify(struct wl_listener *listener, void *data);
void minimizenotify(struct wl_listener *listener, void *data);
void motionabsolute(struct wl_listener *listener, void *data);
void motionnotify(uint32_t time, struct wlr_input_device *device, double sx,
				  double sy, double sx_unaccel, double sy_unaccel);
void motionrelative(struct wl_listener *listener, void *data);

void reset_foreign_tolevel(Client *c, Monitor *oldmon, Monitor *newmon);
void add_foreign_topleve(Client *c);
void exchange_two_client(Client *c1, Client *c2);
void outputmgrapply(struct wl_listener *listener, void *data);
void outputmgrapplyortest(struct wlr_output_configuration_v1 *config,
						  int32_t test);
void outputmgrtest(struct wl_listener *listener, void *data);
void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
				  uint32_t time);
void printstatus(enum ipc_watch_type type);
void powermgrsetmode(struct wl_listener *listener, void *data);
void rendermon(struct wl_listener *listener, void *data);
void requestdecorationmode(struct wl_listener *listener, void *data);
void requestdrmlease(struct wl_listener *listener, void *data);
void requeststartdrag(struct wl_listener *listener, void *data);
void setcursor(struct wl_listener *listener, void *data);
void setfloating(Client *c, int32_t floating);
void setfakefullscreen(Client *c, int32_t fakefullscreen);
void setfullscreen(Client *c, int32_t fullscreen, bool rearrange);
void setmaximizescreen(Client *c, int32_t maximizescreen, bool rearrange);
void reset_maximizescreen_size(Client *c);
void setgaps(int32_t oh, int32_t ov, int32_t ih, int32_t iv);

void setmon(Client *c, Monitor *m, uint32_t newtags, bool focus);
void setpsel(struct wl_listener *listener, void *data);
void setsel(struct wl_listener *listener, void *data);
void startdrag(struct wl_listener *listener, void *data);

void unlocksession(struct wl_listener *listener, void *data);
void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
void unmapnotify(struct wl_listener *listener, void *data);
void updatemons(struct wl_listener *listener, void *data);
void updatetitle(struct wl_listener *listener, void *data);
void urgent(struct wl_listener *listener, void *data);
void view(const Arg *arg, bool want_animation);

void handle_keyboard_shortcuts_inhibit_new_inhibitor(
	struct wl_listener *listener, void *data);
void virtualkeyboard(struct wl_listener *listener, void *data);
void virtualpointer(struct wl_listener *listener, void *data);
void warp_cursor(const Client *c);
Monitor *xytomon(double x, double y);
Monitor *get_monitor_nearest_to(int32_t x, int32_t y);
void handle_iamge_copy_capture_new_session(struct wl_listener *listener,
										   void *data);
void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc,
			  LayerSurface **pl, MangoGroupBar **tb, double *nx, double *ny);
void clear_fullscreen_flag(Client *c);
pid_t getparentprocess(pid_t p);
int32_t isdescprocess(pid_t p, pid_t c);
Client *termforwin(Client *w);
void client_replace(Client *c, Client *w, bool is_group_change_member,
					bool is_swallow);

void warp_cursor_to_selmon(Monitor *m);
uint32_t want_restore_fullscreen(Client *target_client);
void overview_restore(Client *c, const Arg *arg);
void overview_backup(Client *c);
void set_minimized(Client *c);

void show_scratchpad(Client *c);
void show_hide_client(Client *c);
void tag_client(const Arg *arg, Client *target_client);

struct wlr_box setclient_coordinate_center(Client *c, Monitor *m,
										   struct wlr_box geom, int32_t offsetx,
										   int32_t offsety);
uint32_t get_tags_first_tag(uint32_t tags);

struct wlr_output_mode *get_nearest_output_mode(struct wlr_output *output,
												int32_t width, int32_t height,
												float refresh);

void client_set_opacity(Client *c, double opacity);
void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int32_t sx,
								int32_t sy, void *data);

Client *direction_select(const Arg *arg);
void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
				 bool changefocus);

<<<<<<< HEAD
void client_send_frame_done(Client *c, const struct timespec *now);
void overview_layout_card(Client *c);
void overview_destroy_card(Client *c);
void overview_card_set_corner_radii(Client *c, struct fx_corner_radii corners);
Client *center_tiled_select(Monitor *m);
void handlecursoractivity(void);
int32_t hidecursor(void *data);
bool check_hit_no_border(Client *c);
void reset_keyboard_layout(void);
void client_update_oldmonname_record(Client *c, Monitor *m);
void pending_kill_client(Client *c);
uint32_t get_tags_first_tag_num(uint32_t source_tags);
void set_layer_open_animaiton(LayerSurface *l, struct wlr_box geo);
=======
static void buffer_set_effect(Client *c, BufferData buffer_data);
static void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
										   int32_t sx, int32_t sy, void *data);
static void client_send_frame_done(Client *c, const struct timespec *now);
static void overview_layout_card(Client *c);
static void overview_destroy_card(Client *c);
static void overview_card_set_corner_radii(Client *c,
										   struct fx_corner_radii corners);
static void client_set_pending_state(Client *c);
static void layer_set_pending_state(LayerSurface *l);
static void set_rect_size(struct wlr_scene_rect *rect, int32_t width,
						  int32_t height);
static Client *center_tiled_select(Monitor *m);
static void handlecursoractivity(void);
static int32_t hidecursor(void *data);
static bool check_hit_no_border(Client *c);
static void reset_keyboard_layout(void);
static void client_update_oldmonname_record(Client *c, Monitor *m);
static void pending_kill_client(Client *c);
static uint32_t get_tags_first_tag_num(uint32_t source_tags);
static uint32_t get_mon_curtag(const Monitor *m);
static uint32_t get_client_tag_idx(const Client *c);
static bool is_special_active(const Monitor *m);
static bool special_has_clients(const Monitor *m);
static uint32_t get_monitor_active_tagset(const Monitor *m);
static uint32_t client_target_layer(Client *c);
static void client_sync_layer(Client *c);
static void special_update_dim(Monitor *m);
static void set_layer_open_animaiton(LayerSurface *l, struct wlr_box geo);
static void init_fadeout_layers(LayerSurface *l);
static void layer_actual_size(LayerSurface *l, int32_t *width, int32_t *height);
static void get_layer_target_geometry(LayerSurface *l,
									  struct wlr_box *target_box);
static void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
									  int32_t sx, int32_t sy, void *data);
static double find_animation_curve_at(double t, int32_t type);
>>>>>>> c62e01c303b8e19598df0b0b7cd16e466a71fc21

void apply_opacity_to_rect_nodes(Client *c, struct wlr_scene_node *node,
								 double animation_passed);
double all_output_frame_duration_ms();
bool is_scroller_layout(Monitor *m);
bool is_monocle_layout(Monitor *m);
bool is_centertile_layout(Monitor *m);
void create_output(struct wlr_backend *backend, void *data);
void get_layout_abbr(char *abbr, const char *full_name);
void apply_named_scratchpad(Client *target_client);
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title);
bool switch_scratchpad_client_state(Client *c);
bool check_trackpad_disabled(struct wlr_pointer *pointer);
uint32_t get_tag_status(uint32_t tag, Monitor *m);
void view_insert_shift_tags(Monitor *m, uint32_t target);
void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state);
Client *get_next_stack_client(Client *c, bool reverse);
void set_float_malposition(Client *tc);
void set_size_per(Monitor *m, Client *c);
void resize_tile_client(Client *grabc, bool isdrag, int32_t offsetx,
						int32_t offsety, uint32_t time);
void refresh_monitors_workspaces_status(Monitor *m);
void init_client_properties(Client *c);
float *get_border_color(Client *c);
void clear_fullscreen_and_maximized_state(Monitor *m);
void request_fresh_all_monitors(void);
Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating);
void exit_scroller_stack(Client *c);
Client *scroll_get_stack_head_client(Client *c);
bool client_only_in_one_tag(Client *c);
Client *get_focused_stack_client(Client *sc, Client *custom_focus_client);
bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc);
void monitor_stop_skip_frame_timer(Monitor *m);
int monitor_skip_frame_timeout_callback(void *data);
Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly);
bool match_monitor_spec(char *spec, Monitor *m);
void last_cursor_surface_destroy(struct wl_listener *listener, void *data);
int32_t keep_idle_inhibit(void *data);
void check_keep_idle_inhibit(Client *c);
void pre_calculate_before_arrange(Monitor *m, bool want_animation,
								  bool from_view, bool only_calculate);
void client_pending_fullscreen_state(Client *c, int32_t isfullscreen);
void client_pending_maximized_state(Client *c, int32_t ismaximized);
void client_pending_minimized_state(Client *c, int32_t isminimized);
void scroller_insert_stack(Client *c, Client *target_client,
						   bool insert_before);
void dwindle_move_client(DwindleNode **root, Client *c, Client *target,
						 float ratio, int32_t dir);
void dwindle_resize_client_step(Monitor *m, Client *c, int32_t dx, int32_t dy);
void dwindle_resize_client(Monitor *m, Client *c);

struct TagScrollerState *ensure_scroller_state(Monitor *m, uint32_t tag);
struct ScrollerStackNode *find_scroller_node(struct TagScrollerState *st,
											 Client *c);
void sync_scroller_state_to_clients(Monitor *m, uint32_t tag);
void scroller_node_remove(struct TagScrollerState *st,
						  struct ScrollerStackNode *target);
struct ScrollerStackNode *scroller_node_create(struct TagScrollerState *st,
											   Client *c);
void update_scroller_state(Monitor *m);
Client *scroll_get_stack_tail_client(Client *c);
DwindleNode *dwindle_find_leaf(DwindleNode *node, Client *c);
void overview_backup_surface(Client *c);

void create_jump_hints(Monitor *m);
void finish_jump_mode(Monitor *m);
void begin_jump_mode(Monitor *m);

void client_reparent_group(Client *c);
void client_change_mon(Client *c, Monitor *m);
void check_vrr_enable(Client *c);
void output_enable_hdr(Monitor *m, struct wlr_output_state *os, bool enabled,
					   bool silent);
bool mango_scene_output_commit(struct wlr_scene_output *scene_output,
							   struct wlr_output_state *state);
bool mango_output_commit(Monitor *m);
void client_set_group_config(Client *c);

int32_t client_is_x11(Client *c);
struct wlr_surface *client_surface(Client *c);
int32_t toplevel_from_wlr_surface(struct wlr_surface *s, Client **pc,
								  LayerSurface **pl);
void client_activate_surface(struct wlr_surface *s, int32_t activated);
const char *client_get_appid(Client *c);
int32_t client_get_pid(Client *c);
void client_get_clip(Client *c, struct wlr_box *clip);
void client_get_geometry(Client *c, struct wlr_box *geom);
Client *client_get_parent(Client *c);
int32_t client_has_children(Client *c);
const char *client_get_title(Client *c);
int32_t client_is_float_type(Client *c);
int32_t client_is_rendered_on_mon(Client *c, Monitor *m);
int32_t client_is_unmanaged(Client *c);
void client_notify_enter(struct wlr_surface *s, struct wlr_keyboard *kb);
void client_send_close(Client *c);
void client_set_border_color(Client *c, const float color[4]);
void client_set_fullscreen(Client *c, int32_t fullscreen);
void client_set_scale(struct wlr_surface *s, float scale);
void client_update_xwayland_clip(Client *c, struct wlr_box *clip);
void client_update_xwayland_dest_size(Client *c);
uint32_t client_set_size(Client *c, uint32_t width, uint32_t height);
void client_set_minimized(Client *c, bool minimized);
void client_set_maximized(Client *c, bool maximized);
void client_set_tiled(Client *c, uint32_t edges);
int32_t client_should_ignore_focus(Client *c);
int32_t client_is_x11_popup(Client *c);
int32_t client_should_global(Client *c);
int32_t client_should_overtop(Client *c);
int32_t client_wants_focus(Client *c);
int32_t client_wants_fullscreen(Client *c);
bool client_request_minimize(Client *c, void *data);
bool client_request_maximize(Client *c, void *data);
void client_set_size_bound(Client *c);
void client_swap_layout_properties(Client *c1, Client *c2);
void client_swap_monitors_and_tags(Client *c1, Client *c2);
void finish_exchange_arrange_and_focus(Client *c1, Client *c2, Monitor *m1,
									   Monitor *m2);
void client_tile_resize(Client *c, struct wlr_box geo, int32_t interact);
uint32_t generate_client_id(void);
void client_active(Client *c);
void client_pending_force_kill(Client *c);
void client_add_jump_label_node(Client *c);
void client_add_group_bar(Client *c);
void client_focus_group_member(Client *c);
void client_check_tab_node_visible(Client *c);
void client_raise_group(Client *c);
void client_handle_decorate_click(MangoGroupBar *gb);
void client_set_group_mon(Client *c, Monitor *m);
void client_group_detach(Client *c);
void client_group_replace(Client *old, Client *new);
void mango_surface_frame_done(struct wlr_surface *surface, int sx, int sy,
							  void *data);
bool client_force_render(Client *c);
int32_t is_single_bit_set(uint32_t x);
bool layer_ignores_focus(LayerSurface *l);

/* 重排后补充的前置声明 */
void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int32_t sx,
							int32_t sy, void *user_data);
void unminimize(Client *c);
Client *termforwin(Client *w);
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title);
Client *center_tiled_select(Monitor *m);
Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating);
Client *direction_select(const Arg *arg);
Client *focustop(Monitor *m);
Client *get_next_stack_client(Client *c, bool reverse);
float *get_border_color(Client *c);
Client *get_focused_stack_client(Client *sc, Client *custom_focus_client);
Client *xytoclient(double x, double y);
Monitor *dirtomon(enum wlr_direction dir);
Monitor *xytomon(double x, double y);
Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly);

#include "dispatch/bind.h"
#include "layout/layout.h"
<<<<<<< HEAD
#include "common/globals.h"
=======

/* variables */
static const char broken[] = "broken";
static pid_t child_pid = -1;
static int32_t locked;
static uint32_t locked_mods = 0;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wl_event_loop *event_loop;
static struct wlr_backend *backend;
static struct wlr_backend *headless_backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;

static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wl_list clients; /* tiling order */
static struct wl_list fstack;  /* focus order */
static struct wl_list fadeout_clients;
static struct wl_list fadeout_layers;
static struct wlr_idle_notifier_v1 *idle_notifier;
static struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
static struct wlr_keyboard_shortcuts_inhibit_manager_v1
	*keyboard_shortcuts_inhibit;
static struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
static struct wlr_output_power_manager_v1 *power_mgr;
static struct wlr_ext_image_copy_capture_manager_v1 *ext_image_copy_capture_mgr;
static struct wlr_pointer_gestures_v1 *pointer_gestures;
static struct wlr_drm_lease_v1_manager *drm_lease_manager;
struct mango_print_status_manager *print_status_manager;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wlr_session *session;

static struct wlr_scene_rect *root_bg;
static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static const int32_t layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};
static struct wlr_scene_tree *drag_icon;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;

static struct wlr_seat *seat;
static KeyboardGroup *kb_group;
static struct wlr_keyboard
	*last_active_keyboard; /* 最后按键的键盘，get keyboardlayout 用 */
static struct wl_list inputdevices;
static struct wl_list standalone_keyboards; /* 独立键盘链表 */
static struct wl_list keyboard_shortcut_inhibitors;
static uint32_t cursor_mode;
static Client *grabc, *dropc;
static int32_t rzcorner;
static int32_t grabcx, grabcy; /* client-relative */

static struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1
	*ext_foreign_toplevel_image_capture_source_manager_v1;
static struct wl_listener new_foreign_toplevel_capture_request;
static struct wlr_ext_foreign_toplevel_list_v1 *foreign_toplevel_list;

static int32_t drag_begin_cursorx, drag_begin_cursory; /* client-relative */
static bool start_drag_window = false;
static int32_t last_apply_drap_time = 0;

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;
static struct wl_list mons;
static Monitor *selmon;
static struct wlr_scene_output_layout *scene_layout;

static int32_t enablegaps = 1; /* enables gaps, used by togglegaps */
static int32_t axis_apply_time = 0;
static int32_t axis_apply_dir = 0;
static int32_t scroller_focus_lock = 0;

static uint32_t swipe_fingers = 0;
static double swipe_dx = 0;
static double swipe_dy = 0;

bool render_border = true;

uint32_t chvt_backup_tag = 0;
bool allow_frame_scheduling = true;
char chvt_backup_selmon[32] = {0};

struct dvec2 *baked_points_move;
struct dvec2 *baked_points_open;
struct dvec2 *baked_points_tag;
struct dvec2 *baked_points_close;
struct dvec2 *baked_points_focus;
struct dvec2 *baked_points_opafadein;
struct dvec2 *baked_points_opafadeout;

static struct wl_event_source *hide_cursor_source;
static struct wl_event_source *keep_idle_inhibit_source;
static bool cursor_hidden = false;
static bool tag_combo = false;
static char cli_config_path[1024] = {0};
static int active_capture_count = 0;
static bool cli_debug_log = false;
static uint32_t last_hold_keycode = 0;

static KeyMode keymode = {
	.mode = {'d', 'e', 'f', 'a', 'u', 'l', 't', '\0'},
	.isdefault = true,
};

static char *env_vars[] = {"DISPLAY",
						   "WAYLAND_DISPLAY",
						   "XDG_CURRENT_DESKTOP",
						   "XDG_SESSION_TYPE",
						   "XCURSOR_THEME",
						   "XCURSOR_SIZE",
						   "MANGO_INSTANCE_SIGNATURE",
						   NULL};
static struct {
	enum wp_cursor_shape_device_v1_shape shape;
	struct wlr_surface *surface;
	int32_t hotspot_x;
	int32_t hotspot_y;
} last_cursor;

#include "config/preset.h"
// slots: 1..tag_num are normal tags, slot 0 is special tag0,
// tag_num_MAX + 1 is the all-tags view
struct Pertag {
	uint32_t curtag, prevtag;
	int32_t nmasters[PERTAG_SLOTS];
	float mfacts[PERTAG_SLOTS];
	int32_t no_hide[PERTAG_SLOTS];
	int32_t no_render_border[PERTAG_SLOTS];
	int32_t open_as_floating[PERTAG_SLOTS];
	float scroller_default_proportion[PERTAG_SLOTS];
	float scroller_default_proportion_single[PERTAG_SLOTS];
	int32_t scroller_ignore_proportion_single[PERTAG_SLOTS];
	struct DwindleNode *dwindle_root[PERTAG_SLOTS];
	const Layout *ltidxs[PERTAG_SLOTS];
	struct TagScrollerState *scroller_state[PERTAG_SLOTS];
};
>>>>>>> c62e01c303b8e19598df0b0b7cd16e466a71fc21
#include "common/log.h"

/* export an activation token for the process we're about to spawn */

#include "animation/common.h"
#include "input/touch.h"
#include "input/switch.h"
#include "input/tablet.h"
#include "ipc/ipc.h"
#include "layout/dwindle.h"
#include "layout/horizontal.h"
#include "layout/overview.h"
#include "layout/scroll.h"
#include "layout/vertical.h"

#include "input/keyboard.h"
#include "input/pointer.h"
#include "manage/monitor.h"
#include "manage/misc.h"
#include "ext-protocol/ext-workspace.h"
#include "ext-protocol/foreign-toplevel.h"
#include "ext-protocol/hdr.h"
#include "ext-protocol/tearing.h"
#include "ext-protocol/text-input.h"
#include "ext-protocol/xdg-activation.h"
#include "ext-protocol/xdg-output.h"
#include "common/util.h"

void handlesig(int32_t signo) {
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

static void restore_child_signals(void) {
	sigset_t set;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);

	struct sigaction sa_dfl = {.sa_flags = 0, .sa_handler = SIG_DFL};
	sigaction(SIGCHLD, &sa_dfl, NULL);
	sigaction(SIGPIPE, &sa_dfl, NULL);
}

void cleanuplisteners(void) {
	wl_list_remove(&ext_manager_commit_listener.link); // 0.7
	wl_list_remove(&print_status_listener.link);
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_frame.link);
	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
	wl_list_remove(&cursor_touch_down.link);
	wl_list_remove(&cursor_touch_up.link);
	wl_list_remove(&cursor_touch_cancel.link);
	wl_list_remove(&cursor_touch_motion.link);
	wl_list_remove(&cursor_touch_frame.link);
	wl_list_remove(&tablet_tool_proximity.link);
	wl_list_remove(&tablet_tool_axis.link);
	wl_list_remove(&tablet_tool_button.link);
	wl_list_remove(&tablet_tool_tip.link);
	wl_list_remove(&gpu_reset.link);
	wl_list_remove(&new_idle_inhibitor.link);
	wl_list_remove(&layout_change.link);
	wl_list_remove(&new_input_device.link);
	wl_list_remove(&new_virtual_keyboard.link);
	wl_list_remove(&new_virtual_pointer.link);
	wl_list_remove(&new_pointer_constraint.link);
	wl_list_remove(&new_output.link);
	wl_list_remove(&new_xdg_toplevel.link);
	wl_list_remove(&new_xdg_decoration.link);
	wl_list_remove(&new_xdg_popup.link);
	wl_list_remove(&new_layer_surface.link);
	wl_list_remove(&output_mgr_apply.link);
	wl_list_remove(&output_mgr_test.link);
	wl_list_remove(&output_power_mgr_set_mode.link);
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_psel.link);
	wl_list_remove(&request_set_sel.link);
	wl_list_remove(&request_set_cursor_shape.link);
	wl_list_remove(&request_start_drag.link);
	wl_list_remove(&start_drag.link);
	wl_list_remove(&new_session_lock.link);
	wl_list_remove(&new_foreign_toplevel_capture_request.link);
	wl_list_remove(&tearing_new_object.link);
	wl_list_remove(&keyboard_shortcuts_inhibit_new_inhibitor.link);
	wl_list_remove(&ext_image_copy_capture_mgr_new_session.link);
	if (drm_lease_manager) {
		wl_list_remove(&drm_lease_request.link);
	}
#ifdef XWAYLAND
	wl_list_remove(&new_xwayland_surface.link);
	wl_list_remove(&xwayland_ready.link);
#endif
}

void cleanup(void) {
	allow_frame_scheduling = false;

	ipc_cleanup();
	cleanuplisteners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif

	wl_display_destroy_clients(dpy);
	if (child_pid > 0) {
		kill(-child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
	}
	wlr_xcursor_manager_destroy(cursor_mgr);

	destroykeyboardgroup(&kb_group->destroy, NULL);

	mango_im_relay_finish(mango_input_method_relay);

	/* If it's not destroyed manually it will cause a use-after-free of
	 * wlr_seat. Destroy it until it's fixed in the wlroots side */
	wlr_backend_destroy(backend);

	wl_display_destroy(dpy);
	/* Destroy after the wayland display (when the monitors are already
	   destroyed) to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&scene->tree.node);

	mango_text_global_finish();
}

// 修改printstatus函数，接受掩码参数

void quitsignal(int32_t signo) { quit(NULL); }

void set_activation_env() {
	if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
		mango_error(true, WLR_INFO,
					"Not updating dbus execution environment: "
					"DBUS_SESSION_BUS_ADDRESS not set");
		return;
	}

	mango_error(true, WLR_INFO, "Updating dbus execution environment");

	char *env_keys = join_strings(env_vars, " ");

	// first command: dbus-update-activation-environment
	const char *arg1 = env_keys;
	char *cmd1 = string_printf("dbus-update-activation-environment %s", arg1);
	if (!cmd1) {
		mango_error(true, WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd1});
	free(cmd1);

	// second command: systemctl --user
	const char *action = "import-environment";
	char *cmd2 = string_printf("systemctl --user %s %s", action, env_keys);
	if (!cmd2) {
		mango_error(true, WLR_ERROR, "Failed to allocate command string");
		goto cleanup;
	}
	spawn(&(Arg){.v = cmd2});
	free(cmd2);

cleanup:
	free(env_keys);
}

void // 17
run(char *startup_cmd, int readiness_fd) {
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	set_env_display();

	/* Start the backend. This will enumerate outputs and inputs, become the
	 * DRM master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the
	 * startup command */

	if (startup_cmd) {
		int32_t piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((child_pid = fork()) < 0)
			die("startup: fork:");
		if (child_pid == 0) {
			setsid();
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}

	/* Mark stdout as non-blocking to avoid people who does not close stdin
	 * nor consumes it in their startup script getting dwl frozen */
	if (fd_set_nonblock(STDOUT_FILENO) < 0)
		close(STDOUT_FILENO);

	printstatus(IPC_WATCH_ARRANGGE);

	/* At this point the outputs are initialized, choose initial selmon
	 * based on cursor position, and set default cursor image */
	selmon = xytomon(cursor->x, cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");
	handlecursoractivity();

	set_activation_env();

	run_exec();
	run_exec_once();

	/*
	 * If running inside supervision suite like s6, notify about successfull
	 * startup by writing \n to the provided file descriptor and closing it
	 */
	if (readiness_fd > 2) {
		write(readiness_fd, "\n", 1);
		close(readiness_fd);
	}

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */

	wl_display_run(dpy);
}

// 修改信号处理函数，接收掩码参数

void setup(void) {
	setenv("XDG_CURRENT_DESKTOP", "mango", 1);
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);

	parse_config();
	if (cli_debug_log) {
		config.log_level = WLR_DEBUG;
	}
	init_baked_points();

	set_env_without_display();

	int32_t drm_fd, i;
	int32_t sig[] = {SIGCHLD, SIGINT,
					 SIGTERM}; // 不设置SIGPIPE,因为ipc发送失败不应该影响主程序
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	// 单独为 SIGPIPE 设置忽略
	struct sigaction sa_pipe = {.sa_flags = 0, .sa_handler = SIG_IGN};
	sigemptyset(&sa_pipe.sa_mask);
	sigaction(SIGPIPE, &sa_pipe, NULL);

	pthread_atfork(NULL, NULL, restore_child_signals);

	wlr_log_init(config.log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();

	wl_display_set_default_max_buffer_size(dpy, 1024 * 1024);

	event_loop = wl_display_get_event_loop(dpy);

	ipc_init(event_loop);

	tablet_mgr = wlr_tablet_v2_create(dpy);
	/* The backend is a wlroots feature which abstracts the underlying input
	 * and output hardware. The autocreate option will choose the most
	 * suitable backend based on the current environment, such as opening an
	 * X11 window if an X11 server is running. The NULL argument here
	 * optionally allows you to pass in a custom renderer if wlr_renderer
	 * doesn't meet your needs. The backend uses the renderer, for example,
	 * to fall back to software cursors if the backend does not support
	 * hardware cursors (some older GPUs don't). */
	if (!(backend = wlr_backend_autocreate(event_loop, &session)))
		die("couldn't create backend");

	headless_backend = wlr_headless_backend_create(event_loop);
	if (!headless_backend) {
		mango_error(true, WLR_ERROR,
					"Failed to create secondary headless backend");
	} else {
		wlr_multi_backend_add(backend, headless_backend);
	}

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, config.rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Create a renderer with the default implementation */
	if (!(drw = fx_renderer_create(backend)))
		die("couldn't create renderer");

	if (drw->features.input_color_transform) {
		const enum wp_color_manager_v1_render_intent render_intents[] = {
			WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL,
		};
		size_t transfer_functions_len = 0;
		enum wp_color_manager_v1_transfer_function *transfer_functions =
			wlr_color_manager_v1_transfer_function_list_from_renderer(
				drw, &transfer_functions_len);

		size_t primaries_len = 0;
		enum wp_color_manager_v1_primaries *primaries =
			wlr_color_manager_v1_primaries_list_from_renderer(drw,
															  &primaries_len);

		struct wlr_color_manager_v1 *cm = wlr_color_manager_v1_create(
			dpy, 2,
			&(struct wlr_color_manager_v1_options){
				.features =
					{
						.parametric = true,
						.set_mastering_display_primaries = true,
					},
				.render_intents = render_intents,
				.render_intents_len = ARRAY_SIZE(render_intents),
				.transfer_functions = transfer_functions,
				.transfer_functions_len = transfer_functions_len,
				.primaries = primaries,
				.primaries_len = primaries_len,
			});

		free(transfer_functions);
		free(primaries);

		if (cm) {
			wlr_scene_set_color_manager_v1(scene, cm);
		} else {
			mango_error(true, WLR_ERROR, "unable to create color manager");
		}
	}

	wlr_color_representation_manager_v1_create_with_renderer(dpy, 1, drw);

	wl_signal_add(&drw->events.lost, &gpu_reset);

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(drw);
	 * but we need to create manually the linux_dmabuf interface to
	 * integrate it with wlr_scene. */
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_texture_formats(drw, WLR_BUFFER_CAP_DMABUF)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(
			scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 4, drw));
	}

	if (config.syncobj_enable && (drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 &&
		drw->features.timeline && backend->features.timeline)
		wlr_linux_drm_syncobj_manager_v1_create(dpy, 1, drm_fd);

	/* Create a default allocator */
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device
	 * manager handles the clipboard. Each of these wlroots interfaces has
	 * room for you to dig your fingers in and play with their behavior if
	 * you want. Note that the clients cannot set the selection directly
	 * without compositor approval, see the setsel() function. */
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_export_dmabuf_manager_v1_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
	wlr_ext_output_image_capture_source_manager_v1_create(dpy, 1);
	wlr_data_control_manager_v1_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_primary_selection_v1_device_manager_create(dpy);
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_fractional_scale_manager_v1_create(dpy, 1);
	wlr_presentation_create(dpy, backend, 2);
	wlr_subcompositor_create(dpy);
	wlr_alpha_modifier_v1_create(dpy);
	wlr_ext_data_control_manager_v1_create(dpy, 1);
	wlr_fixes_create(dpy, 1);

	// 在 setup 函数中
	wl_signal_init(&mango_print_status);
	wl_signal_add(&mango_print_status, &print_status_listener);

	ext_image_copy_capture_mgr =
		wlr_ext_image_copy_capture_manager_v1_create(dpy, 1);
	wl_signal_add(&ext_image_copy_capture_mgr->events.new_session,
				  &ext_image_copy_capture_mgr_new_session);

	xdg_activation_init();

	wlr_scene_set_gamma_control_manager_v1(
		scene, wlr_gamma_control_manager_v1_create(dpy));

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	wl_signal_add(&power_mgr->events.set_mode, &output_power_mgr_set_mode);

	foreign_toplevel_list = wlr_ext_foreign_toplevel_list_v1_create(dpy, 1);
	ext_foreign_toplevel_image_capture_source_manager_v1 =
		wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(dpy, 1);
	new_foreign_toplevel_capture_request.notify =
		handle_new_foreign_toplevel_capture_request;
	wl_signal_add(&ext_foreign_toplevel_image_capture_source_manager_v1->events
					   .new_request,
				  &new_foreign_toplevel_capture_request);

	tearing_control = wlr_tearing_control_manager_v1_create(dpy, 1);
	tearing_new_object.notify = handle_tearing_new_object;
	wl_signal_add(&tearing_control->events.new_object, &tearing_new_object);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create(dpy);
	wl_signal_add(&output_layout->events.change, &layout_change);
	/* 自定义 xdg-output：对 XWayland 单独处理 */
	xdg_output_init();

	/* Configure a listener to be notified when new outputs are available on
	 * the backend. */
	wl_list_init(&mons);
	wl_signal_add(&backend->events.new_output, &new_output);
	scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

	/* Set up our client lists and the xdg-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&clients);
	wl_list_init(&fstack);
	wl_list_init(&fadeout_clients);
	wl_list_init(&fadeout_layers);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);

	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	wl_signal_add(&idle_inhibit_mgr->events.new_inhibitor, &new_idle_inhibitor);

	keep_idle_inhibit_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), keep_idle_inhibit, NULL);

	layer_shell = wlr_layer_shell_v1_create(dpy, 4);
	wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);
	wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	wl_signal_add(&session_lock_mgr->events.new_lock, &new_session_lock);

	locked_bg =
		wlr_scene_rect_create(layers[LyrBlock], sgeom.width, sgeom.height,
							  (float[4]){0.1, 0.1, 0.1, 1.0});
	wlr_scene_node_set_enabled(&locked_bg->node, false);

	/* Use decoration protocols to negotiate server-side decorations */
	wlr_server_decoration_manager_set_default_mode(
		wlr_server_decoration_manager_create(dpy),
		WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
	wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration,
				  &new_xdg_decoration);

	pointer_constraints = wlr_pointer_constraints_v1_create(dpy);
	wl_signal_add(&pointer_constraints->events.new_constraint,
				  &new_pointer_constraint);

	relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(dpy);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that
	 * cursor images are available at all scale factors on the screen
	 * (necessary for HiDPI support). Scaled cursors will be loaded with
	 * each output. */

	set_xcursor_env();

	cursor_mgr =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);
	/*
	 * wlr_cursor *only* displays an image on screen. It does not move
	 * around when the pointer moves. However, we can attach input devices
	 * to it, and it will generate aggregate events for all of them. In
	 * these events, we can choose how we want to process them, forwarding
	 * them to clients and moving the cursor around. More detail on this
	 * process is described in my input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions
	 * above.
	 */
	wl_signal_add(&cursor->events.motion, &cursor_motion);
	wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
	wl_signal_add(&cursor->events.button, &cursor_button);
	wl_signal_add(&cursor->events.axis, &cursor_axis);
	wl_signal_add(&cursor->events.frame, &cursor_frame);
	wl_signal_add(&cursor->events.tablet_tool_proximity,
				  &tablet_tool_proximity);
	wl_signal_add(&cursor->events.tablet_tool_axis, &tablet_tool_axis);
	wl_signal_add(&cursor->events.tablet_tool_button, &tablet_tool_button);
	wl_signal_add(&cursor->events.tablet_tool_tip, &tablet_tool_tip);

	// 这两句代码会造成obs窗口里的鼠标光标消失,不知道注释有什么影响
	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	wl_signal_add(&cursor_shape_mgr->events.request_set_shape,
				  &request_set_cursor_shape);
	hide_cursor_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
												 hidecursor, cursor);
	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener
	 * to let us know when new input devices are available on the backend.
	 */
	wl_list_init(&inputdevices);
	wl_list_init(&standalone_keyboards);
	wl_list_init(&virtual_keyboards);
	wl_list_init(&tablets);
	wl_list_init(&tablet_pads);
	wl_list_init(&keyboard_shortcut_inhibitors);
	wl_signal_add(&backend->events.new_input, &new_input_device);
	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	wl_signal_add(&virtual_keyboard_mgr->events.new_virtual_keyboard,
				  &new_virtual_keyboard);
	virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(dpy);
	wl_signal_add(&virtual_pointer_mgr->events.new_virtual_pointer,
				  &new_virtual_pointer);

	pointer_gestures = wlr_pointer_gestures_v1_create(dpy);
	LISTEN_STATIC(&cursor->events.swipe_begin, swipe_begin);
	LISTEN_STATIC(&cursor->events.swipe_update, swipe_update);
	LISTEN_STATIC(&cursor->events.swipe_end, swipe_end);
	LISTEN_STATIC(&cursor->events.pinch_begin, pinch_begin);
	LISTEN_STATIC(&cursor->events.pinch_update, pinch_update);
	LISTEN_STATIC(&cursor->events.pinch_end, pinch_end);
	LISTEN_STATIC(&cursor->events.hold_begin, hold_begin);
	LISTEN_STATIC(&cursor->events.hold_end, hold_end);

	/* 触摸支持：初始化触点链表并连接 cursor 触摸事件 */
	wl_list_init(&touch_points);
	wl_signal_add(&cursor->events.touch_down, &cursor_touch_down);
	wl_signal_add(&cursor->events.touch_up, &cursor_touch_up);
	wl_signal_add(&cursor->events.touch_cancel, &cursor_touch_cancel);
	wl_signal_add(&cursor->events.touch_motion, &cursor_touch_motion);
	wl_signal_add(&cursor->events.touch_frame, &cursor_touch_frame);

	seat = wlr_seat_create(dpy, "seat0");

	wl_list_init(&last_cursor_surface_destroy_listener.link);
	wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
	wl_signal_add(&seat->events.request_set_selection, &request_set_sel);
	wl_signal_add(&seat->events.request_set_primary_selection,
				  &request_set_psel);
	wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
	wl_signal_add(&seat->events.start_drag, &start_drag);

	kb_group = createkeyboardgroup();
	last_active_keyboard = kb_group->keyboard;
	wl_list_init(&kb_group->destroy.link);

	keyboard_shortcuts_inhibit = wlr_keyboard_shortcuts_inhibit_v1_create(dpy);
	wl_signal_add(&keyboard_shortcuts_inhibit->events.new_inhibitor,
				  &keyboard_shortcuts_inhibit_new_inhibitor);

	output_mgr = wlr_output_manager_v1_create(dpy);
	wl_signal_add(&output_mgr->events.apply, &output_mgr_apply);
	wl_signal_add(&output_mgr->events.test, &output_mgr_test);

	wlr_scene_set_blur_data(
		scene, config.blur_params.num_passes, config.blur_params.radius,
		config.blur_params.noise, config.blur_params.brightness,
		config.blur_params.contrast, config.blur_params.saturation);

	/* create text_input-, and input_method-protocol relevant globals */
	input_method_manager = wlr_input_method_manager_v2_create(dpy);
	text_input_manager = wlr_text_input_manager_v3_create(dpy);

	mango_input_method_relay = mango_im_relay_create();

	drm_lease_manager = wlr_drm_lease_v1_manager_create(dpy, backend);
	if (drm_lease_manager) {
		wl_signal_add(&drm_lease_manager->events.request, &drm_lease_request);
	} else {
		mango_error(true, WLR_DEBUG,
					"Failed to create wlr_drm_lease_device_v1.");
		mango_error(true, WLR_INFO, "VR will not be available.");
	}

	// 创建顶层管理句柄
	foreign_toplevel_manager = wlr_foreign_toplevel_manager_v1_create(dpy);
	struct wlr_xdg_foreign_registry *foreign_registry =
		wlr_xdg_foreign_registry_create(dpy);
	wlr_xdg_foreign_v1_create(dpy, foreign_registry);
	wlr_xdg_foreign_v2_create(dpy, foreign_registry);

	// ext-workspace协议
	workspaces_init();
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	xwayland =
		wlr_xwayland_create(dpy, compositor, !config.xwayland_persistence);
	if (xwayland) {
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		mango_error(
			true, WLR_ERROR,
			"failed to setup XWayland X server, continuing without it\n");
	}
	sync_keymap = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
										  synckeymap, NULL);
#endif
}

int32_t main(int32_t argc, char *argv[]) {
	char *startup_cmd = NULL;
	int32_t c;
	int readiness_fd = 0;

	while ((c = getopt(argc, argv, "s:c:r:hdvp")) != -1) {
		if (c == 's') {
			startup_cmd = optarg;
		} else if (c == 'd') {
			cli_debug_log = true;
		} else if (c == 'v') {
			printf("mango " VERSION "\n");
			return EXIT_SUCCESS;
		} else if (c == 'c') {
			snprintf(cli_config_path, sizeof(cli_config_path), "%s", optarg);
		} else if (c == 'p') {
			return parse_config() ? EXIT_SUCCESS : EXIT_FAILURE;
		} else if (c == 'r') {
			readiness_fd = atoi(optarg);
			if (readiness_fd < 3) {
				goto usage;
			}
		} else {
			goto usage;
		}
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications
	 * socket
	 */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd, readiness_fd);
	cleanup();
	return EXIT_SUCCESS;
usage:
	printf("Usage: mango [OPTIONS]\n"
		   "\n"
		   "Options:\n"
		   "  -v             Show mango version\n"
		   "  -d             Enable debug log\n"
		   "  -c <file>      Use custom configuration file\n"
		   "  -s <command>   Execute startup command\n"
		   "  -r <fdnum>     When WM is ready, write '\\n' to the given file "
		   "descriptor and close it. fdnum >= 3\n"
		   "  -p             Check configuration file error\n");
	return EXIT_SUCCESS;
}
