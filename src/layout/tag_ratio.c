#include "mango/layout/tag_ratio.h"
#include "mango/common/server.h"
#include "mango/layout/layout.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include <stdlib.h>

struct TagRatioNode {
	Client *client;
	double master_mfact;
	double master_weight;
	double stack_weight;
	float grid_col;
	float grid_row;
	bool seen;
	struct TagRatioNode *all_next;
};

struct TagRatioState {
	struct TagRatioNode *all_first;
	int32_t count;
};

bool layout_uses_tag_ratios(const Layout *layout) {
	if (!layout)
		return false;
	switch (layout->id) {
	case TILE:
	case GRID:
	case DECK:
	case CENTER_TILE:
	case RIGHT_TILE:
	case VERTICAL_TILE:
	case VERTICAL_GRID:
	case VERTICAL_DECK:
	case FAIR:
	case VERTICAL_FAIR:
		return true;
	default:
		return false;
	}
}

static struct TagRatioNode *find_ratio_node(struct TagRatioState *state,
											const Client *c) {
	if (!state || !c)
		return NULL;
	for (struct TagRatioNode *node = state->all_first; node;
		 node = node->all_next)
		if (node->client == c)
			return node;
	return NULL;
}

static struct TagRatioNode *ratio_node_create(struct TagRatioState *state,
											  Client *c, double mfact) {
	struct TagRatioNode *node = calloc(1, sizeof(*node));
	if (!node)
		return NULL;
	node->client = c;
	node->master_mfact = mfact;
	node->master_weight = 1.0;
	node->stack_weight = 1.0;
	node->grid_col = 1.0f;
	node->grid_row = 1.0f;
	node->all_next = state->all_first;
	state->all_first = node;
	state->count++;
	return node;
}

static void ratio_node_remove(struct TagRatioState *state,
							  struct TagRatioNode *target) {
	if (!state || !target)
		return;
	struct TagRatioNode **link = &state->all_first;
	while (*link && *link != target)
		link = &(*link)->all_next;
	if (*link != target)
		return;
	*link = target->all_next;
	if (state->count > 0)
		state->count--;
	free(target);
}

struct TagRatioState *ensure_tag_ratio_state(Monitor *m, uint32_t tag) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS)
		return NULL;
	if (!m->pertag->tag_ratio_state[tag])
		m->pertag->tag_ratio_state[tag] =
			calloc(1, sizeof(struct TagRatioState));
	return m->pertag->tag_ratio_state[tag];
}

static void clear_tag_ratio_state(struct TagRatioState *state) {
	if (!state)
		return;
	struct TagRatioNode *node = state->all_first;
	while (node) {
		struct TagRatioNode *next = node->all_next;
		free(node);
		node = next;
	}
	free(state);
}

static bool ratio_node_belongs(Monitor *m, Client *c) {
	uint32_t tags = c && c->isminimized ? c->oldtags : (c ? c->tags : 0);
	return c && c->mon == m && !c->isunglobal && !c->iskilling &&
		   (c->isglobal || (tags & m->tagset[m->seltags]));
}

static void update_tag_ratio_state(Monitor *m, uint32_t tag) {
	struct TagRatioState *state = ensure_tag_ratio_state(m, tag);
	if (!state)
		return;

	for (struct TagRatioNode *node = state->all_first; node;
		 node = node->all_next)
		node->seen = false;

	struct TagRatioNode *template_node = NULL;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (!ratio_node_belongs(m, c))
			continue;
		struct TagRatioNode *node = find_ratio_node(state, c);
		if (node) {
			node->seen = true;
			if (!template_node)
				template_node = node;
			continue;
		}
		node = ratio_node_create(state, c, m->pertag->mfacts[tag]);
		if (!node)
			continue;
		node->seen = true;
		if (template_node) {
			node->master_mfact = template_node->master_mfact;
			node->master_weight = template_node->master_weight;
			node->stack_weight = template_node->stack_weight;
		} else {
			template_node = node;
		}
	}

	struct TagRatioNode *node = state->all_first;
	while (node) {
		struct TagRatioNode *next = node->all_next;
		if (!node->seen)
			ratio_node_remove(state, node);
		node = next;
	}
}

void tag_ratio_state_seed_client(Client *c) {
	if (!c || !c->mon)
		return;
	uint32_t tag = get_mon_curtag(c->mon);
	if (!layout_uses_tag_ratios(c->mon->pertag->ltidxs[tag]))
		return;
	struct TagRatioState *state = ensure_tag_ratio_state(c->mon, tag);
	if (!state || !ratio_node_belongs(c->mon, c) || find_ratio_node(state, c))
		return;
	struct TagRatioNode *node =
		ratio_node_create(state, c, c->mon->pertag->mfacts[tag]);
	if (!node)
		return;
	node->master_mfact = c->master_mfact_per;
	node->master_weight = c->master_size_weight;
	node->stack_weight = c->stack_size_weight;
	node->grid_col = c->grid_col_per;
	node->grid_row = c->grid_row_per;
}

static void sync_tag_ratio_state_to_clients(Monitor *m, uint32_t tag) {
	struct TagRatioState *state = m->pertag->tag_ratio_state[tag];
	if (!state)
		return;
	for (struct TagRatioNode *node = state->all_first; node;
		 node = node->all_next) {
		Client *c = node->client;
		if (!c)
			continue;
		c->master_mfact_per = node->master_mfact;
		c->master_size_weight = node->master_weight;
		c->stack_size_weight = node->stack_weight;
		c->grid_col_per = node->grid_col;
		c->grid_row_per = node->grid_row;
	}
}

