/*
 * See LICENSE file for copyright and license details.
 */
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "wlr/util/box.h"
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/fx/blur_data.h>
#include <scenefx/types/fx/clipped_region.h>
#include <scenefx/types/fx/corner_location.h>
#include <scenefx/types/wlr_scene.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/libinput.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
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
#include "common/util.h"

/* macros */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define GEZERO(A) ((A) >= 0 ? (A) : 0)
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define INSIDEMON(A)                                                           \
	(A->geom.x >= A->mon->m.x && A->geom.y >= A->mon->m.y &&                   \
	 A->geom.x + A->geom.width <= A->mon->m.x + A->mon->m.width &&             \
	 A->geom.y + A->geom.height <= A->mon->m.y + A->mon->m.height)
#define ISTILED(A)                                                             \
	(A && !(A)->isfloating && !(A)->isminied && !(A)->iskilling &&             \
	 !(A)->ismaxmizescreen && !(A)->isfullscreen)
#define VISIBLEON(C, M)                                                        \
	((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define ISFULLSCREEN(A)                                                        \
	((A)->isfullscreen || (A)->ismaxmizescreen ||                              \
	 (A)->overview_ismaxmizescreenbak || (A)->overview_isfullscreenbak)
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

/* enums */
enum { VERTICAL, HORIZONTAL };
enum { SWIPE_UP, SWIPE_DOWN, SWIPE_LEFT, SWIPE_RIGHT };
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11 };					/* client types */
enum { AxisUp, AxisDown, AxisLeft, AxisRight };		// 滚轮滚动的方向
enum {
	LyrBg,
	LyrBlur,
	LyrBottom,
	LyrTile,
	LyrFloat,
	LyrTop,
	LyrFadeOut,
	LyrFSorOverTop,
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
enum { NONE, OPEN, MOVE, CLOSE, TAG };
enum { UNFOLD, FOLD, INVALIDFOLD };
struct dvec2 {
	double x, y;
};

struct ivec2 {
	int x, y, width, height;
};

typedef struct {
	int i;
	int i2;
	float f;
	float f2;
	char *v;
	char *v2;
	char *v3;
	unsigned int ui;
	unsigned int ui2;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button; // 鼠标按键

typedef struct {
	unsigned int mod;
	unsigned int dir;
	void (*func)(const Arg *);
	const Arg arg;
} Axis;

typedef struct {
	struct wl_list link;
	struct wlr_input_device *wlr_device;
	struct libinput_device *libinput_device;
	struct wl_listener destroy_listener; // 用于监听设备销毁事件
	void *device_data;					 // 新增：指向设备特定数据（如 Switch）
} InputDevice;

typedef struct {
	struct wl_list link;
	struct wlr_switch *wlr_switch;
	struct wl_listener toggle;
	InputDevice *input_dev;
} Switch;

struct dwl_animation {
	bool should_animate;
	bool running;
	bool tagining;
	bool tagouted;
	bool tagouting;
	bool begin_fade_in;
	bool tag_from_rule;
	unsigned int total_frames;
	unsigned int passed_frames;
	unsigned int duration;
	struct wlr_box initial;
	struct wlr_box current;
	int action;
};

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
struct wlr_foreign_toplevel_handle_v1;

typedef struct {
	float width_scale;
	float height_scale;
	int width;
	int height;
	double percent;
	float opacity;
	enum corner_location corner_location;
	bool should_scale;
} BufferData;

typedef struct Client Client;
struct Client {
	/* Must keep these three elements in this order */
	unsigned int type; /* XDGShell or X11* */
	struct wlr_box geom, pending, float_geom, animainit_geom,
		overview_backup_geom, current; /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border; /* top, bottom, left, right */
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_tree *scene_surface;
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
#endif
	unsigned int bw;
	unsigned int tags, oldtags, mini_restore_tag;
	bool dirty;
	unsigned int configure_serial;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int isfloating, isurgent, isfullscreen, isfakefullscreen,
		need_float_size_reduce, isminied, isoverlay, isnosizehint,
		ignore_maximize, ignore_minimize;
	int ismaxmizescreen;
	int overview_backup_bw;
	int fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
		fullscreen_backup_h;
	int overview_isfullscreenbak, overview_ismaxmizescreenbak,
		overview_isfloatingbak;

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;

	const char *animation_type_open;
	const char *animation_type_close;
	int is_in_scratchpad;
	int iscustomsize;
	int is_scratchpad_show;
	int isglobal;
	int isnoborder;
	int isopensilent;
	int istagsilent;
	int iskilling;
	int istagswitching;
	int isnamedscratchpad;
	bool is_pending_open_animation;
	bool is_restoring_from_ov;
	float scroller_proportion;
	bool need_output_flush;
	struct dwl_animation animation;
	int isterm, noswallow;
	pid_t pid;
	Client *swallowing, *swallowedby;
	bool is_clip_to_hide;
	bool drag_to_tile;
	bool scratchpad_switching_mon;
	bool fake_no_border;
	int nofadein;
	int nofadeout;
	int no_force_center;
	int isunglobal;
	float focused_opacity;
	float unfocused_opacity;
	char oldmonname[128];
	int noblur;
};

typedef struct {
	struct wl_list link;
	struct wl_resource *resource;
	Monitor *mon;
} DwlIpcOutput;

typedef struct {
	unsigned int mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wlr_keyboard_group *wlr_group;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	unsigned int mods;			 /* invalid if nsyms == 0 */
	unsigned int keycode;
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} KeyboardGroup;

typedef struct {
	struct wl_list link;
	struct wlr_keyboard *wlr_keyboard;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	unsigned int mods;			 /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} Keyboard;

typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* LayerShell */
	struct wlr_box geom, current, pending, animainit_geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_shadow *shadow;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	struct wl_list fadeout_link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;

	struct dwl_animation animation;
	bool dirty;
	int noblur;
	int noanim;
	int noshadow;
	char *animation_type_open;
	char *animation_type_close;
	bool need_output_flush;
} LayerSurface;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
	const char *name;
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m;		  /* monitor area, layout-relative */
	struct wlr_box w;		  /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	const Layout *lt;
	unsigned int seltags;
	unsigned int tagset[2];
	double mfact;
	int nmaster;

	struct wl_list dwl_ipc_outputs;
	int gappih; /* horizontal gap between windows */
	int gappiv; /* vertical gap between windows */
	int gappoh; /* horizontal outer gaps */
	int gappov; /* vertical outer gaps */
	Pertag *pertag;
	Client *sel, *prevsel;
	int isoverview;
	int is_in_hotarea;
	int gamma_lut_changed;
	int asleep;
	unsigned int visible_clients;
	unsigned int visible_tiling_clients;
	struct wlr_scene_optimized_blur *blur;
	char last_surface_ws_name[256];
	struct wlr_ext_workspace_group_handle_v1 *ext_group;
};

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	const char *id;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isfullscreen;
	float scroller_proportion;
	const char *animation_type_open;
	const char *animation_type_close;
	int isnoborder;
	int monitor;
	unsigned int width;
	unsigned int height;
} Rule;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

/* function declarations */
static void applybounds(
	Client *c,
	struct wlr_box *bbox); // 设置边界规则,能让一些窗口拥有比较适合的大小
static void applyrules(Client *c); // 窗口规则应用,应用config.h中定义的窗口规则
static void
arrange(Monitor *m,
		bool want_animation); // 布局函数,让窗口俺平铺规则移动和重置大小
static void arrangelayer(Monitor *m, struct wl_list *list,
						 struct wlr_box *usable_area, int exclusive);
static void arrangelayers(Monitor *m);
static char *get_autostart_path(char *, unsigned int); // 自启动命令执行
static void axisnotify(struct wl_listener *listener,
					   void *data); // 滚轮事件处理
static void buttonpress(struct wl_listener *listener,
						void *data); // 鼠标按键事件处理
static int ongesture(struct wlr_pointer_swipe_end_event *event);
static void swipe_begin(struct wl_listener *listener, void *data);
static void swipe_update(struct wl_listener *listener, void *data);
static void swipe_end(struct wl_listener *listener, void *data);
static void pinch_begin(struct wl_listener *listener, void *data);
static void pinch_update(struct wl_listener *listener, void *data);
static void pinch_end(struct wl_listener *listener, void *data);
static void hold_begin(struct wl_listener *listener, void *data);
static void hold_end(struct wl_listener *listener, void *data);
static void checkidleinhibitor(struct wlr_surface *exclude);
static void cleanup(void); // 退出清理
static void cleanupkeyboard(struct wl_listener *listener,
							void *data);						  // 退出清理
static void cleanupmon(struct wl_listener *listener, void *data); // 退出清理
static void closemon(Monitor *m);
static void cleanuplisteners(void);
static void toggle_hotarea(int x_root, int y_root); // 触发热区
static void maplayersurfacenotify(struct wl_listener *listener, void *data);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void commitnotify(struct wl_listener *listener, void *data);
static void createdecoration(struct wl_listener *listener, void *data);
static void createidleinhibitor(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_keyboard *keyboard);
static void requestmonstate(struct wl_listener *listener, void *data);
static void createlayersurface(struct wl_listener *listener, void *data);
static void createlocksurface(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createnotify(struct wl_listener *listener, void *data);
static void createpointer(struct wlr_pointer *pointer);
static void configure_pointer(struct libinput_device *device);
static void destroyinputdevice(struct wl_listener *listener, void *data);
static void createswitch(struct wlr_switch *switch_device);
static void switch_toggle(struct wl_listener *listener, void *data);
static void createpointerconstraint(struct wl_listener *listener, void *data);
static void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
static void commitpopup(struct wl_listener *listener, void *data);
static void createpopup(struct wl_listener *listener, void *data);
static void cursorframe(struct wl_listener *listener, void *data);
static void cursorwarptohint(void);
static void destroydecoration(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void destroyidleinhibitor(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void destroylock(SessionLock *lock, int unlocked);
static void destroylocksurface(struct wl_listener *listener, void *data);
static void destroynotify(struct wl_listener *listener, void *data);
static void destroypointerconstraint(struct wl_listener *listener, void *data);
static void destroysessionlock(struct wl_listener *listener, void *data);
static void destroykeyboardgroup(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void setcursorshape(struct wl_listener *listener, void *data);

static void focusclient(Client *c, int lift);

static void setborder_color(Client *c);
static Client *focustop(Monitor *m);
static void fullscreennotify(struct wl_listener *listener, void *data);
static void gpureset(struct wl_listener *listener, void *data);

static int keyrepeat(void *data);

static void inputdevice(struct wl_listener *listener, void *data);
static int keybinding(unsigned int mods, xkb_keysym_t sym,
					  unsigned int keycode);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static bool keypressglobal(struct wlr_surface *last_surface,
						   struct wlr_keyboard *keyboard,
						   struct wlr_keyboard_key_event *event,
						   unsigned int mods, xkb_keysym_t keysym,
						   unsigned int keycode);
static void locksession(struct wl_listener *listener, void *data);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void minimizenotify(struct wl_listener *listener, void *data);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionnotify(unsigned int time, struct wlr_input_device *device,
						 double sx, double sy, double sx_unaccel,
						 double sy_unaccel);
static void motionrelative(struct wl_listener *listener, void *data);

static void reset_foreign_tolevel(Client *c);
static void remove_foreign_topleve(Client *c);
static void add_foreign_topleve(Client *c);
static void exchange_two_client(Client *c1, Client *c2);
static void outputmgrapply(struct wl_listener *listener, void *data);
static void outputmgrapplyortest(struct wlr_output_configuration_v1 *config,
								 int test);
static void outputmgrtest(struct wl_listener *listener, void *data);
static void pointerfocus(Client *c, struct wlr_surface *surface, double sx,
						 double sy, unsigned int time);
static void printstatus(void);
static void quitsignal(int signo);
static void powermgrsetmode(struct wl_listener *listener, void *data);
static void rendermon(struct wl_listener *listener, void *data);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo, int interact);
static void run(char *startup_cmd);
static void setcursor(struct wl_listener *listener, void *data);
static void setfloating(Client *c, int floating);
static void setfakefullscreen(Client *c, int fakefullscreen);
static void setfullscreen(Client *c, int fullscreen);
static void setmaxmizescreen(Client *c, int maxmizescreen);
static void reset_maxmizescreen_size(Client *c);
static void setgaps(int oh, int ov, int ih, int iv);

static void setmon(Client *c, Monitor *m, unsigned int newtags, bool focus);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void startdrag(struct wl_listener *listener, void *data);

static void unlocksession(struct wl_listener *listener, void *data);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void unmapnotify(struct wl_listener *listener, void *data);
static void updatemons(struct wl_listener *listener, void *data);
static void updatetitle(struct wl_listener *listener, void *data);
static void urgent(struct wl_listener *listener, void *data);
static void view(const Arg *arg, bool want_animation);

static void handlesig(int signo);

static void virtualkeyboard(struct wl_listener *listener, void *data);
static void virtualpointer(struct wl_listener *listener, void *data);
static void warp_cursor(const Client *c);
static Monitor *xytomon(double x, double y);
static void xytonode(double x, double y, struct wlr_surface **psurface,
					 Client **pc, LayerSurface **pl, double *nx, double *ny);
static void clear_fullscreen_flag(Client *c);
static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *termforwin(Client *w);
static void swallow(Client *c, Client *w);

static void warp_cursor_to_selmon(Monitor *m);
unsigned int want_restore_fullscreen(Client *target_client);
static void overview_restore(Client *c, const Arg *arg);
static void overview_backup(Client *c);
static int applyrulesgeom(Client *c);
static void set_minimized(Client *c);

static void show_scratchpad(Client *c);
static void show_hide_client(Client *c);
static void tag_client(const Arg *arg, Client *target_client);

static struct wlr_box setclient_coordinate_center(Client *c,
												  struct wlr_box geom,
												  int offsetx, int offsety);
static unsigned int get_tags_first_tag(unsigned int tags);

static struct wlr_output_mode *
get_nearest_output_mode(struct wlr_output *output, int width, int height,
						float refresh);

static void client_commit(Client *c);
static void layer_commit(LayerSurface *l);
static void apply_border(Client *c);
static void client_set_opacity(Client *c, double opacity);
static void init_baked_points(void);
static void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int sx,
									   int sy, void *data);

static Client *direction_select(const Arg *arg);
static void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
						bool changefocus);

static void buffer_set_effect(Client *c, BufferData buffer_data);
static void snap_scene_buffer_apply_effect(struct wlr_scene_buffer *buffer,
										   int sx, int sy, void *data);
static void client_set_pending_state(Client *c);
static void layer_set_pending_state(LayerSurface *l);
static void set_rect_size(struct wlr_scene_rect *rect, int width, int height);
static Client *center_tiled_select(Monitor *m);
static void handlecursoractivity(void);
static int hidecursor(void *data);
static bool check_hit_no_border(Client *c);
static void reset_keyboard_layout(void);
static void client_update_oldmonname_record(Client *c, Monitor *m);
static void pending_kill_client(Client *c);
static unsigned int get_tags_first_tag_num(unsigned int source_tags);
static void set_layer_open_animaiton(LayerSurface *l, struct wlr_box geo);
static void init_fadeout_layers(LayerSurface *l);
static void layer_actual_size(LayerSurface *l, unsigned int *width,
							  unsigned int *height);
static void get_layer_target_geometry(LayerSurface *l,
									  struct wlr_box *target_box);
static void scene_buffer_apply_effect(struct wlr_scene_buffer *buffer, int sx,
									  int sy, void *data);
static double find_animation_curve_at(double t, int type);

static void apply_opacity_to_rect_nodes(Client *c, struct wlr_scene_node *node,
										double animation_passed);
static enum corner_location set_client_corner_location(Client *c);
static double all_output_frame_duration_ms();
static struct wlr_scene_tree *
wlr_scene_tree_snapshot(struct wlr_scene_node *node,
						struct wlr_scene_tree *parent);
static bool is_scroller_layout(Monitor *m);
static void create_output(struct wlr_backend *backend, void *data);
static const char *get_layout_abbr(const char *full_name);
static void apply_named_scratchpad(Client *target_client);
static Client *get_client_by_id_or_title(const char *arg_id,
										 const char *arg_title);
static bool switch_scratchpad_client_state(Client *c);
static bool check_trackpad_disabled(struct wlr_pointer *pointer);
static unsigned int get_tag_status(unsigned int tag, Monitor *m);
static void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state);

#include "data/static_keymap.h"
#include "dispatch/bind_declare.h"
#include "layout/layout.h"

/* variables */
static const char broken[] = "broken";
static pid_t child_pid = -1;
static int locked;
static unsigned int locked_mods = 0;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wl_event_loop *event_loop;
static struct wlr_relative_pointer_manager_v1 *pointer_manager;
static struct wlr_backend *backend;
static struct wlr_backend *headless_backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;

static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_activation_v1 *activation;
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
static struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
static struct wlr_output_power_manager_v1 *power_mgr;
static struct wlr_pointer_gestures_v1 *pointer_gestures;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wlr_session *session;

static struct wlr_scene_rect *root_bg;
static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static const int layermap[] = {LyrBg, LyrBottom, LyrTop, LyrOverlay};
static struct wlr_scene_tree *drag_icon;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;

static struct wlr_seat *seat;
static KeyboardGroup *kb_group;
static struct wl_list keyboards;
static struct wl_list inputdevices;
static unsigned int cursor_mode;
static Client *grabc;
static int grabcx, grabcy; /* client-relative */

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;
static struct wl_list mons;
static Monitor *selmon;

static int enablegaps = 1; /* enables gaps, used by togglegaps */
static int axis_apply_time = 0;
static int axis_apply_dir = 0;
static int scroller_focus_lock = 0;

static unsigned int swipe_fingers = 0;
static double swipe_dx = 0;
static double swipe_dy = 0;

