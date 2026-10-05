#define _GNU_SOURCE

#include "mango/config/error_nag.h"

#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/error_store.h"
#include "mango/manage/monitor.h"
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-server-core.h>

static pid_t nag_pid = -1;
static int nag_pidfd = -1;
static struct wl_event_source *nag_timer = NULL;

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
#ifdef SYS_pidfd_send_signal
	if (nag_pidfd >= 0) {
		syscall(SYS_pidfd_send_signal, nag_pidfd, SIGTERM, NULL, 0);
		close(nag_pidfd);
		nag_pidfd = -1;
		nag_pid = -1;
		return;
	}
#endif
	if (nag_pid <= 0)
		return;

	sigset_t block, old;
	sigemptyset(&block);
	sigaddset(&block, SIGCHLD);
	sigprocmask(SIG_BLOCK, &block, &old);

	int status;
	pid_t ret = waitpid(nag_pid, &status, WNOHANG);
	if (ret == 0)
		kill(nag_pid, SIGTERM);

	nag_pid = -1;
	sigprocmask(SIG_SETMASK, &old, NULL);
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
	char *edit = nag_edit_command();

	int pfd[2];
	if (pipe(pfd) != 0) {
		free(edit);
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pfd[0]);
		close(pfd[1]);
		free(edit);
		return;
	}
	if (pid == 0) {
		dup2(pfd[0], STDIN_FILENO);
		close(pfd[0]);
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

	close(pfd[0]);
	nag_pid = pid;
#ifdef SYS_pidfd_open
	nag_pidfd = syscall(SYS_pidfd_open, pid, 0);
#else
	nag_pidfd = -1;
#endif
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
