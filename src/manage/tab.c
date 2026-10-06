#include "mango/manage/tab.h"
#include "mango/common/scene_node.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/draw/text-node.h"
#include "mango/layout/arrange.h"
#include "mango/layout/layout.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include <stddef.h>
#include <wlr/types/wlr_scene.h>

static int32_t tab_member_count = 0;

static Client *tab_head(Client *c) {
	return client_chain_head(c, CLIENT_TAB_PREV_OFF);
}

static bool tab_is_member(const Client *c) {
	return c && (c->tab_prev || c->tab_next);
}

static void tab_bar_set_enabled(Client *c, bool on) {
	if (!c || !c->tab_bar)
		return;
	wlr_scene_node_set_enabled(&c->tab_bar->scene->node, on);
}

static void tab_chain_focus(Client *head) {
	Client *sel = NULL;
	Client *focus = NULL;

	for (Client *it = head; it; it = it->tab_next) {
		if (it->mon && it == it->mon->sel)
			sel = it;
		if (!focus && it->is_tab_focus)
			focus = it;
	}

	focus = sel ? sel : (focus ? focus : head);

	for (Client *it = head; it; it = it->tab_next)
		it->is_tab_focus = (it == focus);
}

static void tab_leave(Client *c) {
	client_chain_unlink(c, CLIENT_TAB_PREV_OFF, CLIENT_TAB_NEXT_OFF);
	c->is_tab_focus = false;
	tab_bar_set_enabled(c, false);
	tab_member_count--;
	client_update_visibility(c);
}

static bool tab_layout_id_supported(uint32_t id) {
	if (id == MONOCLE)
		return config.monocle_tab_mode;
	if (id == DECK || id == VERTICAL_DECK)
		return config.deck_tab_mode;
	return false;
}

static bool tab_layout_enabled(const Monitor *m) {
	if (!m || !m->pertag)
		return false;
	return tab_layout_id_supported(m->pertag->ltidxs[get_mon_curtag(m)]->id);
}

bool tab_layout_active(const Monitor *m) {
	return m && !m->isoverview && tab_layout_enabled(m);
}

static bool tab_state_ok(const Client *c) {
	if (!c || !c->mon || c->iskilling || c->isminimized)
		return false;
	if (c->isfloating || c->isunglobal || c->is_in_scratchpad)
		return false;
	return true;
}

static bool tab_is_stack_candidate(Monitor *m, Client *target) {
	uint32_t id = m->pertag->ltidxs[get_mon_curtag(m)]->id;
	if (id == MONOCLE)
		return true;
	if (id != DECK && id != VERTICAL_DECK)
		return false;
	return !target->ismaster;
}

static bool tab_member_kept(Monitor *m, uint32_t cur, Client *c) {
	return tab_layout_enabled(m) && (c->tags & cur) && tab_state_ok(c) &&
		   tab_is_stack_candidate(m, c);
}

static bool tab_member_in_view(Client *c, Monitor *m) {
	return c && c->mon == m && VISIBLEON(c, m);
}

static bool tab_member_shown_in_view(Client *c, Monitor *m) {
	return c && c->mon == m && !c->isfullscreen && !c->ismaximizescreen &&
		   !m->isoverview && VISIBLEON(c, m);
}

static void tab_refresh_bars(Client *head) {
	int32_t shown = 0;
	Client *focus = head;

	for (Client *it = head; it; it = it->tab_next) {
		if (it->is_tab_focus)
			focus = it;
		if (tab_member_shown_in_view(it, head->mon))
			shown++;
	}

	bool stacked = shown >= 2;
	bool strip = stacked && tab_member_shown_in_view(focus, head->mon);
	int32_t layer = client_target_layer(focus);

	for (Client *it = head; it; it = it->tab_next) {
		if (it->tab_bar &&
			it->tab_bar->scene->node.parent != server.layers[layer])
			wlr_scene_node_reparent(&it->tab_bar->scene->node,
									server.layers[layer]);

		bool in_view = tab_member_in_view(it, head->mon);
		bool shown = tab_member_shown_in_view(it, head->mon);
		if (in_view)
			it->is_tab_hidden = stacked && shown && !it->is_tab_focus;
		tab_bar_set_enabled(it, strip && shown);
		client_update_visibility(it);
	}
}

/*
 * The tab strip is chain-wide UI: every member owns one bar node and they are
 * laid out side by side to build the strip. Decide once for the whole chain
 * whether it may take pointer input, then apply it to every member, so a
 * tagouting or otherwise hidden chain cannot leave a single clickable segment
 * behind.
 */
