#include "mango/workspace_overview/workspace_overview_internal.h"

uint32_t wsov_tag_bit(uint32_t tag) {
	if (tag < 1 || tag > (uint32_t)config.tag_num)
		return 0;
	return 1u << (tag - 1);
}

uint32_t wsov_client_tags(Client *c) {
	if (!c)
		return 0;
	return c->isglobal ? TAGMASK : (c->tags & TAGMASK);
}

uint32_t wsov_cell_tags_for_client(Client *c) {
	uint32_t tags = 0;
	if (!c)
		return tags;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		for (int j = 0; j < cell->item_count; j++) {
			if (cell->items[j].client == c) {
				tags |= wsov_tag_bit(cell->tag);
				break;
			}
		}
	}
	return tags;
}

uint32_t wsov_layout_affected_tags(Monitor *m) {
	uint32_t active_tags = m->tagset[m->seltags] & TAGMASK;
	if (!active_tags)
		active_tags = wsov_tag_bit(get_mon_curtag(m));
	/*
	 * client_view_on_monitor() toggles seltags, so the inactive set is the
	 * view we just left. Include both sides as well as pertag history: moving a
	 * client can change the layout of the source workspace without changing
	 * the destination workspace membership.
	 */
	uint32_t previous_tags = m->tagset[m->seltags ^ 1] & TAGMASK;
	uint32_t tags = active_tags | previous_tags |
					wsov_tag_bit(m->pertag->curtag) |
					wsov_tag_bit(m->pertag->prevtag);
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || client_is_unmanaged(c) || c->iskilling)
			continue;
		if (c->isglobal || (c->tags & (active_tags | previous_tags)))
			tags |= wsov_client_tags(c);
	}
	return tags;
}

bool wsov_client_base_eligible(Client *c) {
	if (!c || !c->mon || c->mon != wsov.mon || c->iskilling || c->isminimized ||
		c->isunglobal || client_is_unmanaged(c) || client_is_x11_popup(c) ||
		(c->tags & TAG0_MASK))
		return false;
	struct wlr_surface *surface = NULL;
#ifdef XWAYLAND
	if (client_is_x11(c))
		surface = c->surface.xwayland ? c->surface.xwayland->surface : NULL;
	else
#endif
		surface = c->surface.xdg ? c->surface.xdg->surface : NULL;
	if (!surface || !surface->mapped)
		return false;
	return true;
}

bool wsov_client_eligible(Client *c) {
	return wsov_client_base_eligible(c) && !c->is_tab_hidden;
}

bool workspace_overview_active_workspace_will_empty(Client *c) {
	if (!workspace_overview_is_active() || !c || c->mon != wsov.mon)
		return false;
	/* The swallowed client will re-enter the view during unmap processing. */
	if (c->swallowing)
		return false;

	uint32_t active = wsov.mon->tagset[wsov.mon->seltags] & TAGMASK;
	if (!(c->tags & active))
		return false;

	Client *other;
	wl_list_for_each(other, &server.clients, link) {
		if (other == c || !wsov_client_eligible(other))
			continue;
		if (other->isglobal || (other->tags & active))
			return false;
	}
	return true;
}

bool wsov_client_on_tag(Client *c, uint32_t tag) {
	if (!wsov_client_base_eligible(c) ||
		(wsov.mon && wsov.mon->is_ws_overview &&
		 tab_client_hidden_on_tag(wsov.mon, tag, c)))
		return false;
	if (c->isglobal)
		return true;
	uint32_t bit = wsov_tag_bit(tag);
	return bit && (c->tags & bit);
}

Client *wsov_focus_top_for_tag(uint32_t tag) {
	Client *c;
	wl_list_for_each(c, &server.focus_stack, flink) {
		if (wsov_client_on_tag(c, tag))
			return c;
	}
	return NULL;
}

void wsov_sync_monitor_selection(void) {
	if (!wsov.mon || !wsov.target_tag)
		return;
	Client *top = wsov_focus_top_for_tag(wsov.target_tag);
	if (!top)
		return;
	if (wsov.mon->sel != top || !top->isfocusing)
		client_focus(top, 0);
	if (wsov.mon->sel != top)
		wsov.mon->sel = top;
}

bool wsov_tag_has_normal_client(uint32_t tag) {
	uint32_t bit = wsov_tag_bit(tag);
	if (!bit)
		return false;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (wsov_client_eligible(c) && !c->isglobal && (c->tags & bit))
			return true;
	}
	return false;
}

