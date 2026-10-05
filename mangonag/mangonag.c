#if defined(__linux__)
#define _GNU_SOURCE
#elif !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <cairo/cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <pango/pangocairo.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-util.h>

#include "mango/common/input-event-codes.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define NAG_NAMESPACE "mangonag"
#define NAG_MAX_LINES 4

enum nag_edge { NAG_EDGE_TOP, NAG_EDGE_BOTTOM };

struct nag_type {
	uint32_t background;
	uint32_t text;
	uint32_t button_background;
	uint32_t button_text;
	uint32_t border;
	uint32_t border_bottom;
};

static const struct nag_type type_error = {
	.background = 0x201B14FF,
	.text = 0xE1D1B6FF,
	.button_background = 0x3A332AFF,
	.button_text = 0xE1D1B6FF,
	.border = 0xEF2929FF,
	.border_bottom = 0xEF2929FF,
};

static const struct nag_type type_warning = {
	.background = 0xFFA800FF,
	.text = 0x000000FF,
	.button_background = 0xFFC100FF,
	.button_text = 0x000000FF,
	.border = 0xAB7100FF,
	.border_bottom = 0xAB7100FF,
};

static const struct nag_type type_info = {
	.background = 0x323232FF,
	.text = 0xFFFFFFFF,
	.button_background = 0x333333FF,
	.button_text = 0xFFFFFFFF,
	.border = 0x222222FF,
	.border_bottom = 0x444444FF,
};

struct nag_button {
	char *text;
	char *action;
	bool terminal;
	bool dismiss;

	int x, y, width, height;
	int outer_x, outer_y, outer_width, outer_height;
};

struct nag_output {
	struct wl_output *wl_output;
	uint32_t global_name;
	char *name;
	int32_t scale;
	struct wl_list link;
};

struct nag_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
	int width, height;
	bool busy;
	struct wl_buffer_listener release;
};

struct nag_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;

	struct wl_list outputs;
	struct wl_output *output;
	char *output_name;

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	uint32_t width, height;
	int32_t scale;
	uint32_t anchor;
	uint32_t layer;
	int32_t exclusive_zone;

	char *message;
	char *details;
	const struct nag_type *type;
	char *font;

	enum nag_edge edge;
	int timeout_ms;
	char *dismiss_text;

	struct nag_button **buttons;
	size_t buttons_len;

	bool running;
	bool configured;
};

static struct nag_state nag;
static double nag_pointer_x = 0;
static double nag_pointer_y = 0;

static void nag_signal(int signal_number) {
	nag.running = false;
}

static void nag_destroy(struct nag_state *n);

static void rgba_to_cairo(uint32_t color, double *r, double *g, double *b,
						  double *a) {
	*r = ((color >> 24) & 0xFF) / 255.0;
	*g = ((color >> 16) & 0xFF) / 255.0;
	*b = ((color >> 8) & 0xFF) / 255.0;
	*a = (color & 0xFF) / 255.0;
}

static void nag_run_action(struct nag_button *button) {
	if (!button)
		return;
	if (button->dismiss)
		nag.running = false;
	if (!button->action || button->action[0] == '\0')
		return;

	pid_t pid = fork();
	if (pid < 0)
		return;
	if (pid == 0) {
		pid_t pid2 = fork();
		if (pid2 < 0)
			_exit(EXIT_FAILURE);
		if (pid2 == 0) {
			if (!button->terminal) {
				execlp("sh", "sh", "-c", button->action, (char *)NULL);
				_exit(EXIT_FAILURE);
			}
			char script[] = "/tmp/mangonag-XXXXXX";
			int fd = mkstemp(script);
			if (fd < 0) {
				execlp("sh", "sh", "-c", button->action, (char *)NULL);
				_exit(EXIT_FAILURE);
			}
			FILE *f = fdopen(fd, "w");
			fprintf(f, "#!/bin/sh\nrm -f %s\n%s\n", script, button->action);
			fchmod(fd, S_IRUSR | S_IWUSR | S_IXUSR);
			fclose(f);
			char cmd[4096];
			snprintf(cmd, sizeof(cmd),
					 "term=\"$TERMINAL\"; if [ -z \"$term\" ]; then for t in "
					 "foot alacritty kitty wezterm st xterm; do command -v "
					 "\"$t\" >/dev/null 2>&1 && term=\"$t\" && break; done; "
					 "fi; if [ -n \"$term\" ]; then exec \"$term\" -e %s; "
					 "else exec sh %s; fi",
					 script, script);
			execlp("sh", "sh", "-c", cmd, (char *)NULL);
			_exit(EXIT_FAILURE);
		}
		_exit(EXIT_SUCCESS);
	}

	int status;
	waitpid(pid, &status, 0);
}

static void handle_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	struct nag_buffer *buffer = data;
	wl_buffer_destroy(buffer->wl_buffer);
	munmap(buffer->data, buffer->size);
	free(buffer);
}

static void nag_output_name(void *data, struct wl_output *wl_output,
							const char *name) {
	struct nag_output *output = data;
	free(output->name);
	output->name = strdup(name);
}

static void nag_output_geometry(void *data, struct wl_output *wl_output,
								int32_t x, int32_t y, int32_t physical_width,
								int32_t physical_height, int32_t subpixel,
								const char *make, const char *model,
								int32_t transform) {
}

static void nag_output_mode(void *data, struct wl_output *wl_output,
							uint32_t flags, int32_t width, int32_t height,
							int32_t refresh) {
}

