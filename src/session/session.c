#include "session.h"

#include <cjson/cJSON.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "../common/util.h"

extern int32_t mango_session_is_config_enabled(void);
extern int32_t mango_session_write_snapshot(FILE *out);
extern const char *mango_session_client_appid(Client *c);
extern const char *mango_session_client_title(Client *c);
extern const char *mango_session_client_monitor(Client *c);
extern const char *mango_session_lookup_launch_command(const char *app_id,
													   const char *title);
extern void mango_session_remember_client_launch_command(Client *c,
														 const char *command);
extern void mango_session_spawn_command(const char *command);
extern void mango_session_apply_restore_entry(Client *c,
											  const SessionRestoreEntry *entry);
extern void mango_session_spawn_tracker_init(void);
extern void mango_session_spawn_tracker_shutdown(void);

typedef struct {
	SessionRestoreEntry entry;
	bool used;
} PendingSessionEntry;

static PendingSessionEntry *pending_entries;
static size_t pending_count;
static bool restore_started;

static bool ensure_directory_tree(const char *dir) {
	char tmp[PATH_MAX];
	size_t len;

	if (!dir || dir[0] == '\0')
		return false;

	len = strlen(dir);
	if (len >= sizeof(tmp))
		return false;

	memcpy(tmp, dir, len + 1);

	for (char *p = tmp + 1; *p != '\0'; ++p) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(tmp, 0755) < 0 && errno != EEXIST)
			return false;
		*p = '/';
	}

	return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static char *session_data_dir(void) {
	const char *xdg_data = getenv("XDG_DATA_HOME");
	if (xdg_data && xdg_data[0] != '\0')
		return string_printf("%s/mango", xdg_data);

	const char *home = getenv("HOME");
	if (!home || home[0] == '\0')
		return NULL;

	return string_printf("%s/.local/share/mango", home);
}

static char *session_file_path(void) {
	char *dir = session_data_dir();
	char *path;
	if (!dir)
		return NULL;
	path = string_printf("%s/session.json", dir);
	free(dir);
	return path;
}

static bool session_file_is_trusted(const char *path) {
	struct stat st;
	uid_t uid = getuid();

	if (!path || stat(path, &st) != 0)
		return false;

	if (!S_ISREG(st.st_mode))
		return false;
	if (st.st_uid != uid)
		return false;
	if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0)
		return false;

	return true;
}

static void free_pending_entries(void) {
	free(pending_entries);
	pending_entries = NULL;
	pending_count = 0;
}

static bool session_json_string(const cJSON *object, const char *key, char *dest,
								size_t dest_size, bool required) {
	const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	size_t len;

	if (!item)
		return !required;
	if (!cJSON_IsString(item) || !item->valuestring || dest_size == 0)
		return false;

	len = strlen(item->valuestring);
	if ((required && len == 0) || len >= dest_size)
		return false;

	memcpy(dest, item->valuestring, len + 1);
	return true;
}

static bool session_json_int32(const cJSON *object, const char *key, int32_t *out,
							   bool required) {
	const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	double value;

	if (!item)
		return !required;
	if (!cJSON_IsNumber(item))
		return false;

	value = item->valuedouble;
	if (value < INT32_MIN || value > INT32_MAX ||
		value != (double)(int32_t)value)
		return false;

	*out = (int32_t)value;
	return true;
}

static bool session_json_uint32(const cJSON *object, const char *key,
								uint32_t *out, bool required) {
	const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	double value;

	if (!item)
		return !required;
	if (!cJSON_IsNumber(item))
		return false;

	value = item->valuedouble;
	if (value < 0 || value > UINT32_MAX || value != (double)(uint32_t)value)
		return false;

	*out = (uint32_t)value;
	return true;
}

static bool session_json_rect(const cJSON *object, const char *key,
							  SessionRect *rect) {
	const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);

	if (!item)
		return true;
	if (!cJSON_IsObject(item))
		return false;

	return session_json_int32(item, "x", &rect->x, true) &&
		   session_json_int32(item, "y", &rect->y, true) &&
		   session_json_int32(item, "width", &rect->width, true) &&
		   session_json_int32(item, "height", &rect->height, true);
}

