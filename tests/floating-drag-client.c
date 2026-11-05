#define _POSIX_C_SOURCE 200809L
#include "virtual-pointer-client.h"
#include "xdg-shell-client.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client.h>
#ifdef HAVE_XCB
#include <xcb/xcb.h>

static xcb_connection_t *xconnection;
static xcb_window_t xwindow;
static xcb_screen_t *xscreen;
static xcb_atom_t move_atom;

static void synchronize_x11(void) {
	xcb_flush(xconnection);
	free(xcb_get_input_focus_reply(xconnection,
								  xcb_get_input_focus(xconnection), NULL));
}

static void create_x11(const char *display_name) {
	int screen_number;
	xconnection = xcb_connect(display_name, &screen_number);
	if (xcb_connection_has_error(xconnection))
		exit(1);
	xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(xconnection));
	for (int index = 0; index < screen_number; index++)
		xcb_screen_next(&screens);
	xscreen = screens.data;
	xwindow = xcb_generate_id(xconnection);
	uint32_t background = 0x999999;
	xcb_create_window(xconnection, XCB_COPY_FROM_PARENT, xwindow, xscreen->root,
		80, 90, 420, 280, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, xscreen->root_visual,
		XCB_CW_BACK_PIXEL, &background);
	const char class_name[] = "mango-floating-x11-test\0mango-floating-x11-test";
	xcb_change_property(xconnection, XCB_PROP_MODE_REPLACE, xwindow, XCB_ATOM_WM_CLASS,
		XCB_ATOM_STRING, 8, sizeof(class_name), class_name);
	const char title[] = "mango-floating-x11-test";
	xcb_change_property(xconnection, XCB_PROP_MODE_REPLACE, xwindow, XCB_ATOM_WM_NAME,
		XCB_ATOM_STRING, 8, sizeof(title) - 1, title);
	const char atom_name[] = "_NET_WM_MOVERESIZE";
	xcb_intern_atom_reply_t *atom = xcb_intern_atom_reply(xconnection,
		xcb_intern_atom(xconnection, 0, sizeof(atom_name) - 1, atom_name), NULL);
	if (!atom)
		exit(1);
	move_atom = atom->atom;
	free(atom);
	xcb_map_window(xconnection, xwindow);
	xcb_clear_area(xconnection, 0, xwindow, 0, 0, 420, 280);
	synchronize_x11();
}

static void request_x11_move(void) {
	xcb_client_message_event_t event = {
		.response_type = XCB_CLIENT_MESSAGE,
		.format = 32,
		.window = xwindow,
		.type = move_atom,
		.data.data32 = {120, 130, 8, 1, 1},
	};
	xcb_send_event(xconnection, 0, xscreen->root,
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
		(const char *)&event);
	synchronize_x11();
}
#endif

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_shm *shared_memory;
static struct wl_seat *seat;
static struct wl_pointer *pointer;
static struct xdg_wm_base *shell;
static struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
static struct zwlr_virtual_pointer_v1 *virtual_pointer;
static struct wl_surface *surface;
static struct xdg_surface *shell_surface;
static struct xdg_toplevel *toplevel;
static uint32_t press_serial, presses, releases;
static int32_t width = 420, height = 280;
static int move_mode;
static bool configured;
static bool frame_done;

struct Buffer {
	void *pixels;
	size_t length;
};

static void release_buffer(void *data, struct wl_buffer *buffer) {
	struct Buffer *allocation = data;
	wl_buffer_destroy(buffer);
	munmap(allocation->pixels, allocation->length);
	free(allocation);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = release_buffer,
};

static void configure_surface(void *data, struct xdg_surface *configured_surface,
							  uint32_t serial) {
	xdg_surface_ack_configure(configured_surface, serial);
	struct Buffer *allocation = calloc(1, sizeof(*allocation));
	FILE *file = tmpfile();
	if (!allocation || !file)
		exit(1);
	allocation->length = (size_t)width * height * 4;
	if (ftruncate(fileno(file), allocation->length) < 0)
		exit(1);
	allocation->pixels = mmap(NULL, allocation->length, PROT_READ | PROT_WRITE,
							 MAP_SHARED, fileno(file), 0);
	if (allocation->pixels == MAP_FAILED)
		exit(1);
	memset(allocation->pixels, 0x99, allocation->length);
	struct wl_shm_pool *pool = wl_shm_create_pool(shared_memory, fileno(file),
												 allocation->length);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888);
	wl_buffer_add_listener(buffer, &buffer_listener, allocation);
	wl_shm_pool_destroy(pool);
	fclose(file);
	wl_surface_attach(surface, buffer, 0, 0);
	wl_surface_damage(surface, 0, 0, width, height);
	wl_surface_commit(surface);
	configured = true;
}

static const struct xdg_surface_listener surface_listener = {
	.configure = configure_surface,
};

static void configure_toplevel(void *data, struct xdg_toplevel *configured_toplevel,
							   int32_t new_width, int32_t new_height,
							   struct wl_array *states) {
	if (new_width > 0)
		width = new_width;
	if (new_height > 0)
		height = new_height;
}

static void close_toplevel(void *data, struct xdg_toplevel *closed_toplevel) {
	exit(0);
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = configure_toplevel,
	.close = close_toplevel,
};

static void ping_shell(void *data, struct xdg_wm_base *pinged_shell, uint32_t serial) {
	xdg_wm_base_pong(pinged_shell, serial);
}

static const struct xdg_wm_base_listener shell_listener = {.ping = ping_shell};

static void pointer_enter(void *data, struct wl_pointer *entered_pointer,
						  uint32_t serial, struct wl_surface *entered_surface,
						  wl_fixed_t horizontal, wl_fixed_t vertical) {}
