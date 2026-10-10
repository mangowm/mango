#include "mango/layout/floating.h"
#include "mango/animation/client.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/layout/layout.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/log.h>

/*
 * floating.txt format, one app per line:
 *
 *     <app_id> <x> <y> <width> <height>
 *
 * x/y are relative to the top-left corner of the monitor the window is on.
 * Only the main windows of an app are remembered. Sub windows (dialogs such
 * as rename/copy/search, anything with a parent window, fixed-size windows)
 * are left alone and open with the size and place the app chose for them.
 *
 * Older files had a slot number after the app_id; those lines are still read
 * (slot 0 only) and are rewritten in the format above.
 */

#define FLOAT_MIN_SIZE 50

typedef struct {
	char appid[256];
	int32_t x, y, w, h;
} FloatEntry;

static FloatEntry *entries = NULL;
static size_t n_entries = 0, cap_entries = 0;
static bool entries_loaded = false;
static bool entries_dirty = false;

static void floating_path(char *out, size_t size) {
	if (server.cli_config_path[0]) {
		char *tmp = strdup(server.cli_config_path);
		if (tmp) {
			snprintf(out, size, "%s/floating.txt", dirname(tmp));
			free(tmp);
			return;
		}
	}
	const char *home = getenv("HOME");
	snprintf(out, size, "%s/.config/mango/floating.txt", home ? home : "");
}

/* app_id with whitespace replaced, so it fits a whitespace-separated field. */
static void floating_key(Client *c, char *out, size_t size) {
	const char *appid = client_get_appid(c);
	size_t i = 0;
	if (!appid || !*appid)
		appid = "broken";
	for (; appid[i] && i + 1 < size; i++)
		out[i] = isspace((unsigned char)appid[i]) ? '_' : appid[i];
	out[i] = '\0';
}

static FloatEntry *entry_find(const char *key) {
	for (size_t i = 0; i < n_entries; i++)
		if (strcmp(entries[i].appid, key) == 0)
			return &entries[i];
	return NULL;
}

static FloatEntry *entry_add(const char *key) {
	if (n_entries == cap_entries) {
		size_t ncap = cap_entries ? cap_entries * 2 : 16;
		FloatEntry *tmp = realloc(entries, ncap * sizeof(*entries));
		if (!tmp)
			return NULL;
		entries = tmp;
		cap_entries = ncap;
	}
	FloatEntry *e = &entries[n_entries++];
	memset(e, 0, sizeof(*e));
	snprintf(e->appid, sizeof(e->appid), "%s", key);
	return e;
}

static void floating_load(void) {
	char path[1100], line[512];
	FILE *f;

	if (entries_loaded)
		return;
	entries_loaded = true;

	floating_path(path, sizeof(path));
	f = fopen(path, "r");
	if (!f)
		return;

	while (fgets(line, sizeof(line), f)) {
		char key[256];
		int32_t slot, x, y, w, h;
		if (line[0] == '#')
			continue;
		if (sscanf(line, "%255s %d %d %d %d %d", key, &slot, &x, &y, &w, &h) ==
			6) {
			/* Old format with a slot number: keep the first window only. */
			if (slot != 0)
				continue;
		} else if (sscanf(line, "%255s %d %d %d %d", key, &x, &y, &w, &h) !=
				   5) {
			continue;
		}
		if (w < FLOAT_MIN_SIZE || h < FLOAT_MIN_SIZE)
			continue;
		FloatEntry *e = entry_find(key);
		if (!e)
			e = entry_add(key);
		if (!e)
			break;
		e->x = x;
		e->y = y;
		e->w = w;
		e->h = h;
	}
	fclose(f);
}

static void floating_save(void) {
	char path[1100], tmp_path[1110], dir[1100];
	FILE *f;

	floating_path(path, sizeof(path));
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

	snprintf(dir, sizeof(dir), "%s", path);
	mkdir(dirname(dir), 0755); /* no-op when it already exists */

	f = fopen(tmp_path, "w");
	if (!f) {
		wlr_log(WLR_ERROR, "floating: cannot write %s", tmp_path);
		return;
	}
	fprintf(f, "# app_id x y width height (relative to monitor)\n");
	for (size_t i = 0; i < n_entries; i++)
		fprintf(f, "%s %d %d %d %d\n", entries[i].appid, entries[i].x,
				entries[i].y, entries[i].w, entries[i].h);
	if (fclose(f) != 0 || rename(tmp_path, path) != 0)
		wlr_log(WLR_ERROR, "floating: cannot save %s", path);
}

/* Dialogs and other child windows: they keep the size and place their app
 * gave them, and are never saved. */
static bool floating_is_sub_window(Client *c) {
	return client_is_float_type(c) || client_get_parent(c) != NULL;
}