static void nag_output_done(void *data, struct wl_output *wl_output) {
}

static void nag_output_scale(void *data, struct wl_output *wl_output,
							 int32_t factor) {
	struct nag_output *output = data;
	output->scale = factor > 0 ? factor : 1;
}

static void nag_output_description(void *data, struct wl_output *wl_output,
								   const char *description) {
}

static const struct wl_output_listener output_listener = {
	.geometry = nag_output_geometry,
	.mode = nag_output_mode,
	.done = nag_output_done,
	.scale = nag_output_scale,
	.name = nag_output_name,
	.description = nag_output_description,
};

static int nag_create_shm_fd(void) {
#if defined(__linux__)
	return memfd_create(NAG_NAMESPACE, MFD_CLOEXEC);
#else
	char name[64];

	for (unsigned int attempt = 0; attempt < 16; attempt++) {
		snprintf(name, sizeof(name), "/" NAG_NAMESPACE "-%ld-%u",
				 (long)getpid(), attempt);
		int fd =
			shm_open(name, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
		if (errno != EEXIST)
			break;
	}
	return -1;
#endif
}

static struct nag_buffer *nag_buffer_create(int width, int height) {
	int stride = width * 4;
	int size = stride * height;

	int fd = nag_create_shm_fd();
	if (fd < 0)
		return NULL;
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return NULL;
	}

	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(nag.shm, fd, size);
	struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	if (!wl_buffer) {
		munmap(data, size);
		return NULL;
	}

	struct nag_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		wl_buffer_destroy(wl_buffer);
		munmap(data, size);
		return NULL;
	}
	buffer->wl_buffer = wl_buffer;
	buffer->data = data;
	buffer->size = size;
	buffer->width = width;
	buffer->height = height;
	buffer->busy = true;
	buffer->release.release = handle_buffer_release;
	wl_buffer_add_listener(wl_buffer, &buffer->release, buffer);
	return buffer;
}

static PangoLayout *nag_layout(cairo_t *cr, const char *text, int width) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc =
		pango_font_description_from_string(nag.font ? nag.font : "monospace 13");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, text ? text : "", -1);
	pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
	pango_layout_set_single_paragraph_mode(layout, false);
	if (width > 0)
		pango_layout_set_width(layout, width * PANGO_SCALE);
	return layout;
}

struct nag_style {
	int fg;
	bool truecolor;
	uint32_t rgb;
	int bg;
	bool bg_true;
	uint32_t bg_rgb;
	bool bold;
	bool dim;
	bool italic;
	bool underline;
};

static const uint32_t nag_palette[16] = {
	0x262626FF, 0xDE5959FF, 0x84D082FF, 0xE9B959FF, 0xE9B959FF, 0xD5A7E0FF,
	0xA5DE86FF, 0xE1D1B6FF, 0xC6C09CFF, 0xF57474FF, 0xB3EC7BFF, 0xF1E69FFF,
	0xA3C1E0FF, 0xCAACC6FF, 0xF1E69FFF, 0xF4F4F3FF,
};

static uint32_t nag_color(int index) {
	if (index < 0)
		return 0;
	if (index < 16)
		return nag_palette[index];
	if (index < 232) {
		int c = index - 16;
		int r = c / 36;
		int g = (c / 6) % 6;
		int b = c % 6;
		r = r ? 55 + r * 40 : 0;
		g = g ? 55 + g * 40 : 0;
		b = b ? 55 + b * 40 : 0;
		return (uint32_t)((r << 24) | (g << 16) | (b << 8) | 0xFF);
	}
	int v = 8 + (index - 232) * 10;
	return (uint32_t)((v << 24) | (v << 16) | (v << 8) | 0xFF);
}

static void nag_style_reset(struct nag_style *style) {
	style->fg = -1;
	style->truecolor = false;
	style->rgb = 0;
	style->bg = -1;
	style->bg_true = false;
	style->bg_rgb = 0;
	style->bold = false;
	style->dim = false;
	style->italic = false;
	style->underline = false;
}

static void nag_style_code(struct nag_style *style, int code) {
	if (code == 0)
		nag_style_reset(style);
	else if (code == 1)
		style->bold = true;
	else if (code == 2)
		style->dim = true;
	else if (code == 3)
		style->italic = true;
	else if (code == 4)
		style->underline = true;
	else if (code == 22)
		style->bold = style->dim = false;
	else if (code == 23)
		style->italic = false;
	else if (code == 24)
		style->underline = false;
	else if (code == 39) {
		style->fg = -1;
		style->truecolor = false;
	} else if (code == 49) {
		style->bg = -1;
		style->bg_true = false;
	} else if (code >= 30 && code <= 37)
		style->fg = code - 30;
	else if (code >= 90 && code <= 97)
		style->fg = code - 90 + 8;
	else if (code >= 40 && code <= 47)
		style->bg = code - 40;
	else if (code >= 100 && code <= 107)
		style->bg = code - 100 + 8;
}