static void pointer_leave(void *data, struct wl_pointer *left_pointer,
						  uint32_t serial, struct wl_surface *left_surface) {}
static void pointer_motion(void *data, struct wl_pointer *moved_pointer,
						   uint32_t time, wl_fixed_t horizontal, wl_fixed_t vertical) {}
static void pointer_axis(void *data, struct wl_pointer *axis_pointer,
						 uint32_t time, uint32_t axis, wl_fixed_t value) {}

static void pointer_button(void *data, struct wl_pointer *button_pointer,
						   uint32_t serial, uint32_t time, uint32_t button,
						   uint32_t state) {
	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		presses++;
		press_serial = serial;
		if (move_mode)
			xdg_toplevel_move(toplevel, seat, move_mode == 1 ? serial : serial + 1000);
	} else {
		releases++;
	}
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
};

static void seat_capabilities(void *data, struct wl_seat *updated_seat,
							  uint32_t capabilities) {
	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
		pointer = wl_seat_get_pointer(updated_seat);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
							const char *interface, uint32_t version) {
	if (strcmp(interface, wl_compositor_interface.name) == 0)
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
	else if (strcmp(interface, wl_shm_interface.name) == 0)
		shared_memory = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(seat, &seat_listener, NULL);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		shell = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(shell, &shell_listener, NULL);
	} else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0)
		pointer_manager = wl_registry_bind(
			registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_remove,
};

static void synchronize(void) {
	for (int roundtrip = 0; roundtrip < 3; roundtrip++)
		if (wl_display_roundtrip(display) < 0)
			exit(1);
}

static void finish_frame(void *data, struct wl_callback *callback, uint32_t time) {
	frame_done = true;
	wl_callback_destroy(callback);
}

static const struct wl_callback_listener frame_listener = {.done = finish_frame};

int main(int argc, char **argv) {
	if (argc == 3 && strcmp(argv[1], "--display-socket") == 0) {
		int connection = socket(AF_UNIX, SOCK_STREAM, 0);
		struct sockaddr_un address = {.sun_family = AF_UNIX};
		snprintf(address.sun_path, sizeof(address.sun_path), "%s", argv[2]);
		if (connect(connection, (struct sockaddr *)&address, sizeof(address)) < 0)
			return 1;
		const char *display_name = getenv("DISPLAY");
		if (display_name && write(connection, display_name, strlen(display_name)) !=
			(ssize_t)strlen(display_name))
			return 1;
		close(connection);
		return 0;
	}
	display = wl_display_connect(NULL);
	if (!display)
		return 1;
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	synchronize();
	if (!compositor || !shared_memory || !seat || !shell || !pointer_manager)
		return 1;
	virtual_pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(pointer_manager, seat);
	surface = wl_compositor_create_surface(compositor);
	shell_surface = xdg_wm_base_get_xdg_surface(shell, surface);
	xdg_surface_add_listener(shell_surface, &surface_listener, NULL);
	toplevel = xdg_surface_get_toplevel(shell_surface);
	xdg_toplevel_add_listener(toplevel, &toplevel_listener, NULL);
	xdg_toplevel_set_app_id(toplevel, "mango-floating-drag-test");
	xdg_toplevel_set_title(toplevel, "mango-floating-drag-test");
	wl_surface_commit(surface);
	while (!configured)
		if (wl_display_dispatch(display) < 0)
			return 1;
	synchronize();
	puts("{\"ready\":true}");
	fflush(stdout);
	char command[128];
	while (fgets(command, sizeof(command), stdin)) {
		int horizontal, vertical, value;
#ifdef HAVE_XCB
		char display_name[64];
		if (sscanf(command, "x11 %63s", display_name) == 1) {
			create_x11(display_name);
		} else if (strncmp(command, "xmove", 5) == 0) {
			request_x11_move();
		} else if (strncmp(command, "xclose", 6) == 0) {
			xcb_disconnect(xconnection);
			xconnection = NULL;
		} else
#endif
		if (sscanf(command, "move %d %d", &horizontal, &vertical) == 2) {
			zwlr_virtual_pointer_v1_motion(virtual_pointer, 1,
				wl_fixed_from_int(horizontal), wl_fixed_from_int(vertical));
			zwlr_virtual_pointer_v1_frame(virtual_pointer);
		} else if (sscanf(command, "press %d", &value) == 1 ||
				   sscanf(command, "release %d", &value) == 1) {
			zwlr_virtual_pointer_v1_button(virtual_pointer, 1, value,
				command[0] == 'p' ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
			zwlr_virtual_pointer_v1_frame(virtual_pointer);
		} else if (sscanf(command, "mode %d", &value) == 1) {
			move_mode = value;
		} else if (strncmp(command, "stale", 5) == 0) {
			xdg_toplevel_move(toplevel, seat, press_serial);
		} else if (strncmp(command, "frame", 5) == 0) {
			frame_done = false;
			struct wl_callback *callback = wl_surface_frame(surface);
			wl_callback_add_listener(callback, &frame_listener, NULL);
			wl_surface_damage(surface, 0, 0, width, height);
			wl_surface_commit(surface);
			while (!frame_done)
				if (wl_display_dispatch(display) < 0)
					return 1;
		} else if (strncmp(command, "unmap", 5) == 0) {
			wl_surface_attach(surface, NULL, 0, 0);
			wl_surface_commit(surface);
		} else if (strncmp(command, "quit", 4) == 0) {
			break;
		}
		synchronize();
		printf("{\"presses\":%u,\"releases\":%u}\n", presses, releases);
		fflush(stdout);
	}
	wl_display_disconnect(display);
	return 0;
}