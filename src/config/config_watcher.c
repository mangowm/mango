#define _GNU_SOURCE
#include "mango/config/config_watcher.h"

#include <errno.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "mango/common/log.h"
#include "mango/config/parse_config.h"

#define CONFIG_WATCH_EVENTS                                                    \
	(IN_CREATE | IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO |    \
	 IN_DELETE | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF)

typedef struct {
	int wd;
	char *dir;
} ConfigDirWatch;

static int inotify_fd = -1;
static struct wl_event_source *inotify_source = NULL;
static struct wl_event_source *reload_timer = NULL;
static ConfigDirWatch *dir_watches = NULL;
static int dir_watches_count = 0;
static char **watched_files = NULL;
static int watched_files_count = 0;

static void clear_dir_watches(void) {
	for (int i = 0; i < dir_watches_count; i++) {
		if (inotify_fd >= 0)
			inotify_rm_watch(inotify_fd, dir_watches[i].wd);
		free(dir_watches[i].dir);
	}
	free(dir_watches);
	dir_watches = NULL;
	dir_watches_count = 0;
}

static void clear_watched_files(void) {
	for (int i = 0; i < watched_files_count; i++) {
		free(watched_files[i]);
	}
	free(watched_files);
	watched_files = NULL;
	watched_files_count = 0;
}

static char *path_dirname(const char *path) {
	const char *slash = strrchr(path, '/');

	if (!slash)
		return strdup(".");
	if (slash == path)
		return strdup("/");

	size_t len = (size_t)(slash - path);
	char *dir = malloc(len + 1);
	if (!dir)
		return NULL;
	memcpy(dir, path, len);
	dir[len] = '\0';
	return dir;
}

static bool dir_is_watched(const char *dir) {
	for (int i = 0; i < dir_watches_count; i++) {
		if (strcmp(dir_watches[i].dir, dir) == 0)
			return true;
	}
	return false;
}

static void add_dir_watch(const char *dir) {
	if (dir_is_watched(dir))
		return;

	ConfigDirWatch *next =
		realloc(dir_watches, (dir_watches_count + 1) * sizeof(ConfigDirWatch));
	if (!next)
		return;
	dir_watches = next;

	int wd = inotify_add_watch(inotify_fd, dir, CONFIG_WATCH_EVENTS);
	if (wd < 0) {
		return;
	}

	dir_watches[dir_watches_count].wd = wd;
	dir_watches[dir_watches_count].dir = strdup(dir);
	dir_watches_count++;
}

static bool file_is_tracked(const char *path) {
	for (int i = 0; i < watched_files_count; i++) {
		if (strcmp(watched_files[i], path) == 0)
			return true;
	}
	return false;
}

static void track_path(const char *path) {
	if (file_is_tracked(path))
		return;

	char **next =
		realloc(watched_files, (watched_files_count + 1) * sizeof(char *));
	if (!next)
		return;
	watched_files = next;
	watched_files[watched_files_count] = strdup(path);
	watched_files_count++;

	char *dir = path_dirname(path);
	if (dir) {
		add_dir_watch(dir);
		free(dir);
	}
}

static void track_config_path(const char *path) {
	track_path(path);

	char resolved[4096];
	if (realpath(path, resolved) != NULL && strcmp(resolved, path) != 0) {
		track_path(resolved);
	}
}

static int find_dir_watch(int wd) {
	for (int i = 0; i < dir_watches_count; i++) {
		if (dir_watches[i].wd == wd)
			return i;
	}
	return -1;
}

static void handle_event(const struct inotify_event *event, bool *changed) {
	if (event->mask & IN_Q_OVERFLOW) {
		*changed = true;
		return;
	}
	if (event->mask & IN_IGNORED)
		return;
	if (event->len == 0) {
		if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF))
			*changed = true;
		return;
	}

	int index = find_dir_watch(event->wd);
	if (index < 0)
		return;

	const char *dir = dir_watches[index].dir;
	size_t dir_len = strlen(dir);
	char path[4096];
	if (dir_len > 0 && dir[dir_len - 1] == '/')
		snprintf(path, sizeof(path), "%s%s", dir, event->name);
	else
		snprintf(path, sizeof(path), "%s/%s", dir, event->name);

	for (int i = 0; i < watched_files_count; i++) {
		if (strcmp(watched_files[i], path) == 0) {
			*changed = true;
			return;
		}
	}
}

static int on_inotify(int fd, uint32_t mask, void *data) {
	(void)mask;
	(void)data;
	alignas(struct inotify_event) char buffer[4096];
	bool changed = false;

	for (;;) {
		ssize_t len = read(fd, buffer, sizeof(buffer));
		if (len < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (len == 0)
			break;

		size_t offset = 0;
		while (offset + sizeof(struct inotify_event) <= (size_t)len) {
			const struct inotify_event *event =
				(const struct inotify_event *)(buffer + offset);
			handle_event(event, &changed);
			offset += sizeof(struct inotify_event) + event->len;
		}
	}

	if (changed && reload_timer != NULL) {
		wl_event_source_timer_update(reload_timer, 150);
	}
	return 0;
}

static int on_reload_timer(void *data) {
	(void)data;
	reload_config(NULL);
	return 0;
}

void config_watcher_init(struct wl_event_loop *loop) {
	if (inotify_fd >= 0)
		return;

	inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (inotify_fd < 0) {
		mango_error(false, WLR_ERROR,
					"inotify unavailable: config auto reload disabled\n");
		return;
	}

	inotify_source = wl_event_loop_add_fd(loop, inotify_fd, WL_EVENT_READABLE,
										  on_inotify, NULL);
	reload_timer = wl_event_loop_add_timer(loop, on_reload_timer, NULL);

	if (inotify_source == NULL || reload_timer == NULL) {
		if (inotify_source != NULL) {
			wl_event_source_remove(inotify_source);
			inotify_source = NULL;
		}
		if (reload_timer != NULL) {
			wl_event_source_remove(reload_timer);
			reload_timer = NULL;
		}
		close(inotify_fd);
		inotify_fd = -1;
		return;
	}

	config_watcher_update();
}

void config_watcher_update(void) {
	if (inotify_fd < 0)
		return;

	clear_dir_watches();
	clear_watched_files();

	if (!config.auto_reload_config)
		return;

	int count = 0;
	char **paths = config_get_file_paths(&count);
	for (int i = 0; i < count; i++) {
		if (paths[i])
			track_config_path(paths[i]);
	}
}

void config_watcher_destroy(void) {
	if (inotify_source != NULL) {
		wl_event_source_remove(inotify_source);
		inotify_source = NULL;
	}
	if (reload_timer != NULL) {
		wl_event_source_remove(reload_timer);
		reload_timer = NULL;
	}
	if (inotify_fd >= 0) {
		clear_dir_watches();
		close(inotify_fd);
		inotify_fd = -1;
	}
	clear_watched_files();
}