static void nag_style_params(struct nag_style *style, const char *params) {
	int values[64];
	int count = 0;
	const char *p = params;

	while (*p != '\0' && count < 64) {
		values[count++] = atoi(p);
		while (*p != '\0' && *p != ';')
			p++;
		if (*p == ';')
			p++;
	}

	if (count == 0) {
		nag_style_reset(style);
		return;
	}

	for (int i = 0; i < count; i++) {
		int code = values[i];
		if (code == 38 && i + 2 < count && values[i + 1] == 5) {
			style->fg = values[i + 2];
			style->truecolor = false;
			i += 2;
		} else if (code == 38 && i + 4 < count && values[i + 1] == 2) {
			style->rgb =
				(uint32_t)((values[i + 2] << 24) | (values[i + 3] << 16) |
						   (values[i + 4] << 8) | 0xFF);
			style->truecolor = true;
			i += 4;
		} else if (code == 48 && i + 2 < count && values[i + 1] == 5) {
			style->bg = values[i + 2];
			style->bg_true = false;
			i += 2;
		} else if (code == 48 && i + 4 < count && values[i + 1] == 2) {
			style->bg_rgb =
				(uint32_t)((values[i + 2] << 24) | (values[i + 3] << 16) |
						   (values[i + 4] << 8) | 0xFF);
			style->bg_true = true;
			i += 4;
		} else {
			nag_style_code(style, code);
		}
	}
}

static void nag_attr_emit(PangoAttrList *list, const struct nag_style *style,
						  int start, int end) {
	if (end <= start)
		return;

	PangoAttribute *attr;

	if (style->truecolor || style->fg >= 0) {
		uint32_t color = style->truecolor ? style->rgb : nag_color(style->fg);
		attr =
			pango_attr_foreground_new((guint16)(((color >> 24) & 0xFF) * 257),
									  (guint16)(((color >> 16) & 0xFF) * 257),
									  (guint16)(((color >> 8) & 0xFF) * 257));
		attr->start_index = start;
		attr->end_index = end;
		pango_attr_list_insert(list, attr);
	}
	if (style->bg_true || style->bg >= 0) {
		uint32_t color = style->bg_true ? style->bg_rgb : nag_color(style->bg);
		attr =
			pango_attr_background_new((guint16)(((color >> 24) & 0xFF) * 257),
									  (guint16)(((color >> 16) & 0xFF) * 257),
									  (guint16)(((color >> 8) & 0xFF) * 257));
		attr->start_index = start;
		attr->end_index = end;
		pango_attr_list_insert(list, attr);
	}
	if (style->bold) {
		attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
		attr->start_index = start;
		attr->end_index = end;
		pango_attr_list_insert(list, attr);
	}
	if (style->italic) {
		attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
		attr->start_index = start;
		attr->end_index = end;
		pango_attr_list_insert(list, attr);
	}
	if (style->underline) {
		attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
		attr->start_index = start;
		attr->end_index = end;
		pango_attr_list_insert(list, attr);
	}
	if (style->dim) {
		attr = pango_attr_foreground_alpha_new(0x6000);
		attr->start_index = start;
		attr->end_index = end;
		pango_attr_list_insert(list, attr);
	}
}

static size_t nag_ansi_skip(const char *text) {
	if ((unsigned char)text[0] != 0x1b || text[1] != '[')
		return 0;

	size_t i = 2;
	while (text[i] != '\0' &&
		   !((unsigned char)text[i] >= '@' && (unsigned char)text[i] <= '~'))
		i++;
	if (text[i] == '\0')
		return 0;
	return i + 1;
}

static bool nag_tag_match(const char *text, int *length, uint32_t *fg,
						  uint32_t *bg) {
	static const struct {
		const char *tag;
		uint32_t fg;
		uint32_t bg;
	} tags[] = {
		{"[ERROR]", 0x201B14FF, 0xEF2929FF},
		{"[WARN]", 0x201B14FF, 0xEAD96BFF},
		{"[INFO]", 0x201B14FF, 0x729FCFFF},
		{"[DEBUG]", 0x201B14FF, 0x8AE234FF},
	};

	for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++) {
		size_t n = strlen(tags[i].tag);
		if (strncmp(text, tags[i].tag, n) == 0) {
			*length = (int)n;
			*fg = tags[i].fg;
			*bg = tags[i].bg;
			return true;
		}
	}
	return false;
}

static PangoAttrList *nag_ansi_attrs(const char *text, char **clean_out) {
	if (!text)
		text = "";

	size_t len = strlen(text);
	char *clean = malloc(len + 1);
	if (!clean) {
		*clean_out = NULL;
		return NULL;
	}

	PangoAttrList *list = pango_attr_list_new();
	struct nag_style style;
	nag_style_reset(&style);

	size_t offset = 0;
	size_t run_start = 0;
	const char *p = text;
	bool line_start = true;

	while (*p != '\0') {
		size_t skip = nag_ansi_skip(p);
		if (skip > 0) {
			if (p[skip - 1] == 'm') {
				nag_attr_emit(list, &style, (int)run_start, (int)offset);
				char params[64];
				size_t n = skip - 3;
				if (n >= sizeof(params))
					n = sizeof(params) - 1;
				memcpy(params, p + 2, n);
				params[n] = '\0';
				nag_style_params(&style, params);
				run_start = offset;
			}
			p += skip;
			continue;
		}

		if (line_start) {
			int tag_len = 0;
			uint32_t tag_fg = 0;
			uint32_t tag_bg = 0;
			if (nag_tag_match(p, &tag_len, &tag_fg, &tag_bg)) {
				int start = (int)offset;
				for (int i = 0; i < tag_len; i++)
					clean[offset++] = p[i];
				p += tag_len;
				if (*p == ':')
					clean[offset++] = *p++;
				struct nag_style tag_style;
				nag_style_reset(&tag_style);
				tag_style.truecolor = true;
				tag_style.rgb = tag_fg;
				tag_style.bg_true = true;
				tag_style.bg_rgb = tag_bg;
				tag_style.bold = true;
				nag_attr_emit(list, &tag_style, start, (int)offset);
				run_start = offset;
				line_start = false;
				continue;
			}
			line_start = false;
		}

		if (*p == '\n') {
			nag_attr_emit(list, &style, (int)run_start, (int)offset);
			clean[offset++] = '\n';
			nag_style_reset(&style);
			run_start = offset;
			line_start = true;
			p++;
			continue;
		}

		clean[offset++] = *p++;
	}

	nag_attr_emit(list, &style, (int)run_start, (int)offset);
	clean[offset] = '\0';
	*clean_out = clean;
	return list;
}