static bool parse_session_entry(const cJSON *object,
								SessionRestoreEntry *entry) {
	memset(entry, 0, sizeof(*entry));

	return cJSON_IsObject(object) &&
		   session_json_string(object, "app_id", entry->app_id,
							   sizeof(entry->app_id), true) &&
		   session_json_string(object, "title", entry->title,
							   sizeof(entry->title), false) &&
		   session_json_string(object, "monitor", entry->monitor,
							   sizeof(entry->monitor), false) &&
		   session_json_string(object, "launch_command", entry->launch_command,
							   sizeof(entry->launch_command), false) &&
		   session_json_int32(object, "pid", &entry->pid, false) &&
		   session_json_uint32(object, "tags", &entry->tags, false) &&
		   session_json_int32(object, "is_floating", &entry->is_floating,
							  false) &&
		   session_json_int32(object, "is_fullscreen", &entry->is_fullscreen,
							  false) &&
		   session_json_int32(object, "is_minimized", &entry->is_minimized,
							  false) &&
		   session_json_rect(object, "geom", &entry->geom) &&
		   session_json_rect(object, "float_geom", &entry->float_geom);
}

static bool load_pending_entries(void) {
	char *path = session_file_path();
	char *contents = NULL;
	FILE *in = NULL;
	cJSON *root = NULL;
	const cJSON *object;
	bool loaded = false;
	size_t index = 0;

	free_pending_entries();
	if (!path)
		return false;

	if (access(path, F_OK) != 0)
		goto cleanup;

	if (!session_file_is_trusted(path)) {
		wlr_log(WLR_ERROR,
				"mango session: refusing to restore from untrusted session file: %s",
				path);
		goto cleanup;
	}

	in = fopen(path, "r");
	if (!in)
		goto cleanup;

	if (fseek(in, 0, SEEK_END) != 0)
		goto cleanup;
	long size = ftell(in);
	if (size < 0 || fseek(in, 0, SEEK_SET) != 0)
		goto cleanup;

	contents = ecalloc((size_t)size + 1, 1);
	if (fread(contents, 1, (size_t)size, in) != (size_t)size)
		goto cleanup;
	contents[size] = '\0';

	root = cJSON_ParseWithOpts(contents, NULL, true);
	if (!cJSON_IsArray(root)) {
		wlr_log(WLR_ERROR, "mango session: invalid session file: %s", path);
		goto cleanup;
	}

	cJSON_ArrayForEach(object, root) {
		SessionRestoreEntry entry;

		if (!parse_session_entry(object, &entry)) {
			wlr_log(WLR_ERROR,
					"mango session: skipping invalid session entry at index %zu",
					index++);
			continue;
		}

		PendingSessionEntry *new_entries = realloc(
			pending_entries, sizeof(*pending_entries) * (pending_count + 1));
		if (!new_entries)
			goto cleanup;

		pending_entries = new_entries;
		pending_entries[pending_count].entry = entry;
		pending_entries[pending_count].used = false;
		pending_count++;
		index++;
	}
	loaded = pending_count > 0;

cleanup:
	if (in)
		fclose(in);
	cJSON_Delete(root);
	free(contents);
	free(path);
	return loaded;
}

static PendingSessionEntry *find_pending_match(const char *appid, const char *title,
											   const char *monitor) {
	PendingSessionEntry *fallback = NULL;
	PendingSessionEntry *monitor_match = NULL;

	for (size_t i = 0; i < pending_count; ++i) {
		PendingSessionEntry *candidate = &pending_entries[i];
		if (candidate->used)
			continue;
		if (strcmp(candidate->entry.app_id, appid) != 0)
			continue;

		if (title && title[0] != '\0' &&
			strcmp(candidate->entry.title, title) == 0) {
			if (monitor && monitor[0] != '\0' &&
				strcmp(candidate->entry.monitor, monitor) == 0)
				return candidate;
			if (!fallback)
				fallback = candidate;
		}

		if (!monitor_match && monitor && monitor[0] != '\0' &&
			strcmp(candidate->entry.monitor, monitor) == 0) {
			monitor_match = candidate;
		}

		if (!fallback)
			fallback = candidate;
	}

	return monitor_match ? monitor_match : fallback;
}