bool render_border = true;

struct dvec2 *baked_points_move;
struct dvec2 *baked_points_open;
struct dvec2 *baked_points_tag;
struct dvec2 *baked_points_close;

static struct wl_event_source *hide_source;
static bool cursor_hidden = false;
static bool tag_combo = false;

static struct {
	enum wp_cursor_shape_device_v1_shape shape;
	struct wlr_surface *surface;
	int hotspot_x;
	int hotspot_y;
} last_cursor;

#include "client/client.h"
#include "config/preset.h"

struct Pertag {
	unsigned int curtag, prevtag;	 /* current and previous tag */
	int nmasters[LENGTH(tags) + 1];	 /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1];	 /* mfacts per tag */
	float smfacts[LENGTH(tags) + 1]; /* smfacts per tag */
	bool no_hide[LENGTH(tags) + 1];	 /* no_hide per tag */
	const Layout
		*ltidxs[LENGTH(tags) + 1]; /* matrix of tags and layouts indexes  */
};

static struct wl_listener cursor_axis = {.notify = axisnotify};
static struct wl_listener cursor_button = {.notify = buttonpress};
static struct wl_listener cursor_frame = {.notify = cursorframe};
static struct wl_listener cursor_motion = {.notify = motionrelative};
static struct wl_listener cursor_motion_absolute = {.notify = motionabsolute};
static struct wl_listener gpu_reset = {.notify = gpureset};
static struct wl_listener layout_change = {.notify = updatemons};
static struct wl_listener new_idle_inhibitor = {.notify = createidleinhibitor};
static struct wl_listener new_input_device = {.notify = inputdevice};
static struct wl_listener new_virtual_keyboard = {.notify = virtualkeyboard};
static struct wl_listener new_virtual_pointer = {.notify = virtualpointer};
static struct wl_listener new_pointer_constraint = {
	.notify = createpointerconstraint};
static struct wl_listener new_output = {.notify = createmon};
static struct wl_listener new_xdg_toplevel = {.notify = createnotify};
static struct wl_listener new_xdg_popup = {.notify = createpopup};
static struct wl_listener new_xdg_decoration = {.notify = createdecoration};
static struct wl_listener new_layer_surface = {.notify = createlayersurface};
static struct wl_listener output_mgr_apply = {.notify = outputmgrapply};
static struct wl_listener output_mgr_test = {.notify = outputmgrtest};
static struct wl_listener output_power_mgr_set_mode = {.notify =
														   powermgrsetmode};
static struct wl_listener request_activate = {.notify = urgent};
static struct wl_listener request_cursor = {.notify = setcursor};
static struct wl_listener request_set_psel = {.notify = setpsel};
static struct wl_listener request_set_sel = {.notify = setsel};
static struct wl_listener request_set_cursor_shape = {.notify = setcursorshape};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};
static struct wl_listener start_drag = {.notify = startdrag};
static struct wl_listener new_session_lock = {.notify = locksession};

#ifdef XWAYLAND
static void activatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
static void setgeometrynotify(struct wl_listener *listener, void *data);
static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
#endif

#include "animation/client.h"
#include "animation/common.h"
#include "animation/layer.h"
#include "animation/tag.h"
#include "config/parse_config.h"
#include "dispatch/bind_define.h"
#include "ext-protocol/all.h"
#include "fetch/fetch.h"
#include "layout/horizontal.h"
#include "layout/vertical.h"

void client_change_mon(Client *c, Monitor *m) {
	setmon(c, m, c->tags, true);
	reset_foreign_tolevel(c);
	if (c->isfloating) {
		c->float_geom = c->geom = setclient_coordinate_center(c, c->geom, 0, 0);
	}
}

void applybounds(Client *c, struct wlr_box *bbox) {
	/* set minimum possible */
	c->geom.width = MAX(1 + 2 * (int)c->bw, c->geom.width);
	c->geom.height = MAX(1 + 2 * (int)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

/*清除全屏标志,还原全屏时清0的border*/
void clear_fullscreen_flag(Client *c) {
	if (c->isfullscreen) {
		setfullscreen(c, false);
	}

	if (c->ismaxmizescreen) {
		setmaxmizescreen(c, 0);
	}
}

void minimized(const Arg *arg) {

	if (selmon && selmon->isoverview)
		return;

	if (selmon->sel && !selmon->sel->isminied) {
		set_minimized(selmon->sel);
	}
}

void show_scratchpad(Client *c) {
	c->is_scratchpad_show = 1;
	if (c->isfullscreen || c->ismaxmizescreen) {
		c->isfullscreen = 0; // 清除窗口全屏标志
		c->ismaxmizescreen = 0;
		c->bw = c->isnoborder ? 0 : borderpx;
	}

	/* return if fullscreen */
	if (!c->isfloating) {
		setfloating(c, 1);
		c->geom.width = c->iscustomsize
							? c->float_geom.width
							: c->mon->w.width * scratchpad_width_ratio;
		c->geom.height = c->iscustomsize
							 ? c->float_geom.height
							 : c->mon->w.height * scratchpad_height_ratio;
		// 重新计算居中的坐标
		c->float_geom = c->geom = c->animainit_geom = c->animation.current =
			setclient_coordinate_center(c, c->geom, 0, 0);
		c->iscustomsize = 1;
		resize(c, c->geom, 0);
	}

	c->oldtags = c->mon->tagset[c->mon->seltags];
	wl_list_remove(&c->link);					  // 从原来位置移除
	wl_list_insert(clients.prev->next, &c->link); // 插入开头
	show_hide_client(c);
	setborder_color(c);
}

void client_update_oldmonname_record(Client *c, Monitor *m) {
	if (!c || c->iskilling || !client_surface(c)->mapped)
		return;
	memset(c->oldmonname, 0, sizeof(c->oldmonname));
	strncpy(c->oldmonname, m->wlr_output->name, sizeof(c->oldmonname) - 1);
	c->oldmonname[sizeof(c->oldmonname) - 1] = '\0';
}

void swallow(Client *c, Client *w) {
	c->bw = w->bw;
	c->isfloating = w->isfloating;
	c->isurgent = w->isurgent;
	c->isfullscreen = w->isfullscreen;
	c->ismaxmizescreen = w->ismaxmizescreen;
	c->isminied = w->isminied;
	c->is_in_scratchpad = w->is_in_scratchpad;
	c->is_scratchpad_show = w->is_scratchpad_show;
	c->tags = w->tags;
	c->geom = w->geom;
	c->float_geom = w->float_geom;
	c->scroller_proportion = w->scroller_proportion;
	wl_list_insert(&w->link, &c->link);
	wl_list_insert(&w->flink, &c->flink);

	if (w->foreign_toplevel)
		remove_foreign_topleve(w);

	wlr_scene_node_set_enabled(&w->scene->node, false);
	wlr_scene_node_set_enabled(&c->scene->node, true);
	wlr_scene_node_set_enabled(&c->scene_surface->node, true);

	if (!c->foreign_toplevel && c->mon)
		add_foreign_toplevel(c);

	if (c->isminied && c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,
													 false);
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, true);
	}
}

bool switch_scratchpad_client_state(Client *c) {

	if (scratchpad_cross_monitor && selmon && c->mon != selmon &&
		c->is_in_scratchpad) {
		// 保存原始monitor用于尺寸计算
		Monitor *oldmon = c->mon;
		c->scratchpad_switching_mon = true;
		c->mon = selmon;
		reset_foreign_tolevel(c);
		client_update_oldmonname_record(c, selmon);

		// 根据新monitor调整窗口尺寸
		c->float_geom.width =
			(int)(c->float_geom.width * c->mon->w.width / oldmon->w.width);
		c->float_geom.height =
			(int)(c->float_geom.height * c->mon->w.height / oldmon->w.height);

		c->float_geom = setclient_coordinate_center(c, c->float_geom, 0, 0);

		// 只有显示状态的scratchpad才需要聚焦和返回true
		if (c->is_scratchpad_show) {
			c->tags = get_tags_first_tag(selmon->tagset[selmon->seltags]);
			resize(c, c->float_geom, 0);
			arrange(selmon, false);
			focusclient(c, true);
			c->scratchpad_switching_mon = false;
			return true;
		} else {
			resize(c, c->float_geom, 0);
			c->scratchpad_switching_mon = false;
		}
	}

	if (c->is_in_scratchpad && c->is_scratchpad_show &&
		(selmon->tagset[selmon->seltags] & c->tags) == 0) {
		unsigned int target =
			get_tags_first_tag(selmon->tagset[selmon->seltags]);
		tag_client(&(Arg){.ui = target}, c);
		return true;
	} else if (c->is_in_scratchpad && c->is_scratchpad_show &&
			   (selmon->tagset[selmon->seltags] & c->tags) != 0) {
		set_minimized(c);
		return true;
	} else if (c && c->is_in_scratchpad && !c->is_scratchpad_show) {
		show_scratchpad(c);
		return true;
	}

	return false;
}

void apply_named_scratchpad(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {

		if (!scratchpad_cross_monitor && c->mon != selmon) {
			continue;
		}

		if (single_scratchpad && c->is_in_scratchpad && c->is_scratchpad_show &&
			c != target_client) {
			set_minimized(c);
		}
	}

	if (!target_client->is_in_scratchpad) {
		set_minimized(target_client);
		switch_scratchpad_client_state(target_client);
	} else
		switch_scratchpad_client_state(target_client);
}

void gpureset(struct wl_listener *listener, void *data) {
	struct wlr_renderer *old_drw = drw;
	struct wlr_allocator *old_alloc = alloc;
	struct Monitor *m;

	wlr_log(WLR_DEBUG, "gpu reset");

	if (!(drw = fx_renderer_create(backend)))
		die("couldn't recreate renderer");

	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't recreate allocator");

	wl_list_remove(&gpu_reset.link);
	wl_signal_add(&drw->events.lost, &gpu_reset);

	wlr_compositor_set_renderer(compositor, drw);

	wl_list_for_each(m, &mons, link) {
		wlr_output_init_render(m->wlr_output, alloc, drw);
	}

	wlr_allocator_destroy(old_alloc);
	wlr_renderer_destroy(old_drw);
}

void handlesig(int signo) {
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	else if (signo == SIGINT || signo == SIGTERM)
		quit(NULL);
}

void toggle_hotarea(int x_root, int y_root) {
	// 左下角热区坐标计算,兼容多显示屏
	Arg arg = {0};

	// 在刚启动的时候,selmon为NULL,但鼠标可能已经处于热区,
	// 必须判断避免奔溃
	if (!selmon)
		return;

	if (grabc)
		return;

	unsigned hx = selmon->m.x + hotarea_size;
	unsigned hy = selmon->m.y + selmon->m.height - hotarea_size;

	if (enable_hotarea == 1 && selmon->is_in_hotarea == 0 && y_root > hy &&
		x_root < hx && x_root >= selmon->m.x &&
		y_root <= (selmon->m.y + selmon->m.height)) {
		toggleoverview(&arg);
		selmon->is_in_hotarea = 1;
	} else if (enable_hotarea == 1 && selmon->is_in_hotarea == 1 &&
			   (y_root <= hy || x_root >= hx || x_root < selmon->m.x ||
				y_root > (selmon->m.y + selmon->m.height))) {
		selmon->is_in_hotarea = 0;
	}
}

static void apply_rule_properties(Client *c, const ConfigWinRule *r) {
	APPLY_INT_PROP(c, r, isterm);
	APPLY_INT_PROP(c, r, noswallow);
	APPLY_INT_PROP(c, r, nofadein);
	APPLY_INT_PROP(c, r, nofadeout);
	APPLY_INT_PROP(c, r, no_force_center);
	APPLY_INT_PROP(c, r, isfloating);
	APPLY_INT_PROP(c, r, isfullscreen);
	APPLY_INT_PROP(c, r, isnoborder);
	APPLY_INT_PROP(c, r, isopensilent);
	APPLY_INT_PROP(c, r, istagsilent);
	APPLY_INT_PROP(c, r, isnamedscratchpad);
	APPLY_INT_PROP(c, r, isglobal);
	APPLY_INT_PROP(c, r, isoverlay);
	APPLY_INT_PROP(c, r, ignore_maximize);
	APPLY_INT_PROP(c, r, ignore_minimize);
	APPLY_INT_PROP(c, r, isnosizehint);
	APPLY_INT_PROP(c, r, isunglobal);
	APPLY_INT_PROP(c, r, noblur);

	APPLY_FLOAT_PROP(c, r, scroller_proportion);
	APPLY_FLOAT_PROP(c, r, focused_opacity);
	APPLY_FLOAT_PROP(c, r, unfocused_opacity);

	APPLY_STRING_PROP(c, r, animation_type_open);
	APPLY_STRING_PROP(c, r, animation_type_close);
}

int applyrulesgeom(Client *c) {
	/* rule matching */
	const char *appid, *title;
	ConfigWinRule *r;
	int hit = 0;
	int ji;

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (ji = 0; ji < config.window_rules_count; ji++) {
		if (config.window_rules_count < 1)
			break;
		r = &config.window_rules[ji];

		if (!is_window_rule_matches(r, appid, title))
			continue;

		c->geom.width = r->width > 0 ? r->width : c->geom.width;
		c->geom.height = r->height > 0 ? r->height : c->geom.height;

		if (!c->isnosizehint)
			client_set_size_bound(c);

		// 重新计算居中的坐标
		if (r->offsetx != 0 || r->offsety != 0 || r->width > 0 || r->height > 0)
			c->geom =
				setclient_coordinate_center(c, c->geom, r->offsetx, r->offsety);
		if (r->height > 0 || r->width > 0 || r->offsetx != 0 ||
			r->offsety != 0) {
			hit = 1;
		} else {
			hit = 0;
		}
	}
	return hit;
}

void applyrules(Client *c) {
	/* rule matching */
	const char *appid, *title;
	unsigned int i, newtags = 0;
	const ConfigWinRule *r;
	Monitor *mon = selmon, *m;
	Client *fc;
	bool hit_rule_pos = false;

	c->isfloating = client_is_float_type(c);
	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (i = 0; i < config.window_rules_count; i++) {

		r = &config.window_rules[i];

		// rule matching
		if (!is_window_rule_matches(r, appid, title))
			continue;

		// set general properties
		apply_rule_properties(c, r);

		// set tags
		newtags |= (r->tags > 0) ? r->tags : 0;

		// set monitor of client
		wl_list_for_each(m, &mons, link) {
			if (regex_match(r->monitor, m->wlr_output->name)) {
				mon = m;
			}
		}

		if (c->isnamedscratchpad) {
			c->isfloating = 1;
		}

		// set geometry of floating client

		if (r->width > 0)
			c->float_geom.width = r->width;
		if (r->height > 0)
			c->float_geom.height = r->height;

		if (r->offsetx || r->offsety || r->width > 0 || r->height > 0) {
			hit_rule_pos = true;
			c->iscustomsize = 1;
			c->float_geom = setclient_coordinate_center(c, c->float_geom,
														r->offsetx, r->offsety);
		}
		if (c->isfloating) {
			c->geom = c->float_geom.width > 0 && c->float_geom.height > 0
						  ? c->float_geom
						  : c->geom;
			if (!c->isnosizehint)
				client_set_size_bound(c);
		}
	}

	// if no geom rule hit and is normal winodw, use the center pos and record
	// the hit size
	if (!hit_rule_pos &&
		(!client_is_x11(c) || !client_should_ignore_focus(c))) {
		c->float_geom = c->geom = setclient_coordinate_center(c, c->geom, 0, 0);
	}

	/*-----------------------apply rule action-------------------------*/

	// rule action only apply after map not apply in the init commit
	if (!client_surface(c)->mapped)
		return;

	// apply swallow rule
	c->pid = client_get_pid(c);
	if (!c->noswallow && !c->isfloating && !client_is_float_type(c) &&
		!c->surface.xdg->initial_commit) {
		Client *p = termforwin(c);
		if (p) {
			c->swallowedby = p;
			p->swallowing = c;
			wl_list_remove(&c->link);
			wl_list_remove(&c->flink);
			swallow(c, p);
			wl_list_remove(&p->link);
			wl_list_remove(&p->flink);
			mon = p->mon;
			newtags = p->tags;
		}
	}

	int fullscreen_state_backup = c->isfullscreen || client_wants_fullscreen(c);
	setmon(c, mon, newtags,
		   !c->isopensilent && (!c->istagsilent || !newtags ||
								newtags & mon->tagset[mon->seltags]));

	if (c->mon &&
		!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]) &&
		!c->isopensilent && !c->istagsilent) {
		c->animation.tag_from_rule = true;
		view_in_mon(&(Arg){.ui = c->tags}, true, c->mon, true);
	}

	setfullscreen(c, fullscreen_state_backup);

	/*
	if there is a new non-floating window in the current tag, the fullscreen
	window in the current tag will exit fullscreen and participate in tiling
	*/
	wl_list_for_each(fc, &clients,
					 link) if (fc && fc != c && c->tags & fc->tags &&
							   VISIBLEON(fc, c->mon) && ISFULLSCREEN(fc) &&
							   !c->isfloating) {
		clear_fullscreen_flag(fc);
		arrange(c->mon, false);
	}

	// apply named scratchpad rule
	if (c->isnamedscratchpad) {
		apply_named_scratchpad(c);
	}

	// apply overlay rule
	if (c->isoverlay) {
		wlr_scene_node_reparent(&selmon->sel->scene->node, layers[LyrOverlay]);
		wlr_scene_node_raise_to_top(&selmon->sel->scene->node);
	}
}