static PangoLayout *nag_layout_markup(cairo_t *cr, const char *text,
									  int width) {
	char *clean = NULL;
	PangoAttrList *attrs = nag_ansi_attrs(text ? text : "", &clean);

	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc =
		pango_font_description_from_string(nag.font ? nag.font : "monospace 13");
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, clean ? clean : "", -1);
	if (attrs)
		pango_layout_set_attributes(layout, attrs);
	pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
	pango_layout_set_single_paragraph_mode(layout, false);
	if (width > 0)
		pango_layout_set_width(layout, width * PANGO_SCALE);

	if (attrs)
		pango_attr_list_unref(attrs);
	free(clean);
	return layout;
}

static int nag_text_height(const char *text) {
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(surface);
	PangoLayout *layout = nag_layout_markup(cr, text, 0);
	int w = 0, h = 0;
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	return h;
}

static void nag_compute_layout(int cairo_width) {
	const int padding = 8;
	int border = 3;
	int button_border = 3;
	int button_padding = 3;

	int max_height = nag_text_height(nag.message) + padding * 2;

	for (size_t i = 0; i < nag.buttons_len; i++) {
		struct nag_button *button = nag.buttons[i];
		cairo_surface_t *surface =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
		cairo_t *cr = cairo_create(surface);
		PangoLayout *layout = nag_layout(cr, button->text, 0);
		int text_w = 0, text_h = 0;
		pango_layout_get_pixel_size(layout, &text_w, &text_h);
		g_object_unref(layout);
		cairo_destroy(cr);
		cairo_surface_destroy(surface);

		int ideal = text_h + button_padding * 2 + button_border * 2 + border * 2;
		if (ideal > max_height)
			max_height = ideal;
	}

	nag.height = max_height;
	if (nag.height < 1)
		nag.height = 1;
}

static char *nag_truncate_lines(const char *text, int max_chars);

static void nag_render(void) {
	if (!nag.surface || nag.width == 0 || nag.height == 0)
		return;

	int scale = nag.scale > 0 ? nag.scale : 1;
	struct nag_buffer *buffer =
		nag_buffer_create(nag.width * scale, nag.height * scale);
	if (!buffer)
		return;

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		buffer->data, CAIRO_FORMAT_ARGB32, nag.width * scale,
		nag.height * scale, nag.width * scale * 4);
	cairo_t *cr = cairo_create(surface);
	cairo_scale(cr, scale, scale);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

	double r, g, b, a;
	const int padding = 8;
	const int border_thickness = 3;
	const int button_border = 3;
	const int button_padding = 3;
	const int button_gap = 20;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	rgba_to_cairo(nag.type->background, &r, &g, &b, &a);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_paint(cr);

	int buttons_total = 0;
	for (size_t i = 0; i < nag.buttons_len; i++) {
		PangoLayout *tl = nag_layout(cr, nag.buttons[i]->text, 0);
		int tw = 0, th = 0;
		pango_layout_get_pixel_size(tl, &tw, &th);
		g_object_unref(tl);
		buttons_total += tw + button_padding * 2 + button_border * 2;
	}
	if (nag.buttons_len > 1)
		buttons_total += (int)(nag.buttons_len - 1) * button_gap;

	int text_avail = (int)nag.width - border_thickness * 2 - padding * 2 -
					 buttons_total - 24;
	if (text_avail < 32)
		text_avail = 32;

	int char_w = 1;
	PangoLayout *measure = nag_layout(cr, "0000000000", 0);
	int mw = 0, mh = 0;
	pango_layout_get_pixel_size(measure, &mw, &mh);
	g_object_unref(measure);
	if (mw > 0)
		char_w = mw / 10;
	if (char_w < 1)
		char_w = 1;

	char *display = nag_truncate_lines(nag.message, text_avail / char_w);

	int text_w = 0, text_h = 0;
	PangoLayout *layout = nag_layout_markup(cr, display, 0);
	pango_layout_get_pixel_size(layout, &text_w, &text_h);
	rgba_to_cairo(nag.type->text, &r, &g, &b, &a);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_move_to(cr, padding, (nag.height - text_h) / 2);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	free(display);

	int x = nag.width - border_thickness - 8;
	for (size_t i = 0; i < nag.buttons_len; i++) {
		struct nag_button *button = nag.buttons[i];

		layout = nag_layout(cr, button->text, 0);
		pango_layout_get_pixel_size(layout, &text_w, &text_h);
		g_object_unref(layout);

		button->x = x - button_border - text_w - button_padding * 2 + 1;
		button->width = text_w + button_padding * 2;
		button->height = text_h + button_padding * 2;
		button->y = ((int)nag.height - button->height) / 2;

		button->outer_x = button->x - button_border;
		button->outer_y = button->y - button_border;
		button->outer_width = button->width + button_border * 2;
		button->outer_height = button->height + button_border * 2;

		rgba_to_cairo(nag.type->border, &r, &g, &b, &a);
		cairo_set_source_rgba(cr, r, g, b, a);
		cairo_rectangle(cr, button->outer_x, button->outer_y,
						button->outer_width, button->outer_height);
		cairo_fill(cr);

		rgba_to_cairo(nag.type->button_background, &r, &g, &b, &a);
		cairo_set_source_rgba(cr, r, g, b, a);
		cairo_rectangle(cr, button->x, button->y, button->width,
						button->height);
		cairo_fill(cr);

		rgba_to_cairo(nag.type->button_text, &r, &g, &b, &a);
		cairo_set_source_rgba(cr, r, g, b, a);
		layout = nag_layout(cr, button->text, 0);
		cairo_move_to(cr, button->x + button_padding,
					  button->y + button_padding);
		pango_cairo_show_layout(cr, layout);
		g_object_unref(layout);

		x = button->outer_x - button_gap;
	}

	rgba_to_cairo(nag.type->border_bottom, &r, &g, &b, &a);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_rectangle(cr, 0, nag.height - border_thickness, nag.width,
					border_thickness);
	cairo_fill(cr);

	rgba_to_cairo(nag.type->border, &r, &g, &b, &a);
	cairo_set_source_rgba(cr, r, g, b, a);
	cairo_set_line_width(cr, border_thickness);
	cairo_rectangle(cr, border_thickness / 2.0, border_thickness / 2.0,
					nag.width - border_thickness,
					nag.height - border_thickness);
	cairo_stroke(cr);

	cairo_destroy(cr);
	cairo_surface_flush(surface);

	wl_surface_set_buffer_scale(nag.surface, scale);
	wl_surface_attach(nag.surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(nag.surface, 0, 0, nag.width * scale,
							 nag.height * scale);
	wl_surface_commit(nag.surface);

	cairo_surface_destroy(surface);
}