uint32_t wsov_collect_workspace_tags(Monitor *m, int *count) {
	uint32_t mask = 0;
	int n = 0;

	if (!m) {
		if (count)
			*count = 0;
		return 0;
	}

	if (m->workspace_overview_scope == ALL_WORKSPACE) {
		if (count)
			*count = config.tag_num;
		return ((uint32_t)1 << config.tag_num) - 1;
	}

	for (int tag = 1; tag <= config.tag_num; tag++) {
		uint32_t bit = wsov_tag_bit((uint32_t)tag);
		if (wsov_tag_has_normal_client((uint32_t)tag) ||
			(config.workspace_overview_occupied_keep_seen_tags &&
			 (wsov.retained_tags & bit))) {
			mask |= wsov_tag_bit((uint32_t)tag);
			n++;
		}
	}
	if (count)
		*count = n;
	return mask;
}

struct wsov_cell *wsov_cell_for_tag(uint32_t tag) {
	for (int i = 0; i < wsov.cell_count; i++) {
		if (wsov.cells[i].tag == tag)
			return &wsov.cells[i];
	}
	return NULL;
}

void wsov_select_default_target(void) {
	if (wsov.cell_count <= 0) {
		wsov.target_tag = 0;
		return;
	}
	if (wsov_cell_for_tag(wsov.target_tag))
		return;

	uint32_t previous_target = wsov.target_tag;
	/*
	 * The target workspace itself was removed. Continue to the nearest
	 * following occupied workspace; when there is none, use the nearest
	 * preceding one instead of jumping back to the first workspace.
	 */
	if (previous_target) {
		for (int i = 0; i < wsov.cell_count; i++) {
			if (wsov.cells[i].tag > previous_target) {
				wsov.target_tag = wsov.cells[i].tag;
				return;
			}
		}
		for (int i = wsov.cell_count - 1; i >= 0; i--) {
			if (wsov.cells[i].tag < previous_target) {
				wsov.target_tag = wsov.cells[i].tag;
				return;
			}
		}
	}

	uint32_t active = wsov.mon->tagset[wsov.mon->seltags] & TAGMASK;
	for (int i = 0; i < wsov.cell_count; i++) {
		if (active & wsov_tag_bit(wsov.cells[i].tag)) {
			wsov.target_tag = wsov.cells[i].tag;
			return;
		}
	}
	wsov.target_tag = wsov.cells[0].tag;
}

bool wsov_set_target_from_point(double x, double y) {
	struct wsov_cell *cell = wsov_hit_cell_at(x, y);
	if (!cell)
		cell = wsov_drop_cell_at(x, y);
	if (!cell || cell->tag == wsov.target_tag)
		return false;

	wsov.target_tag = cell->tag;
	/*
	 * Keep the opening camera anchored to the card it started from. The target
	 * and focus may still follow pointer motion, but changing camera_tag while
	 * the zoom is in progress makes the camera jump to another card.
	 */
	if (!wsov.animating)
		wsov.camera_tag = cell->tag;
	return true;
}

void wsov_settle_tag_animations(Monitor *m, uint32_t tags) {
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m || client_is_unmanaged(c) || c->iskilling)
			continue;
		if (!c->isglobal && !(c->tags & tags))
			continue;
		if (!c->animation.tagining && !c->animation.tagouting &&
			c->animation.action != TAG)
			continue;
		c->animation.running = false;
		c->animation.tagining = false;
		c->animation.tagouting = false;
		c->animation.tagouted = false;
		c->need_output_flush = false;
		client_set_finish_geometry(c);
		client_update_visibility(c);
	}
}

bool wsov_cell_has_client(const struct wsov_cell *cell, const Client *client) {
	if (!client)
		return false;
	for (int i = 0; i < cell->item_count; i++) {
		if (cell->items[i].client == client)
			return true;
	}
	return false;
}

void wsov_render_selection_clients(Client *previous, Client *current) {
	if (previous == current)
		return;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (wsov_cell_has_client(cell, previous) ||
			wsov_cell_has_client(cell, current))
			wsov_render_cell(cell, wsov_output_scale());
	}
}

void wsov_apply_target_selection(bool activate_view) {
	if (!wsov.mon || wsov.closing)
		return;
	Client *previous = wsov.mon->sel;
	struct wsov_cell *previous_live_cell = wsov.live_cell;
	if (activate_view)
		wsov_activate_target_tag();
	wsov_sync_monitor_selection();
	if (!wsov.live_cell || wsov.live_cell->tag != wsov.target_tag)
		wsov_rebuild_live_projection();
	else
		wsov_layout_live_projection();
	/*
	 * The old live cell needs its complete off-screen copy immediately after
	 * the live projection moves. Focusing the same client (notably a floating
	 * global window) makes the selection diff empty, so it cannot be relied on
	 * to repaint that cell. Repaint both sides before the scroll animation
	 * advances to avoid a one-frame gap in either preview.
	 */
	if (previous_live_cell)
		wsov_render_cell(previous_live_cell, wsov_output_scale());
	if (wsov.live_cell && wsov.live_cell != previous_live_cell)
		wsov_render_cell(wsov.live_cell, wsov_output_scale());
	wsov_render_selection_clients(previous, wsov.mon->sel);
	wsov_update_styles();
	wsov_apply_camera(wsov.progress);
}