void // 17
arrange(Monitor *m, bool want_animation) {
	Client *c;

	if (!m)
		return;

	if (!m->wlr_output->enabled)
		return;

	m->visible_clients = 0;
	m->visible_tiling_clients = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->iskilling)
			continue;

		if (c->mon == m && (c->isglobal || c->isunglobal)) {
			c->tags = m->tagset[m->seltags];
			if (c->mon->sel == NULL)
				focusclient(c, 0);
		}

		if (c->mon == m) {
			if (VISIBLEON(c, m)) {

				m->visible_clients++;
				if (ISTILED(c))
					m->visible_tiling_clients++;

				set_arrange_visible(m, c, want_animation);
			} else {
				set_arrange_hidden(m, c, want_animation);
			}
		}

		if (c->mon == m && c->ismaxmizescreen && !c->animation.tagouted &&
			!c->animation.tagouting && VISIBLEON(c, m)) {
			reset_maxmizescreen_size(c);
		}
	}

	if (m->isoverview) {
		overviewlayout.arrange(m);
	} else {
		m->pertag->ltidxs[m->pertag->curtag]->arrange(m);
	}

	motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
				  int exclusive) {
	LayerSurface *l;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0) ||
			!layer_surface->initialized)
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
											 usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
									l->scene->node.y);
	}
}

void apply_window_snap(Client *c) {
	int snap_up = 99999, snap_down = 99999, snap_left = 99999,
		snap_right = 99999;
	int snap_up_temp = 0, snap_down_temp = 0, snap_left_temp = 0,
		snap_right_temp = 0;
	int snap_up_screen = 0, snap_down_screen = 0, snap_left_screen = 0,
		snap_right_screen = 0;
	int snap_up_mon = 0, snap_down_mon = 0, snap_left_mon = 0,
		snap_right_mon = 0;

	unsigned int cbw = !render_border || c->fake_no_border ? borderpx : 0;
	unsigned int tcbw;
	unsigned int cx, cy, cw, ch, tcx, tcy, tcw, tch;
	cx = c->geom.x + cbw;
	cy = c->geom.y + cbw;
	cw = c->geom.width - 2 * cbw;
	ch = c->geom.height - 2 * cbw;

	Client *tc;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	if (!c->isfloating || !enable_floating_snap)
		return;

	wl_list_for_each(tc, &clients, link) {
		if (tc && tc->isfloating && !tc->iskilling &&
			client_surface(tc)->mapped && VISIBLEON(tc, c->mon)) {

			tcbw = !render_border || tc->fake_no_border ? borderpx : 0;
			tcx = tc->geom.x + tcbw;
			tcy = tc->geom.y + tcbw;
			tcw = tc->geom.width - 2 * tcbw;
			tch = tc->geom.height - 2 * tcbw;

			snap_left_temp = cx - tcx - tcw;
			snap_right_temp = tcx - cx - cw;
			snap_up_temp = cy - tcy - tch;
			snap_down_temp = tcy - cy - ch;

			if (snap_left_temp < snap_left && snap_left_temp >= 0) {
				snap_left = snap_left_temp;
			}
			if (snap_right_temp < snap_right && snap_right_temp >= 0) {
				snap_right = snap_right_temp;
			}
			if (snap_up_temp < snap_up && snap_up_temp >= 0) {
				snap_up = snap_up_temp;
			}
			if (snap_down_temp < snap_down && snap_down_temp >= 0) {
				snap_down = snap_down_temp;
			}
		}
	}

	snap_left_mon = cx - c->mon->m.x;
	snap_right_mon = c->mon->m.x + c->mon->m.width - cx - cw;
	snap_up_mon = cy - c->mon->m.y;
	snap_down_mon = c->mon->m.y + c->mon->m.height - cy - ch;

	if (snap_up_mon >= 0 && snap_up_mon < snap_up)
		snap_up = snap_up_mon;
	if (snap_down_mon >= 0 && snap_down_mon < snap_down)
		snap_down = snap_down_mon;
	if (snap_left_mon >= 0 && snap_left_mon < snap_left)
		snap_left = snap_left_mon;
	if (snap_right_mon >= 0 && snap_right_mon < snap_right)
		snap_right = snap_right_mon;

	snap_left_screen = cx - c->mon->w.x;
	snap_right_screen = c->mon->w.x + c->mon->w.width - cx - cw;
	snap_up_screen = cy - c->mon->w.y;
	snap_down_screen = c->mon->w.y + c->mon->w.height - cy - ch;

	if (snap_up_screen >= 0 && snap_up_screen < snap_up)
		snap_up = snap_up_screen;
	if (snap_down_screen >= 0 && snap_down_screen < snap_down)
		snap_down = snap_down_screen;
	if (snap_left_screen >= 0 && snap_left_screen < snap_left)
		snap_left = snap_left_screen;
	if (snap_right_screen >= 0 && snap_right_screen < snap_right)
		snap_right = snap_right_screen;

	if (snap_left < snap_right && snap_left < snap_distance) {
		c->geom.x = c->geom.x - snap_left;
	}

	if (snap_right <= snap_left && snap_right < snap_distance) {
		c->geom.x = c->geom.x + snap_right;
	}

	if (snap_up < snap_down && snap_up < snap_distance) {
		c->geom.y = c->geom.y - snap_up;
	}

	if (snap_down <= snap_up && snap_down < snap_distance) {
		c->geom.y = c->geom.y + snap_down;
	}

	c->float_geom = c->geom;
	resize(c, c->geom, 0);
}

void reset_exclusive_layer(Monitor *m) {
	LayerSurface *l;
	int i;
	unsigned int layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};

	if (!m)
		return;

	for (i = 0; i < (int)LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(l, &m->layers[layers_above_shell[i]], link) {
			if (locked ||
				l->layer_surface->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE ||
				!l->mapped || l == exclusive_focus)
				continue;
			/* Deactivate the focused client. */
			focusclient(NULL, 0);
			exclusive_focus = l;
			client_notify_enter(l->layer_surface->surface,
								wlr_seat_get_keyboard(seat));
			return;
		}
	}
}

void arrangelayers(Monitor *m) {
	int i;
	struct wlr_box usable_area = m->m;

	if (!m->wlr_output->enabled)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m, false);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	/* Find topmost keyboard interactive layer, if such a layer exists */
	reset_exclusive_layer(m);
}

void // 鼠标滚轮事件
axisnotify(struct wl_listener *listener, void *data) {

	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	struct wlr_keyboard *keyboard, *hard_keyboard;
	unsigned int mods, hard_mods;
	AxisBinding *a;
	int ji;
	unsigned int adir;
	// IDLE_NOTIFY_ACTIVITY;
	handlecursoractivity();
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	hard_keyboard = &kb_group->wlr_group->keyboard;
	hard_mods = hard_keyboard ? wlr_keyboard_get_modifiers(hard_keyboard) : 0;

	keyboard = wlr_seat_get_keyboard(seat);
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	mods = mods | hard_mods;

	if (event->orientation == WL_POINTER_AXIS_VERTICAL_SCROLL)
		adir = event->delta > 0 ? AxisDown : AxisUp;
	else
		adir = event->delta > 0 ? AxisRight : AxisLeft;

	for (ji = 0; ji < config.axis_bindings_count; ji++) {
		if (config.axis_bindings_count < 1)
			break;
		a = &config.axis_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(a->mod) && // 按键一致
			adir == a->dir && a->func) { // 滚轮方向判断一致且处理函数存在
			if (event->time_msec - axis_apply_time > axis_bind_apply_timeout ||
				axis_apply_dir * event->delta < 0) {
				a->func(&a->arg);
				axis_apply_time = event->time_msec;
				axis_apply_dir = event->delta > 0 ? 1 : -1;
				return; // 如果成功匹配就不把这个滚轮事件传送给客户端了
			} else {
				axis_apply_dir = event->delta > 0 ? 1 : -1;
				axis_apply_time = event->time_msec;
				return;
			}
		}
	}

	/* TODO: allow usage of scroll whell for mousebindings, it can be
	 * implemented checking the event's orientation and the delta of the event
	 */
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat, // 滚轮事件发送给客户端也就是窗口
								 event->time_msec, event->orientation,
								 event->delta, event->delta_discrete,
								 event->source, event->relative_direction);
}

int ongesture(struct wlr_pointer_swipe_end_event *event) {
	struct wlr_keyboard *keyboard, *hard_keyboard;
	unsigned int mods, hard_mods;
	const GestureBinding *g;
	unsigned int motion;
	unsigned int adx = (int)round(fabs(swipe_dx));
	unsigned int ady = (int)round(fabs(swipe_dy));
	int handled = 0;
	int ji;

	if (event->cancelled) {
		return handled;
	}

	// Require absolute distance movement beyond a small thresh-hold
	if (adx * adx + ady * ady < swipe_min_threshold * swipe_min_threshold) {
		return handled;
	}

	if (adx > ady) {
		motion = swipe_dx < 0 ? SWIPE_LEFT : SWIPE_RIGHT;
	} else {
		motion = swipe_dy < 0 ? SWIPE_UP : SWIPE_DOWN;
	}

	hard_keyboard = &kb_group->wlr_group->keyboard;
	hard_mods = hard_keyboard ? wlr_keyboard_get_modifiers(hard_keyboard) : 0;

	keyboard = wlr_seat_get_keyboard(seat);
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	mods = mods | hard_mods;

	for (ji = 0; ji < config.gesture_bindings_count; ji++) {
		if (config.gesture_bindings_count < 1)
			break;
		g = &config.gesture_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(g->mod) &&
			swipe_fingers == g->fingers_count && motion == g->motion &&
			g->func) {
			g->func(&g->arg);
			handled = 1;
		}
	}
	return handled;
}

void swipe_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_begin_event *event = data;

	// Forward swipe begin event to client
	wlr_pointer_gestures_v1_send_swipe_begin(pointer_gestures, seat,
											 event->time_msec, event->fingers);
}

void swipe_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_update_event *event = data;

	swipe_fingers = event->fingers;
	// Accumulate swipe distance
	swipe_dx += event->dx;
	swipe_dy += event->dy;

	// Forward swipe update event to client
	wlr_pointer_gestures_v1_send_swipe_update(
		pointer_gestures, seat, event->time_msec, event->dx, event->dy);
}

void swipe_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_end_event *event = data;
	ongesture(event);
	swipe_dx = 0;
	swipe_dy = 0;
	// Forward swipe end event to client
	wlr_pointer_gestures_v1_send_swipe_end(pointer_gestures, seat,
										   event->time_msec, event->cancelled);
}

void pinch_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_begin_event *event = data;

	// Forward pinch begin event to client
	wlr_pointer_gestures_v1_send_pinch_begin(pointer_gestures, seat,
											 event->time_msec, event->fingers);
}

void pinch_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_update_event *event = data;

	// Forward pinch update event to client
	wlr_pointer_gestures_v1_send_pinch_update(
		pointer_gestures, seat, event->time_msec, event->dx, event->dy,
		event->scale, event->rotation);
}

void pinch_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_end_event *event = data;

	// Forward pinch end event to client
	wlr_pointer_gestures_v1_send_pinch_end(pointer_gestures, seat,
										   event->time_msec, event->cancelled);
}

void hold_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_begin_event *event = data;

	// Forward hold begin event to client
	wlr_pointer_gestures_v1_send_hold_begin(pointer_gestures, seat,
											event->time_msec, event->fingers);
}

void hold_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_end_event *event = data;

	// Forward hold end event to client
	wlr_pointer_gestures_v1_send_hold_end(pointer_gestures, seat,
										  event->time_msec, event->cancelled);
}

void place_drag_tile_client(Client *c) {
	Client *tc = NULL;
	Client *closest_client = NULL;
	long min_distant = LONG_MAX;
	long temp_distant;
	int x, y;

	wl_list_for_each(tc, &clients, link) {
		if (tc != c && ISTILED(tc) && VISIBLEON(tc, c->mon)) {
			x = tc->geom.x + (int)(tc->geom.width / 2) - cursor->x;
			y = tc->geom.y + (int)(tc->geom.height / 2) - cursor->y;
			temp_distant = x * x + y * y;
			if (temp_distant < min_distant) {
				min_distant = temp_distant;
				closest_client = tc;
			}
		}
	}
	if (closest_client && closest_client->link.prev != &c->link) {
		wl_list_remove(&c->link);
		c->link.next = &closest_client->link;
		c->link.prev = closest_client->link.prev;
		closest_client->link.prev->next = &c->link;
		closest_client->link.prev = &c->link;
	} else if (closest_client) {
		exchange_two_client(c, closest_client);
	}
	setfloating(c, 0);
}

bool check_trackpad_disabled(struct wlr_pointer *pointer) {
	struct libinput_device *device;

	if (!disable_trackpad)
		return false;

	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {

		// 如果是触摸板且被禁用，忽略事件
		if (libinput_device_config_tap_get_finger_count(device) > 0) {
			return true; // 不处理事件
		}
	}

	return false;
}

void // 鼠标按键事件
buttonpress(struct wl_listener *listener, void *data) {
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *hard_keyboard, *keyboard;
	unsigned int hard_mods, mods;
	Client *c;
	LayerSurface *l;
	struct wlr_surface *surface;
	Client *tmpc;
	int ji;
	const MouseBinding *m;
	struct wlr_surface *old_pointer_focus_surface =
		seat->pointer_state.focused_surface;

	handlecursoractivity();
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	switch (event->state) {
	case WL_POINTER_BUTTON_STATE_PRESSED:
		cursor_mode = CurPressed;
		selmon = xytomon(cursor->x, cursor->y);
		if (locked)
			break;

		xytonode(cursor->x, cursor->y, &surface, NULL, NULL, NULL, NULL);
		if (toplevel_from_wlr_surface(surface, &c, &l) >= 0) {
			if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
				focusclient(c, 1);

			if (surface != old_pointer_focus_surface) {
				wlr_seat_pointer_notify_clear_focus(seat);
				motionnotify(0, NULL, 0, 0, 0, 0);
			}

			// 聚焦按需要交互焦点的layer，但注意不能抢占独占焦点的layer
			if (l && !exclusive_focus &&
				l->layer_surface->current.keyboard_interactive ==
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
				focusclient(NULL, 0);
				client_notify_enter(l->layer_surface->surface,
									wlr_seat_get_keyboard(seat));
			}
		}

		// 当鼠标焦点在layer上的时候，不检测虚拟键盘的mod状态，
		// 避免layer虚拟键盘锁死mod按键状态
		hard_keyboard = &kb_group->wlr_group->keyboard;
		hard_mods =
			hard_keyboard ? wlr_keyboard_get_modifiers(hard_keyboard) : 0;

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard && !l ? wlr_keyboard_get_modifiers(keyboard) : 0;

		mods = mods | hard_mods;

		for (ji = 0; ji < config.mouse_bindings_count; ji++) {
			if (config.mouse_bindings_count < 1)
				break;
			m = &config.mouse_bindings[ji];
			if (CLEANMASK(mods) == CLEANMASK(m->mod) &&
				event->button == m->button && m->func &&
				(selmon->isoverview == 1 || m->button == BTN_MIDDLE) && c) {
				m->func(&m->arg);
				return;
			} else if (CLEANMASK(mods) == CLEANMASK(m->mod) &&
					   event->button == m->button && m->func &&
					   CLEANMASK(m->mod) != 0) {
				m->func(&m->arg);
				return;
			}
		}
		break;
	case WL_POINTER_BUTTON_STATE_RELEASED:
		/* If you released any buttons, we exit interactive move/resize mode. */
		if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
			cursor_mode = CurNormal;
			/* Clear the pointer focus, this way if the cursor is over a surface
			 * we will send an enter event after which the client will provide
			 * us a cursor surface */
			wlr_seat_pointer_clear_focus(seat);
			motionnotify(0, NULL, 0, 0, 0, 0);
			/* Drop the window off on its new monitor */
			if (grabc == selmon->sel) {
				selmon->sel = NULL;
			}
			selmon = xytomon(cursor->x, cursor->y);
			client_update_oldmonname_record(grabc, selmon);
			setmon(grabc, selmon, 0, true);
			reset_foreign_tolevel(grabc);
			selmon->prevsel = ISTILED(selmon->sel) ? selmon->sel : NULL;
			selmon->sel = grabc;
			tmpc = grabc;
			grabc = NULL;
			if (tmpc->drag_to_tile && drag_tile_to_tile) {
				place_drag_tile_client(tmpc);
			} else {
				apply_window_snap(tmpc);
			}
			tmpc->drag_to_tile = false;
			return;
		} else {
			cursor_mode = CurNormal;
		}
		break;
	}
	/* If the event wasn't handled by the compositor, notify the client with
	 * pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
								   event->state);
}

void checkidleinhibitor(struct wlr_surface *exclude) {
	int inhibited = 0, unused_lx, unused_ly;
	struct wlr_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
		struct wlr_surface *surface =
			wlr_surface_get_root_surface(inhibitor->surface);
		struct wlr_scene_tree *tree = surface->data;
		if (exclude != surface &&
			(inhibit_regardless_of_visibility ||
			 (!tree ||
			  wlr_scene_node_coords(&tree->node, &unused_lx, &unused_ly)))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void setcursorshape(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == seat->pointer_state.focused_client) {
		last_cursor.shape = event->shape;
		last_cursor.surface = NULL;
		if (!cursor_hidden)
			wlr_cursor_set_xcursor(cursor, cursor_mgr,
								   wlr_cursor_shape_v1_name(event->shape));
	}
}

void cleanuplisteners(void) {
	wl_list_remove(&cursor_axis.link);
	wl_list_remove(&cursor_button.link);
	wl_list_remove(&cursor_frame.link);
	wl_list_remove(&cursor_motion.link);
	wl_list_remove(&cursor_motion_absolute.link);
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
	wl_list_remove(&request_activate.link);
	wl_list_remove(&request_cursor.link);
	wl_list_remove(&request_set_psel.link);
	wl_list_remove(&request_set_sel.link);
	wl_list_remove(&request_set_cursor_shape.link);
	wl_list_remove(&request_start_drag.link);
	wl_list_remove(&start_drag.link);
	wl_list_remove(&new_session_lock.link);
#ifdef XWAYLAND
	wl_list_remove(&new_xwayland_surface.link);
	wl_list_remove(&xwayland_ready.link);
#endif
}

void cleanup(void) {
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

	dwl_im_relay_finish(dwl_input_method_relay);

	/* If it's not destroyed manually it will cause a use-after-free of
	 * wlr_seat. Destroy it until it's fixed in the wlroots side */
	wlr_backend_destroy(backend);

	wl_display_destroy(dpy);
	/* Destroy after the wayland display (when the monitors are already
	   destroyed) to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&scene->tree.node);
}

void // 17
cleanupkeyboard(struct wl_listener *listener, void *data) {
	Keyboard *kb = wl_container_of(listener, kb, destroy);

	wl_event_source_remove(kb->key_repeat_source);
	wl_list_remove(&kb->link);
	wl_list_remove(&kb->modifiers.link);
	wl_list_remove(&kb->key.link);
	wl_list_remove(&kb->destroy.link);
	free(kb);
}

void cleanupmon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	unsigned int i;

	/* m->layers[i] are intentionally not unlinked */
	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	// clean ext-workspaces grouplab
	wlr_ext_workspace_group_handle_v1_output_leave(m->ext_group, m->wlr_output);
	wlr_ext_workspace_group_handle_v1_destroy(m->ext_group);
	cleanup_workspaces_by_monitor(m);

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	if (m->lock_surface)
		destroylocksurface(&m->destroy_lock_surface, NULL);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	wlr_scene_output_destroy(m->scene_output);

	closemon(m);
	if (m->blur) {
		wlr_scene_node_destroy(&m->blur->node);
		m->blur = NULL;
	}
	free(m->pertag);
	free(m);
}