static void nag_layer_configure(void *data,
								struct zwlr_layer_surface_v1 *layer_surface,
								uint32_t serial, uint32_t width,
								uint32_t height) {
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	if (nag.configured && width == nag.width && height == nag.height)
		return;
	if (width > 0)
		nag.width = width;
	if (height > 0)
		nag.height = height;
	nag.configured = true;
	nag_render();
}

static void nag_layer_closed(void *data,
							 struct zwlr_layer_surface_v1 *layer_surface) {
	nag.running = false;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = nag_layer_configure,
	.closed = nag_layer_closed,
};

static void nag_pointer_button(void *data, struct wl_pointer *pointer,
							  uint32_t serial, uint32_t time, uint32_t button,
							  uint32_t state) {
	if (button != BTN_LEFT || state != WL_POINTER_BUTTON_STATE_PRESSED)
		return;

	for (size_t i = 0; i < nag.buttons_len; i++) {
		struct nag_button *b = nag.buttons[i];
		if (nag_pointer_x < b->outer_x ||
			nag_pointer_x >= b->outer_x + b->outer_width ||
			nag_pointer_y < b->outer_y ||
			nag_pointer_y >= b->outer_y + b->outer_height)
			continue;
		nag_run_action(b);
		return;
	}
}

static void nag_pointer_enter(void *data, struct wl_pointer *pointer,
							  uint32_t serial, struct wl_surface *surface,
							  wl_fixed_t sx, wl_fixed_t sy) {
	nag_pointer_x = wl_fixed_to_double(sx);
	nag_pointer_y = wl_fixed_to_double(sy);
}

static void nag_pointer_leave(void *data, struct wl_pointer *pointer,
							  uint32_t serial, struct wl_surface *surface) {
}

static void nag_pointer_motion(void *data, struct wl_pointer *pointer,
							   uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
	nag_pointer_x = wl_fixed_to_double(sx);
	nag_pointer_y = wl_fixed_to_double(sy);
}

static void nag_pointer_axis(void *data, struct wl_pointer *pointer,
							 uint32_t time, uint32_t axis, wl_fixed_t value) {
}

static void nag_pointer_frame(void *data, struct wl_pointer *pointer) {
}

static void nag_pointer_axis_source(void *data, struct wl_pointer *pointer,
									uint32_t source) {
}

static void nag_pointer_axis_stop(void *data, struct wl_pointer *pointer,
								 uint32_t time, uint32_t axis) {
}

static void nag_pointer_axis_discrete(void *data, struct wl_pointer *pointer,
									  uint32_t axis, int32_t discrete) {
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = nag_pointer_enter,
	.leave = nag_pointer_leave,
	.motion = nag_pointer_motion,
	.button = nag_pointer_button,
	.axis = nag_pointer_axis,
	.frame = nag_pointer_frame,
	.axis_source = nag_pointer_axis_source,
	.axis_stop = nag_pointer_axis_stop,
	.axis_discrete = nag_pointer_axis_discrete,
};

static void nag_keyboard_keymap(void *data, struct wl_keyboard *keyboard,
								uint32_t format, int32_t fd, uint32_t size) {
	close(fd);
}

static void nag_keyboard_enter(void *data, struct wl_keyboard *keyboard,
							   uint32_t serial, struct wl_surface *surface,
							   struct wl_array *keys) {
}

static void nag_keyboard_leave(void *data, struct wl_keyboard *keyboard,
							   uint32_t serial, struct wl_surface *surface) {
}

static void nag_keyboard_key(void *data, struct wl_keyboard *keyboard,
							 uint32_t serial, uint32_t time, uint32_t key,
							 uint32_t state) {
	if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
		return;
	if (key == 1)
		nag.running = false;
}

