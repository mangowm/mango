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
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>

struct TagTabNode {
	struct TagTabNode *prev;
	struct TagTabNode *next;
	struct TagTabNode *all_next;
	Client *client;
};

struct TagTabState {
	struct TagTabNode *head;
	struct TagTabNode *tail;
	struct TagTabNode *all_first;
	uint32_t focus_id;
	int32_t count;
};

static Client *tab_head(Client *c) {
	return client_chain_head(c, CLIENT_TAB_PREV_OFF);
}

Client *tab_get_adjacent_member(Client *c, bool next) {
	if (!c || !(c->tab_prev || c->tab_next))
		return NULL;

	Client *target = next ? c->tab_next : c->tab_prev;
	if (target)
		return target;

	target = c;
	if (next) {
		while (target->tab_prev)
			target = target->tab_prev;
	} else {
		while (target->tab_next)
			target = target->tab_next;
	}
	return target != c ? target : NULL;
}

static bool tab_is_member(const Client *c) {
	return c && (c->tab_prev || c->tab_next);
}

static void tab_bar_set_enabled(Client *c, bool on) {
	if (!c || !c->tab_bar)
		return;
	wlr_scene_node_set_enabled(&c->tab_bar->scene->node, on);
}

static bool tab_layout_enabled_for_tag(const Monitor *m, uint32_t tag) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS)
		return false;
	uint32_t id = m->pertag->ltidxs[tag]->id;
	if (id == MONOCLE)
		return config.monocle_tab_mode;
	if (id == DECK || id == VERTICAL_DECK)
		return config.deck_tab_mode;
	return false;
}

static bool is_tab_layout(const Monitor *m) {
	return m && tab_layout_enabled_for_tag(m, get_mon_curtag(m));
}

bool tab_layout_active(const Monitor *m) {
	return m && !m->isoverview && is_tab_layout(m);
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

static bool tab_client_in_state(Monitor *m, Client *c) {
	return c && VISIBLEON(c, m) && tab_state_ok(c) &&
		   tab_is_stack_candidate(m, c);
}

static bool tab_member_in_view(Client *c, Monitor *m) {
	return c && m && c->mon == m && !m->isoverview && VISIBLEON(c, m);
}

static bool tab_focus_can_render(const Client *c) {
	return c && !c->isfullscreen && !c->ismaximizescreen;
}

static struct TagTabState *ensure_tab_state(Monitor *m, uint32_t tag) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS)
		return NULL;
	if (!m->pertag->tab_state[tag])
		m->pertag->tab_state[tag] = calloc(1, sizeof(struct TagTabState));
	return m->pertag->tab_state[tag];
}

static void clear_tab_state(struct TagTabState *state) {
	if (!state)
		return;
	struct TagTabNode *node = state->all_first;
	while (node) {
		struct TagTabNode *next = node->all_next;
		free(node);
		node = next;
	}
	free(state);
}

static struct TagTabNode *find_tab_node(struct TagTabState *state,
										const Client *client) {
	if (!state || !client)
		return NULL;
	for (struct TagTabNode *node = state->all_first; node;
		 node = node->all_next)
		if (node->client == client)
			return node;
	return NULL;
}

static struct TagTabNode *tab_node_create(struct TagTabState *state,
										  Client *client) {
	struct TagTabNode *node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;
	node->client = client;
	node->all_next = state->all_first;
	state->all_first = node;
	state->count++;
	return node;
}

static void tab_node_append(struct TagTabState *state,
							struct TagTabNode *node) {
	node->prev = state->tail;
	node->next = NULL;
	if (state->tail)
		state->tail->next = node;
	else
		state->head = node;
	state->tail = node;
}

static void tab_node_remove(struct TagTabState *state,
							struct TagTabNode *node) {
	if (!state || !node)
		return;

	if (node->prev)
		node->prev->next = node->next;
	else if (state->head == node)
		state->head = node->next;
	if (node->next)
		node->next->prev = node->prev;
	else if (state->tail == node)
		state->tail = node->prev;
	if (state->count > 0)
		state->count--;
	if (node->client && state->focus_id == node->client->id)
		state->focus_id = 0;

	struct TagTabNode **link = &state->all_first;
	while (*link && *link != node)
		link = &(*link)->all_next;
	if (*link == node)
		*link = node->all_next;
	free(node);
}