void closemon(Monitor *m) {
	/* update selmon if needed and
	 * move closed monitor's clients to the focused one */
	Client *c;
	int i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do /* don't switch to disabled mons */
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);

		if (!selmon->wlr_output->enabled)
			selmon = NULL;
	}

	wl_list_for_each(c, &clients, link) {
		if (c->mon == m) {

			if (selmon == NULL) {
				remove_foreign_topleve(c);
				c->mon = NULL;
			} else {
				client_change_mon(c, selmon);
			}
			// record the oldmonname which is used to restore
			if (c->oldmonname[0] == '\0') {
				client_update_oldmonname_record(c, m);
			}
		}
	}
	if (selmon) {
		focusclient(focustop(selmon), 1);
		printstatus();
	}
}

static void iter_layer_scene_buffers(struct wlr_scene_buffer *buffer, int sx,
									 int sy, void *user_data) {
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	wlr_scene_buffer_set_backdrop_blur(buffer, true);
	wlr_scene_buffer_set_backdrop_blur_ignore_transparent(buffer, true);
	if (blur_optimized) {
		wlr_scene_buffer_set_backdrop_blur_optimized(buffer, true);
	} else {
		wlr_scene_buffer_set_backdrop_blur_optimized(buffer, false);
	}
}

void maplayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, map);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	int ji;
	ConfigLayerRule *r;

	l->mapped = 1;

	if (!l->mon)
		return;
	strncpy(l->mon->last_surface_ws_name, layer_surface->namespace,
			sizeof(l->mon->last_surface_ws_name) - 1); // 最多拷贝255个字符
	l->mon->last_surface_ws_name[sizeof(l->mon->last_surface_ws_name) - 1] =
		'\0'; // 确保字符串以null结尾

	// 初始化几何位置
	get_layer_target_geometry(l, &l->geom);

	l->noanim = 0;
	l->dirty = false;
	l->noblur = 0;
	l->shadow = NULL;
	l->need_output_flush = true;

	// 应用layer规则
	for (ji = 0; ji < config.layer_rules_count; ji++) {
		if (config.layer_rules_count < 1)
			break;
		if (regex_match(config.layer_rules[ji].layer_name,
						l->layer_surface->namespace)) {

			r = &config.layer_rules[ji];
			APPLY_INT_PROP(l, r, noblur);
			APPLY_INT_PROP(l, r, noanim);
			APPLY_INT_PROP(l, r, noshadow);
			APPLY_STRING_PROP(l, r, animation_type_open);
			APPLY_STRING_PROP(l, r, animation_type_close);
		}
	}

	// 初始化阴影
	if (layer_surface->current.exclusive_zone == 0 &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
		l->shadow = wlr_scene_shadow_create(l->scene, 0, 0, border_radius,
											shadows_blur, shadowscolor);
		wlr_scene_node_lower_to_bottom(&l->shadow->node);
		wlr_scene_node_set_enabled(&l->shadow->node, true);
	}

	// 初始化动画
	if (animations && layer_animations && !l->noanim) {
		l->animation.duration = animation_duration_open;
		l->animation.action = OPEN;
		layer_set_pending_state(l);
	}
	// 刷新布局，让窗口能感应到exclude_zone变化以及设置独占表面
	arrangelayers(l->mon);
}

void commitlayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->current.layer]];
	struct wlr_layer_surface_v1_state old_state;
	struct wlr_box box;

	if (l->layer_surface->initial_commit) {
		client_set_scale(layer_surface->surface, l->mon->wlr_output->scale);

		/* Temporarily set the layer's current state to pending
		 * so that we can easily arrange it */
		old_state = l->layer_surface->current;
		l->layer_surface->current = l->layer_surface->pending;
		arrangelayers(l->mon);
		l->layer_surface->current = old_state;

		return;
	}

	get_layer_target_geometry(l, &box);

	if (animations && layer_animations && !l->noanim && l->mapped &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
		layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND &&
		!wlr_box_equal(&box, &l->geom)) {

		l->geom.x = box.x;
		l->geom.y = box.y;
		l->geom.width = box.width;
		l->geom.height = box.height;
		l->animation.action = MOVE;
		l->animation.duration = animation_duration_move;
		l->need_output_flush = true;
		layer_set_pending_state(l);
	}

	if (blur && blur_layer) {
		// 设置非背景layer模糊

		if (!l->noblur &&
			layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM &&
			layer_surface->current.layer !=
				ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {

			wlr_scene_node_for_each_buffer(&l->scene->node,
										   iter_layer_scene_buffers, l);
		}
	}

	if (blur) {
		// 如果背景层发生变化,标记优化的模糊背景缓存需要更新
		if (layer_surface->current.layer ==
			ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
			if (l->mon) {
				wlr_scene_optimized_blur_mark_dirty(l->mon->blur);
			}
		}
	}

	if (layer_surface == exclusive_focus &&
		layer_surface->current.keyboard_interactive !=
			ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE)
		exclusive_focus = NULL;

	if (layer_surface->current.committed == 0 &&
		l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (scene_layer != l->scene->node.parent) {
		wlr_scene_node_reparent(&l->scene->node, scene_layer);
		wl_list_remove(&l->link);
		wl_list_insert(&l->mon->layers[layer_surface->current.layer], &l->link);
		wlr_scene_node_reparent(
			&l->popups->node,
			(layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
				 ? layers[LyrTop]
				 : scene_layer));
	}

	arrangelayers(l->mon);
}

void commitnotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, commit);
	struct wlr_box *new_geo;

	if (c->surface.xdg->initial_commit) {
		// xdg client will first enter this before mapnotify
		applyrules(c);
		if (c->mon) {
			client_set_scale(client_surface(c), c->mon->wlr_output->scale);
		}
		setmon(c, NULL, 0,
			   true); /* Make sure to reapply rules in mapnotify() */

		client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
								WLR_EDGE_RIGHT);

		uint32_t serial = wlr_xdg_surface_schedule_configure(c->surface.xdg);
		if (serial > 0) {
			c->configure_serial = serial;
		}

		uint32_t wm_caps = WLR_XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU |
						   WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
						   WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
						   WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
		wlr_xdg_toplevel_set_wm_capabilities(c->surface.xdg->toplevel, wm_caps);

		if (c->mon) {
			wlr_xdg_toplevel_set_bounds(c->surface.xdg->toplevel,
										c->mon->w.width - 2 * c->bw,
										c->mon->w.height - 2 * c->bw);
		}

		if (c->decoration)
			requestdecorationmode(&c->set_decoration_mode, c->decoration);
		return;
	}

	if (!c || c->iskilling || c->animation.tagouting || c->animation.tagouted ||
		c->animation.tagining)
		return;

	if (c->configure_serial &&
		c->configure_serial <= c->surface.xdg->current.configure_serial)
		c->configure_serial = 0;

	if (!c->dirty) {
		new_geo = &c->surface.xdg->geometry;
		c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
				   new_geo->height != c->geom.height - 2 * c->bw ||
				   new_geo->x != 0 || new_geo->y != 0;
	}

	if (c == grabc || !c->dirty)
		return;

	resize(c, c->geom, 0);

	new_geo = &c->surface.xdg->geometry;
	c->dirty = new_geo->width != c->geom.width - 2 * c->bw ||
			   new_geo->height != c->geom.height - 2 * c->bw ||
			   new_geo->x != 0 || new_geo->y != 0;
}

void destroydecoration(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
}

void commitpopup(struct wl_listener *listener, void *data) {
	struct wlr_surface *surface = data;
	struct wlr_xdg_popup *popup = wlr_xdg_popup_try_from_wlr_surface(surface);
	LayerSurface *l = NULL;
	Client *c = NULL;
	struct wlr_box box;
	int type = -1;

	if (!popup->base->initial_commit)
		return;

	type = toplevel_from_wlr_surface(popup->base->surface, &c, &l);
	if (!popup->parent || type < 0)
		return;
	popup->base->surface->data =
		wlr_scene_xdg_surface_create(popup->parent->data, popup->base);
	if ((l && !l->mon) || (c && !c->mon)) {
		wlr_xdg_popup_destroy(popup);
		return;
	}
	box = type == LayerShell ? l->mon->m : c->mon->w;
	box.x -= (type == LayerShell ? l->scene->node.x : c->geom.x);
	box.y -= (type == LayerShell ? l->scene->node.y : c->geom.y);
	wlr_xdg_popup_unconstrain_from_box(popup, &box);
	wl_list_remove(&listener->link);
	free(listener);
}

void createdecoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
		   requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

void createidleinhibitor(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

void createkeyboard(struct wlr_keyboard *keyboard) {

	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(keyboard, kb_group->wlr_group->keyboard.keymap);

	wlr_keyboard_notify_modifiers(keyboard, 0, 0, locked_mods, 0);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(kb_group->wlr_group, keyboard);
}

KeyboardGroup *createkeyboardgroup(void) {
	KeyboardGroup *group = ecalloc(1, sizeof(*group));
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	group->wlr_group = wlr_keyboard_group_create();
	group->wlr_group->data = group;

	/* Prepare an XKB keymap and assign it to the keyboard group. */
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!(keymap = xkb_keymap_new_from_names(context, &xkb_rules,
											 XKB_KEYMAP_COMPILE_NO_FLAGS)))
		die("failed to compile keymap");

	wlr_keyboard_set_keymap(&group->wlr_group->keyboard, keymap);

	if (numlockon) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_NUM);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (unsigned int)1 << mod_index;
	}

	if (capslock) {
		xkb_mod_index_t mod_index =
			xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
		if (mod_index != XKB_MOD_INVALID)
			locked_mods |= (unsigned int)1 << mod_index;
	}

	if (locked_mods)
		wlr_keyboard_notify_modifiers(&group->wlr_group->keyboard, 0, 0,
									  locked_mods, 0);

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(&group->wlr_group->keyboard, repeat_rate,
								 repeat_delay);

	/* Set up listeners for keyboard events */
	LISTEN(&group->wlr_group->keyboard.events.key, &group->key, keypress);
	LISTEN(&group->wlr_group->keyboard.events.modifiers, &group->modifiers,
		   keypressmod);

	group->key_repeat_source =
		wl_event_loop_add_timer(event_loop, keyrepeat, group);

	/* A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same wlr_keyboard_group, which provides a single wlr_keyboard interface
	 * for all of them. Set this combined wlr_keyboard as the seat keyboard.
	 */
	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	return group;
}

void createlayersurface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->pending.layer]];

	if (!layer_surface->output &&
		!(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	l->type = LayerShell;
	LISTEN(&surface->events.map, &l->map, maplayersurfacenotify);
	LISTEN(&surface->events.commit, &l->surface_commit,
		   commitlayersurfacenotify);
	LISTEN(&surface->events.unmap, &l->unmap, unmaplayersurfacenotify);
	LISTEN(&layer_surface->events.destroy, &l->destroy,
		   destroylayersurfacenotify);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer =
		wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(
		layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP
			? layers[LyrTop]
			: scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer], &l->link);
	wlr_surface_send_enter(surface, layer_surface->output);
}

void createlocksurface(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
		wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width,
										  m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface,
		   destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

struct wlr_output_mode *get_nearest_output_mode(struct wlr_output *output,
												int width, int height,
												float refresh) {
	struct wlr_output_mode *mode, *nearest_mode = NULL;
	float min_diff = 99999.0f;

	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width == width && mode->height == height) {
			float mode_refresh = mode->refresh / 1000.0f;
			float diff = fabsf(mode_refresh - refresh);

			if (diff < min_diff) {
				min_diff = diff;
				nearest_mode = mode;
			}
		}
	}

	return nearest_mode;
}

void enable_adaptive_sync(Monitor *m, struct wlr_output_state *state) {
	wlr_output_state_set_adaptive_sync_enabled(state, true);
	if (!wlr_output_test_state(m->wlr_output, state)) {
		wlr_output_state_set_adaptive_sync_enabled(state, false);
		wlr_log(WLR_DEBUG, "failed to enable adaptive sync for output %s",
				m->wlr_output->name);
	} else {
		wlr_log(WLR_INFO, "adaptive sync enabled for output %s",
				m->wlr_output->name);
	}
}

void createmon(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct wlr_output *wlr_output = data;
	const ConfigMonitorRule *r;
	unsigned int i;
	int ji, jk;
	struct wlr_output_state state;
	Monitor *m;
	struct wlr_output_mode *internal_mode = NULL;
	bool custom_monitor_mode = false;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	m = wlr_output->data = ecalloc(1, sizeof(*m));
	m->wlr_output = wlr_output;

	wl_list_init(&m->dwl_ipc_outputs);

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	wlr_output_state_init(&state);
	/* Initialize monitor state using configured rules */
	m->gappih = gappih;
	m->gappiv = gappiv;
	m->gappoh = gappoh;
	m->gappov = gappov;
	m->isoverview = 0;
	m->sel = NULL;
	m->is_in_hotarea = 0;
	m->tagset[0] = m->tagset[1] = 1;
	float scale = 1;
	m->mfact = default_mfact;
	m->nmaster = default_nmaster;
	enum wl_output_transform rr = WL_OUTPUT_TRANSFORM_NORMAL;
	wlr_output_state_set_scale(&state, scale);
	wlr_output_state_set_transform(&state, rr);

	m->lt = &layouts[0];
	for (ji = 0; ji < config.monitor_rules_count; ji++) {
		if (config.monitor_rules_count < 1)
			break;

		r = &config.monitor_rules[ji];
		if (!r->name || regex_match(r->name, wlr_output->name)) {
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			m->m.x = r->x;
			m->m.y = r->y;
			if (r->layout) {
				for (jk = 0; jk < LENGTH(layouts); jk++) {
					if (strcmp(layouts[jk].name, r->layout) == 0) {
						m->lt = &layouts[jk];
					}
				}
			}
			scale = r->scale;
			rr = r->rr;

			if (r->width > 0 && r->height > 0 && r->refresh > 0) {
				internal_mode = get_nearest_output_mode(m->wlr_output, r->width,
														r->height, r->refresh);
				if (internal_mode) {
					custom_monitor_mode = true;
					wlr_output_state_set_mode(&state, internal_mode);
				} else if (wlr_output_is_headless(m->wlr_output)) {
					custom_monitor_mode = true;
					wlr_output_state_set_custom_mode(
						&state, r->width, r->height,
						(int)roundf(r->refresh * 1000));
				}
			}
			wlr_output_state_set_scale(&state, r->scale);
			wlr_output_state_set_transform(&state, r->rr);
			break;
		}
	}

	/* The mode is a tuple of (width, height, refresh rate), and each
	 * monitor supports only a specific set of modes. We just pick the
	 * monitor's preferred mode; a more sophisticated compositor would let
	 * the user configure it. */
	if (!custom_monitor_mode)
		wlr_output_state_set_mode(&state,
								  wlr_output_preferred_mode(wlr_output));

	if (adaptive_sync) {
		enable_adaptive_sync(m, &state);
	}

	/* Set up event listeners */
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state,
		   requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);
	m->pertag = calloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;
		m->pertag->smfacts[i] = default_smfact;
		m->pertag->ltidxs[i] = m->lt;
	}

	// apply tag rule
	for (i = 1; i <= config.tag_rules_count; i++) {
		for (jk = 0; jk < LENGTH(layouts); jk++) {
			if (config.tag_rules_count > 0 &&
				config.tag_rules[i - 1].layout_name &&
				strcmp(layouts[jk].name, config.tag_rules[i - 1].layout_name) ==
					0) {
				m->pertag->ltidxs[config.tag_rules[i - 1].id] = &layouts[jk];
				m->pertag->no_hide[config.tag_rules[i - 1].id] =
					config.tag_rules[i - 1].no_hide;
			}
		}
	}

	/* The xdg-protocol specifies:
	 *
	 * If the fullscreened surface is not opaque, the compositor must make
	 * sure that other screen content not part of the same surface tree (made
	 * up of subsurfaces, popups or similarly coupled surfaces) are not
	 * visible below the fullscreened surface.
	 *
	 */

	/* Adds this to the output layout in the order it was configured.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	m->scene_output = wlr_scene_output_create(scene, wlr_output);
	if (m->m.x == -1 && m->m.y == -1)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);

	if (blur) {
		m->blur = wlr_scene_optimized_blur_create(&scene->tree, 0, 0);
		wlr_scene_node_set_position(&m->blur->node, m->m.x, m->m.y);
		wlr_scene_node_reparent(&m->blur->node, layers[LyrBlur]);
		wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
		// wlr_scene_node_set_enabled(&m->blur->node, 1);
	}
	m->ext_group = wlr_ext_workspace_group_handle_v1_create(
		ext_manager, WLR_EXT_WORKSPACE_HANDLE_V1_CAP_ACTIVATE);
	wlr_ext_workspace_group_handle_v1_output_enter(m->ext_group, m->wlr_output);

	for (i = 1; i <= LENGTH(tags); i++) {
		add_workspace_by_tag(i, m);
	}

	printstatus();
}

void // fix for 0.5
createnotify(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_toplevel *toplevel = data;
	Client *c = NULL;

	/* Allocate a Client for this surface */
	c = toplevel->base->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = toplevel->base;
	c->bw = borderpx;

	LISTEN(&toplevel->base->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&toplevel->base->surface->events.map, &c->map, mapnotify);
	LISTEN(&toplevel->base->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&toplevel->events.destroy, &c->destroy, destroynotify);
	LISTEN(&toplevel->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&toplevel->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&toplevel->events.request_minimize, &c->minimize, minimizenotify);
	LISTEN(&toplevel->events.set_title, &c->set_title, updatetitle);
}