void sync_tag_ratio_state_from_clients(Monitor *m, uint32_t tag) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS ||
		!layout_uses_tag_ratios(m->pertag->ltidxs[tag]))
		return;
	struct TagRatioState *state = m->pertag->tag_ratio_state[tag];
	if (!state)
		return;
	for (struct TagRatioNode *node = state->all_first; node;
		 node = node->all_next) {
		Client *c = node->client;
		if (!c)
			continue;
		node->master_mfact = c->master_mfact_per;
		node->master_weight = c->master_size_weight;
		node->stack_weight = c->stack_size_weight;
		node->grid_col = c->grid_col_per;
		node->grid_row = c->grid_row_per;
	}
}

int32_t tag_ratio_state_client_count(Monitor *m, uint32_t tag) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS ||
		!m->pertag->tag_ratio_state[tag])
		return 0;
	return m->pertag->tag_ratio_state[tag]->count;
}

void tag_ratio_state_begin(Monitor *m, uint32_t tag, bool from_view) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS ||
		!layout_uses_tag_ratios(m->pertag->ltidxs[tag]))
		return;
	if (!from_view)
		sync_tag_ratio_state_from_clients(m, tag);
	update_tag_ratio_state(m, tag);
	sync_tag_ratio_state_to_clients(m, tag);
}

void tag_ratio_state_remove_client_in_tag(Monitor *m, uint32_t tag, Client *c) {
	struct TagRatioState *state = m->pertag->tag_ratio_state[tag];
	struct TagRatioNode *node;
	while (state && (node = find_ratio_node(state, c)))
		ratio_node_remove(state, node);
}

void tag_ratio_state_replace_client_in_tag(Monitor *m, uint32_t tag,
										   Client *old, Client *new) {
	if (!old || !new || old == new)
		return;
	struct TagRatioState *state = m->pertag->tag_ratio_state[tag];
	if (!state)
		return;
	struct TagRatioNode *old_node = find_ratio_node(state, old);
	if (!old_node)
		return;
	struct TagRatioNode *new_node;
	while ((new_node = find_ratio_node(state, new)) && new_node != old_node)
		ratio_node_remove(state, new_node);
	old_node->client = new;
	struct TagRatioNode *node;
	while ((node = find_ratio_node(state, old)))
		ratio_node_remove(state, node);
}

void tag_ratio_state_copy(Monitor *target, uint32_t dst_tag,
						  const Monitor *source, uint32_t src_tag) {
	if (!target || !source || !target->pertag || !source->pertag ||
		dst_tag >= PERTAG_SLOTS || src_tag >= PERTAG_SLOTS)
		return;
	if (target == source && dst_tag == src_tag)
		return;

	struct TagRatioState *dst = ensure_tag_ratio_state(target, dst_tag);
	clear_tag_ratio_state(dst);
	target->pertag->tag_ratio_state[dst_tag] = NULL;

	const struct TagRatioState *src = source->pertag->tag_ratio_state[src_tag];
	if (!src)
		return;
	dst = ensure_tag_ratio_state(target, dst_tag);
	if (!dst)
		return;
	for (const struct TagRatioNode *node = src->all_first; node;
		 node = node->all_next) {
		struct TagRatioNode *copy =
			ratio_node_create(dst, node->client, node->master_mfact);
		if (!copy)
			continue;
		copy->master_weight = node->master_weight;
		copy->stack_weight = node->stack_weight;
		copy->grid_col = node->grid_col;
		copy->grid_row = node->grid_row;
	}
}

void tag_ratio_state_move(Monitor *m, uint32_t dst_tag, uint32_t src_tag) {
	if (!m || !m->pertag || dst_tag >= PERTAG_SLOTS ||
		src_tag >= PERTAG_SLOTS || dst_tag == src_tag)
		return;
	clear_tag_ratio_state(m->pertag->tag_ratio_state[dst_tag]);
	m->pertag->tag_ratio_state[dst_tag] = m->pertag->tag_ratio_state[src_tag];
	m->pertag->tag_ratio_state[src_tag] = NULL;
}

void tag_ratio_state_reset(Monitor *m, uint32_t tag) {
	if (!m || !m->pertag || tag >= PERTAG_SLOTS)
		return;
	clear_tag_ratio_state(m->pertag->tag_ratio_state[tag]);
	m->pertag->tag_ratio_state[tag] = NULL;
}

void cleanup_monitor_tag_ratio_states(Monitor *m) {
	if (!m || !m->pertag)
		return;
	for (uint32_t tag = 0; tag < PERTAG_SLOTS; tag++)
		tag_ratio_state_reset(m, tag);
}

void tag_ratio_state_set_master_mfact(Monitor *m, uint32_t tag, double mfact) {
	struct TagRatioState *state = ensure_tag_ratio_state(m, tag);
	if (!state)
		return;
	m->pertag->mfacts[tag] = mfact;
	bool current = get_mon_curtag(m) == tag;
	for (struct TagRatioNode *node = state->all_first; node;
		 node = node->all_next) {
		node->master_mfact = mfact;
		if (current && node->client)
			node->client->master_mfact_per = mfact;
	}
}
