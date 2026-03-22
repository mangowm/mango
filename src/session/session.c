#include "session.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

typedef struct {
	SessionRestoreEntry entry;
	bool used;
} PendingSessionEntry;

static PendingSessionEntry *pending_entries;
static size_t pending_count;
static bool restore_started;

static bool mkdir_p(const char *dir) {
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

static void free_pending_entries(void) {
	free(pending_entries);
	pending_entries = NULL;
	pending_count = 0;
}

static const char *skip_ws(const char *p) {
	while (p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
		++p;
	return p;
}

static const char *find_matching_brace(const char *start) {
	int depth = 0;
	bool in_string = false;
	bool escaped = false;

	for (const char *p = start; p && *p != '\0'; ++p) {
		if (in_string) {
			if (escaped) {
				escaped = false;
			} else if (*p == '\\') {
				escaped = true;
			} else if (*p == '"') {
				in_string = false;
			}
			continue;
		}

		if (*p == '"') {
			in_string = true;
		} else if (*p == '{') {
			depth++;
		} else if (*p == '}') {
			depth--;
			if (depth == 0)
				return p;
		}
	}

	return NULL;
}

static bool parse_json_string_value(const char *value, char *dest, size_t dest_size) {
	size_t i = 0;
	const char *p = skip_ws(value);

	if (!p || *p != '"' || dest_size == 0)
		return false;

	++p;
	while (*p != '\0' && *p != '"' && i + 1 < dest_size) {
		if (*p == '\\') {
			++p;
			if (*p == '\0')
				break;
			switch (*p) {
			case 'n':
				dest[i++] = '\n';
				break;
			case 'r':
				dest[i++] = '\r';
				break;
			case 't':
				dest[i++] = '\t';
				break;
			case '\\':
			case '"':
			case '/':
				dest[i++] = *p;
				break;
			default:
				dest[i++] = *p;
				break;
			}
			++p;
			continue;
		}
		dest[i++] = *p++;
	}

	dest[i] = '\0';
	return true;
}

static bool extract_json_string(const char *obj, const char *key, char *dest,
								size_t dest_size) {
	const char *p = strstr(obj, key);
	if (!p)
		return false;
	p = strchr(p, ':');
	if (!p)
		return false;
	return parse_json_string_value(p + 1, dest, dest_size);
}

static bool extract_json_int(const char *obj, const char *key, int32_t *out) {
	char *end = NULL;
	const char *p = strstr(obj, key);
	long value;

	if (!p)
		return false;
	p = strchr(p, ':');
	if (!p)
		return false;
	p = skip_ws(p + 1);
	if (!p)
		return false;

	value = strtol(p, &end, 10);
	if (end == p)
		return false;

	*out = (int32_t)value;
	return true;
}

static bool extract_json_object_range(const char *obj, const char *key,
									  const char **start, const char **end) {
	const char *p = strstr(obj, key);
	if (!p)
		return false;
	p = strchr(p, ':');
	if (!p)
		return false;
	p = skip_ws(p + 1);
	if (!p || *p != '{')
		return false;
	*start = p;
	*end = find_matching_brace(p);
	return *end != NULL;
}

static bool parse_geom_object(const char *obj, const char *key, SessionRect *geom) {
	const char *start = NULL, *end = NULL;
	char *sub = NULL;
	bool ok;
	size_t len;

	if (!extract_json_object_range(obj, key, &start, &end))
		return false;

	len = (size_t)(end - start + 1);
	sub = ecalloc(len + 1, 1);
	memcpy(sub, start, len);

	ok = extract_json_int(sub, "\"x\"", &geom->x) &&
		 extract_json_int(sub, "\"y\"", &geom->y) &&
		 extract_json_int(sub, "\"width\"", &geom->width) &&
		 extract_json_int(sub, "\"height\"", &geom->height);

	free(sub);
	return ok;
}

static bool parse_session_entry(const char *obj, SessionRestoreEntry *entry) {
	int32_t tags = 0;

	memset(entry, 0, sizeof(*entry));

	if (!extract_json_string(obj, "\"app_id\"", entry->app_id,
							 sizeof(entry->app_id)))
		return false;

	extract_json_string(obj, "\"title\"", entry->title, sizeof(entry->title));
	extract_json_string(obj, "\"monitor\"", entry->monitor,
						sizeof(entry->monitor));
	extract_json_string(obj, "\"launch_command\"", entry->launch_command,
						sizeof(entry->launch_command));
	extract_json_int(obj, "\"pid\"", &entry->pid);
	extract_json_int(obj, "\"tags\"", &tags);
	entry->tags = (uint32_t)tags;
	extract_json_int(obj, "\"is_floating\"", &entry->is_floating);
	extract_json_int(obj, "\"is_fullscreen\"", &entry->is_fullscreen);
	extract_json_int(obj, "\"is_minimized\"", &entry->is_minimized);
	parse_geom_object(obj, "\"geom\"", &entry->geom);
	parse_geom_object(obj, "\"float_geom\"", &entry->float_geom);

	return true;
}

static bool load_pending_entries(void) {
	char *path = session_file_path();
	char *contents = NULL;
	FILE *in = NULL;
	const char *cursor;
	bool loaded = false;

	free_pending_entries();
	if (!path)
		return false;

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

	cursor = contents;
	while ((cursor = strchr(cursor, '{')) != NULL) {
		const char *end = find_matching_brace(cursor);
		SessionRestoreEntry entry;
		char *obj;
		size_t len;

		if (!end)
			break;

		len = (size_t)(end - cursor + 1);
		obj = ecalloc(len + 1, 1);
		memcpy(obj, cursor, len);

		if (parse_session_entry(obj, &entry)) {
				PendingSessionEntry *new_entries = realloc(
					pending_entries, sizeof(*pending_entries) * (pending_count + 1));
				if (!new_entries) {
					free(obj);
					goto cleanup;
				}
				pending_entries = new_entries;
				pending_entries[pending_count].entry = entry;
				pending_entries[pending_count].used = false;
				pending_count++;
			loaded = true;
		}

		free(obj);
		cursor = end + 1;
	}

cleanup:
	if (in)
		fclose(in);
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

void session_init(void) {}

void session_shutdown(void) {
	free_pending_entries();
	restore_started = false;
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
	if (!dir || !mkdir_p(dir))
		goto cleanup;

	tmp_path = string_printf("%s/session.json.tmp", dir);
	final_path = string_printf("%s/session.json", dir);
	if (!tmp_path || !final_path)
		goto cleanup;

	out = fopen(tmp_path, "w");
	if (!out)
		goto cleanup;

	count = mango_session_write_snapshot(out);
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
	free(dir);
	free(tmp_path);
	free(final_path);
}

bool session_is_enabled(void) { return mango_session_is_config_enabled() != 0; }

bool session_is_restorable_client(Client *c) {
	(void)c;
	return false;
}
