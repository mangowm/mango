#undef _POSIX_C_SOURCE
#define _XOPEN_SOURCE 700
#include "mango/config/error_nag.h"

#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/error_store.h"
#include "mango/manage/monitor.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server-core.h>

static struct wl_client *nag_client = NULL;
static struct wl_listener nag_client_destroy = {0};
static struct wl_event_source *nag_timer = NULL;

static void nag_client_destroyed(struct wl_listener *listener, void *data) {
	(void)listener;
	(void)data;

	wl_list_remove(&nag_client_destroy.link);
	wl_list_init(&nag_client_destroy.link);
	nag_client = NULL;
}

static bool nag_set_cloexec(int fd) {
	int flags = fcntl(fd, F_GETFD);
	if (flags < 0)
		return false;

	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

static void nag_strip_ansi(const char *src, char *dst, size_t dst_size) {
	size_t j = 0;

	for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size;) {
		if ((unsigned char)src[i] == '\033' && src[i + 1] == '[') {
			size_t k = i + 2;
			while (src[k] != '\0' && !((unsigned char)src[k] >= '@' &&
									   (unsigned char)src[k] <= '~'))
				k++;
			if (src[k] == '\0')
				break;
			i = k + 1;
			continue;
		}
		dst[j++] = src[i++];
	}
	dst[j] = '\0';
}

static void nag_kill(void) {
	if (nag_client == NULL)
		return;

	/* Tearing down the compositor-side client closes the connection, which
	 * makes mangonag leave its main loop and exit on its own. The destroy
	 * listener clears nag_client for us. */
	wl_client_destroy(nag_client);
}

static bool nag_first_target(char *path, size_t path_size, int *line_out) {
	FILE *file = fopen(config_error_store_path(), "r");
	if (!file)
		return false;

	bool found = false;
	char raw[1024];
	char line[1024];
	while (fgets(raw, sizeof(raw), file)) {
		nag_strip_ansi(raw, line, sizeof(line));
		char *p = strstr(line, "[Index] ");
		if (p) {
			p += 8;
			while (*p == ' ' || *p == '\t')
				p++;
			size_t n = strlen(p);
			while (n > 0 &&
				   (p[n - 1] == '\n' || p[n - 1] == '\r' || p[n - 1] == ' '))
				p[--n] = '\0';
			char *colon = strrchr(p, ':');
			if (colon && colon[1] >= '0' && colon[1] <= '9') {
				*line_out = atoi(colon + 1);
				*colon = '\0';
			}
			snprintf(path, path_size, "%s", p);
			found = true;
			break;
		}
		p = strstr(line, "File \"");
		if (p) {
			p += 6;
			char *end = strchr(p, '"');
			if (end) {
				size_t n = (size_t)(end - p);
				if (n >= path_size)
					n = path_size - 1;
				memcpy(path, p, n);
				path[n] = '\0';
				char *lp = strstr(end, "line ");
				if (lp)
					*line_out = atoi(lp + 5);
				found = true;
				break;
			}
		}
	}
	fclose(file);
	return found;
}

static char *nag_shell_quote(const char *s) {
	size_t len = 2;
	for (const char *p = s; *p != '\0'; p++)
		len += (*p == '\'') ? 4 : 1;

	char *out = malloc(len + 1);
	if (!out)
		return NULL;

	char *o = out;
	*o++ = '\'';
	for (const char *p = s; *p != '\0'; p++) {
		if (*p == '\'') {
			memcpy(o, "'\\''", 4);
			o += 4;
		} else {
			*o++ = *p;
		}
	}
	*o++ = '\'';
	*o = '\0';
	return out;
}

static char *nag_edit_command(void) {
	char path[PATH_MAX] = {0};
	int line = 0;
	nag_first_target(path, sizeof(path), &line);
	if (path[0] == '\0')
		return NULL;

	if (path[0] != '/') {
		char resolved[PATH_MAX];
		if (realpath(path, resolved))
			snprintf(path, sizeof(path), "%s", resolved);
	}

	const char *editor = getenv("EDITOR");
	if (!editor || editor[0] == '\0')
		editor = getenv("VISUAL");
	if (!editor || editor[0] == '\0')
		editor = "vi";

	char name[256];
	snprintf(name, sizeof(name), "%s", editor);
	char *base = strrchr(name, '/');
	base = base ? base + 1 : name;
	char *stop = base;
	while (*stop != '\0' && *stop != ' ')
		stop++;
	*stop = '\0';

	char *path_arg = nag_shell_quote(path);
	if (!path_arg)
		return NULL;

	if (line <= 0) {
		char *cmd = string_printf("%s %s", editor, path_arg);
		free(path_arg);
		return cmd;
	}

	char *with_line = string_printf("%s:%d", path, line);
	char *line_arg = with_line ? nag_shell_quote(with_line) : NULL;
	free(with_line);
	if (!line_arg) {
		free(path_arg);
		return NULL;
	}

	static const char *plus[] = {"vi",	  "vim",		 "nvim", "gvim",
								 "emacs", "emacsclient", "nano", NULL};
	static const char *colon[] = {"hx", "helix", "micro", "zed", NULL};
	static const char *code[] = {"code", "codium", NULL};

	char *cmd = NULL;
	for (int i = 0; plus[i] && !cmd; i++)
		if (strcmp(base, plus[i]) == 0)
			cmd = string_printf("%s +%d %s", editor, line, path_arg);
	for (int i = 0; colon[i] && !cmd; i++)
		if (strcmp(base, colon[i]) == 0)
			cmd = string_printf("%s %s", editor, line_arg);
	for (int i = 0; code[i] && !cmd; i++)
		if (strcmp(base, code[i]) == 0)
			cmd = string_printf("%s -g %s", editor, line_arg);
	if (!cmd)
		cmd = string_printf("%s %s", editor, path_arg);

	free(path_arg);
	free(line_arg);
	return cmd;
}

