#include "arg.h"
#include "dwl-ipc-unstable-v2-protocol.h"
#include "dynarr.h"
#include <ctype.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-util.h>

#define die(fmt, ...)                                                          \
	do {                                                                       \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__);                              \
		exit(EXIT_FAILURE);                                                    \
	} while (0)

char *argv0;

static enum {
	NONE = 0,
	SET = 1 << 0,
	GET = 1 << 1,
	WATCH = 1 << 2 | GET,
} mode = NONE;

static int32_t Oflag;
static int32_t Tflag;
static int32_t Lflag;
static int32_t oflag;
static int32_t tflag;
static int32_t lflag;
static int32_t cflag;
static int32_t vflag;
static int32_t mflag;
static int32_t fflag;
static int32_t qflag;
static int32_t dflag;
static int32_t xflag;
static int32_t eflag;
static int32_t kflag;
static int32_t bflag;
static int32_t Aflag;

static uint32_t occ, seltags, total_clients, urg;

static char *output_name;
static int32_t tagcount;
static char *tagset;
static char *layout_name;
static int32_t layoutcount, layout_idx;
static char *client_tags;
static char *dispatch_cmd;
static char *dispatch_arg1;
static char *dispatch_arg2;
static char *dispatch_arg3;
static char *dispatch_arg4;
static char *dispatch_arg5;

struct output {
	char *output_name;
	uint32_t name;
};
static DYNARR_DEF(struct output) outputs;

static struct wl_display *display;
static struct zdwl_ipc_manager_v2 *dwl_ipc_manager;

// 为每个回调定义专用的空函数
static void noop_geometry(void *data, struct wl_output *wl_output, int32_t x,
						  int32_t y, int32_t physical_width,
						  int32_t physical_height, int32_t subpixel,
						  const char *make, const char *model,
						  int32_t transform) {}

static void noop_mode(void *data, struct wl_output *wl_output, uint32_t flags,
					  int32_t width, int32_t height, int32_t refresh) {}

static void noop_done(void *data, struct wl_output *wl_output) {}

static void noop_scale(void *data, struct wl_output *wl_output,
					   int32_t factor) {}

static void noop_description(void *data, struct wl_output *wl_output,
							 const char *description) {}

// 将 n 转换为 9 位二进制字符串，结果存入 buf（至少长度 10）
void bin_str_9bits(char *buf, uint32_t n) {
	for (int32_t i = 8; i >= 0; i--) {
		*buf++ = ((n >> i) & 1) ? '1' : '0';
	}
	*buf = '\0'; // 字符串结尾
}

static void dwl_ipc_tags(void *data,
						 struct zdwl_ipc_manager_v2 *dwl_ipc_manager,
						 uint32_t count) {
	tagcount = count;
	if (Tflag && mode & GET)
		printf("%d\n", tagcount);
}

static void dwl_ipc_layout(void *data,
						   struct zdwl_ipc_manager_v2 *dwl_ipc_manager,
						   const char *name) {
	if (lflag && mode & SET && strcmp(layout_name, name) == 0)
		layout_idx = layoutcount;
	if (Lflag && mode & GET)
		printf("%s\n", name);
	layoutcount++;
}

static const struct zdwl_ipc_manager_v2_listener dwl_ipc_listener = {
	.tags = dwl_ipc_tags, .layout = dwl_ipc_layout};