static bool session_resolve_launch_command(SessionRestoreEntry *entry) {
	const char *mapped_command;

	if (!entry || entry->app_id[0] == '\0')
		return false;

	/* Explicit user mapping wins over best-effort persisted launch data. */
	mapped_command = mango_session_lookup_launch_command(entry->app_id,
														 entry->title);
	if (mapped_command && mapped_command[0] != '\0') {
		strncpy(entry->launch_command, mapped_command,
				sizeof(entry->launch_command) - 1);
		entry->launch_command[sizeof(entry->launch_command) - 1] = '\0';
		return true;
	}

	return entry->launch_command[0] != '\0';
}

static void session_spawn_restore_entries(void) {
	for (size_t i = 0; i < pending_count; ++i) {
		SessionRestoreEntry *entry = &pending_entries[i].entry;

		if (!session_resolve_launch_command(entry))
			continue;

		mango_session_spawn_command(entry->launch_command);
	}
}

void session_init(void) { mango_session_spawn_tracker_init(); }

void session_shutdown(void) {
	free_pending_entries();
	restore_started = false;
	mango_session_spawn_tracker_shutdown();
}

void session_maybe_restore_startup(void) {
	if (!session_is_enabled() || restore_started)
		return;

	restore_started = true;
	if (!load_pending_entries())
		return;

	session_spawn_restore_entries();
}

void session_handle_client_mapped(Client *c) {
	PendingSessionEntry *match;
	const char *appid;
	const char *title;
	const char *monitor;

	if (!restore_started || pending_count == 0 || !c)
		return;

	appid = mango_session_client_appid(c);
	if (!appid || appid[0] == '\0' || strcmp(appid, "broken") == 0)
		return;

	title = mango_session_client_title(c);
	monitor = mango_session_client_monitor(c);
	match = find_pending_match(appid, title, monitor);
	if (!match)
		return;

	mango_session_remember_client_launch_command(c, match->entry.launch_command);
	mango_session_apply_restore_entry(c, &match->entry);
	match->used = true;
}

void session_handle_client_destroyed(Client *c) {
	(void)c;
}

void session_save_now(bool is_final_shutdown) {
	FILE *out = NULL;
	char *dir = NULL, *tmp_path = NULL, *final_path = NULL;
	int32_t count = 0;

	if (!session_is_enabled())
		return;

	dir = session_data_dir();
	if (!dir || !ensure_directory_tree(dir))
		goto cleanup;

	tmp_path = string_printf("%s/session.json.tmp", dir);
	final_path = string_printf("%s/session.json", dir);
	if (!tmp_path || !final_path)
		goto cleanup;

	out = fopen(tmp_path, "w");
	if (!out)
		goto cleanup;

	count = mango_session_write_snapshot(out);
	if (count < 0) {
		wlr_log(WLR_ERROR, "mango session: failed to serialize session state");
		goto cleanup;
	}
	if (fclose(out) != 0) {
		out = NULL;
		goto cleanup;
	}
	out = NULL;

	if (count <= 0 && is_final_shutdown) {
		unlink(tmp_path);
		goto cleanup;
	}

	if (rename(tmp_path, final_path) != 0)
		unlink(tmp_path);

cleanup:
	if (out)
		fclose(out);
	if (tmp_path)
		unlink(tmp_path);
	free(dir);
	free(tmp_path);
	free(final_path);
}

bool session_is_enabled(void) { return mango_session_is_config_enabled() != 0; }

bool session_is_restorable_client(Client *c) {
	(void)c;
	return false;
}