void destroyinputdevice(struct wl_listener *listener, void *data) {
	InputDevice *input_dev =
		wl_container_of(listener, input_dev, destroy_listener);

	// 清理设备特定数据
	if (input_dev->device_data) {
		// 根据设备类型进行特定清理
		switch (input_dev->wlr_device->type) {
		case WLR_INPUT_DEVICE_SWITCH: {
			Switch *sw = (Switch *)input_dev->device_data;
			// 移除 toggle 监听器
			wl_list_remove(&sw->toggle.link);
			// 释放 Switch 内存
			free(sw);
			break;
		}
		// 可以添加其他设备类型的清理代码
		default:
			break;
		}
		input_dev->device_data = NULL;
	}

	// 从设备列表中移除
	wl_list_remove(&input_dev->link);
	// 移除 destroy 监听器
	wl_list_remove(&input_dev->destroy_listener.link);
	// 释放内存
	free(input_dev);
}

void configure_pointer(struct libinput_device *device) {
	if (libinput_device_config_tap_get_finger_count(device)) {
		libinput_device_config_tap_set_enabled(device, tap_to_click);
		libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
		libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
		libinput_device_config_tap_set_button_map(device, button_map);
		libinput_device_config_scroll_set_natural_scroll_enabled(
			device, trackpad_natural_scrolling);
	} else {
		libinput_device_config_scroll_set_natural_scroll_enabled(
			device, mouse_natural_scrolling);
	}

	if (libinput_device_config_dwt_is_available(device))
		libinput_device_config_dwt_set_enabled(device, disable_while_typing);

	if (libinput_device_config_left_handed_is_available(device))
		libinput_device_config_left_handed_set(device, left_handed);

	if (libinput_device_config_middle_emulation_is_available(device))
		libinput_device_config_middle_emulation_set_enabled(
			device, middle_button_emulation);

	if (libinput_device_config_scroll_get_methods(device) !=
		LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
		libinput_device_config_scroll_set_method(device, scroll_method);
	if (libinput_device_config_scroll_get_methods(device) ==
		LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
		libinput_device_config_scroll_set_button(device, scroll_button);

	if (libinput_device_config_click_get_methods(device) !=
		LIBINPUT_CONFIG_CLICK_METHOD_NONE)
		libinput_device_config_click_set_method(device, click_method);

	if (libinput_device_config_send_events_get_modes(device))
		libinput_device_config_send_events_set_mode(device, send_events_mode);

	if (libinput_device_config_accel_is_available(device)) {
		libinput_device_config_accel_set_profile(device, accel_profile);
		libinput_device_config_accel_set_speed(device, accel_speed);
	}
}

void createpointer(struct wlr_pointer *pointer) {

	struct libinput_device *device = NULL;

	if (wlr_input_device_is_libinput(&pointer->base) &&
		(device = wlr_libinput_get_device_handle(&pointer->base))) {

		configure_pointer(device);

		InputDevice *input_dev = calloc(1, sizeof(InputDevice));
		input_dev->wlr_device = &pointer->base;
		input_dev->libinput_device = device;

		input_dev->destroy_listener.notify = destroyinputdevice;
		wl_signal_add(&pointer->base.events.destroy,
					  &input_dev->destroy_listener);

		wl_list_insert(&inputdevices, &input_dev->link);
	}
	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void switch_toggle(struct wl_listener *listener, void *data) {
	// 获取包含监听器的结构体
	Switch *sw = wl_container_of(listener, sw, toggle);

	// 处理切换事件
	struct wlr_switch_toggle_event *event = data;
	SwitchBinding *s;
	int ji;

	for (ji = 0; ji < config.switch_bindings_count; ji++) {
		if (config.switch_bindings_count < 1)
			break;
		s = &config.switch_bindings[ji];
		if (event->switch_state == s->fold && s->func) {
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

void createpointerconstraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		ecalloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
	LISTEN(&pointer_constraint->constraint->events.destroy,
		   &pointer_constraint->destroy, destroypointerconstraint);
}

void createpopup(struct wl_listener *listener, void *data) {
	/* This event is raised when a client (either xdg-shell or layer-shell)
	 * creates a new popup. */
	struct wlr_xdg_popup *popup = data;
	LISTEN_STATIC(&popup->base->surface->events.commit, commitpopup);
}

void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint) {
	if (active_constraint == constraint)
		return;

	if (active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);

	active_constraint = constraint;
	wlr_pointer_constraint_v1_send_activated(constraint);
}

void cursorframe(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at
	 * the same time, in which case a frame event won't be sent in between.
	 */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat);
}

void cursorwarptohint(void) {
	Client *c = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
	if (c && active_constraint->current.cursor_hint.enabled) {
		wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw,
						sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}

void destroydragicon(struct wl_listener *listener, void *data) {
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
	wl_list_remove(&listener->link);
	free(listener);
}

void destroyidleinhibitor(struct wl_listener *listener, void *data) {
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager
	 */
	checkidleinhibitor(wlr_surface_get_root_surface(data));
	wl_list_remove(&listener->link);
	free(listener);
}

void destroylayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->scene->node);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

void destroylock(SessionLock *lock, int unlock) {
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	wlr_scene_node_set_enabled(&locked_bg->node, false);

	focusclient(focustop(selmon), 0);
	motionnotify(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void destroylocksurface(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface,
		*lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface) {
		if (exclusive_focus && !locked) {
			exclusive_focus = NULL;
			reset_exclusive_layer(m);
		}
		return;
	}

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface = wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface, wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		exclusive_focus = NULL;
		reset_exclusive_layer(selmon);
		focusclient(focustop(selmon), 1);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	}
}

void // 0.7 custom
destroynotify(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	free(c);
}

void destroypointerconstraint(struct wl_listener *listener, void *data) {
	PointerConstraint *pointer_constraint =
		wl_container_of(listener, pointer_constraint, destroy);

	if (active_constraint == pointer_constraint->constraint) {
		cursorwarptohint();
		active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
}

void destroysessionlock(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

void destroykeyboardgroup(struct wl_listener *listener, void *data) {
	KeyboardGroup *group = wl_container_of(listener, group, destroy);
	wl_event_source_remove(group->key_repeat_source);
	wl_list_remove(&group->key.link);
	wl_list_remove(&group->modifiers.link);
	wl_list_remove(&group->destroy.link);
	wlr_keyboard_group_destroy(group->wlr_group);
	free(group);
}

void focusclient(Client *c, int lift) {
	struct wlr_surface *old_keyboard_focus_surface =
		seat->keyboard_state.focused_surface;

	if (locked)
		return;

	if (c && c->iskilling)
		return;

	if (c && !client_surface(c)->mapped)
		return;

	if (c && client_should_ignore_focus(c)) {
		return;
	}

	/* Raise client in stacking order if requested */
	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层

	if (c && client_surface(c) == old_keyboard_focus_surface && selmon &&
		selmon->sel)
		return;

	if (selmon && selmon->sel && selmon->sel != c &&
		selmon->sel->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(
			selmon->sel->foreign_toplevel, false);
	}

	if (c && !c->iskilling && !client_is_unmanaged(c) && c->mon) {

		selmon = c->mon;
		selmon->prevsel = selmon->sel;
		selmon->sel = c;

		// decide whether need to re-arrange

		if (c && selmon->prevsel &&
			(selmon->prevsel->tags & selmon->tagset[selmon->seltags]) &&
			(c->tags & selmon->tagset[selmon->seltags]) && !c->isfloating &&
			!c->isfullscreen && !c->ismaxmizescreen &&
			is_scroller_layout(selmon)) {
			arrange(selmon, false);
		}

		// change focus link position
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);

		// change border color
		c->isurgent = 0;
		setborder_color(c);
	}

	if (c && !c->iskilling && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);

	/* Deactivate old client if focus is changing */
	if (old_keyboard_focus_surface &&
		(!c || client_surface(c) != old_keyboard_focus_surface)) {
		/* If an exclusive_focus layer is focused, don't focus or activate
		 * the client, but only update its position in fstack to render its
		 * border with focuscolor and focus it after the exclusive_focus
		 * layer is closed. */
		Client *w = NULL;
		LayerSurface *l = NULL;
		int type =
			toplevel_from_wlr_surface(old_keyboard_focus_surface, &w, &l);
		if (type == LayerShell && l->scene->node.enabled &&
			l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
			l == exclusive_focus) {
			return;
		} else if (w && w == exclusive_focus && client_wants_focus(w)) {
			return;
			/* Don't deactivate old_keyboard_focus_surface client if the new
			 * one wants focus, as this causes issues with winecfg and
			 * probably other clients */
		} else if (w && !client_is_unmanaged(w) &&
				   (!c || !client_wants_focus(c))) {
			setborder_color(w);

			client_activate_surface(old_keyboard_focus_surface, 0);
		}
	}
	printstatus();

	if (!c) {
		/* With no client, all we have left is to clear focus */
		if (selmon && selmon->sel)
			selmon->sel =
				NULL; // 这个很关键,因为很多地方用到当前窗口做计算,不重置成NULL就会到处有野指针

		// clear text input focus state
		dwl_im_relay_set_focus(dwl_input_method_relay, NULL);
		wlr_seat_keyboard_notify_clear_focus(seat);
		return;
	}

	/* Change cursor surface */
	motionnotify(0, NULL, 0, 0, 0, 0);

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

	// set text input focus
	dwl_im_relay_set_focus(dwl_input_method_relay, client_surface(c));
	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);
}

void // 0.6
fullscreennotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, fullscreen);

	if (!c || c->iskilling)
		return;

	setfullscreen(c, client_wants_fullscreen(c));
}

void requestmonstate(struct wl_listener *listener, void *data) {
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(event->output, event->state);
	updatemons(NULL, NULL);
}

void inputdevice(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct wlr_input_device *device = data;
	unsigned int caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_TABLET:
		createtablet(device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		tablet_pad = wlr_tablet_pad_create(tablet_mgr, seat, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
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
	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&kb_group->wlr_group->devices))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

int keyrepeat(void *data) {
	KeyboardGroup *group = data;
	int i;
	if (!group->nsyms || group->wlr_group->keyboard.repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(
		group->key_repeat_source,
		1000 / group->wlr_group->keyboard.repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keybinding(group->mods, group->keysyms[i], group->keycode);

	return 0;
}

int // 17
keybinding(unsigned int mods, xkb_keysym_t sym, unsigned int keycode) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its
	 * own processing.
	 */
	int handled = 0;
	const KeyBinding *k;
	int ji;
	for (ji = 0; ji < config.key_bindings_count; ji++) {
		if (config.key_bindings_count < 1)
			break;
		k = &config.key_bindings[ji];
		if (CLEANMASK(mods) == CLEANMASK(k->mod) &&
			((k->keysymcode.type == KEY_TYPE_SYM &&
			  normalize_keysym(sym) ==
				  normalize_keysym(k->keysymcode.keysym)) ||
			 (k->keysymcode.type == KEY_TYPE_CODE &&
			  keycode == k->keysymcode.keycode)) &&
			k->func) {
			k->func(&k->arg);
			handled = 1;
		}
	}
	return handled;
}

bool keypressglobal(struct wlr_surface *last_surface,
					struct wlr_keyboard *keyboard,
					struct wlr_keyboard_key_event *event, unsigned int mods,
					xkb_keysym_t keysym, unsigned int keycode) {
	Client *c = NULL, *lastc = focustop(selmon);
	unsigned int keycodes[32] = {0};
	int reset = false;
	const char *appid = NULL;
	const char *title = NULL;
	int ji;
	const ConfigWinRule *r;

	for (ji = 0; ji < config.window_rules_count; ji++) {
		if (config.window_rules_count < 1)
			break;
		r = &config.window_rules[ji];

		if (!r->globalkeybinding.mod ||
			(!r->globalkeybinding.keysymcode.keysym &&
			 !r->globalkeybinding.keysymcode.keycode))
			continue;

		/* match key only (case insensitive) ignoring mods */
		if (((r->globalkeybinding.keysymcode.type == KEY_TYPE_SYM &&
			  r->globalkeybinding.keysymcode.keysym == keysym) ||
			 (r->globalkeybinding.keysymcode.type == KEY_TYPE_CODE &&
			  r->globalkeybinding.keysymcode.keycode == keycode)) &&
			r->globalkeybinding.mod == mods) {
			wl_list_for_each(c, &clients, link) {
				if (c && c != lastc) {
					appid = client_get_appid(c);
					title = client_get_title(c);

					if ((r->title && regex_match(r->title, title) && !r->id) ||
						(r->id && regex_match(r->id, appid) && !r->title) ||
						(r->id && regex_match(r->id, appid) && r->title &&
						 regex_match(r->title, title))) {
						reset = true;
						wlr_seat_keyboard_enter(seat, client_surface(c),
												keycodes, 0,
												&keyboard->modifiers);
						wlr_seat_keyboard_send_key(seat, event->time_msec,
												   event->keycode,
												   event->state);
						goto done;
					}
				}
			}
		}
	}

done:
	if (reset)
		wlr_seat_keyboard_enter(seat, last_surface, keycodes, 0,
								&keyboard->modifiers);
	return reset;
}

void keypress(struct wl_listener *listener, void *data) {
	int i;
	/* This event is raised when a key is pressed or released. */
	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	struct wlr_surface *last_surface = seat->keyboard_state.focused_surface;
	struct wlr_xdg_surface *xdg_surface =
		last_surface ? wlr_xdg_surface_try_from_wlr_surface(last_surface)
					 : NULL;
	int pass = 0;
	bool hit_global = false;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface =
		last_surface ? wlr_xwayland_surface_try_from_wlr_surface(last_surface)
					 : NULL;
#endif

	/* Translate libinput keycode -> xkbcommon */
	unsigned int keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(group->wlr_group->keyboard.xkb_state,
									   keycode, &syms);

	int handled = 0;
	unsigned int mods = wlr_keyboard_get_modifiers(&group->wlr_group->keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	// ov tab mode detect moe key release
	if (ov_tab_mode && !locked &&
		event->state == WL_KEYBOARD_KEY_STATE_RELEASED &&
		(keycode == 133 || keycode == 37 || keycode == 64 || keycode == 50 ||
		 keycode == 134 || keycode == 105 || keycode == 108 || keycode == 62) &&
		selmon && selmon->sel) {
		if (selmon->isoverview && selmon->sel) {
			toggleoverview(&(Arg){.i = -1});
		}
	}

	/* On _press_ if there is no active screen locker,
	 * attempt to process a compositor keybinding. */
	if (!locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++)
			handled = keybinding(mods, syms[i], keycode) || handled;
	} else if (event->state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		tag_combo = false;
	}

	if (handled && group->wlr_group->keyboard.repeat_info.delay > 0) {
		group->mods = mods;
		group->keysyms = syms;
		group->keycode = keycode;
		group->nsyms = nsyms;
		wl_event_source_timer_update(
			group->key_repeat_source,
			group->wlr_group->keyboard.repeat_info.delay);
	} else {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	/* don't pass when popup is focused
	 * this is better than having popups (like fuzzel or wmenu) closing
	 * while typing in a passed keybind */
	pass = (xdg_surface && xdg_surface->role != WLR_XDG_SURFACE_ROLE_POPUP) ||
		   !last_surface
#ifdef XWAYLAND
		   || xsurface
#endif
		;
	/* passed keys don't get repeated */
	if (pass && syms)
		hit_global = keypressglobal(last_surface, &group->wlr_group->keyboard,
									event, mods, syms[0], keycode);

	if (hit_global) {
		return;
	}
	if (!dwl_im_keyboard_grab_forward_key(group, event)) {
		wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
		/* Pass unhandled keycodes along to the client. */
		wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
									 event->state);
	}
}

void keypressmod(struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	if (!dwl_im_keyboard_grab_forward_modifiers(group)) {

		wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
		/* Send modifiers to the client. */
		wlr_seat_keyboard_notify_modifiers(
			seat, &group->wlr_group->keyboard.modifiers);
	}
}

void pending_kill_client(Client *c) {
	// c->iskilling = 1; //不可以提前标记已经杀掉，因为有些客户端可能拒绝
	client_send_close(c);
}

void locksession(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	wlr_scene_node_set_enabled(&locked_bg->node, true);
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface,
		   createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy, destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}

static void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int sx,
								   int sy, void *user_data) {
	Client *c = user_data;

	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}

	struct wlr_surface *surface = scene_surface->surface;
	/* we dont blur subsurfaces */
	if (wlr_subsurface_try_from_wlr_surface(surface) != NULL)
		return;

	if (blur && c && !c->noblur) {
		wlr_scene_buffer_set_backdrop_blur(buffer, true);
		wlr_scene_buffer_set_backdrop_blur_ignore_transparent(buffer, false);
		if (blur_optimized) {
			wlr_scene_buffer_set_backdrop_blur_optimized(buffer, true);
		} else {
			wlr_scene_buffer_set_backdrop_blur_optimized(buffer, false);
		}
	} else {
		wlr_scene_buffer_set_backdrop_blur(buffer, false);
	}
}

void init_client_properties(Client *c) {
	c->ismaxmizescreen = 0;
	c->isfullscreen = 0;
	c->need_float_size_reduce = 0;
	c->iskilling = 0;
	c->istagswitching = 0;
	c->isglobal = 0;
	c->isminied = 0;
	c->isoverlay = 0;
	c->isunglobal = 0;
	c->is_in_scratchpad = 0;
	c->isnamedscratchpad = 0;
	c->is_scratchpad_show = 0;
	c->need_float_size_reduce = 0;
	c->is_clip_to_hide = 0;
	c->is_restoring_from_ov = 0;
	c->isurgent = 0;
	c->need_output_flush = 0;
	c->scroller_proportion = scroller_default_proportion;
	c->is_pending_open_animation = true;
	c->drag_to_tile = false;
	c->scratchpad_switching_mon = false;
	c->fake_no_border = false;
	c->focused_opacity = focused_opacity;
	c->unfocused_opacity = unfocused_opacity;
	c->nofadein = 0;
	c->nofadeout = 0;
	c->no_force_center = 0;
	c->isnoborder = 0;
	c->isnosizehint = 0;
	c->ignore_maximize = 0;
	c->ignore_minimize = 1;
	c->iscustomsize = 0;
}

void // old fix to 0.5
mapnotify(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *p = NULL;
	Client *at_client;
	Client *c = wl_container_of(listener, c, map);
	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	init_client_properties(c);

	// set special window properties
	if (client_is_unmanaged(c) || client_should_ignore_focus(c)) {
		c->bw = 0;
		c->isnoborder = 1;
	} else {
		c->bw = borderpx;
	}

	if (client_should_global(c)) {
		c->isunglobal = 1;
	}

	// init client geom
	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;

	/* Handle unmanaged clients first so we can return prior create borders
	 */
	if (client_is_unmanaged(c)) {
		/* Unmanaged clients always are floating */
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			LISTEN(&c->surface.xwayland->events.set_geometry, &c->set_geometry,
				   setgeometrynotify);
		}
#endif
		return;
	}

	// extra node
	c->border = wlr_scene_rect_create(c->scene, 0, 0,
									  c->isurgent ? urgentcolor : bordercolor);
	wlr_scene_node_lower_to_bottom(&c->border->node);
	wlr_scene_node_set_position(&c->border->node, 0, 0);
	wlr_scene_rect_set_corner_radius(c->border, border_radius,
									 border_radius_location_default);
	wlr_scene_node_set_enabled(&c->border->node, true);

	c->shadow = wlr_scene_shadow_create(c->scene, 0, 0, border_radius,
										shadows_blur, shadowscolor);

	wlr_scene_node_lower_to_bottom(&c->shadow->node);
	wlr_scene_node_set_enabled(&c->shadow->node, true);

	if (new_is_master && selmon && !is_scroller_layout(selmon))
		// tile at the top
		wl_list_insert(&clients, &c->link); // 新窗口是master,头部入栈
	else if (selmon && is_scroller_layout(selmon) &&
			 selmon->visible_tiling_clients > 0) {

		if (selmon->sel && ISTILED(selmon->sel) &&
			VISIBLEON(selmon->sel, selmon)) {
			at_client = selmon->sel;
		} else {
			at_client = center_tiled_select(selmon);
		}

		if (at_client) {
			at_client->link.next->prev = &c->link;
			c->link.prev = &at_client->link;
			c->link.next = at_client->link.next;
			at_client->link.next = &c->link;
		} else {
			wl_list_insert(clients.prev, &c->link); // 尾部入栈
		}
	} else
		wl_list_insert(clients.prev, &c->link); // 尾部入栈
	wl_list_insert(&fstack, &c->flink);

	/* Set initial monitor, tags, floating status, and focus:
	 * we always consider floating, clients that have parent and thus
	 * we set the same tags and monitor than its parent, if not
	 * try to apply rules for them */
	if ((p = client_get_parent(c))) {
		c->isfloating = 1;
		setmon(c, p->mon, p->tags, true);
	} else {
		applyrules(c);
	}

	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
							WLR_EDGE_RIGHT);

	// apply buffer effects of client
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   iter_xdg_scene_buffers, c);

	// set border color
	setborder_color(c);

	// make sure the animation is open type
	c->is_pending_open_animation = true;
	resize(c, c->geom, 0);
	printstatus();
}