static void nag_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
								   uint32_t serial, uint32_t mods_depressed,
								   uint32_t mods_latched,
								   uint32_t mods_locked,
								   uint32_t group) {
}

static void nag_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
									 int32_t rate, int32_t delay) {
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = nag_keyboard_keymap,
	.enter = nag_keyboard_enter,
	.leave = nag_keyboard_leave,
	.key = nag_keyboard_key,
	.modifiers = nag_keyboard_modifiers,
	.repeat_info = nag_keyboard_repeat_info,
};

static void nag_seat_capabilities(void *data, struct wl_seat *seat,
								  uint32_t capabilities) {
	if (capabilities & WL_SEAT_CAPABILITY_POINTER && !nag.pointer) {
		nag.pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(nag.pointer, &pointer_listener, NULL);
	}
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !nag.keyboard) {
		nag.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(nag.keyboard, &keyboard_listener, NULL);
	}
}

static void nag_seat_name(void *data, struct wl_seat *seat, const char *name) {
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = nag_seat_capabilities,
	.name = nag_seat_name,
};

static void nag_registry_global(void *data, struct wl_registry *registry,
								uint32_t name, const char *interface,
								uint32_t version) {
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		nag.compositor =
			wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		nag.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		nag.seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
		wl_seat_add_listener(nag.seat, &seat_listener, NULL);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		nag.layer_shell = wl_registry_bind(
			registry, name, &zwlr_layer_shell_v1_interface, 4);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct nag_output *output = calloc(1, sizeof(*output));
		if (!output)
			return;
		output->global_name = name;
		output->wl_output =
			wl_registry_bind(registry, name, &wl_output_interface, 4);
		wl_output_add_listener(output->wl_output, &output_listener, output);
		wl_list_insert(nag.outputs.prev, &output->link);
	}
}

static void nag_registry_global_remove(void *data, struct wl_registry *registry,
									   uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	.global = nag_registry_global,
	.global_remove = nag_registry_global_remove,
};

static void nag_setup_surface(void) {
	nag.surface = wl_compositor_create_surface(nag.compositor);
	nag.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		nag.layer_shell, nag.surface, nag.output, nag.layer, NAG_NAMESPACE);
	zwlr_layer_surface_v1_add_listener(nag.layer_surface,
									   &layer_surface_listener, NULL);
	zwlr_layer_surface_v1_set_anchor(nag.layer_surface, nag.anchor);
	zwlr_layer_surface_v1_set_keyboard_interactivity(
		nag.layer_surface, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

	nag_compute_layout(0);
	zwlr_layer_surface_v1_set_size(nag.layer_surface, 0, nag.height);
	zwlr_layer_surface_v1_set_exclusive_zone(nag.layer_surface, nag.height);
	wl_surface_commit(nag.surface);
}

static const struct nag_long_option {
	const char *name;
	char short_option;
} nag_long_options[] = {
	{"message", 'm'},
	{"detailed-message", 'l'},
	{"type", 't'},
	{"edge", 'e'},
	{"layer", 'y'},
	{"output", 'o'},
	{"font", 'f'},
	{"dismiss-button", 's'},
	{"button", 'b'},
	{"button-no-terminal", 'B'},
	{"button-dismiss", 'z'},
	{"button-dismiss-no-terminal", 'Z'},
	{"timeout", 'T'},
	{"help", 'h'},
};

static char **nag_expand_long_options(char **argv, int *argc, char **pool_out) {
	int capacity = *argc * 2 + 1;
	char **out = malloc((size_t)capacity * sizeof(*out));
	char *pool = malloc((size_t)*argc * 3 + 1);

	if (!out || !pool) {
		free(out);
		free(pool);
		return NULL;
	}
	*pool_out = pool;

	int n = 0;
	for (int i = 0; i < *argc; i++) {
		const char *arg = argv[i];
		const char *value = NULL;
		char short_option = '\0';

		if (i > 0 && arg[0] == '-' && arg[1] == '-' && arg[2] != '\0') {
			const char *body = arg + 2;
			const char *eq = strchr(body, '=');
			size_t length = eq ? (size_t)(eq - body) : strlen(body);

			for (size_t j = 0;
				 j < sizeof(nag_long_options) / sizeof(nag_long_options[0]);
				 j++) {
				if (strlen(nag_long_options[j].name) == length &&
					memcmp(nag_long_options[j].name, body, length) == 0) {
					short_option = nag_long_options[j].short_option;
					break;
				}
			}
			if (eq)
				value = eq + 1;
		}

		if (short_option != '\0') {
			pool[0] = '-';
			pool[1] = short_option;
			pool[2] = '\0';
			out[n++] = pool;
			pool += 3;
			if (value)
				out[n++] = (char *)value;
			continue;
		}

		out[n++] = argv[i];
	}

	out[n] = NULL;
	*argc = n;
	return out;
}

static void nag_add_button(const char *text, const char *action, bool terminal,
						   bool dismiss) {
	struct nag_button *button = calloc(1, sizeof(*button));
	if (!button)
		return;
	button->text = strdup(text);
	button->action = action ? strdup(action) : NULL;
	button->terminal = terminal;
	button->dismiss = dismiss;
	struct nag_button **buttons =
		realloc(nag.buttons, (nag.buttons_len + 1) * sizeof(*nag.buttons));
	if (!buttons) {
		free(button->text);
		free(button->action);
		free(button);
		return;
	}
	nag.buttons = buttons;
	nag.buttons[nag.buttons_len++] = button;
}