static bool floating_eligible(Client *c, Monitor *m) {
	return VISIBLEON(c, m) && ISNORMAL(c) && !ISFULLSCREEN(c) &&
		   !c->is_in_scratchpad;
}

static bool layout_is_floating(Monitor *m) {
	const Layout *l = m->pertag->ltidxs[get_mon_curtag(m)];
	return l && l->id == FLOATING;
}

void floating_prepare(Monitor *m) {
	Client *c;
	bool on = layout_is_floating(m) && !m->isoverview;

	wl_list_for_each(c, &server.clients, link) {
		if (!VISIBLEON(c, m))
			continue;
		if (on) {
			if (floating_eligible(c, m) && !c->isfloating) {
				c->isfloating = 1;
				c->float_forced = 1;
			}
		} else if (!m->isoverview) {
			/* Left the floating layout: give tiling back to the windows that
			 * were only floating because of it, and restore on next entry. */
			if (c->float_forced) {
				c->isfloating = 0;
				c->float_forced = 0;
			}
			c->float_restored = 0;
		}
	}
}

/* Store the window's current box in its app's entry; true when it changed. */
static bool record_geometry(Client *c, Monitor *m) {
	char key[256];
	FloatEntry *e;
	bool changed = false;

	if (c->geom.width < FLOAT_MIN_SIZE || c->geom.height < FLOAT_MIN_SIZE)
		return false;

	floating_key(c, key, sizeof(key));
	e = entry_find(key);
	if (!e) {
		e = entry_add(key);
		if (!e)
			return false;
		changed = true;
	}
	int32_t rx = c->geom.x - m->m.x, ry = c->geom.y - m->m.y;
	if (e->x != rx || e->y != ry || e->w != c->geom.width ||
		e->h != c->geom.height) {
		e->x = rx;
		e->y = ry;
		e->w = c->geom.width;
		e->h = c->geom.height;
		changed = true;
	}
	if (changed)
		entries_dirty = true;
	return changed;
}

void floating_flush(void) {
	if (!entries_dirty)
		return;
	entries_dirty = false;
	floating_save();
}

/* Called from resize(), which every move/resize (mouse, keyboard, snapping)
 * goes through. Only the in-memory entry is updated; the file is written by
 * floating_flush(). */
void floating_note(Client *c) {
	if (!c || !c->mon || !c->isfloating || !c->float_restored || c->iskilling ||
		c->isminimized || c->isfullscreen || c->ismaximizescreen ||
		c->is_in_scratchpad)
		return;
	if (c->mon->isoverview || !VISIBLEON(c, c->mon) ||
		!layout_is_floating(c->mon))
		return;
	if (floating_is_sub_window(c))
		return;
	floating_load();
	record_geometry(c, c->mon);
}

void floating_layout(Monitor *m) {
	Client *c;

	floating_load();

	wl_list_for_each(c, &server.clients, link) {
		char key[256];
		FloatEntry *e;
		struct wlr_box box;
		struct wlr_surface *surface;

		if (!floating_eligible(c, m) || !c->isfloating)
			continue;
		/* The window under the pointer is being dragged; saved on release. */
		if (c == server.grab_client)
			continue;
		/* Dialogs keep the size and place their app chose. */
		if (floating_is_sub_window(c))
			continue;
		/* Not mapped yet: resize() would do nothing, so try again later. */
		surface = client_surface(c);
		if (!surface || !surface->mapped)
			continue;

		floating_key(c, key, sizeof(key));
		e = entry_find(key);

		if (!c->float_restored) {
			c->float_restored = 1;
			if (e) {
				box.x = m->m.x + e->x;
				box.y = m->m.y + e->y;
				box.width = e->w;
				box.height = e->h;
				if (box.width > m->m.width)
					box.width = m->m.width;
				if (box.height > m->m.height)
					box.height = m->m.height;
				/* Keep a saved window reachable after a monitor change. */
				if (box.x + box.width > m->m.x + m->m.width)
					box.x = m->m.x + m->m.width - box.width;
				if (box.y + box.height > m->m.y + m->m.height)
					box.y = m->m.y + m->m.height - box.height;
				if (box.x < m->m.x)
					box.x = m->m.x;
				if (box.y < m->m.y)
					box.y = m->m.y;
				c->float_geom = box;
				c->iscustomsize = 1;
				resize(c, box, (ResizeOpts){.interact = 0});
				continue;
			}
			if (c->float_geom.width > 0 && c->float_geom.height > 0)
				resize(c, c->float_geom, (ResizeOpts){.interact = 0});
		}

		/* Remember wherever the window ended up. */
		record_geometry(c, m);
	}

	floating_flush();
}
