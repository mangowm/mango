#include "mango/config/watcher.h"

#include <errno.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mango/common/log.h"
#include "mango/config/parse.h"

#if defined(__linux__)
#define CONFIG_WATCH_INOTIFY 1
#elif defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) ||  \
	defined(__NetBSD__) || defined(__OpenBSD__)
#define CONFIG_WATCH_KQUEUE 1
#endif

#if defined(CONFIG_WATCH_INOTIFY)
#include <sys/inotify.h>
#elif defined(CONFIG_WATCH_KQUEUE)
#include <fcntl.h>
#include <sys/event.h>
#endif

#define CONFIG_WATCH_DEBOUNCE_MS 150

typedef struct {
	char *path;
	int fd;
	bool seen;
	bool exists;
	dev_t dev;
	ino_t ino;
	off_t size;
	struct timespec mtime;
} WatchedFile;

static struct wl_event_source *watch_timer = NULL;
static WatchedFile *watched_files = NULL;
static int watched_files_count = 0;
static bool watcher_active = false;

#if defined(CONFIG_WATCH_INOTIFY) || defined(CONFIG_WATCH_KQUEUE)
static void schedule_check(void) {
	if (watcher_active && watch_timer != NULL)
		wl_event_source_timer_update(watch_timer, CONFIG_WATCH_DEBOUNCE_MS);
}
#endif

static bool refresh_file_state(WatchedFile *file) {
	struct stat st;
	bool exists = stat(file->path, &st) == 0;
	bool changed = false;

	if (file->seen) {
		if (file->exists != exists)
			changed = true;
		else if (exists)
			changed = file->dev != st.st_dev || file->ino != st.st_ino ||
					  file->size != st.st_size ||
					  file->mtime.tv_sec != st.st_mtim.tv_sec ||
					  file->mtime.tv_nsec != st.st_mtim.tv_nsec;
	}

	file->seen = true;
	file->exists = exists;
	if (exists) {
		file->dev = st.st_dev;
		file->ino = st.st_ino;
		file->size = st.st_size;
		file->mtime = st.st_mtim;
	}

	return changed;
}

static void clear_watched_files(void) {
	for (int i = 0; i < watched_files_count; i++) {
		if (watched_files[i].fd >= 0)
			close(watched_files[i].fd);
		free(watched_files[i].path);
	}
	free(watched_files);
	watched_files = NULL;
	watched_files_count = 0;
}

static bool file_is_tracked(const char *path) {
	for (int i = 0; i < watched_files_count; i++) {
		if (strcmp(watched_files[i].path, path) == 0)
			return true;
	}
	return false;
}

static void track_path(const char *path) {
	if (file_is_tracked(path))
		return;

	WatchedFile *next =
		realloc(watched_files, (watched_files_count + 1) * sizeof(WatchedFile));
	if (next == NULL)
		return;
	watched_files = next;

	WatchedFile *file = &watched_files[watched_files_count];
	file->path = strdup(path);
	if (file->path == NULL)
		return;

	file->fd = -1;
	file->seen = false;
	file->exists = false;
	refresh_file_state(file);
	watched_files_count++;
}

#if defined(CONFIG_WATCH_INOTIFY)

#define CONFIG_WATCH_EVENTS                                                    \
	(IN_CREATE | IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVED_FROM | IN_MOVED_TO |    \
	 IN_DELETE | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF)

typedef struct {
	int wd;
	char *dir;
} DirWatch;

static int inotify_fd = -1;
static struct wl_event_source *inotify_source = NULL;
static DirWatch *dir_watches = NULL;
static int dir_watches_count = 0;

static char *path_dirname(const char *path) {
	const char *slash = strrchr(path, '/');

	if (slash == NULL)
		return strdup(".");
	if (slash == path)
		return strdup("/");

	size_t len = (size_t)(slash - path);
	char *dir = malloc(len + 1);
	if (dir == NULL)
		return NULL;
	memcpy(dir, path, len);
	dir[len] = '\0';
	return dir;
}

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

	DirWatch *next =
		realloc(dir_watches, (dir_watches_count + 1) * sizeof(DirWatch));
	if (next == NULL)
		return;
	dir_watches = next;

	int wd = inotify_add_watch(inotify_fd, dir, CONFIG_WATCH_EVENTS);
	if (wd < 0)
		return;

	dir_watches[dir_watches_count].wd = wd;
	dir_watches[dir_watches_count].dir = strdup(dir);
	dir_watches_count++;
}

static int find_dir_watch(int wd) {
	for (int i = 0; i < dir_watches_count; i++) {
		if (dir_watches[i].wd == wd)
			return i;
	}
	return -1;
}

static bool event_matches(const struct inotify_event *event) {
	if (event->mask & IN_Q_OVERFLOW)
		return true;
	if (event->mask & IN_IGNORED)
		return false;
	if (event->len == 0)
		return (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) != 0;

	int index = find_dir_watch(event->wd);
	if (index < 0)
		return false;

	const char *dir = dir_watches[index].dir;
	size_t dir_len = strlen(dir);
	char path[4096];
	if (dir_len > 0 && dir[dir_len - 1] == '/')
		snprintf(path, sizeof(path), "%s%s", dir, event->name);
	else
		snprintf(path, sizeof(path), "%s/%s", dir, event->name);

	for (int i = 0; i < watched_files_count; i++) {
		if (strcmp(watched_files[i].path, path) == 0)
			return true;
	}
	return false;
}