static void clear_tab_projection(Monitor *m) {
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (m->is_ws_overview && c->wsov_hidden)
			continue;
		bool was_hidden = c->is_tab_hidden;
		c->tab_prev = NULL;
		c->tab_next = NULL;
		c->is_tab_focus = false;
		c->is_tab_hidden = false;
		tab_bar_set_enabled(c, false);
		if (was_hidden)
			client_update_visibility(c);
	}
}

static void update_tab_state(Monitor *m) {
	uint32_t tag = get_mon_curtag(m);
	struct TagTabState *state = ensure_tab_state(m, tag);
	if (!state)
		return;

	struct TagTabNode *node = state->all_first;
	while (node) {
		struct TagTabNode *next = node->all_next;
		if (!tab_client_in_state(m, node->client))
			tab_node_remove(state, node);
		node = next;
	}

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (!tab_client_in_state(m, c))
			continue;
		node = find_tab_node(state, c);
		if (!node) {
			node = tab_node_create(state, c);
			if (!node)
				continue;
			tab_node_append(state, node);
		}
	}
}

static Client *get_tab_state_focus(const Monitor *m,
								   struct TagTabState *state) {
	Client *selected = NULL;
	Client *remembered = NULL;
	Client *fallback = NULL;

	for (struct TagTabNode *node = state->head; node; node = node->next) {
		if (node->client->mon != m)
			continue;
		if (!fallback)
			fallback = node->client;
		if (node->client == m->sel && node->client->isfocusing)
			selected = node->client;
		if (state->focus_id && node->client->id == state->focus_id)
			remembered = node->client;
	}

	Client *focus = selected ? selected : (remembered ? remembered : fallback);
	if (!focus)
		state->focus_id = 0;
	return focus;
}

bool tab_client_hidden_on_tag(const Monitor *m, uint32_t tag, const Client *c) {
	if (!m || !m->pertag || tag == 0 || tag >= PERTAG_SLOTS || !c ||
		c->mon != m || !tab_layout_enabled_for_tag(m, tag))
		return false;

	struct TagTabState *state = m->pertag->tab_state[tag];
	if (!state || state->count < 2 || !state->focus_id ||
		state->focus_id == c->id || !tab_state_ok(c) || c->isglobal)
		return false;

	uint32_t layout_id = m->pertag->ltidxs[tag]->id;
	if ((layout_id == DECK || layout_id == VERTICAL_DECK) && c->ismaster)
		return false;
	uint32_t bit = 1u << (tag - 1);
	return c->tags & bit;
}

static void tab_refresh_bars(Client *head) {
	if (!head || !head->mon)
		return;

	int32_t members = 0;
	Client *focus = head;

	for (Client *it = head; it; it = it->tab_next) {
		if (it->is_tab_focus)
			focus = it;
		if (tab_member_in_view(it, head->mon))
			members++;
	}

	bool stacked = members >= 2;
	bool strip = stacked && tab_focus_can_render(focus);
	int32_t layer = client_target_layer(focus);

	for (Client *it = head; it; it = it->tab_next) {
		if (it->tab_bar &&
			it->tab_bar->scene->node.parent != server.layers[layer])
			wlr_scene_node_reparent(&it->tab_bar->scene->node,
									server.layers[layer]);

		bool in_view = tab_member_in_view(it, head->mon);
		if (in_view)
			it->is_tab_hidden = stacked && !it->is_tab_focus;
		tab_bar_set_enabled(it, strip && in_view);
		client_update_visibility(it);
	}
}

static void sync_tab_state_to_clients(Monitor *m, uint32_t tag) {
	struct TagTabState *state = m->pertag->tab_state[tag];
	clear_tab_projection(m);
	if (!state)
		return;

	Client *focus = get_tab_state_focus(m, state);
	if (focus)
		state->focus_id = focus->id;

	Client *head = NULL;
	Client *tail = NULL;
	int32_t count = 0;
	for (struct TagTabNode *node = state->head; node; node = node->next) {
		Client *c = node->client;
		if (c->mon != m)
			continue;
		if (!head)
			head = c;
		c->tab_prev = tail;
		c->tab_next = NULL;
		if (tail)
			tail->tab_next = c;
		c->is_tab_focus = (c == focus);
		c->is_tab_hidden = false;
		tail = c;
		count++;
	}

	if (count >= 2)
		tab_refresh_bars(head);
}