void tab_update_input_penetration(Client *c) {
	if (!c || !c->mon || !tab_is_member(c))
		return;

	Client *head = tab_head(c);
	Client *focus = head;
	int32_t shown = 0;

	for (Client *it = head; it; it = it->tab_next) {
		if (it->is_tab_focus)
			focus = it;
		if (tab_member_shown_in_view(it, c->mon))
			shown++;
	}

	bool strip = shown >= 2 && tab_layout_active(c->mon) &&
				 tab_member_shown_in_view(focus, c->mon);
	bool ignore_hit = !strip;

	for (Client *it = head; it; it = it->tab_next) {
		if (!it->tab_bar)
			continue;
		mango_scene_node_set_ignore_hit(&it->tab_bar->scene->node, ignore_hit);
	}
}

void tab_sync_focus(Client *c) {
	if (!c || !tab_is_member(c) || c->is_tab_focus)
		return;

	Client *head = tab_head(c);
	for (Client *it = head; it; it = it->tab_next)
		it->is_tab_focus = (it == c);

	tab_refresh_bars(head);
}

void tab_detach_client(Client *c) {
	if (!tab_is_member(c))
		return;

	Client *head = tab_head(c);
	Client *next = c->tab_next;

	tab_leave(c);

	Client *rest = (head == c) ? next : head;
	if (!rest)
		return;

	if (!rest->tab_next) {
		tab_leave(rest);
		return;
	}

	tab_chain_focus(rest);
	tab_refresh_bars(rest);
}

void tab_replace_client(Client *old, Client *new) {
	if (!old || !new || !tab_is_member(old))
		return;

	bool was_focus = old->is_tab_focus;
	bool was_hidden = old->is_tab_hidden;

	if (tab_is_member(new)) {
		if (tab_head(new) == tab_head(old)) {
			tab_detach_client(old);
			if (was_focus)
				tab_sync_focus(new);
			return;
		}
		tab_leave(new);
	}

	new->tab_prev = old->tab_prev;
	new->tab_next = old->tab_next;
	if (old->tab_prev)
		old->tab_prev->tab_next = new;
	if (old->tab_next)
		old->tab_next->tab_prev = new;

	old->tab_prev = NULL;
	old->tab_next = NULL;
	old->is_tab_focus = false;
	old->is_tab_hidden = false;
	tab_bar_set_enabled(old, false);

	new->is_tab_focus = was_focus;
	new->is_tab_hidden = was_hidden;
}

void tab_sync_monitor(Monitor *m) {
	if (!m || !m->wlr_output || !m->wlr_output->enabled || m->iscleanuping)
		return;

	if (m->isoverview)
		return;

	if (!tab_layout_enabled(m) && tab_member_count == 0)
		return;

	uint32_t cur = m->tagset[m->seltags];
	bool can_merge = tab_layout_enabled(m);
	Client *c;

	wl_list_for_each(c, &server.clients, link) {
		if (!tab_is_member(c) || c->mon != m)
			continue;
		if (tab_member_kept(m, cur, c))
			continue;
		tab_detach_client(c);
	}

	Client *head = NULL, *tail = NULL;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || !(c->tags & cur))
			continue;
		if (!tab_is_member(c)) {
			if (c->is_tab_hidden) {
				c->is_tab_hidden = false;
				client_update_visibility(c);
			}
			if (!can_merge)
				continue;
			if (!tab_state_ok(c) || !tab_is_stack_candidate(m, c))
				continue;
			tab_member_count++;
		}

		if (tab_is_member(c))
			client_chain_unlink(c, CLIENT_TAB_PREV_OFF, CLIENT_TAB_NEXT_OFF);

		c->tab_prev = tail;
		c->tab_next = NULL;
		if (tail)
			tail->tab_next = c;
		else
			head = c;
		tail = c;
	}

	if (!head || !head->tab_next) {
		if (head)
			tab_member_count--;
		return;
	}

	tab_chain_focus(head);
	tab_refresh_bars(head);
}

void tab_focus_member(Client *c) {
	if (!tab_is_member(c) || !c->mon)
		return;

	client_focus(c, 1);
	arrange(c->mon, false, false);
}

void client_raise_tab(Client *c) {
	if (!c || !c->mon || !tab_is_member(c))
		return;

	Client *head = tab_head(c);

	for (Client *it = head; it; it = it->tab_next) {
		if (it != c && it->scene->node.parent == c->scene->node.parent)
			wlr_scene_node_place_above(&c->scene->node, &it->scene->node);
		if (it->tab_bar)
			wlr_scene_node_raise_to_top(&it->tab_bar->scene->node);
	}
}