void wsov_refresh_target_selection(void) { wsov_apply_target_selection(true); }

uint32_t wsov_active_camera_tag(void) {
	if (!wsov.mon)
		return 0;
	uint32_t current = get_mon_curtag(wsov.mon);
	if (wsov_cell_for_tag(current))
		return current;

	uint32_t active = wsov.mon->tagset[wsov.mon->seltags] & TAGMASK;
	for (int i = 0; i < wsov.cell_count; i++) {
		uint32_t tag = wsov.cells[i].tag;
		if (active & wsov_tag_bit(tag))
			return tag;
	}
	return 0;
}

struct wsov_cell *wsov_target_cell(void) {
	return wsov_cell_for_tag(wsov.target_tag);
}

void wsov_select_direction(int dir_x, int dir_y) {
	struct wsov_cell *current = wsov_target_cell();
	if (!current || wsov.cell_count <= 1)
		return;

	struct wlr_box current_box = wsov_cell_layout_box(current);
	int32_t current_l = current_box.x;
	int32_t current_r = current_box.x + current_box.width;
	int32_t current_t = current_box.y;
	int32_t current_b = current_box.y + current_box.height;
	int32_t current_cx = current_l + current_box.width / 2;
	int32_t current_cy = current_t + current_box.height / 2;

	struct wsov_cell *target = NULL;
	int64_t best_distance = LLONG_MAX;
	int64_t best_center_distance = LLONG_MAX;
	for (int i = 0; i < wsov.cell_count; i++) {
		struct wsov_cell *cell = &wsov.cells[i];
		if (cell == current)
			continue;

		struct wlr_box box = wsov_cell_layout_box(cell);
		int32_t left = box.x;
		int32_t right = box.x + box.width;
		int32_t top = box.y;
		int32_t bottom = box.y + box.height;
		int32_t center_x = left + box.width / 2;
		int32_t center_y = top + box.height / 2;
		int64_t main_distance = 0;
		int64_t orth_distance = 0;
		bool match_direction = false;

		if (dir_x < 0) {
			if (center_x < current_cx ||
				(center_x == current_cx && left < current_l)) {
				match_direction = true;
				main_distance = current_l - right;
				orth_distance = bottom < current_t
									? current_t - bottom
									: (top > current_b ? top - current_b : 0);
			}
		} else if (dir_x > 0) {
			if (center_x > current_cx ||
				(center_x == current_cx && left > current_l)) {
				match_direction = true;
				main_distance = left - current_r;
				orth_distance = bottom < current_t
									? current_t - bottom
									: (top > current_b ? top - current_b : 0);
			}
		} else if (dir_y < 0) {
			if (center_y < current_cy ||
				(center_y == current_cy && top < current_t)) {
				match_direction = true;
				main_distance = current_t - bottom;
				orth_distance = right < current_l
									? current_l - right
									: (left > current_r ? left - current_r : 0);
			}
		} else if (dir_y > 0) {
			if (center_y > current_cy ||
				(center_y == current_cy && top > current_t)) {
				match_direction = true;
				main_distance = top - current_b;
				orth_distance = right < current_l
									? current_l - right
									: (left > current_r ? left - current_r : 0);
			}
		}
		if (!match_direction)
			continue;

		bool orth_overlap = dir_x ? (bottom >= current_t && top <= current_b)
								  : (right >= current_l && left <= current_r);
		if (config.focusdir_only_zone_overlap && !orth_overlap)
			continue;

		int64_t penalty = 0;
		if (main_distance < 0) {
			penalty = 10000000000LL;
			main_distance = -main_distance;
		}
		int64_t distance = penalty + main_distance * main_distance +
						   orth_distance * orth_distance;
		int64_t center_distance = dir_y ? llabs((int64_t)center_x - current_cx)
										: llabs((int64_t)center_y - current_cy);
		if (distance < best_distance ||
			(distance == best_distance &&
			 center_distance < best_center_distance)) {
			best_distance = distance;
			best_center_distance = center_distance;
			target = cell;
		}
	}

	if (!target)
		return;

	wsov.target_tag = target->tag;
	wsov.scroll_center_tag = target->tag;
	if (!wsov.animating)
		wsov.camera_tag = wsov.target_tag;
	wsov_animate_scroll_to_target();
	wsov_refresh_target_selection();
}

void wsov_activate_target_tag(void) {
	if (!workspace_overview_is_active() || wsov.closing || !wsov.mon ||
		!wsov.target_tag)
		return;
	uint32_t target_bit = wsov_tag_bit(wsov.target_tag);
	if (!target_bit ||
		(wsov.mon->tagset[wsov.mon->seltags] & TAGMASK) == target_bit)
		return;
	client_view_on_monitor(&(Arg){.ui = target_bit}, false, wsov.mon, true);
}
