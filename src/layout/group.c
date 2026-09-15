#include "mango/layout/group.h"
#include "mango/common/server.h"
#include "mango/config/parse_config.h"
#include "mango/draw/text-node.h"
#include "mango/layout/arrange.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"

Client *group_capture_get_parent(void) {
	if (!config.group_capture_spawn || !server.selected_monitor)
		return NULL;

	Client *sel = server.selected_monitor->sel;
	if (sel && (sel->group_prev || sel->group_next || sel->isgroupfocusing))
		return sel;

	return NULL;
}

bool group_capture_spawn(Client *c, Client *group_parent) {
	if (!config.group_capture_spawn || !group_parent || !c->mon ||
		c->mon != group_parent->mon ||
		!(group_parent->group_prev || group_parent->group_next ||
		  group_parent->isgroupfocusing))
		return false;

	c->group_prev = group_parent->group_prev;
	if (group_parent->group_prev)
		group_parent->group_prev->group_next = c;
	c->group_next = group_parent;
	group_parent->group_prev = c;
	c->is_pending_open_animation = false;
	client_focus_group_member(c);
	return true;
}

void client_add_group_bar(Client *c) {

	if (config.group_bar_height <= 0) {
		return;
	}

	uint32_t layer = client_target_layer(c);

	c->group_bar = mango_group_bar_create(c, GroupBar, server.layers[layer],
										  config.groupbardata, 0, 0);
	wlr_scene_node_lower_to_bottom(&c->group_bar->scene_buffer->node);
	wlr_scene_node_set_enabled(&c->group_bar->scene_buffer->node, false);
	mango_group_bar_update(c->group_bar, client_get_display_title(c),
						   c->mon ? c->mon->wlr_output->scale
						   : server.selected_monitor
							   ? server.selected_monitor->wlr_output->scale
							   : 1.0f);
}

void client_focus_group_member(Client *c) {
	if (!c->group_prev && !c->group_next)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	Client *cur_focusing = NULL;
	while (cur) {
		if (cur != c && cur->isgroupfocusing && cur->mon)
			cur_focusing = cur;
		cur = cur->group_next;
		if (cur == head)
			break;
	}

	if (!cur_focusing || !cur_focusing->mon)
		return;

	if (cur_focusing->mon->isoverview)
		return;

	cur_focusing->isgroupfocusing = false;
	c->mon = cur_focusing->mon;
	client_replace(c, cur_focusing, true, false);
	mango_group_bar_set_focus(cur_focusing->group_bar, false);

	c->isgroupfocusing = true;
	mango_group_bar_set_focus(c->group_bar, true);

	client_reparent_group(c);

	client_focus(c, 1);

	arrange(c->mon, false, false);
}

void client_check_tab_node_visible(Client *c) {

	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (!c->mon->isoverview && cur->group_bar &&
			(cur->group_next || cur->group_prev || cur->isgroupfocusing) &&
			TAGMATCH(c, c->mon) && ISNORMAL(c) && !c->isfullscreen) {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   true);
		} else {
			wlr_scene_node_set_enabled(&cur->group_bar->scene_buffer->node,
									   false);
		}
		cur = cur->group_next;
	}
}

void client_raise_group(Client *c) {
	if (!c || !c->mon)
		return;

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_raise_to_top(&cur->group_bar->scene_buffer->node);
		}
		wlr_scene_node_raise_to_top(&cur->scene->node);
		cur = cur->group_next;
	}
}

void client_reparent_group(Client *c) {
	if (!c || !c->mon)
		return;

	int32_t layer = client_target_layer(c);

	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->group_bar) {
			wlr_scene_node_reparent(&cur->group_bar->scene_buffer->node,
									server.layers[layer]);
		}
		wlr_scene_node_reparent(&cur->scene->node, server.layers[layer]);
		cur = cur->group_next;
	}
}

void client_handle_decorate_click(MangoGroupBar *gb) {

	if (!gb)
		return;

	if (gb->node_data) {
		Client *c = gb->node_data;
		client_focus_group_member(c);
	}
}

void client_set_group_mon(Client *c, Monitor *m) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		client_change_mon(cur, m);
		cur = cur->group_next;
	}
}

void client_set_group_config(Client *c) {
	Client *head = c;
	while (head->group_prev)
		head = head->group_prev;

	Client *cur = head;
	while (cur) {
		if (cur->jump_label_node)
			mango_jump_label_node_apply_config(cur->jump_label_node,
											   &config.jumplabeldata);
		wlr_scene_rect_set_color(cur->droparea, config.dropcolor);
		wlr_scene_rect_set_color(cur->splitindicator[0], config.splitcolor);
		wlr_scene_rect_set_color(cur->splitindicator[1], config.splitcolor);
		mango_group_bar_apply_config(cur->group_bar, &config.groupbardata);
		cur = cur->group_next;
	}
}

void client_group_detach(Client *c) {
	if (c->group_prev)
		c->group_prev->group_next = c->group_next;
	if (c->group_next)
		c->group_next->group_prev = c->group_prev;
	c->group_prev = NULL;
	c->group_next = NULL;
	c->isgroupfocusing = false;
}

void client_group_replace(Client *old, Client *new) {
	client_group_detach(new);

	new->group_prev = old->group_prev;
	new->group_next = old->group_next;
	if (old->group_prev)
		old->group_prev->group_next = new;
	if (old->group_next)
		old->group_next->group_prev = new;
	old->group_prev = NULL;
	old->group_next = NULL;

	if (client_is_parked(old) || (!new->group_prev && !new->group_next)) {
		new->isgroupfocusing = false;
	} else {
		new->isgroupfocusing = old->isgroupfocusing;
	}
}