static void nag_pipe_text(int fd) {
	char buffer[1024];
	int total = 0;

	FILE *file = fopen(config_error_store_path(), "r");
	if (!file)
		return;
	while (fgets(buffer, sizeof(buffer), file)) {
		if (buffer[0] != '\n' && buffer[0] != '\0')
			total++;
	}
	fclose(file);

	int shown = 0;

	file = fopen(config_error_store_path(), "r");
	if (!file)
		return;
	while (shown < 3 && fgets(buffer, sizeof(buffer), file)) {
		if (buffer[0] == '\n')
			continue;
		write(fd, buffer, strlen(buffer));
		if (buffer[strlen(buffer) - 1] != '\n')
			write(fd, "\n", 1);
		shown++;
	}
	fclose(file);

	int hidden = total - shown;
	if (hidden > 0) {
		char summary[80];
		snprintf(summary, sizeof(summary), "\xe2\x80\xa6 (+%d more line%s)\n",
				 hidden, hidden == 1 ? "" : "s");
		write(fd, summary, strlen(summary));
	}
}

static void nag_spawn(void) {
	if (!server.display)
		return;

	nag_kill();

	char *edit = nag_edit_command();

	int pfd[2];
	if (pipe(pfd) != 0) {
		free(edit);
		return;
	}

	int sockets[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
		close(pfd[0]);
		close(pfd[1]);
		free(edit);
		return;
	}

	/* Keep both ends close-on-exec; the child re-enables its end so
	 * libwayland can pick it up from WAYLAND_SOCKET after the exec. */
	if (!nag_set_cloexec(sockets[0]) || !nag_set_cloexec(sockets[1])) {
		close(sockets[0]);
		close(sockets[1]);
		close(pfd[0]);
		close(pfd[1]);
		free(edit);
		return;
	}

	struct wl_client *client = wl_client_create(server.display, sockets[0]);
	if (client == NULL) {
		close(sockets[0]);
		close(sockets[1]);
		close(pfd[0]);
		close(pfd[1]);
		free(edit);
		return;
	}

	nag_client = client;
	nag_client_destroy.notify = nag_client_destroyed;
	wl_client_add_destroy_listener(client, &nag_client_destroy);

	pid_t pid = fork();
	if (pid < 0) {
		wl_client_destroy(client);
		close(sockets[1]);
		close(pfd[0]);
		close(pfd[1]);
		free(edit);
		return;
	}
	if (pid == 0) {
		close(sockets[0]);

		int flags = fcntl(sockets[1], F_GETFD);
		if (flags >= 0)
			fcntl(sockets[1], F_SETFD, flags & ~FD_CLOEXEC);

		char socket_str[16];
		snprintf(socket_str, sizeof(socket_str), "%d", sockets[1]);
		setenv("WAYLAND_SOCKET", socket_str, true);

		if (pfd[0] != STDIN_FILENO) {
			dup2(pfd[0], STDIN_FILENO);
			close(pfd[0]);
		}
		close(pfd[1]);
		const char *out = NULL;
		if (server.selected_monitor && server.selected_monitor->wlr_output)
			out = server.selected_monitor->wlr_output->name;
		if (out && edit)
			execlp("mangonag", "mangonag", "-o", out, "-t", "error", "-e",
				   "top", "-l", "-s", "Close", "-b", "Edit", edit,
				   (char *)NULL);
		else if (out)
			execlp("mangonag", "mangonag", "-o", out, "-t", "error", "-e",
				   "top", "-l", "-s", "Close", (char *)NULL);
		else if (edit)
			execlp("mangonag", "mangonag", "-t", "error", "-e", "top", "-l",
				   "-s", "Close", "-b", "Edit", edit, (char *)NULL);
		else
			execlp("mangonag", "mangonag", "-t", "error", "-e", "top", "-l",
				   "-s", "Close", (char *)NULL);
		_exit(127);
	}

	close(sockets[1]);
	close(pfd[0]);
	nag_pipe_text(pfd[1]);
	close(pfd[1]);
	free(edit);
}

void config_error_nag_update(void) {
	if (!server.event_loop)
		return;

	FILE *file = fopen(config_error_store_path(), "r");
	bool has_error = false;
	if (file) {
		int c = fgetc(file);
		if (c != EOF)
			has_error = true;
		fclose(file);
	}

	nag_kill();
	if (has_error)
		nag_spawn();
}

static int nag_initial_cb(void *data) {
	if (nag_timer) {
		wl_event_source_remove(nag_timer);
		nag_timer = NULL;
	}
	config_error_nag_update();
	return 0;
}

void config_error_nag_init(void) {
	if (!server.event_loop)
		return;
	nag_timer =
		wl_event_loop_add_timer(server.event_loop, nag_initial_cb, NULL);
	if (nag_timer)
		wl_event_source_timer_update(nag_timer, 1);
}

void config_error_nag_destroy(void) {
	if (nag_timer) {
		wl_event_source_remove(nag_timer);
		nag_timer = NULL;
	}
	nag_kill();
}