static void
dwl_ipc_output_toggle_visibility(void *data,
								 struct zdwl_ipc_output_v2 *dwl_ipc_output) {
	if (!vflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("toggle\n");
}

static void dwl_ipc_output_active(void *data,
								  struct zdwl_ipc_output_v2 *dwl_ipc_output,
								  uint32_t active) {
	if (!oflag) {
		if (mode & SET && !output_name && active)
			output_name = strdup(data);
		return;
	}
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("selmon %u\n", active ? 1 : 0);
}

static void dwl_ipc_output_tag(void *data,
							   struct zdwl_ipc_output_v2 *dwl_ipc_output,
							   uint32_t tag, uint32_t state, uint32_t clients,
							   uint32_t focused) {
	if (!tflag)
		return;
	if (state == ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE)
		seltags |= 1 << tag;
	if (state == ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT)
		urg |= 1 << tag;
	if (clients > 0)
		occ |= 1 << tag;

	// 累计所有 tag 的 clients 总数
	total_clients += clients;

	if (!(mode & GET))
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("tag %u %u %u %u\n", tag + 1, state, clients, focused);
}

static void dwl_ipc_output_layout(void *data,
								  struct zdwl_ipc_output_v2 *dwl_ipc_output,
								  uint32_t layout) {}

static void dwl_ipc_output_layout_symbol(
	void *data, struct zdwl_ipc_output_v2 *dwl_ipc_output, const char *layout) {
	if (!(lflag && mode & GET))
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("layout %s\n", layout);
}

static void dwl_ipc_output_title(void *data,
								 struct zdwl_ipc_output_v2 *dwl_ipc_output,
								 const char *title) {
	if (!(cflag && mode & GET))
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("title %s\n", title);
}

static void dwl_ipc_output_appid(void *data,
								 struct zdwl_ipc_output_v2 *dwl_ipc_output,
								 const char *appid) {
	if (!(cflag && mode & GET))
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("appid %s\n", appid);
}

static void dwl_ipc_output_x(void *data,
							 struct zdwl_ipc_output_v2 *dwl_ipc_output,
							 int32_t x) {
	if (!xflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("x %d\n", x);
}

static void dwl_ipc_output_y(void *data,
							 struct zdwl_ipc_output_v2 *dwl_ipc_output,
							 int32_t y) {
	if (!xflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("y %d\n", y);
}

static void dwl_ipc_output_width(void *data,
								 struct zdwl_ipc_output_v2 *dwl_ipc_output,
								 int32_t width) {
	if (!xflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("width %d\n", width);
}

static void dwl_ipc_output_height(void *data,
								  struct zdwl_ipc_output_v2 *dwl_ipc_output,
								  int32_t height) {
	if (!xflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("height %d\n", height);
}

static void dwl_ipc_output_last_layer(void *data,
									  struct zdwl_ipc_output_v2 *dwl_ipc_output,
									  const char *last_layer) {
	if (!eflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("last_layer %s\n", last_layer);
}

static void dwl_ipc_output_kb_layout(void *data,
									 struct zdwl_ipc_output_v2 *dwl_ipc_output,
									 const char *kb_layout) {
	if (!kflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("kb_layout %s\n", kb_layout);
}

static void
dwl_ipc_output_scalefactor(void *data,
						   struct zdwl_ipc_output_v2 *dwl_ipc_output,
						   const uint32_t scalefactor) {
	if (!Aflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("scale_factor %f\n", scalefactor / 100.0f);
}

static void dwl_ipc_output_keymode(void *data,
								   struct zdwl_ipc_output_v2 *dwl_ipc_output,
								   const char *keymode) {
	if (!bflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("keymode %s\n", keymode);
}

static void dwl_ipc_output_fullscreen(void *data,
									  struct zdwl_ipc_output_v2 *dwl_ipc_output,
									  uint32_t is_fullscreen) {
	if (!mflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("fullscreen %u\n", is_fullscreen);
}

static void dwl_ipc_output_floating(void *data,
									struct zdwl_ipc_output_v2 *dwl_ipc_output,
									uint32_t is_floating) {
	if (!fflag)
		return;
	char *output_name = data;
	if (output_name)
		printf("%s ", output_name);
	printf("floating %u\n", is_floating);
}

static void dwl_ipc_output_frame(void *data,
								 struct zdwl_ipc_output_v2 *dwl_ipc_output) {
	if (mode & SET) {
		if (data && (!output_name || strcmp(output_name, (char *)data)))
			return;
		if (qflag) {
			zdwl_ipc_output_v2_quit(dwl_ipc_output);
		}
		if (lflag) {
			zdwl_ipc_output_v2_set_layout(dwl_ipc_output, layout_idx);
		}
		if (tflag) {
			uint32_t mask = seltags;
			char *t = tagset;
			int32_t i = 0;

			for (; *t && *t >= '0' && *t <= '9'; t++)
				i = *t - '0' + i * 10;

			if (!*t)
				mask = 1 << (i - 1);

			for (; *t; t++, i++) {
				switch (*t) {
				case '-':
					mask &= ~(1 << (i - 1));
					break;
				case '+':
					mask |= 1 << (i - 1);
					break;
				case '^':
					mask ^= 1 << (i - 1);
					break;
				}
			}

			if ((i - 1) > tagcount)
				die("bad tagset %s", tagset);

			zdwl_ipc_output_v2_set_tags(dwl_ipc_output, mask, 0);
		}
		if (cflag) {
			uint32_t and = ~0, xor = 0;
			char *t = client_tags;
			int32_t i = 0;

			for (; *t && *t >= '0' && *t <= '9'; t++)
				i = *t - '0' + i * 10;

			if (!*t)
				t = "+";

			for (; *t; t++, i++) {
				switch (*t) {
				case '-':
					and &= ~(1 << (i - 1));
					break;
				case '+':
					and &= ~(1 << (i - 1));
					xor |= 1 << (i - 1);
					break;
				case '^':
					xor |= 1 << (i - 1);
					break;
				}
			}
			if ((i - 1) > tagcount)
				die("bad client tagset %s", client_tags);

			zdwl_ipc_output_v2_set_client_tags(dwl_ipc_output, and, xor);
		}
		if (dflag) {
			zdwl_ipc_output_v2_dispatch(
				dwl_ipc_output, dispatch_cmd, dispatch_arg1, dispatch_arg2,
				dispatch_arg3, dispatch_arg4, dispatch_arg5);
		}
		wl_display_flush(display);
		usleep(1000);
		exit(0);
	} else {
		if (tflag) {
			char *output_name = data;

			printf("%s clients %u\n", output_name, total_clients);

			char occ_str[10], seltags_str[10], urg_str[10];

			bin_str_9bits(occ_str, occ);
			bin_str_9bits(seltags_str, seltags);
			bin_str_9bits(urg_str, urg);
			printf("%s tags %u %u %u\n", output_name, occ, seltags, urg);
			printf("%s tags %s %s %s\n", output_name, occ_str, seltags_str,
				   urg_str);
			occ = seltags = total_clients = urg = 0;
		}
	}
	fflush(stdout);
}

static const struct zdwl_ipc_output_v2_listener dwl_ipc_output_listener = {
	.toggle_visibility = dwl_ipc_output_toggle_visibility,
	.active = dwl_ipc_output_active,
	.tag = dwl_ipc_output_tag,
	.layout = dwl_ipc_output_layout,
	.title = dwl_ipc_output_title,
	.appid = dwl_ipc_output_appid,
	.layout_symbol = dwl_ipc_output_layout_symbol,
	.fullscreen = dwl_ipc_output_fullscreen,
	.floating = dwl_ipc_output_floating,
	.x = dwl_ipc_output_x,
	.y = dwl_ipc_output_y,
	.width = dwl_ipc_output_width,
	.height = dwl_ipc_output_height,
	.last_layer = dwl_ipc_output_last_layer,
	.kb_layout = dwl_ipc_output_kb_layout,
	.keymode = dwl_ipc_output_keymode,
	.scalefactor = dwl_ipc_output_scalefactor,
	.frame = dwl_ipc_output_frame,
};

static void wl_output_name(void *data, struct wl_output *output,
						   const char *name) {
	if (outputs.arr) {
		struct output *o = (struct output *)data;
		o->output_name = strdup(name);
		printf("+ ");
	}
	if (Oflag)
		printf("%s\n", name);
	if (output_name && strcmp(output_name, name) != 0) {
		wl_output_release(output);
		return;
	}
	struct zdwl_ipc_output_v2 *dwl_ipc_output =
		zdwl_ipc_manager_v2_get_output(dwl_ipc_manager, output);
	zdwl_ipc_output_v2_add_listener(dwl_ipc_output, &dwl_ipc_output_listener,
									output_name ? NULL : strdup(name));
}

static const struct wl_output_listener output_listener = {
	.geometry = noop_geometry,
	.mode = noop_mode,
	.done = noop_done,
	.scale = noop_scale,
	.name = wl_output_name,
	.description = noop_description,
};

static void global_add(void *data, struct wl_registry *wl_registry,
					   uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *o =
			wl_registry_bind(wl_registry, name, &wl_output_interface,
							 WL_OUTPUT_NAME_SINCE_VERSION);
		if (!outputs.arr) {
			wl_output_add_listener(o, &output_listener, NULL);
		} else {
			DYNARR_PUSH(&outputs, (struct output){.name = name});
			wl_output_add_listener(o, &output_listener,
								   &outputs.arr[outputs.len - 1]);
		}
	} else if (strcmp(interface, zdwl_ipc_manager_v2_interface.name) == 0) {
		dwl_ipc_manager = wl_registry_bind(wl_registry, name,
										   &zdwl_ipc_manager_v2_interface, 2);
		zdwl_ipc_manager_v2_add_listener(dwl_ipc_manager, &dwl_ipc_listener,
										 NULL);
	}
}

static void global_remove(void *data, struct wl_registry *wl_registry,
						  uint32_t name) {
	if (!outputs.arr)
		return;
	struct output *o = outputs.arr;
	for (size_t i = 0; i < outputs.len; i++, o++) {
		if (o->name == name) {
			printf("- %s\n", o->output_name);
			free(o->output_name);
			*o = DYNARR_POP(&outputs);
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = global_add,
	.global_remove = global_remove,
};

static void usage(void) {
	fprintf(stderr,
			"mmsg - MangoWM IPC\n"
			"\n"
			"SYNOPSIS:\n"
			"\tmmsg [-OTLq]\n"
			"\tmmsg [-o <output>] -s [-t <tags>] [-l <layout>] [-c <tags>] [-d "
			"<cmd>,<arg1>,<arg2>,<arg3>,<arg4>,<arg5>]\n"
			"\tmmsg [-o <output>] (-g | -w) [-OotlcvmfxekbA]\n"
			"\n"
			"OPERATION MODES:\n"
			"\t-g           Get values (tags, layout, focused client)\n"
			"\t-s           Set values (switch tags, layouts)\n"
			"\t-w           Watch mode (stream events)\n"
			"\n"
			"GENERAL OPTIONS:\n"
			"\t-O           Get all output (monitor) information\n"
			"\t-T           Get number of tags\n"
			"\t-L           Get all available layouts\n"
			"\t-q           Quit mango\n"
			"\t-o <output>  Select output (monitor)\n"
			"\n"
			"GET OPTIONS (used with -g or -w):\n"
			"\t-O           Get output name\n"
			"\t-o           Get output (monitor) focus information\n"
			"\t-t           Get selected tags\n"
			"\t-l           Get current layout\n"
			"\t-c           Get title and appid of focused clients\n"
			"\t-v           Get visibility of statusbar\n"
			"\t-m           Get fullscreen status\n"
			"\t-f           Get floating status\n"
			"\t-x           Get focused client geometry\n"
			"\t-e           Get name of last focused layer\n"
			"\t-k           Get current keyboard layout\n"
			"\t-b           Get current keybind mode\n"
			"\t-A           Get scale factor of monitor\n"
			"\n"
			"SET OPTIONS (used with -s):\n"
			"\t-o <output>         Select output (monitor)\n"
			"\t-t <tags>           Set selected tags (can be used with [+-^.] "
			"modifiers)\n"
			"\t-l <layout>         Set current layout\n"
			"\t-c <tags>           Get title and appid of focused client\n"
			"\t-d <cmd>,<args...>  Dispatch internal command (max 5 args)\n");
	exit(2);
}

int32_t main(int32_t argc, char *argv[]) {
	ARGBEGIN {
	case 'q':
		qflag = 1;
		if (!(mode & GET)) {
			mode = SET;
		}
		break;
	case 's':
		if (mode != NONE)
			usage();
		mode = SET;
		break;
	case 'g':
		if (mode != NONE)
			usage();
		mode = GET;
		break;
	case 'w':
		if (mode != NONE)
			usage();
		mode = WATCH;
		break;
	case 'o':
		if (mode == SET)
			output_name = EARGF(usage());
		else
			output_name = ARGF();
		if (!output_name)
			oflag = 1;
		break;
	case 't':
		tflag = 1;
		if (!(mode & GET)) {
			mode = SET;
			tagset = EARGF(usage());
		}
		break;
	case 'l':
		lflag = 1;
		if (!(mode & GET)) {
			mode = SET;
			layout_name = EARGF(usage());
		}
		break;
	case 'c':
		cflag = 1;
		if (!(mode & GET)) {
			mode = SET;
			client_tags = EARGF(usage());
		}
		break;
	case 'd':
		dflag = 1;
		if (!(mode & GET)) {
			mode = SET;
			char *arg = EARGF(usage());

			// Trim leading and trailing whitespace from entire argument first
			while (isspace(*arg))
				arg++;
			char *end = arg + strlen(arg) - 1;
			while (end > arg && isspace(*end))
				end--;
			*(end + 1) = '\0';

			dispatch_cmd = arg;
			char *comma1 = strchr(arg, ',');
			if (comma1) {
				*comma1 = '\0';

				// Trim trailing whitespace from command
				end = dispatch_cmd + strlen(dispatch_cmd) - 1;
				while (end > dispatch_cmd && isspace(*end))
					end--;
				*(end + 1) = '\0';

				dispatch_arg1 = comma1 + 1;
				// Trim leading whitespace from arg1
				while (isspace(*dispatch_arg1))
					dispatch_arg1++;

				// Trim trailing whitespace from arg1 before looking for next
				// comma
				end = dispatch_arg1 + strlen(dispatch_arg1) - 1;
				while (end > dispatch_arg1 && isspace(*end))
					end--;
				*(end + 1) = '\0';

				char *comma2 = strchr(dispatch_arg1, ',');
				if (comma2) {
					*comma2 = '\0';
					dispatch_arg2 = comma2 + 1;
					// Trim leading whitespace from arg2
					while (isspace(*dispatch_arg2))
						dispatch_arg2++;

					// Trim trailing whitespace from arg2 before looking for
					// next comma
					end = dispatch_arg2 + strlen(dispatch_arg2) - 1;
					while (end > dispatch_arg2 && isspace(*end))
						end--;
					*(end + 1) = '\0';

					char *comma3 = strchr(dispatch_arg2, ',');
					if (comma3) {
						*comma3 = '\0';
						dispatch_arg3 = comma3 + 1;
						// Trim leading whitespace from arg3
						while (isspace(*dispatch_arg3))
							dispatch_arg3++;

						// Trim trailing whitespace from arg3 before looking for
						// next comma
						end = dispatch_arg3 + strlen(dispatch_arg3) - 1;
						while (end > dispatch_arg3 && isspace(*end))
							end--;
						*(end + 1) = '\0';

						char *comma4 = strchr(dispatch_arg3, ',');
						if (comma4) {
							*comma4 = '\0';
							dispatch_arg4 = comma4 + 1;
							// Trim leading whitespace from arg4
							while (isspace(*dispatch_arg4))
								dispatch_arg4++;

							// Trim trailing whitespace from arg4 before looking
							// for next comma
							end = dispatch_arg4 + strlen(dispatch_arg4) - 1;
							while (end > dispatch_arg4 && isspace(*end))
								end--;
							*(end + 1) = '\0';

							char *comma5 = strchr(dispatch_arg4, ',');
							if (comma5) {
								*comma5 = '\0';
								dispatch_arg5 = comma5 + 1;
								// Trim leading whitespace from arg5
								while (isspace(*dispatch_arg5))
									dispatch_arg5++;

								// Trim trailing whitespace from arg5
								end = dispatch_arg5 + strlen(dispatch_arg5) - 1;
								while (end > dispatch_arg5 && isspace(*end))
									end--;
								*(end + 1) = '\0';
							} else {
								dispatch_arg5 = "";
							}
						} else {
							dispatch_arg4 = "";
							dispatch_arg5 = "";
						}
					} else {
						dispatch_arg3 = "";
						dispatch_arg4 = "";
						dispatch_arg5 = "";
					}
				} else {
					dispatch_arg2 = "";
					dispatch_arg3 = "";
					dispatch_arg4 = "";
					dispatch_arg5 = "";
				}
			} else {
				dispatch_arg1 = "";
				dispatch_arg2 = "";
				dispatch_arg3 = "";
				dispatch_arg4 = "";
				dispatch_arg5 = "";
			}
		}
		break;
	case 'O':
		Oflag = 1;
		if (mode && !(mode & GET))
			usage();
		if (mode & WATCH)
			DYNARR_INIT(&outputs);
		else
			mode = GET;
		break;
	case 'T':
		Tflag = 1;
		if (mode && mode != GET)
			usage();
		mode = GET;
		break;
	case 'L':
		Lflag = 1;
		if (mode && mode != GET)
			usage();
		mode = GET;
		break;
	case 'v':
		vflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'm':
		mflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'f':
		fflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'x':
		xflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'e':
		eflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'k':
		kflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'b':
		bflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	case 'A':
		Aflag = 1;
		if (mode == SET)
			usage();
		mode |= GET;
		break;
	default:
		fprintf(stderr, "bad option %c\n", ARGC());
		usage();
	}
	ARGEND
	if (mode == NONE)
		usage();
	if (mode & GET && !output_name &&
		!(oflag || tflag || lflag || Oflag || Tflag || Lflag || cflag ||
		  vflag || mflag || fflag || xflag || eflag || kflag || bflag ||
		  Aflag || dflag))
		oflag = tflag = lflag = cflag = vflag = mflag = fflag = xflag = eflag =
			kflag = bflag = Aflag = 1;

	display = wl_display_connect(NULL);
	if (!display)
		die("bad display");

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);

	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	if (!dwl_ipc_manager)
		die("bad dwl-ipc protocol");

	wl_display_roundtrip(display);

	if (mode == WATCH)
		while (wl_display_dispatch(display) != -1)
			;

	return 0;
}