void tab_sync_current(Monitor *m) {
	if (!m || !m->pertag)
		return;
	if (!is_tab_layout(m)) {
		clear_tab_projection(m);
		return;
	}
	uint32_t tag = get_mon_curtag(m);
	sync_tab_state_to_clients(m, tag);
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
	int32_t members = 0;

	for (Client *it = head; it; it = it->tab_next) {
		if (it->is_tab_focus)
			focus = it;
		if (tab_member_in_view(it, c->mon))
			members++;
	}

	bool strip = members >= 2 && tab_layout_active(c->mon) &&
				 tab_focus_can_render(focus);
	bool ignore_hit = !strip;

	for (Client *it = head; it; it = it->tab_next) {
		if (!it->tab_bar)
			continue;
		mango_scene_node_set_ignore_hit(&it->tab_bar->scene->node, ignore_hit);
	}
}

void tab_sync_focus(Client *c) {
	if (!c || !c->mon)
		return;
	uint32_t tag = get_mon_curtag(c->mon);
	struct TagTabState *state = ensure_tab_state(c->mon, tag);
	if (!state || !find_tab_node(state, c))
		return;
	state->focus_id = c->id;
	if (!tab_is_member(c) || c->is_tab_focus)
		return;

	Client *head = tab_head(c);
	for (Client *it = head; it; it = it->tab_next)
		it->is_tab_focus = (it == c);

	tab_refresh_bars(head);
}

bool tab_remove_client_in_tag(Monitor *m, uint32_t tag, Client *c) {
	struct TagTabState *state = m->pertag->tab_state[tag];
	struct TagTabNode *node;
	bool changed = false;
	while (state && (node = find_tab_node(state, c))) {
		tab_node_remove(state, node);
		changed = true;
	}
	return changed;
}

bool tab_replace_client_in_tag(Monitor *m, uint32_t tag, Client *old,
							   Client *new) {
	if (!old || !new || old == new)
		return false;
	struct TagTabState *state = m->pertag->tab_state[tag];
	if (!state)
		return false;
	struct TagTabNode *old_node = find_tab_node(state, old);
	if (!old_node)
		return false;
	struct TagTabNode *new_node = find_tab_node(state, new);
	if (new_node && new_node != old_node)
		tab_node_remove(state, new_node);
	if (state->focus_id == old->id)
		state->focus_id = new->id;
	old_node->client = new;
	struct TagTabNode *node;
	while ((node = find_tab_node(state, old)))
		tab_node_remove(state, node);
	return true;
}

void tab_sync_monitor(Monitor *m) {
	if (!m || !m->wlr_output || !m->wlr_output->enabled || m->iscleanuping)
		return;

	if (m->isoverview)
		return;

	if (!is_tab_layout(m)) {
		clear_tab_projection(m);
		return;
	}

	uint32_t tag = get_mon_curtag(m);
	update_tab_state(m);
	sync_tab_state_to_clients(m, tag);
}

void cleanup_monitor_tab(Monitor *m) {
	if (!m || !m->pertag)
		return;
	for (int tag = 0; tag < PERTAG_SLOTS; tag++) {
		if (m->pertag->tab_state[tag]) {
			clear_tab_state(m->pertag->tab_state[tag]);
			m->pertag->tab_state[tag] = NULL;
		}
	}
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

void client_set_tab_bar_config(Client *c) {
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

	Client *head = tab_head(c);

	if (!tab_layout_active(c->mon) || !tab_focus_can_render(c)) {
		for (Client *it = head; it; it = it->tab_next)
			tab_bar_set_enabled(it, false);
		return;
	}

	int32_t count = 0;

	for (Client *it = head; it; it = it->tab_next)
		if (tab_member_in_view(it, c->mon))
			count++;

	if (count < 2)
		return;

	struct wlr_box anchor = c->animation.current;
	if (c->animation.running && c->animation.action == OPEN)
		anchor = c->geom;

	int32_t tab_x = anchor.x;
	int32_t group_h =
		client_wants_group_bar(c) ? (int32_t)config.group_bar_height : 0;
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
		if (!tab_member_in_view(cur, c->mon))
			continue;
		int32_t w = bar_w + (i < rem ? 1 : 0);
		global_draw_tab_bar(cur, x, tab_y, w, th);
		mango_bar_decoration_set_focus(cur->tab_bar, cur->is_tab_focus);
		x += w;
		i++;
	}
}
