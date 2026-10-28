#include "mango/common/scene_node.h"
#include "mango/common/server.h"
#include "mango/draw/text-node.h"
#include "mango/draw/title-message-bar.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/switcher/switcher.h"
#include <glib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>

static char *title_message_bar_text(Client *c) {
	return g_strdup_printf("%s\n%s", client_get_title(c), client_get_appid(c));
}

static void title_message_bar_init(Monitor *m) {
	if (!m || m->title_message_bar)
		return;

	m->title_message_bar = mango_bar_decoration_create(
		m, TitleMessageBar, false, server.layers[LyrSpecialOverlay],
		config.title_message_bardata, 0, 0);
	if (!m->title_message_bar)
		return;

	mango_scene_node_set_ignore_hit(&m->title_message_bar->scene->node, true);
	mango_bar_decoration_set_focus(m->title_message_bar, true);
	mango_bar_decoration_set_center_secondary_line(m->title_message_bar, true);
	wlr_scene_node_set_enabled(&m->title_message_bar->scene->node, false);
}

static void title_message_bar_set_text(MangoBarDecoration *bar, Client *c,
									   float scale) {
	const char *title = client_get_title(c);
	const char *appid = client_get_appid(c);
	size_t title_len = strlen(title);
	const char *last = bar->last_text;
	if (last && strncmp(last, title, title_len) == 0 &&
		last[title_len] == '\n' && strcmp(last + title_len + 1, appid) == 0 &&
		bar->last_scale == scale)
		return;

	char *text = title_message_bar_text(c);
	mango_bar_decoration_update(bar, text, scale);
	g_free(text);
}

static bool overcircle_title_bar_geometry(Monitor *m, bool use_animation,
										  Client **client,
										  struct wlr_box *box) {
	if (!is_overcircle_mode(m) || !m->sel ||
		config.title_message_bar_height == 0)
		return false;

	Client *c = m->sel;
	struct wlr_box geo = use_animation ? c->animation.current : c->geom;
	if (geo.width <= 0 || geo.height <= 0)
		geo = use_animation ? c->geom : c->animation.current;
	if (geo.width <= 0)
		return false;

	if (client)
		*client = c;
	if (box) {
		*box = (struct wlr_box){
			.x = geo.x,
			.y = geo.y + geo.height + (int)config.title_message_bar_gap,
			.width = geo.width,
			.height = (int)config.title_message_bar_height,
		};
	}
	return true;
}

static bool title_message_bar_geometry(Monitor *m, bool use_animation,
									   Client **client, struct wlr_box *box) {
	if (switcher_title_message_bar_geometry(m, client, box))
		return true;
	return overcircle_title_bar_geometry(m, use_animation, client, box);
}

void title_message_bar_finish(Monitor *m) {
	if (!m || !m->title_message_bar)
		return;

	mango_bar_decoration_destroy(m->title_message_bar);
	m->title_message_bar = NULL;
}

void title_message_bar_apply_config(Monitor *m) {
	if (!m || !m->title_message_bar)
		return;

	mango_bar_decoration_apply_config(m->title_message_bar,
									  &config.title_message_bardata);
	mango_bar_decoration_set_focus(m->title_message_bar, true);
}

void title_message_bar_hide(Monitor *m) {
	if (!m || !m->title_message_bar)
		return;
	if (m->title_message_bar->scene->node.enabled)
		wlr_scene_node_set_enabled(&m->title_message_bar->scene->node, false);
}

void title_message_bar_update_title(Client *c) {
	if (!c || !c->mon || !c->mon->title_message_bar)
		return;
	if (!c->mon->title_message_bar->scene->node.enabled)
		return;

	Client *target = NULL;
	if (switcher_title_message_bar_geometry(c->mon, &target, NULL)) {
		if (target != c)
			return;
	} else if (is_overcircle_mode(c->mon)) {
		if (c->mon->sel != c)
			return;
	} else {
		return;
	}

	MangoBarDecoration *bar = c->mon->title_message_bar;
	title_message_bar_set_text(bar, c, c->mon->wlr_output->scale);
}

static void title_message_bar_refresh(Monitor *m, bool use_animation) {
	if (!m)
		return;

	Client *c = NULL;
	struct wlr_box box;
	if (!title_message_bar_geometry(m, use_animation, &c, &box) ||
		box.width <= 0 || box.height <= 0) {
		title_message_bar_hide(m);
		return;
	}

	title_message_bar_init(m);
	MangoBarDecoration *bar = m->title_message_bar;
	if (!bar)
		return;

	bool was_enabled = bar->scene->node.enabled;
	if (!was_enabled)
		wlr_scene_node_set_enabled(&bar->scene->node, true);

	title_message_bar_set_text(bar, c, m->wlr_output->scale);
	mango_bar_decoration_set_size(bar, box.width, box.height);

	wlr_scene_node_set_position(&bar->scene->node, box.x, box.y);
	if (!was_enabled)
		wlr_scene_node_raise_to_top(&bar->scene->node);
}

void title_message_bar_update(Monitor *m) {
	title_message_bar_refresh(m, false);
}

void title_message_bar_update_geometry(Monitor *m) {
	title_message_bar_refresh(m, true);
}