static char *nag_read_stdin(void) {
	char *buffer = NULL;
	size_t length = 0;
	char *line = NULL;
	size_t line_size = 0;

	while (getline(&line, &line_size, stdin) != -1) {
		size_t n = strlen(line);
		char *next = realloc(buffer, length + n + 1);
		if (!next)
			break;
		buffer = next;
		memcpy(buffer + length, line, n);
		length += n;
		buffer[length] = '\0';
	}
	free(line);
	return buffer;
}

static char *nag_limit_lines(const char *text, int max_lines) {
	int lines = 1;
	for (const char *p = text; *p != '\0'; p++)
		if (*p == '\n')
			lines++;
	if (lines <= max_lines)
		return strdup(text);

	int keep = max_lines - 1;
	char *out = malloc(strlen(text) + 64);
	if (!out)
		return strdup(text);

	size_t o = 0;
	const char *p = text;
	for (int i = 0; i < keep && p; i++) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		memcpy(out + o, p, len);
		o += len;
		out[o++] = '\n';
		p = nl ? nl + 1 : NULL;
	}
	char summary[64];
	snprintf(summary, sizeof(summary), "\033[0m\xe2\x80\xa6 (+%d more)",
			 lines - keep);
	strcpy(out + o, summary);
	return out;
}

static char *nag_truncate_lines(const char *text, int max_chars) {
	if (!text)
		text = "";
	char *out = malloc(strlen(text) * 4 + 64);
	if (!out)
		return strdup(text);

	size_t o = 0;
	const char *p = text;
	for (;;) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);

		int codepoints = 0;
		size_t cut = len;
		for (size_t i = 0; i < len;) {
			size_t skip = nag_ansi_skip(p + i);
			if (skip > 0) {
				i += skip;
				continue;
			}
			unsigned char c = (unsigned char)p[i];
			if ((c & 0xC0) != 0x80) {
				if (codepoints == max_chars) {
					cut = i;
					break;
				}
				codepoints++;
			}
			i++;
		}

		memcpy(out + o, p, cut);
		o += cut;
		if (cut < len) {
			memcpy(out + o, "\033[0m\xe2\x80\xa6", 7);
			o += 7;
		}
		if (!nl)
			break;
		out[o++] = '\n';
		p = nl + 1;
	}
	out[o] = '\0';
	return out;
}

static void usage(void) {
	fprintf(stderr,
			"Usage: mangonag [options]\n"
			"  -m, --message <text>              message text\n"
			"  -l, --detailed-message            read extra lines from stdin\n"
			"  -t, --type <error|warning|info>   color preset\n"
			"  -e, --edge <top|bottom>           edge to anchor to\n"
			"  -y, --layer <overlay|top|bottom>  layer to appear on\n"
			"  -o, --output <name>               output to show on\n"
			"  -f, --font <pango font>           font description\n"
			"  -s, --dismiss-button <text>       dismiss button text\n"
			"  -b, --button <text> <command>     terminal button\n"
			"  -B, --button-no-terminal ...      run directly\n"
			"  -z, --button-dismiss <text> <cmd> command then dismiss\n"
			"  -Z, --button-dismiss-no-terminal  run directly then dismiss\n"
			"  -T, --timeout <ms>                auto dismiss after ms (0 = never)\n"
			"  -h, --help                        show this help\n");
}