void // 0.5 custom
maximizenotify(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
	 */
	// Client *c = wl_container_of(listener, c, maximize);
	// if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
	// 		< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
	// 	wlr_xdg_surface_schedule_configure(c->surface.xdg);
	// togglemaxmizescreen(&(Arg){0});
	Client *c = wl_container_of(listener, c, maximize);

	if (!c || !c->mon || c->iskilling || c->ignore_maximize)
		return;

	if (c->ismaxmizescreen || c->isfullscreen)
		setmaxmizescreen(c, 0);
	else
		setmaxmizescreen(c, 1);
}

void unminimize(Client *c) {
	if (c && c->is_in_scratchpad && c->is_scratchpad_show) {
		c->isminied = 0;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		return;
	}

	if (c && c->isminied) {
		show_hide_client(c);
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		setborder_color(c);
		arrange(c->mon, false);
		return;
	}
}

void set_minimized(Client *c) {

	if (!c || !c->mon)
		return;

	c->isglobal = 0;
	c->oldtags = c->mon->tagset[c->mon->seltags];
	c->mini_restore_tag = c->tags;
	c->tags = 0;
	c->isminied = 1;
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	focusclient(focustop(selmon), 1);
	arrange(c->mon, false);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, false);
	wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, true);
	wl_list_remove(&c->link);				// 从原来位置移除
	wl_list_insert(clients.prev, &c->link); // 插入尾部
}

void // 0.5 custom
minimizenotify(struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
	 */
	// Client *c = wl_container_of(listener, c, maximize);
	// if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
	// 		< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
	// 	wlr_xdg_surface_schedule_configure(c->surface.xdg);
	// togglemaxmizescreen(&(Arg){0});

	Client *c = wl_container_of(listener, c, minimize);

	if (!c || !c->mon || c->iskilling || c->isminied)
		return;

	if (client_request_minimize(c, data) && !c->ignore_minimize) {
		if (!c->isminied)
			set_minimized(c);
		client_set_minimized(c, true);
	} else {
		if (c->isminied)
			unminimize(c);
		client_set_minimized(c, false);
	}
}

void motionabsolute(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an
	 * _absolute_ motion event, from 0..1 on each axis. This happens, for
	 * example, when wlroots is running under a Wayland window rather than
	 * KMS+DRM, and you move the mouse over the window. You could enter the
	 * window from any edge, so we have to warp the mouse there. There is
	 * also some hardware which emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	if (!event->time_msec) /* this is 0 with virtual pointer */
		wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x,
								 event->y);

	wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base,
										 event->x, event->y, &lx, &ly);
	dx = lx - cursor->x;
	dy = ly - cursor->y;
	motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

void motionnotify(unsigned int time, struct wlr_input_device *device, double dx,
				  double dy, double dx_unaccel, double dy_unaccel) {
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_pointer_constraint_v1 *constraint;
	bool should_lock = false;

	/* Find the client under the pointer and send the event along. */
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag &&
		surface != seat->pointer_state.focused_surface &&
		toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w,
								  &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->scene->node.x : w->geom.x);
		sy = cursor->y - (l ? l->scene->node.y : w->geom.y);
	}

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			relative_pointer_mgr, seat, (uint64_t)time * 1000, dx, dy,
			dx_unaccel, dy_unaccel);

		wl_list_for_each(constraint, &pointer_constraints->constraints, link)
			cursorconstrain(constraint);

		if (active_constraint && cursor_mode != CurResize &&
			cursor_mode != CurMove) {
			toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
			if (c && active_constraint->surface ==
						 seat->pointer_state.focused_surface) {
				sx = cursor->x - c->geom.x - c->bw;
				sy = cursor->y - c->geom.y - c->bw;
				if (wlr_region_confine(&active_constraint->region, sx, sy,
									   sx + dx, sy + dy, &sx_confined,
									   &sy_confined)) {
					dx = sx_confined - sx;
					dy = sy_confined - sy;
				}

				if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
					return;
			}
		}

		wlr_cursor_move(cursor, device, dx, dy);
		handlecursoractivity();
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

		/* Update selmon (even while dragging a window) */
		if (sloppyfocus)
			selmon = xytomon(cursor->x, cursor->y);
	}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&drag_icon->node, (int)round(cursor->x),
								(int)round(cursor->y));

	/* If we are currently grabbing the mouse, handle and return */
	if (cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		grabc->iscustomsize = 1;
		grabc->float_geom =
			(struct wlr_box){.x = (int)round(cursor->x) - grabcx,
							 .y = (int)round(cursor->y) - grabcy,
							 .width = grabc->geom.width,
							 .height = grabc->geom.height};
		resize(grabc, grabc->float_geom, 1);
		return;
	} else if (cursor_mode == CurResize) {
		grabc->iscustomsize = 1;
		grabc->float_geom =
			(struct wlr_box){.x = grabc->geom.x,
							 .y = grabc->geom.y,
							 .width = (int)round(cursor->x) - grabc->geom.x,
							 .height = (int)round(cursor->y) - grabc->geom.y};
		resize(grabc, grabc->float_geom, 1);
		return;
	}

	/* If there's no client surface under the cursor, set the cursor image
	 * to a default. This is what makes the cursor image appear when you
	 * move it off of a client or over its border. */
	if (!surface && !seat->drag && !cursor_hidden)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	if (c && c->mon && !c->animation.running &&
		(!(c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
		   c->geom.x < c->mon->m.x ||
		   c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height ||
		   c->geom.y < c->mon->m.y) ||
		 !ISTILED(c))) {
		scroller_focus_lock = 0;
	}

	should_lock = false;
	if (!scroller_focus_lock ||
		!(c && c->mon &&
		  (c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
		   c->geom.x < c->mon->m.x ||
		   c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height ||
		   c->geom.y < c->mon->m.y))) {
		if (c && c->mon && is_scroller_layout(c->mon) &&
			(c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
			 c->geom.x < c->mon->m.x ||
			 c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height ||
			 c->geom.y < c->mon->m.y)) {
			should_lock = true;
		}

		if (!(!edge_scroller_pointer_focus && c && c->mon &&
			  is_scroller_layout(c->mon) &&
			  (c->geom.x < c->mon->m.x || c->geom.y < c->mon->m.y ||
			   c->geom.x + c->geom.width > c->mon->m.x + c->mon->m.width ||
			   c->geom.y + c->geom.height > c->mon->m.y + c->mon->m.height)))
			pointerfocus(c, surface, sx, sy, time);

		if (should_lock && c && c->mon && ISTILED(c) && c == c->mon->sel) {
			scroller_focus_lock = 1;
		}
	}
}

void motionrelative(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a
	 * _relative_ pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor
	 * automatically handles constraining the motion to the output layout,
	 * as well as any special configuration applied for the specific input
	 * device which generated the event. You can pass NULL for the device if
	 * you want to move the cursor around without any input. */

	if (check_trackpad_disabled(event->pointer)) {
		return;
	}

	motionnotify(event->time_msec, &event->pointer->base, event->delta_x,
				 event->delta_y, event->unaccel_dx, event->unaccel_dy);
	toggle_hotarea(cursor->x, cursor->y);
}

void outputmgrapply(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void // 0.7 custom
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test) {
	/*
	 * Called when a client such as wlr-randr requests a change in output
	 * configuration. This is only one way that the layout can be changed,
	 * so any Monitor information should be updated by updatemons() after an
	 * output_layout.change event, not here.
	 */
	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		/* Ensure displays previously disabled by
		 * wlr-output-power-management-v1 are properly handled*/
		m->asleep = 0;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(
				&state, config_head->state.custom_mode.width,
				config_head->state.custom_mode.height,
				config_head->state.custom_mode.refresh);

		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(
			&state, config_head->state.adaptive_sync_enabled);

	apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state)
				   : wlr_output_commit_state(wlr_output, &state);

		/* Don't move monitors if position wouldn't change, this to avoid
		 * wlroots marking the output as manually configured.
		 * wlr_output_layout_add does not like disabled outputs */
		if (!test && wlr_output->enabled &&
			(m->m.x != config_head->state.x || m->m.y != config_head->state.y))
			wlr_output_layout_add(output_layout, wlr_output,
								  config_head->state.x, config_head->state.y);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);

	/* https://codeberg.org/dwl/dwl/issues/577 */
	updatemons(NULL, NULL);
}

void outputmgrtest(struct wl_listener *listener, void *data) {
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}

void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
				  unsigned int time) {
	struct timespec now;

	if (surface != seat->pointer_state.focused_surface && sloppyfocus && time &&
		c && !client_is_unmanaged(c))
		focusclient(c, 0);

	/* If surface is NULL, clear pointer focus */
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (!time) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	/* Let the client know that the mouse cursor has entered one
	 * of its surfaces, and make keyboard focus follow if desired.
	 * wlroots makes this a no-op if surface is already focused */
	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

void // 17
printstatus(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		// Update workspace active states
		dwl_ext_workspace_printstatus(m);

		// Update IPC output status
		dwl_ipc_output_printstatus(m);
	}
}

void powermgrsetmode(struct wl_listener *listener, void *data) {
	struct wlr_output_power_v1_set_mode_event *event = data;
	struct wlr_output_state state = {0};
	Monitor *m = event->output->data;

	if (!m)
		return;

	m->gamma_lut_changed = 1; /* Reapply gamma LUT when re-enabling the ouput */
	wlr_output_state_set_enabled(&state, event->mode);
	wlr_output_commit_state(m->wlr_output, &state);

	m->asleep = !event->mode;
	updatemons(NULL, NULL);
}

void quitsignal(int signo) { quit(NULL); }

void scene_buffer_apply_opacity(struct wlr_scene_buffer *buffer, int sx, int sy,
								void *data) {
	wlr_scene_buffer_set_opacity(buffer, *(double *)data);
}

void client_set_opacity(Client *c, double opacity) {
	wlr_scene_node_for_each_buffer(&c->scene_surface->node,
								   scene_buffer_apply_opacity, &opacity);
}

void rendermon(struct wl_listener *listener, void *data) {
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c, *tmp;
	struct wlr_output_state pending = {0};
	LayerSurface *l, *tmpl;
	int i;
	struct wl_list *layer_list;

	struct timespec now;
	bool need_more_frames = false;

	for (i = 0; i < LENGTH(m->layers); i++) {
		layer_list = &m->layers[i];
		// Draw frames for all layer
		wl_list_for_each_safe(l, tmpl, layer_list, link) {
			need_more_frames = layer_draw_frame(l) || need_more_frames;
		}
	}

	wl_list_for_each_safe(c, tmp, &fadeout_clients, fadeout_link) {
		need_more_frames = client_draw_fadeout_frame(c) || need_more_frames;
	}

	wl_list_for_each_safe(l, tmpl, &fadeout_layers, fadeout_link) {
		need_more_frames = layer_draw_fadeout_frame(l) || need_more_frames;
	}

	// Draw frames for all clients
	wl_list_for_each(c, &clients, link) {
		need_more_frames = client_draw_frame(c) || need_more_frames;
		if (!animations && c->configure_serial && !c->isfloating &&
			client_is_rendered_on_mon(c, m) && !client_is_stopped(c))
			goto skip;
	}

	wlr_scene_output_commit(m->scene_output, NULL);

skip:

	// Send frame done notification
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);

	// // Clean up pending state
	wlr_output_state_finish(&pending);

	if (need_more_frames) {
		wlr_output_schedule_frame(m->wlr_output);
	}
}

