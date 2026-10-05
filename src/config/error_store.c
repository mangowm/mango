#define _GNU_SOURCE

#include "mango/config/error_store.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char store_path[PATH_MAX];
static FILE *store_file = NULL;
static bool store_active = false;

static bool store_dir_ok(const char *path) {
	struct stat st;

	if (lstat(path, &st) != 0)
		return false;
	if (!S_ISDIR(st.st_mode))
		return false;
	if (st.st_uid != getuid())
		return false;
	if ((st.st_mode & 0077) != 0)
		chmod(path, 0700);
	return true;
}

static bool store_dir_prepare(const char *path) {
	if (store_dir_ok(path))
		return true;
	if (mkdir(path, 0700) == 0)
		return true;
	return store_dir_ok(path);
}

const char *config_error_store_path(void) {
	if (store_path[0] == '\0') {
		char dir[PATH_MAX];
		dir[0] = '\0';

		const char *runtime = getenv("XDG_RUNTIME_DIR");
		if (runtime && runtime[0] == '/') {
			snprintf(dir, sizeof(dir), "%s/mango", runtime);
			if (!store_dir_prepare(dir))
				dir[0] = '\0';
		}
		if (dir[0] == '\0') {
			snprintf(dir, sizeof(dir), "/tmp/mango-%d", (int)getuid());
			if (!store_dir_prepare(dir))
				dir[0] = '\0';
		}
		if (dir[0] == '\0')
			return store_path;
		snprintf(store_path, sizeof(store_path), "%s/config-errors.log", dir);
	}
	return store_path;
}

void config_error_store_begin(void) {
	config_error_store_end();

	const char *path = config_error_store_path();
	if (path[0] == '\0')
		return;

	int fd =
		open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (fd < 0)
		return;
	store_file = fdopen(fd, "w");
	if (store_file == NULL) {
		close(fd);
		return;
	}
	store_active = true;
}

void config_error_store_end(void) {
	if (store_file) {
		fclose(store_file);
		store_file = NULL;
	}
	store_active = false;
}

bool config_error_store_active(void) { return store_active; }

void config_error_store_record(const char *message) {
	if (!store_active || store_file == NULL || message == NULL)
		return;

	size_t len = strlen(message);
	char *clean = malloc(len + 1);
	if (clean == NULL)
		return;
	memcpy(clean, message, len + 1);

	size_t clean_len = strlen(clean);
	while (clean_len > 0 &&
		   (clean[clean_len - 1] == '\n' || clean[clean_len - 1] == '\r' ||
			clean[clean_len - 1] == ' ' || clean[clean_len - 1] == '\t'))
		clean[--clean_len] = '\0';

	if (clean_len > 0) {
		fputs(clean, store_file);
		if (strchr(clean, '\033') != NULL &&
			!(clean_len >= 4 && strcmp(clean + clean_len - 4, "\033[0m") == 0))
			fputs("\033[0m", store_file);
		fputc('\n', store_file);
		fflush(store_file);
	}

	free(clean);
}