int main(int argc, char *argv[]) {
	memset(&nag, 0, sizeof(nag));
	wl_list_init(&nag.outputs);
	nag.type = &type_error;
	nag.layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	nag.anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				 ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				 ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	nag.dismiss_text = strdup("X");
	nag.font = strdup("monospace 13");
	nag.running = true;

	char *long_pool = NULL;
	char **nag_argv = nag_expand_long_options(argv, &argc, &long_pool);
	if (!nag_argv)
		return EXIT_FAILURE;
	argv = nag_argv;

	int option;
	while ((option = getopt(argc, argv, "m:lt:e:y:o:f:s:b:B:z:Z:T:h")) != -1) {
		switch (option) {
		case 'm':
			free(nag.message);
			nag.message = strdup(optarg);
			break;
		case 'l':
			nag.details = nag_read_stdin();
			break;
		case 't':
			if (strcmp(optarg, "warning") == 0)
				nag.type = &type_warning;
			else if (strcmp(optarg, "info") == 0)
				nag.type = &type_info;
			else
				nag.type = &type_error;
			break;
		case 'e':
			if (strcmp(optarg, "bottom") == 0) {
				nag.edge = NAG_EDGE_BOTTOM;
				nag.anchor =
					ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
					ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
					ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
			}
			break;
		case 'y':
			if (strcmp(optarg, "overlay") == 0)
				nag.layer = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
			else if (strcmp(optarg, "bottom") == 0)
				nag.layer = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
			else
				nag.layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
			break;
		case 'o':
			free(nag.output_name);
			nag.output_name = strdup(optarg);
			break;
		case 'f':
			free(nag.font);
			nag.font = strdup(optarg);
			break;
		case 's':
			free(nag.dismiss_text);
			nag.dismiss_text = strdup(optarg);
			break;
		case 'b':
		case 'B':
		case 'z':
		case 'Z': {
			const char *text = optarg;
			const char *action = NULL;
			if (optind < argc && argv[optind][0] != '-') {
				action = argv[optind];
				optind++;
			}
			nag_add_button(text, action, option == 'b' || option == 'z',
						   option == 'z' || option == 'Z');
			break;
		}
		case 'T':
			nag.timeout_ms = atoi(optarg);
			break;
		case 'h':
			usage();
			free(nag_argv);
			free(long_pool);
			return EXIT_SUCCESS;
		default:
			usage();
			free(nag_argv);
			free(long_pool);
			return EXIT_FAILURE;
		}
	}
	free(nag_argv);
	free(long_pool);

	if (nag.details && nag.details[0] != '\0') {
		size_t a = nag.message ? strlen(nag.message) : 0;
		size_t b = strlen(nag.details);
		char *combined = malloc(a + b + 2);
		if (combined) {
			if (nag.message)
				memcpy(combined, nag.message, a);
			combined[a] = a > 0 ? '\n' : '\0';
			memcpy(combined + a + (a > 0 ? 1 : 0), nag.details, b);
			combined[a + b + (a > 0 ? 1 : 0)] = '\0';
			free(nag.message);
			nag.message = combined;
		}
	}

	if (nag.dismiss_text && nag.dismiss_text[0] != '\0')
		nag_add_button(nag.dismiss_text, NULL, false, true);

	if (nag.message) {
		size_t n = strlen(nag.message);
		while (n > 0 && (nag.message[n - 1] == '\n' ||
						 nag.message[n - 1] == '\r' ||
						 nag.message[n - 1] == ' ' ||
						 nag.message[n - 1] == '\t'))
			nag.message[--n] = '\0';
		char *capped = nag_limit_lines(nag.message, NAG_MAX_LINES);
		if (capped) {
			free(nag.message);
			nag.message = capped;
		}
	}

	struct sigaction sa = {.sa_handler = nag_signal, .sa_flags = 0};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	nag.display = wl_display_connect(NULL);
	if (!nag.display) {
		fprintf(stderr, "mangonag: failed to connect to the compositor\n");
		return EXIT_FAILURE;
	}
	nag.registry = wl_display_get_registry(nag.display);
	wl_registry_add_listener(nag.registry, &registry_listener, NULL);
	wl_display_roundtrip(nag.display);
	wl_display_roundtrip(nag.display);

	if (!nag.compositor || !nag.shm || !nag.layer_shell) {
		fprintf(stderr, "mangonag: missing required globals\n");
		nag_destroy(&nag);
		return EXIT_FAILURE;
	}

	if (nag.output_name) {
		struct nag_output *output;
		wl_list_for_each(output, &nag.outputs, link) {
			if (output->name && strcmp(output->name, nag.output_name) == 0) {
				nag.output = output->wl_output;
				if (output->scale > 0)
					nag.scale = output->scale;
				break;
			}
		}
	} else {
		struct nag_output *output;
		wl_list_for_each(output, &nag.outputs, link) {
			if (output->scale > nag.scale)
				nag.scale = output->scale;
		}
	}

	nag_setup_surface();

	int64_t deadline_ms = 0;
	if (nag.timeout_ms > 0) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		deadline_ms = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000 +
					  nag.timeout_ms;
	}

	while (nag.running) {
		if (wl_display_dispatch_pending(nag.display) < 0)
			break;
		if (wl_display_flush(nag.display) < 0 && errno != EAGAIN)
			break;

		struct pollfd pfd = {
			.fd = wl_display_get_fd(nag.display),
			.events = POLLIN,
		};
		int timeout = -1;
		if (nag.timeout_ms > 0) {
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			int64_t now_ms =
				(int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
			int64_t remaining = deadline_ms - now_ms;
			if (remaining <= 0)
				break;
			timeout = (int)remaining;
		}
		int ret = poll(&pfd, 1, timeout);
		if (ret == 0) {
			break;
		} else if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		if (pfd.revents & POLLIN) {
			if (wl_display_dispatch(nag.display) < 0)
				break;
		}
	}

	nag_destroy(&nag);
	return EXIT_SUCCESS;
}

static void nag_destroy(struct nag_state *n) {
	if (n->layer_surface) {
		zwlr_layer_surface_v1_destroy(n->layer_surface);
		n->layer_surface = NULL;
	}
	if (n->surface) {
		wl_surface_destroy(n->surface);
		n->surface = NULL;
	}
	if (n->display) {
		wl_display_flush(n->display);
	}
	if (n->pointer)
		wl_pointer_destroy(n->pointer);
	if (n->keyboard)
		wl_keyboard_destroy(n->keyboard);
	if (n->seat)
		wl_seat_destroy(n->seat);
	if (n->layer_shell)
		zwlr_layer_shell_v1_destroy(n->layer_shell);
	if (n->compositor)
		wl_compositor_destroy(n->compositor);
	if (n->shm)
		wl_shm_destroy(n->shm);
	if (n->registry)
		wl_registry_destroy(n->registry);

	struct nag_output *output, *tmp;
	wl_list_for_each_safe(output, tmp, &n->outputs, link) {
		wl_list_remove(&output->link);
		if (output->wl_output)
			wl_output_destroy(output->wl_output);
		free(output->name);
		free(output);
	}

	for (size_t i = 0; i < n->buttons_len; i++) {
		struct nag_button *button = n->buttons[i];
		if (!button)
			continue;
		free(button->text);
		free(button->action);
		free(button);
	}
	free(n->buttons);
	free(n->message);
	free(n->details);
	free(n->font);
	free(n->dismiss_text);
	free(n->output_name);

	if (n->display)
		wl_display_disconnect(n->display);
}