void client_reparent_tab(Client *c) {
	if (!c || !c->mon || !tab_is_member(c))
		return;

	Client *head = tab_head(c);
	Client *focus = head;

	for (Client *it = head; it; it = it->tab_next)
		if (it->is_tab_focus)
			focus = it;

	int32_t layer = client_target_layer(focus);
	for (Client *it = head; it; it = it->tab_next) {
		if (!it->tab_bar)
			continue;
		wlr_scene_node_reparent(&it->tab_bar->scene->node,
								server.layers[layer]);
	}
}

void client_add_tab_bar(Client *c) {
	if (!c || config.tab_bar_height <= 0)
		return;

	c->tab_bar = mango_bar_decoration_create(
		c, TabBar, true, server.layers[client_target_layer(c)],
		config.tabbardata, 0, 0);
	if (!c->tab_bar)
		return;
	wlr_scene_node_lower_to_bottom(&c->tab_bar->scene->node);
	wlr_scene_node_set_enabled(&c->tab_bar->scene->node, false);
	client_update_tab_bar_title(c);
}

void client_remove_tab_bar(Client *c) {
	if (!c || !c->tab_bar)
		return;
	mango_bar_decoration_destroy(c->tab_bar);
	c->tab_bar = NULL;
}

void client_update_tab_bar_title(Client *c) {
	if (!c || !c->tab_bar)
		return;
	mango_bar_decoration_update(c->tab_bar, client_get_title(c),
								c->mon ? c->mon->wlr_output->scale
								: server.selected_monitor
									? server.selected_monitor->wlr_output->scale
									: 1.0f);
}

void client_apply_tab_bar_config(Client *c) {
	if (!c || !c->tab_bar)
		return;
	mango_bar_decoration_apply_config(c->tab_bar, &config.tabbardata);
}

void global_draw_tab_bar(Client *c, int32_t x, int32_t y, int32_t width,
						 int32_t height) {
	if (!c || !c->tab_bar)
		return;

	wlr_scene_node_set_position(&c->tab_bar->scene->node, x, y);
	mango_bar_decoration_set_size(c->tab_bar, width, height);
}

void client_draw_tabbar(Client *c, struct ivec2 offsets) {
	if (!c || !c->tab_bar || !c->mon || !c->is_tab_focus)
		return;

	if (!tab_layout_active(c->mon)) {
		Client *head = tab_head(c);
		for (Client *it = head; it; it = it->tab_next)
			tab_bar_set_enabled(it, false);
		return;
	}

	Client *head = tab_head(c);
	int32_t count = 0;

	for (Client *it = head; it; it = it->tab_next)
		if (tab_member_shown_in_view(it, c->mon))
			count++;

	if (count < 2)
		return;

	struct wlr_box anchor = c->animation.current;
	if (c->animation.running && c->animation.action == OPEN)
		anchor = c->geom;

	int32_t tab_x = anchor.x;
	int32_t group_h =
		(c->group_next || c->group_prev) ? (int32_t)config.group_bar_height : 0;
	/* Tab strip sits above the group strip when both are present. */
	int32_t tab_y = anchor.y - (int32_t)config.tab_bar_height - group_h;
	int32_t tw = anchor.width;
	int32_t th = (int32_t)config.tab_bar_height;

	int32_t top_over = offsets.y;
	int32_t bottom_over = offsets.height;
	int32_t left_over = offsets.x;
	int32_t right_over = offsets.width;

	if (top_over > 0) {
		tab_y = c->mon->m.y;
		th = (int32_t)config.tab_bar_height - top_over;
	}
	if (bottom_over > 0)
		th = th - GEZERO(bottom_over - anchor.height);
	if (right_over > 0)
		tw = tw - right_over;
	if (left_over > 0) {
		tab_x = c->mon->m.x;
		tw = tw - left_over;
	}

	if (tw <= 0 || th <= 0)
		return;

	int32_t bar_w = tw / count;
	int32_t rem = tw % count;
	int32_t x = tab_x;
	int32_t i = 0;

	for (Client *cur = head; cur && i < count; cur = cur->tab_next) {
		if (!tab_member_shown_in_view(cur, c->mon))
			continue;
		int32_t w = bar_w + (i < rem ? 1 : 0);
		global_draw_tab_bar(cur, x, tab_y, w, th);
		mango_bar_decoration_set_focus(cur->tab_bar, cur->is_tab_focus);
		x += w;
		i++;
	}
}