void requestdecorationmode(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	if (c->surface.xdg->initialized)
		wlr_xdg_toplevel_decoration_v1_set_mode(
			c->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void requeststartdrag(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
											  event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void setborder_color(Client *c) {
	if (!c || !c->mon)
		return;
	if (c->isurgent) {
		client_set_border_color(c, urgentcolor);
		return;
	}
	if (c->is_in_scratchpad && selmon && c == selmon->sel) {
		client_set_border_color(c, scratchpadcolor);
	} else if (c->isglobal && selmon && c == selmon->sel) {
		client_set_border_color(c, globalcolor);
	} else if (c->isoverlay && selmon && c == selmon->sel) {
		client_set_border_color(c, overlaycolor);
	} else if (c->ismaxmizescreen && selmon && c == selmon->sel) {
		client_set_border_color(c, maxmizescreencolor);
	} else if (selmon && c == selmon->sel) {
		client_set_border_color(c, focuscolor);
	} else {
		client_set_border_color(c, bordercolor);
	}
}

void exchange_two_client(Client *c1, Client *c2) {

	Monitor *tmp_mon;
	unsigned int tmp_tags;

	if (c1 == NULL || c2 == NULL ||
		(!exchange_cross_monitor && c1->mon != c2->mon)) {
		return;
	}

	struct wl_list *tmp1_prev = c1->link.prev;
	struct wl_list *tmp2_prev = c2->link.prev;
	struct wl_list *tmp1_next = c1->link.next;
	struct wl_list *tmp2_next = c2->link.next;

	// wl_list
	// 是双向链表,其中clients是头部节点,它的下一个节点是第一个客户端的链表节点
	// 最后一个客户端的链表节点的下一个节点也指向clients,但clients本身不是客户端的链表节点
	// 客户端遍历从clients的下一个节点开始,到检测到客户端节点的下一个是clients结束

	// 当c1和c2为相邻节点时
	if (c1->link.next == &c2->link) {
		c1->link.next = c2->link.next;
		c1->link.prev = &c2->link;
		c2->link.next = &c1->link;
		c2->link.prev = tmp1_prev;
		tmp1_prev->next = &c2->link;
		tmp2_next->prev = &c1->link;
	} else if (c2->link.next == &c1->link) {
		c2->link.next = c1->link.next;
		c2->link.prev = &c1->link;
		c1->link.next = &c2->link;
		c1->link.prev = tmp2_prev;
		tmp2_prev->next = &c1->link;
		tmp1_next->prev = &c2->link;
	} else { // 不为相邻节点
		c2->link.next = tmp1_next;
		c2->link.prev = tmp1_prev;
		c1->link.next = tmp2_next;
		c1->link.prev = tmp2_prev;

		tmp1_prev->next = &c2->link;
		tmp1_next->prev = &c2->link;
		tmp2_prev->next = &c1->link;
		tmp2_next->prev = &c1->link;
	}

	if (exchange_cross_monitor) {
		tmp_mon = c2->mon;
		tmp_tags = c2->tags;
		setmon(c2, c1->mon, c1->tags, false);
		setmon(c1, tmp_mon, tmp_tags, false);
		arrange(c1->mon, false);
		arrange(c2->mon, false);
		focusclient(c1, 0);
	} else {
		arrange(c1->mon, false);
		focusclient(c1, 0);
	}
}

void // 17
run(char *startup_cmd) {
	char autostart_temp_path[1024];
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	/* Start the backend. This will enumerate outputs and inputs, become the
	 * DRM master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the
	 * startup command */
	if (!startup_cmd)
		startup_cmd = get_autostart_path(autostart_temp_path,
										 sizeof(autostart_temp_path));
	if (startup_cmd) {
		int piperw[2];
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

	printstatus();

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

	run_exec();
	run_exec_once();

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */

	wl_display_run(dpy);
}

void setcursor(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client provides a cursor
	 * image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a
	 * enter event, which will result in the client requesting set the
	 * cursor surface
	 */
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == seat->pointer_state.focused_client) {
		last_cursor.shape = 0;
		last_cursor.surface = event->surface;
		last_cursor.hotspot_x = event->hotspot_x;
		last_cursor.hotspot_y = event->hotspot_y;
		if (!cursor_hidden)
			wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x,
								   event->hotspot_y);
	}
}

void // 0.5
setfloating(Client *c, int floating) {

	Client *fc;
	int hit;
	struct wlr_box target_box, backup_box;
	c->isfloating = floating;
	bool window_size_outofrange = false;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	target_box = c->geom;

	if (floating == 1 && c != grabc) {

		if (c->isfullscreen || c->ismaxmizescreen) {
			c->isfullscreen = 0; // 清除窗口全屏标志
			c->ismaxmizescreen = 0;
			c->bw = c->isnoborder ? 0 : borderpx;
		}

		// 重新计算居中的坐标
		if (!client_is_x11(c) || !client_should_ignore_focus(c))
			target_box = setclient_coordinate_center(c, target_box, 0, 0);
		backup_box = c->geom;
		hit = applyrulesgeom(c);
		target_box = hit == 1 ? c->geom : target_box;
		c->geom = backup_box;

		// restore to the memeroy geom
		if (c->float_geom.width > 0 && c->float_geom.height > 0) {
			if (c->mon && c->float_geom.width >= c->mon->w.width - gappoh) {
				c->float_geom.width = c->mon->w.width * 0.9;
				window_size_outofrange = true;
			}
			if (c->mon && c->float_geom.height >= c->mon->w.height - gappov) {
				c->float_geom.height = c->mon->w.height * 0.9;
				window_size_outofrange = true;
			}
			if (window_size_outofrange) {
				c->float_geom =
					setclient_coordinate_center(c, c->float_geom, 0, 0);
			}
			resize(c, c->float_geom, 0);
		} else {
			resize(c, target_box, 0);
		}

		c->need_float_size_reduce = 0;
	} else if (c->isfloating && c == grabc) {
		c->need_float_size_reduce = 0;
	} else {
		c->need_float_size_reduce = 1;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		// 让当前tag中的全屏窗口退出全屏参与平铺
		wl_list_for_each(fc, &clients,
						 link) if (fc && fc != c && VISIBLEON(fc, c->mon) &&
								   c->tags & fc->tags && ISFULLSCREEN(fc)) {
			clear_fullscreen_flag(fc);
		}
	}

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFSorOverTop]);
	} else {
		wlr_scene_node_reparent(&c->scene->node,
								layers[c->isfloating ? LyrFloat : LyrTile]);
	}

	arrange(c->mon, false);
	setborder_color(c);
	printstatus();
}

void reset_maxmizescreen_size(Client *c) {
	c->geom.x = c->mon->w.x + gappoh;
	c->geom.y = c->mon->w.y + gappov;
	c->geom.width = c->mon->w.width - 2 * gappoh;
	c->geom.height = c->mon->w.height - 2 * gappov;
	resize(c, c->geom, 0);
}

void setmaxmizescreen(Client *c, int maxmizescreen) {
	struct wlr_box maxmizescreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	c->ismaxmizescreen = maxmizescreen;

	if (maxmizescreen) {

		if (c->isfullscreen)
			setfullscreen(c, 0);

		if (c->isfloating)
			c->float_geom = c->geom;
		if (selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}

		maxmizescreen_box.x = c->mon->w.x + gappoh;
		maxmizescreen_box.y = c->mon->w.y + gappov;
		maxmizescreen_box.width = c->mon->w.width - 2 * gappoh;
		maxmizescreen_box.height = c->mon->w.height - 2 * gappov;
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
		resize(c, maxmizescreen_box, 0);
		c->ismaxmizescreen = 1;
	} else {
		c->bw = c->isnoborder ? 0 : borderpx;
		c->ismaxmizescreen = 0;
		if (c->isfloating)
			setfloating(c, 1);
		arrange(c->mon, false);
	}

	wlr_scene_node_reparent(&c->scene->node, layers[maxmizescreen	? LyrTile
													: c->isfloating ? LyrFloat
																	: LyrTile]);
}

void setfakefullscreen(Client *c, int fakefullscreen) {
	c->isfakefullscreen = fakefullscreen;
	if (!c->mon)
		return;
	if (c->isfullscreen)
		setfullscreen(c, 0);
	else
		client_set_fullscreen(c, fakefullscreen);
}

void setfullscreen(Client *c, int fullscreen) // 用自定义全屏代理自带全屏
{
	c->isfullscreen = fullscreen;

	if (!c || !c->mon || !client_surface(c)->mapped || c->iskilling)
		return;

	client_set_fullscreen(c, fullscreen);

	if (fullscreen) {
		if (c->ismaxmizescreen)
			setmaxmizescreen(c, 0);

		if (c->isfloating)
			c->float_geom = c->geom;
		if (selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}

		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node); // 将视图提升到顶层
		resize(c, c->mon->m, 1);
		c->isfullscreen = 1;
		// c->isfloating = 0;
	} else {
		c->bw = c->isnoborder ? 0 : borderpx;
		c->isfullscreen = 0;
		c->isfakefullscreen = 0;
		if (c->isfloating)
			setfloating(c, 1);
	}

	if (c->isoverlay) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrOverlay]);
	} else if (client_should_overtop(c) && c->isfloating) {
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFSorOverTop]);
	} else {
		wlr_scene_node_reparent(&c->scene->node,
								layers[fullscreen	   ? LyrFSorOverTop
									   : c->isfloating ? LyrFloat
													   : LyrTile]);
	}

	arrange(c->mon, false);
}

void setgaps(int oh, int ov, int ih, int iv) {
	selmon->gappoh = MAX(oh, 0);
	selmon->gappov = MAX(ov, 0);
	selmon->gappih = MAX(ih, 0);
	selmon->gappiv = MAX(iv, 0);
	arrange(selmon, false);
}

void reset_keyboard_layout(void) {
	if (!kb_group || !kb_group->wlr_group || !seat) {
		wlr_log(WLR_ERROR, "Invalid keyboard group or seat");
		return;
	}

	struct wlr_keyboard *keyboard = &kb_group->wlr_group->keyboard;
	if (!keyboard || !keyboard->keymap) {
		wlr_log(WLR_ERROR, "Invalid keyboard or keymap");
		return;
	}

	// Get current layout
	xkb_layout_index_t current = xkb_state_serialize_layout(
		keyboard->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	const int num_layouts = xkb_keymap_num_layouts(keyboard->keymap);
	if (num_layouts < 1) {
		wlr_log(WLR_INFO, "No layouts available");
		return;
	}

	// Create context
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!context) {
		wlr_log(WLR_ERROR, "Failed to create XKB context");
		return;
	}

	// Get layout abbreviations
	const char **layout_ids = calloc(num_layouts, sizeof(char *));
	if (!layout_ids) {
		wlr_log(WLR_ERROR, "Failed to allocate layout IDs");
		goto cleanup_context;
	}

	for (int i = 0; i < num_layouts; i++) {
		layout_ids[i] =
			get_layout_abbr(xkb_keymap_layout_get_name(keyboard->keymap, i));
		if (!layout_ids[i]) {
			wlr_log(WLR_ERROR, "Failed to get layout abbreviation");
			goto cleanup_layouts;
		}
	}

	// Keep the same rules but just reapply them
	struct xkb_rule_names rules = xkb_rules;

	// Create new keymap with current rules
	struct xkb_keymap *new_keymap =
		xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!new_keymap) {
		wlr_log(WLR_ERROR, "Failed to create keymap for layouts: %s",
				rules.layout);
		goto cleanup_layouts;
	}

	// Apply the same keymap (this will reset the layout state)
	unsigned int depressed = keyboard->modifiers.depressed;
	unsigned int latched = keyboard->modifiers.latched;
	unsigned int locked = keyboard->modifiers.locked;

	wlr_keyboard_set_keymap(keyboard, new_keymap);

	wlr_keyboard_notify_modifiers(keyboard, depressed, latched, locked, 0);
	keyboard->modifiers.group = current; // Keep the same layout index

	// Update seat
	wlr_seat_set_keyboard(seat, keyboard);
	wlr_seat_keyboard_notify_modifiers(seat, &keyboard->modifiers);

	// Cleanup
	xkb_keymap_unref(new_keymap);

cleanup_layouts:
	free(layout_ids);

cleanup_context:
	xkb_context_unref(context);
}

void setmon(Client *c, Monitor *m, unsigned int newtags, bool focus) {
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;

	if (oldmon && oldmon->sel == c) {
		oldmon->sel = NULL;
	}

	if (oldmon && oldmon->prevsel == c) {
		oldmon->prevsel = NULL;
	}

	c->mon = m;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon, false);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		resize(c, c->geom, 0);
		c->tags =
			newtags ? newtags
					: m->tagset[m->seltags]; /* assign tags of target monitor */
		setfloating(c, c->isfloating);
		setfullscreen(c, c->isfullscreen); /* This will call arrange(c->mon) */
	}
	if (m && focus)
		focusclient(focustop(m), 1);

	if (m) {

		if (c->foreign_toplevel) {
			remove_foreign_topleve(c);
		}

		add_foreign_toplevel(c);
		if (m->sel && m->sel->foreign_toplevel)
			wlr_foreign_toplevel_handle_v1_set_activated(
				m->sel->foreign_toplevel, false);
		if (c->foreign_toplevel)
			wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,
														 true);
	}
}

void setpsel(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void setsel(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the
	 * selection, usually when the user copies something. wlroots allows
	 * compositors to ignore such requests if they so choose, but in dwl we
	 * always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

void show_hide_client(Client *c) {

	unsigned int target = get_tags_first_tag(c->oldtags);
	tag_client(&(Arg){.ui = target}, c);
	// c->tags = c->oldtags;
	c->isminied = 0;
	wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel, false);
	focusclient(c, 1);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel, true);
}

void create_output(struct wlr_backend *backend, void *data) {
	bool *done = data;
	if (*done) {
		return;
	}

	if (wlr_backend_is_wl(backend)) {
		wlr_wl_output_create(backend);
		*done = true;
	} else if (wlr_backend_is_headless(backend)) {
		wlr_headless_add_output(backend, 1920, 1080);
		*done = true;
	}
#if WLR_HAS_X11_BACKEND
	else if (wlr_backend_is_x11(backend)) {
		wlr_x11_output_create(backend);
		*done = true;
	}
#endif
}

void setup(void) {

	setenv("XCURSOR_SIZE", "24", 1);
	setenv("XDG_CURRENT_DESKTOP", "mango", 1);

	parse_config();
	init_baked_points();

	int drm_fd, i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	wlr_log_init(log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();
	event_loop = wl_display_get_event_loop(dpy);
	pointer_manager = wlr_relative_pointer_manager_v1_create(dpy);
	tablet_mgr = wlr_tablet_v2_create(dpy);
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. The NULL argument here optionally allows you
	 * to pass in a custom renderer if wlr_renderer doesn't meet your needs. The
	 * backend uses the renderer, for example, to fall back to software cursors
	 * if the backend does not support hardware cursors (some older GPUs
	 * don't). */
	if (!(backend = wlr_backend_autocreate(event_loop, &session)))
		die("couldn't create backend");

	headless_backend = wlr_headless_backend_create(event_loop);
	if (!headless_backend) {
		wlr_log(WLR_ERROR, "Failed to create secondary headless backend");
	} else {
		wlr_multi_backend_add(backend, headless_backend);
	}

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Create a renderer with the default implementation */
	if (!(drw = fx_renderer_create(backend)))
		die("couldn't create renderer");

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
			scene, wlr_linux_dmabuf_v1_create_with_renderer(dpy, 5, drw));
	}

	if (syncobj_enable && (drm_fd = wlr_renderer_get_drm_fd(drw)) >= 0 &&
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
	wlr_ext_image_copy_capture_manager_v1_create(dpy, 1);
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

	/* Initializes the interface used to implement urgency hints */
	activation = wlr_xdg_activation_v1_create(dpy);
	wl_signal_add(&activation->events.request_activate, &request_activate);

	wlr_scene_set_gamma_control_manager_v1(
		scene, wlr_gamma_control_manager_v1_create(dpy));

	power_mgr = wlr_output_power_manager_v1_create(dpy);
	wl_signal_add(&power_mgr->events.set_mode, &output_power_mgr_set_mode);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create(dpy);
	wl_signal_add(&output_layout->events.change, &layout_change);
	wlr_xdg_output_manager_v1_create(dpy, output_layout);

	/* Configure a listener to be notified when new outputs are available on
	 * the backend. */
	wl_list_init(&mons);
	wl_signal_add(&backend->events.new_output, &new_output);

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
	// cursor_mgr = wlr_xcursor_manager_create(cursor_theme, 24);
	cursor_mgr = wlr_xcursor_manager_create(config.cursor_theme, cursor_size);

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
	hide_source = wl_event_loop_add_timer(wl_display_get_event_loop(dpy),
										  hidecursor, cursor);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener
	 * to let us know when new input devices are available on the backend.
	 */
	wl_list_init(&keyboards);
	wl_list_init(&inputdevices);
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

	seat = wlr_seat_create(dpy, "seat0");
	wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
	wl_signal_add(&seat->events.request_set_selection, &request_set_sel);
	wl_signal_add(&seat->events.request_set_primary_selection,
				  &request_set_psel);
	wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
	wl_signal_add(&seat->events.start_drag, &start_drag);

	kb_group = createkeyboardgroup();
	wl_list_init(&kb_group->destroy.link);

	output_mgr = wlr_output_manager_v1_create(dpy);
	wl_signal_add(&output_mgr->events.apply, &output_mgr_apply);
	wl_signal_add(&output_mgr->events.test, &output_mgr_test);

	// blur
	wlr_scene_set_blur_data(scene, blur_params.num_passes, blur_params.radius,
							blur_params.noise, blur_params.brightness,
							blur_params.contrast, blur_params.saturation);

	/* create text_input-, and input_method-protocol relevant globals */
	input_method_manager = wlr_input_method_manager_v2_create(dpy);
	text_input_manager = wlr_text_input_manager_v3_create(dpy);

	dwl_input_method_relay = dwl_im_relay_create();

	wl_global_create(dpy, &zdwl_ipc_manager_v2_interface, 2, NULL,
					 dwl_ipc_manager_bind);

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
	xwayland = wlr_xwayland_create(dpy, compositor, !xwayland_persistence);
	if (xwayland) {
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr,
				"failed to setup XWayland X server, continuing without it\n");
	}
#endif
}

void startdrag(struct wl_listener *listener, void *data) {
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

void tag_client(const Arg *arg, Client *target_client) {
	Client *fc;
	if (target_client && arg->ui & TAGMASK) {

		target_client->tags = arg->ui & TAGMASK;
		target_client->istagswitching = 1;

		wl_list_for_each(fc, &clients, link) {
			if (fc && fc != target_client && target_client->tags & fc->tags &&
				ISFULLSCREEN(fc) && !target_client->isfloating) {
				clear_fullscreen_flag(fc);
			}
		}
		view(&(Arg){.ui = arg->ui, .i = arg->i}, true);

	} else {
		view(arg, true);
	}

	focusclient(target_client, 1);
	printstatus();
}

void overview(Monitor *m) { grid(m); }

// 目标窗口有其他窗口和它同个tag就返回0
unsigned int want_restore_fullscreen(Client *target_client) {
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (c && c != target_client && c->tags == target_client->tags &&
			c == selmon->sel) {
			return 0;
		}
	}

	return 1;
}