static int on_inotify_event(int fd, uint32_t mask, void *data) {
	alignas(struct inotify_event) char buffer[4096];
	bool pending = false;

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
			if (event_matches(event))
				pending = true;
			offset += sizeof(struct inotify_event) + event->len;
		}
	}

	if (pending)
		schedule_check();
	return 0;
}

static bool backend_init(struct wl_event_loop *loop) {
	inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (inotify_fd < 0)
		return false;

	inotify_source = wl_event_loop_add_fd(loop, inotify_fd, WL_EVENT_READABLE,
										  on_inotify_event, NULL);
	if (inotify_source == NULL) {
		close(inotify_fd);
		inotify_fd = -1;
		return false;
	}

	return true;
}

static void backend_destroy(void) {
	if (inotify_source != NULL) {
		wl_event_source_remove(inotify_source);
		inotify_source = NULL;
	}
	clear_dir_watches();
	if (inotify_fd >= 0) {
		close(inotify_fd);
		inotify_fd = -1;
	}
}

static void backend_sync(void) {
	if (inotify_fd < 0)
		return;

	clear_dir_watches();
	for (int i = 0; i < watched_files_count; i++) {
		char *dir = path_dirname(watched_files[i].path);
		if (dir != NULL) {
			add_dir_watch(dir);
			free(dir);
		}
	}
}

#elif defined(CONFIG_WATCH_KQUEUE)

static int kqueue_fd = -1;
static struct wl_event_source *kqueue_source = NULL;

static void add_file_watch(WatchedFile *file) {
	struct kevent change;

	file->fd = open(file->path, O_RDONLY | O_CLOEXEC);
	if (file->fd < 0)
		return;

	EV_SET(&change, (uintptr_t)file->fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
		   NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME |
			   NOTE_REVOKE,
		   0, NULL);
	if (kevent(kqueue_fd, &change, 1, NULL, 0, NULL) < 0) {
		close(file->fd);
		file->fd = -1;
	}
}

static int on_kqueue_event(int fd, uint32_t mask, void *data) {
	struct kevent events[8];
	struct timespec timeout = {0, 0};
	bool pending = false;

	for (;;) {
		int count = kevent(fd, NULL, 0, events, 8, &timeout);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (count == 0)
			break;
		pending = true;
	}

	if (pending)
		schedule_check();
	return 0;
}

static bool backend_init(struct wl_event_loop *loop) {
	kqueue_fd = kqueue();
	if (kqueue_fd < 0)
		return false;

	kqueue_source = wl_event_loop_add_fd(loop, kqueue_fd, WL_EVENT_READABLE,
										 on_kqueue_event, NULL);
	if (kqueue_source == NULL) {
		close(kqueue_fd);
		kqueue_fd = -1;
		return false;
	}

	return true;
}

static void backend_destroy(void) {
	if (kqueue_source != NULL) {
		wl_event_source_remove(kqueue_source);
		kqueue_source = NULL;
	}
	if (kqueue_fd >= 0) {
		close(kqueue_fd);
		kqueue_fd = -1;
	}
}

static void backend_sync(void) {
	if (kqueue_fd < 0)
		return;

	for (int i = 0; i < watched_files_count; i++)
		add_file_watch(&watched_files[i]);
}

#else

static bool backend_init(struct wl_event_loop *loop) { return false; }

static void backend_destroy(void) {}

static void backend_sync(void) {}

#endif

static int on_watch_timer(void *data) {

	bool changed = false;
	for (int i = 0; i < watched_files_count; i++) {
		if (refresh_file_state(&watched_files[i]))
			changed = true;
	}

	if (changed)
		reload_config(NULL);

	return 0;
}

void config_watcher_init(struct wl_event_loop *loop) {
	if (watch_timer != NULL)
		return;

	watch_timer = wl_event_loop_add_timer(loop, on_watch_timer, NULL);
	if (watch_timer == NULL) {
		mango_error(
			false, WLR_ERROR,
			"config auto reload disabled: no event loop timer available\n");
		return;
	}

	if (!backend_init(loop)) {
		mango_error(false, WLR_ERROR,
					"config auto reload disabled: no filesystem event backend "
					"available\n");
		return;
	}

	watcher_active = true;
	config_watcher_update();
}

void config_watcher_update(void) {
	clear_watched_files();

	if (!watcher_active || !config.auto_reload_config)
		return;

	int count = 0;
	char **paths = config_get_file_paths(&count);
	for (int i = 0; i < count; i++) {
		if (paths[i] != NULL)
			track_path(paths[i]);
	}

	backend_sync();
}

void config_watcher_destroy(void) {
	backend_destroy();

	if (watch_timer != NULL) {
		wl_event_source_remove(watch_timer);
		watch_timer = NULL;
	}

	watcher_active = false;
	clear_watched_files();
}