// 普通视图切换到overview时保存窗口的旧状态
void overview_backup(Client *c) {
	c->overview_isfloatingbak = c->isfloating;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->overview_ismaxmizescreenbak = c->ismaxmizescreen;
	c->overview_isfullscreenbak = c->isfullscreen;
	c->animation.tagining = false;
	c->animation.tagouted = false;
	c->animation.tagouting = false;
	c->overview_backup_geom = c->geom;
	c->overview_backup_bw = c->bw;
	if (c->isfloating) {
		c->isfloating = 0;
	}
	if (c->isfullscreen || c->ismaxmizescreen) {
		c->isfullscreen = 0; // 清除窗口全屏标志
		c->ismaxmizescreen = 0;
	}
	c->bw = c->isnoborder ? 0 : borderpx;
}

// overview切回到普通视图还原窗口的状态
void overview_restore(Client *c, const Arg *arg) {
	c->isfloating = c->overview_isfloatingbak;
	c->isfullscreen = c->overview_isfullscreenbak;
	c->ismaxmizescreen = c->overview_ismaxmizescreenbak;
	c->overview_isfloatingbak = 0;
	c->overview_isfullscreenbak = 0;
	c->overview_ismaxmizescreenbak = 0;
	c->geom = c->overview_backup_geom;
	c->bw = c->overview_backup_bw;
	c->animation.tagining = false;
	c->is_restoring_from_ov = (arg->ui & c->tags & TAGMASK) == 0 ? true : false;

	if (c->isfloating) {
		// XRaiseWindow(dpy, c->win); // 提升悬浮窗口到顶层
		resize(c, c->overview_backup_geom, 0);
	} else if (c->isfullscreen || c->ismaxmizescreen) {
		if (want_restore_fullscreen(c) && c->ismaxmizescreen) {
			setmaxmizescreen(c, 1);
		} else if (want_restore_fullscreen(c) && c->isfullscreen) {
			setfullscreen(c, 1);
		} else {
			c->isfullscreen = 0;
			c->ismaxmizescreen = 0;
			setfullscreen(c, false);
		}
	} else {
		if (c->is_restoring_from_ov) {
			c->is_restoring_from_ov = false;
			resize(c, c->overview_backup_geom, 0);
		}
	}

	if (c->bw == 0 &&
		!c->isfullscreen) { // 如果是在ov模式中创建的窗口,没有bw记录
		c->bw = c->isnoborder ? 0 : borderpx;
	}
}

void handlecursoractivity(void) {
	wl_event_source_timer_update(hide_source, cursor_hide_timeout * 1000);

	if (!cursor_hidden)
		return;

	cursor_hidden = false;

	if (last_cursor.shape)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
							   wlr_cursor_shape_v1_name(last_cursor.shape));
	else
		wlr_cursor_set_surface(cursor, last_cursor.surface,
							   last_cursor.hotspot_x, last_cursor.hotspot_y);
}

int hidecursor(void *data) {
	wlr_cursor_unset_image(cursor);
	cursor_hidden = true;
	return 1;
}

// 显示所有tag 或 跳转到聚焦窗口的tag
void toggleoverview(const Arg *arg) {
	Client *c;

	if (selmon->isoverview && ov_tab_mode && arg->i != -1 && selmon->sel) {
		focusstack(&(Arg){.i = 1});
		return;
	}

	selmon->isoverview ^= 1;
	unsigned int target;
	unsigned int visible_client_number = 0;

	if (selmon->isoverview) {
		wl_list_for_each(c, &clients,
						 link) if (c && c->mon == selmon &&
								   !client_is_unmanaged(c) &&
								   !client_should_ignore_focus(c) &&
								   !c->isminied && !c->isunglobal) {
			visible_client_number++;
		}
		if (visible_client_number > 0) {
			target = ~0 & TAGMASK;
		} else {
			selmon->isoverview ^= 1;
			return;
		}
	} else if (!selmon->isoverview && selmon->sel) {
		target = get_tags_first_tag(selmon->sel->tags);
	} else if (!selmon->isoverview && !selmon->sel) {
		target = (1 << (selmon->pertag->prevtag - 1));
		view(&(Arg){.ui = target}, false);
		refresh_monitors_workspaces_status(selmon);
		return;
	}

	// 正常视图到overview,退出所有窗口的浮动和全屏状态参与平铺,
	// overview到正常视图,还原之前退出的浮动和全屏窗口状态
	if (selmon->isoverview) {
		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && !client_is_unmanaged(c) &&
				!client_should_ignore_focus(c) && !c->isunglobal)
				overview_backup(c);
		}
	} else {
		wl_list_for_each(c, &clients, link) {
			if (c && c->mon == selmon && !c->iskilling &&
				!client_is_unmanaged(c) && !c->isunglobal &&
				!client_should_ignore_focus(c) && client_surface(c)->mapped)
				overview_restore(c, &(Arg){.ui = target});
		}
	}

	view(&(Arg){.ui = target}, false);

	if (ov_tab_mode && selmon->isoverview && selmon->sel) {
		focusstack(&(Arg){.i = 1});
	}

	refresh_monitors_workspaces_status(selmon);
}

void unlocksession(struct wl_listener *listener, void *data) {
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

void unmaplayersurfacenotify(struct wl_listener *listener, void *data) {
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;

	init_fadeout_layers(l);

	wlr_scene_node_set_enabled(&l->scene->node, false);
	if (l == exclusive_focus)
		exclusive_focus = NULL;
	if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
		arrangelayers(l->mon);
	if (l->layer_surface->surface == seat->keyboard_state.focused_surface)
		focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void unmapnotify(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown.
	 */
	Client *c = wl_container_of(listener, c, unmap);
	Monitor *m;
	c->iskilling = 1;

	if (animations && !c->is_clip_to_hide && !c->isminied &&
		(!c->mon || VISIBLEON(c, c->mon)))
		init_fadeout_client(c);

	if (c->swallowedby) {
		c->swallowedby->mon = c->mon;
		swallow(c->swallowedby, c);
	}

	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		if (c == m->sel) {
			m->sel = NULL;
		}
		if (c == m->prevsel) {
			m->prevsel = NULL;
		}
	}

	if (c->mon && c->mon == selmon) {
		Client *nextfocus = focustop(selmon);

		if (nextfocus) {
			focusclient(nextfocus, 0);
		}

		if (!nextfocus && selmon->isoverview) {
			Arg arg = {0};
			toggleoverview(&arg);
		}
	}

	if (client_is_unmanaged(c)) {
#ifdef XWAYLAND
		if (client_is_x11(c)) {
			wl_list_remove(&c->set_geometry.link);
		}
#endif
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {
		if (!c->swallowing)
			wl_list_remove(&c->link);
		setmon(c, NULL, 0, true);
		if (!c->swallowing)
			wl_list_remove(&c->flink);
	}

	if (c->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	if (c->swallowedby) {
		setfullscreen(c->swallowedby, c->isfullscreen);
		setmaxmizescreen(c->swallowedby, c->ismaxmizescreen);
		c->swallowedby->swallowing = NULL;
		c->swallowedby = NULL;
	}

	if (c->swallowing) {
		c->swallowing->swallowedby = NULL;
		c->swallowing = NULL;
	}

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void updatemons(struct wl_listener *listener, void *data) {
	/*
	 * Called whenever the output layout changes: adding or removing a
	 * monitor, changing an output's mode or position, etc. This is where
	 * the change officially happens and we update geometry, window
	 * positions, focus, and the stored configuration in wlroots'
	 * output-manager implementation.
	 */
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	Client *c;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m;
	int mon_pos_offsetx, mon_pos_offsety, oldx, oldy;

	/* First remove from the layout the disabled monitors */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled || m->asleep)
			continue;
		config_head =
			wlr_output_configuration_head_v1_create(config, m->wlr_output);
		config_head->state.enabled = 0;
		/* Remove this output from the layout to avoid cursor enter inside
		 * it */
		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){0};
	}
	/* Insert outputs that need to */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled &&
			!wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout, m->wlr_output);
	}

	/* Now that we update the output layout we can get its box */
	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

	/* Make sure the clients are hidden when dwl is locked */
	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head =
			wlr_output_configuration_head_v1_create(config, m->wlr_output);

		oldx = m->m.x;
		oldy = m->m.y;
		/* Get the effective monitor geometry to use for surfaces */
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		mon_pos_offsetx = m->m.x - oldx;
		mon_pos_offsety = m->m.y - oldy;

		wl_list_for_each(c, &clients, link) {
			// floating window position auto adjust the change of monitor
			// position
			if (c->isfloating && c->mon == m) {
				c->geom.x += mon_pos_offsetx;
				c->geom.y += mon_pos_offsety;
				c->float_geom = c->geom;
				resize(c, c->geom, 1);
			}

			// restore window to old monitor
			if (c->mon && c->mon != m && client_surface(c)->mapped &&
				strcmp(c->oldmonname, m->wlr_output->name) == 0) {
				client_change_mon(c, m);
			}
		}

		/*
		 must put it under the floating window position adjustment,
		 Otherwise, incorrect floating window calculations will occur here.
		 */
		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		if (blur && m->blur) {
			wlr_scene_node_set_position(&m->blur->node, m->m.x, m->m.y);
			wlr_scene_optimized_blur_set_size(m->blur, m->m.width, m->m.height);
		}

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width,
												  m->m.height);
		}

		/* Calculate the effective monitor geometry to use for clients */
		arrangelayers(m);
		/* Don't move clients to the left output when plugging monitors */
		arrange(m, false);
		/* make sure fullscreen clients have the right size */
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		/* Try to re-set the gamma LUT when updating monitors,
		 * it's only really needed when enabling a disabled output, but meh.
		 */
		m->gamma_lut_changed = 1;

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;

		if (!selmon)
			selmon = m;
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped) {
				client_change_mon(c, selmon);
			}
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
								wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface, 1);
		}
	}

	/* FIXME: figure out why the cursor image is at 0,0 after turning all
	 * the monitors on.
	 * Move the cursor image where it used to be. It does not generate a
	 * wl_pointer.motion event for the clients, it's only the image what
	 * it's at the wrong position after all. */
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, config);
}

void updatetitle(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_title);

	if (!c || c->iskilling)
		return;

	const char *title;
	title = client_get_title(c);
	if (title && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel, title);
	if (c == focustop(c->mon))
		printstatus();
}

void // 17 fix to 0.5
urgent(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	if (focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view(&(Arg){.ui = c->tags}, true);
		focusclient(c, 1);
	} else if (c != focustop(selmon)) {
		if (client_surface(c)->mapped)
			client_set_border_color(c, urgentcolor);
		c->isurgent = 1;
		printstatus();
	}
}

void view_in_mon(const Arg *arg, bool want_animation, Monitor *m,
				 bool changefocus) {
	unsigned int i, tmptag;

	if (!m || (arg->ui != (~0 & TAGMASK) && m->isoverview)) {
		return;
	}

	if (arg->ui == 0) {
		return;
	}

	if (arg->ui == UINT32_MAX) {
		m->pertag->prevtag = m->tagset[m->seltags];
		m->seltags ^= 1; /* toggle sel tagset */
		m->pertag->curtag = m->tagset[m->seltags];
		goto toggleseltags;
	}

	if ((m->tagset[m->seltags] & arg->ui & TAGMASK) != 0) {
		want_animation = false;
	}

	m->seltags ^= 1; /* toggle sel tagset */

	if (arg->ui & TAGMASK) {
		m->tagset[m->seltags] = arg->ui & TAGMASK;
		tmptag = m->pertag->curtag;

		if (arg->ui == (~0 & TAGMASK))
			m->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i) && i < LENGTH(tags) && arg->ui != 0;
				 i++)
				;
			m->pertag->curtag = i >= LENGTH(tags) ? LENGTH(tags) : i + 1;
		}

		m->pertag->prevtag =
			tmptag == m->pertag->curtag ? m->pertag->prevtag : tmptag;
	} else {
		tmptag = m->pertag->prevtag;
		m->pertag->prevtag = m->pertag->curtag;
		m->pertag->curtag = tmptag;
	}

toggleseltags:

	if (changefocus)
		focusclient(focustop(m), 1);
	arrange(m, want_animation);
	printstatus();
}

void view(const Arg *arg, bool want_animation) {
	Monitor *m;
	if (arg->i) {
		view_in_mon(arg, want_animation, selmon, true);
		wl_list_for_each(m, &mons, link) {
			if (!m->wlr_output->enabled || m == selmon)
				continue;
			// only arrange, not change monitor focus
			view_in_mon(arg, want_animation, m, false);
		}
	} else {
		view_in_mon(arg, want_animation, selmon, true);
	}
}

void virtualkeyboard(struct wl_listener *listener, void *data) {
	struct wlr_virtual_keyboard_v1 *kb = data;
	/* virtual keyboards shouldn't share keyboard group */
	KeyboardGroup *group = createkeyboardgroup();
	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(&kb->keyboard, group->wlr_group->keyboard.keymap);
	LISTEN(&kb->keyboard.base.events.destroy, &group->destroy,
		   destroykeyboardgroup);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(group->wlr_group, &kb->keyboard);
}

void warp_cursor(const Client *c) {
	if (cursor->x < c->geom.x || cursor->x > c->geom.x + c->geom.width ||
		cursor->y < c->geom.y || cursor->y > c->geom.y + c->geom.height) {
		wlr_cursor_warp_closest(cursor, NULL, c->geom.x + c->geom.width / 2.0,
								c->geom.y + c->geom.height / 2.0);
		motionnotify(0, NULL, 0, 0, 0, 0);
	}
}

void warp_cursor_to_selmon(Monitor *m) {

	wlr_cursor_warp_closest(cursor, NULL, m->w.x + m->w.width / 2.0,
							m->w.y + m->w.height / 2.0);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
	handlecursoractivity();
}

void virtualpointer(struct wl_listener *listener, void *data) {
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_input_device *device = &event->new_pointer->pointer.base;

	wlr_cursor_attach_input_device(cursor, device);
	if (event->suggested_output)
		wlr_cursor_map_input_to_output(cursor, device, event->suggested_output);

	handlecursoractivity();
}

#ifdef XWAYLAND
void activatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, activate);
	bool need_arrange = false;

	if (!c || c->iskilling || !c->foreign_toplevel || client_is_unmanaged(c))
		return;

	if (c && c->swallowing)
		return;

	if (c->isminied) {
		c->isminied = 0;
		c->tags = c->mini_restore_tag;
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
		c->isnamedscratchpad = 0;
		wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,
													 false);
		setborder_color(c);
		if (VISIBLEON(c, c->mon)) {
			need_arrange = true;
		}
	}

	if (focus_on_activate && !c->istagsilent && c != selmon->sel) {
		if (!(c->mon == selmon && c->tags & c->mon->tagset[c->mon->seltags]))
			view(&(Arg){.ui = c->tags}, true);
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
		focusclient(c, 1);
		need_arrange = true;
	} else if (c != focustop(selmon)) {
		c->isurgent = 1;
		if (client_surface(c)->mapped)
			client_set_border_color(c, urgentcolor);
	}

	if (need_arrange) {
		arrange(c->mon, false);
	}

	printstatus();
}

void configurex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!client_surface(c) || !client_surface(c)->mapped) {
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if (client_is_unmanaged(c)) {
		wlr_scene_node_set_position(&c->scene->node, event->x, event->y);
		wlr_xwayland_surface_configure(c->surface.xwayland, event->x, event->y,
									   event->width, event->height);
		return;
	}
	if ((c->isfloating && c != grabc) ||
		!c->mon->pertag->ltidxs[c->mon->pertag->curtag]->arrange) {
		resize(c,
			   (struct wlr_box){.x = event->x - c->bw,
								.y = event->y - c->bw,
								.width = event->width + c->bw * 2,
								.height = event->height + c->bw * 2},
			   0);
	} else {
		arrange(c->mon, false);
	}
}

void createnotifyx11(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;
	Client *c;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
		   fullscreennotify);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}

void associatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

void dissociatex11(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
}

void sethints(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon) || !c || !c->surface.xwayland->hints)
		return;

	c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus();

	if (c->isurgent && surface && surface->mapped)
		client_set_border_color(c, urgentcolor);
}

void xwaylandready(struct wl_listener *listener, void *data) {
	struct wlr_xcursor *xcursor;

	/* assign the one and only seat */
	wlr_xwayland_set_seat(xwayland, seat);

	/* Set the default XWayland cursor to match the rest of dwl. */
	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(
			xwayland, xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
			xcursor->images[0]->width, xcursor->images[0]->height,
			xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);
}

static void setgeometrynotify(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, set_geometry);

	wlr_scene_node_set_position(&c->scene->node, c->surface.xwayland->x,
								c->surface.xwayland->y);
	motionnotify(0, NULL, 0, 0, 0, 0);
}
#endif

static Monitor *find_monitor_by_name(const char *output_name) {
	if (!output_name) return NULL;
	Monitor *m;
	wl_list_for_each(m, &mons, link) {
			if (m->wlr_output && strcmp(m->wlr_output->name, output_name) == 0) {
				return m;
			}
		}
	return NULL;
}

int main(int argc, char *argv[]) {
	char *startup_cmd = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:hdv")) != -1) {
		if (c == 's')
			startup_cmd = optarg;
		else if (c == 'd')
			log_level = WLR_DEBUG;
		else if (c == 'v')
			die("mango " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications
	 * socket
	 */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v] [-d] [-s startup command]", argv[0]);
}
