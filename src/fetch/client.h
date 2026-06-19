bool check_hit_no_border(Client *c) {
	bool hit_no_border = false;

	if (!c->mon)
		return false;

	if (c->tags <= 0)
		return false;

	if (!render_border) {
		hit_no_border = true;
	}

	if (c->mon && !c->mon->isoverview &&
		c->mon->pertag->no_render_border[get_tags_first_tag_num(c->tags)]) {
		hit_no_border = true;
	}

	if (config.no_border_when_single && c && c->mon &&
		((ISSCROLLTILED(c) && c->mon->visible_scroll_tiling_clients == 1) ||
		 c->mon->visible_clients == 1)) {
		hit_no_border = true;
	}
	return hit_no_border;
}

Client *termforwin(Client *w) {
	Client *c = NULL;

	if (!w->pid || w->isterm || w->noswallow)
		return NULL;

	wl_list_for_each(c, &fstack, flink) {
		if (c->isterm && !c->swallowing && c->pid &&
			isdescprocess(c->pid, w->pid)) {
			return c;
		}
	}

	return NULL;
}
Client *get_client_by_id_or_title(const char *arg_id, const char *arg_title) {
	Client *target_client = NULL;
	const char *appid, *title;
	Client *c = NULL;
	wl_list_for_each(c, &clients, link) {
		if (!config.scratchpad_cross_monitor && c->mon != selmon) {
			continue;
		}

		if (c->swallowedby) {
			appid = client_get_appid(c->swallowedby);
			title = client_get_title(c->swallowedby);
		} else {
			appid = client_get_appid(c);
			title = client_get_title(c);
		}

		if (!appid) {
			appid = broken;
		}

		if (!title) {
			title = broken;
		}

		if (arg_id && strncmp(arg_id, "none", 4) == 0)
			arg_id = NULL;

		if (arg_title && strncmp(arg_title, "none", 4) == 0)
			arg_title = NULL;

		if ((arg_title && regex_match(arg_title, title) && !arg_id) ||
			(arg_id && regex_match(arg_id, appid) && !arg_title) ||
			(arg_id && regex_match(arg_id, appid) && arg_title &&
			 regex_match(arg_title, title))) {
			target_client = c;
			break;
		}
	}
	return target_client;
}
struct wlr_box // 计算客户端居中坐标
setclient_coordinate_center(Client *c, Monitor *tm, struct wlr_box geom,
							int32_t offsetx, int32_t offsety) {
	struct wlr_box tempbox;
	int32_t offset = 0;
	int32_t len = 0;
	Monitor *m = tm ? tm : selmon;

	if (!m)
		return geom;

	uint32_t cbw = c && check_hit_no_border(c) ? c->bw : 0;

	if ((!c || !c->no_force_center) && m) {
		tempbox.x = m->w.x + (m->w.width - geom.width) / 2;
		tempbox.y = m->w.y + (m->w.height - geom.height) / 2;
	} else {
		tempbox.x = geom.x;
		tempbox.y = geom.y;
	}

	tempbox.width = geom.width;
	tempbox.height = geom.height;

	if (offsetx != 0) {
		len = (m->w.width - tempbox.width - 2 * m->gappoh) / 2;
		offset = len * (offsetx / 100.0);
		tempbox.x += offset;

		// 限制窗口在屏幕内
		if (tempbox.x < m->m.x) {
			tempbox.x = m->m.x - cbw;
		}
		if (tempbox.x + tempbox.width > m->m.x + m->m.width) {
			tempbox.x = m->m.x + m->m.width - tempbox.width + cbw;
		}
	}
	if (offsety != 0) {
		len = (m->w.height - tempbox.height - 2 * m->gappov) / 2;
		offset = len * (offsety / 100.0);
		tempbox.y += offset;

		// 限制窗口在屏幕内
		if (tempbox.y < m->m.y) {
			tempbox.y = m->m.y - cbw;
		}
		if (tempbox.y + tempbox.height > m->m.y + m->m.height) {
			tempbox.y = m->m.y + m->m.height - tempbox.height + cbw;
		}
	}

	return tempbox;
}
/* Helper: Check if rule matches client */
static bool is_window_rule_matches(const ConfigWinRule *r, const char *appid,
								   const char *title) {
	return (r->title && regex_match(r->title, title) && !r->id) ||
		   (r->id && regex_match(r->id, appid) && !r->title) ||
		   (r->id && regex_match(r->id, appid) && r->title &&
			regex_match(r->title, title));
}

Client *center_tiled_select(Monitor *m) {
	Client *c = NULL;
	Client *target_c = NULL;
	int64_t mini_distance = -1;
	int32_t dirx, diry;
	int64_t distance;
	wl_list_for_each(c, &clients, link) {
		if (c && VISIBLEON(c, m) && ISSCROLLTILED(c) &&
			client_surface(c)->mapped && !c->isfloating &&
			!client_is_unmanaged(c)) {
			dirx = c->geom.x + c->geom.width / 2 - (m->w.x + m->w.width / 2);
			diry = c->geom.y + c->geom.height / 2 - (m->w.y + m->w.height / 2);
			distance = dirx * dirx + diry * diry;
			if (distance < mini_distance || mini_distance == -1) {
				mini_distance = distance;
				target_c = c;
			}
		}
	}
	return target_c;
}

Client *find_client_by_direction(Client *tc, const Arg *arg,
								 bool findfloating) {
	Client *c = NULL;
	Client *tempFocusClients = NULL;
	Client *tempSameMonitorFocusClients = NULL;
	int64_t distance = LLONG_MAX;
	int64_t same_monitor_distance = LLONG_MAX;

	int32_t tc_l = tc->geom.x;
	int32_t tc_r = tc->geom.x + tc->geom.width;
	int32_t tc_t = tc->geom.y;
	int32_t tc_b = tc->geom.y + tc->geom.height;
	int32_t tc_cx = tc_l + tc->geom.width / 2;
	int32_t tc_cy = tc_t + tc->geom.height / 2;

	for (int32_t step = 0; step < 2; step++) {
		if (step == 1 && tempFocusClients)
			break;

		wl_list_for_each(c, &clients, link) {
			if (!c || c == tc)
				continue;
			if (!findfloating && c->isfloating)
				continue;
			if (c->is_monocle_hide)
				continue;
			if (c->isunglobal)
				continue;
			if (!config.focus_cross_monitor && c->mon != tc->mon)
				continue;
			if (!(c->tags & c->mon->tagset[c->mon->seltags]))
				continue;

			if (step == 0 && ((!tc->mon->isoverview &&
							   !client_is_in_same_stack(tc, c, NULL)) ||
							  c->mon != tc->mon))
				continue;

			int32_t c_l = c->geom.x;
			int32_t c_r = c->geom.x + c->geom.width;
			int32_t c_t = c->geom.y;
			int32_t c_b = c->geom.y + c->geom.height;
			int32_t c_cx = c_l + c->geom.width / 2;
			int32_t c_cy = c_t + c->geom.height / 2;

			int64_t main_dist = 0;
			int64_t orth_dist = 0;
			bool match_dir = false;

			switch (arg->i) {
			case LEFT:
				if (c_cx < tc_cx || (c_cx == tc_cx && c_l < tc_l)) {
					match_dir = true;
					main_dist = tc_l - c_r;
					orth_dist = (c_b < tc_t)
									? (tc_t - c_b)
									: ((c_t > tc_b) ? (c_t - tc_b) : 0);
				}
				break;
			case RIGHT:
				if (c_cx > tc_cx || (c_cx == tc_cx && c_l > tc_l)) {
					match_dir = true;
					main_dist = c_l - tc_r;
					orth_dist = (c_b < tc_t)
									? (tc_t - c_b)
									: ((c_t > tc_b) ? (c_t - tc_b) : 0);
				}
				break;
			case UP:
				if (c_cy < tc_cy || (c_cy == tc_cy && c_t < tc_t)) {
					match_dir = true;
					main_dist = tc_t - c_b;
					orth_dist = (c_r < tc_l)
									? (tc_l - c_r)
									: ((c_l > tc_r) ? (c_l - tc_r) : 0);
				}
				break;
			case DOWN:
				if (c_cy > tc_cy || (c_cy == tc_cy && c_t > tc_t)) {
					match_dir = true;
					main_dist = c_t - tc_b;
					orth_dist = (c_r < tc_l)
									? (tc_l - c_r)
									: ((c_l > tc_r) ? (c_l - tc_r) : 0);
				}
				break;
			default:
				continue;
			}

			if (!match_dir)
				continue;

			int64_t penalty = 0;
			if (main_dist < 0) {
				penalty = 10000000000LL; // 主方向重叠（反方向）的极大惩罚
				main_dist = -main_dist;
			}

			// 正交方向无重叠惩罚，优先选择在同一行/列的窗口
			int64_t no_overlap_penalty = 0;
			if (orth_dist > 0) {
				// LEFT/RIGHT 时 orth_dist 是垂直间距，>0 表示垂直无重叠
				// UP/DOWN  时 orth_dist 是水平间距，>0 表示水平无重叠
				no_overlap_penalty = 10000000LL;
			}

			int64_t tmp_distance = penalty + no_overlap_penalty +
								   (main_dist * main_dist) +
								   (orth_dist * orth_dist);

			if (tmp_distance < distance) {
				distance = tmp_distance;
				tempFocusClients = c;
			}
			if (c->mon == tc->mon && tmp_distance < same_monitor_distance) {
				same_monitor_distance = tmp_distance;
				tempSameMonitorFocusClients = c;
			}
		}
	}

	if (tempSameMonitorFocusClients)
		return tempSameMonitorFocusClients;
	return tempFocusClients;
}

Client *direction_select(const Arg *arg) {

	Client *tc = arg->tc ? arg->tc : selmon->sel;

	if (!tc)
		return NULL;

	if (tc && (tc->isfullscreen || tc->ismaximizescreen) &&
		(!is_scroller_layout(selmon) || tc->isfloating)) {
		return NULL;
	}

	return find_client_by_direction(tc, arg, true);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client *focustop(Monitor *m) {
	Client *c = NULL;
	wl_list_for_each(c, &fstack, flink) {
		if (c->iskilling || c->isunglobal)
			continue;
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

Client *get_next_stack_client(Client *c, bool reverse) {
	if (!c || !c->mon)
		return NULL;

	Client *next = NULL;
	if (reverse) {
		wl_list_for_each_reverse(next, &c->link, link) {
			if (&next->link == &clients)
				continue; /* wrap past the sentinel node */

			if (next->isunglobal)
				continue;

			if (next != c && next->mon && VISIBLEON(next, c->mon))
				return next;
		}
	} else {
		wl_list_for_each(next, &c->link, link) {
			if (&next->link == &clients)
				continue; /* wrap past the sentinel node */

			if (next->isunglobal)
				continue;

			if (next != c && next->mon && VISIBLEON(next, c->mon))
				return next;
		}
	}
	return NULL;
}

float *get_border_color(Client *c) {

	if (c->mon != selmon) {
		return config.bordercolor;
	} else if (c->isurgent) {
		return config.urgentcolor;
	} else if (c->is_in_scratchpad && selmon && c == selmon->sel) {
		return config.scratchpadcolor;
	} else if (c->isglobal && selmon && c == selmon->sel) {
		return config.globalcolor;
	} else if (c->isoverlay && selmon && c == selmon->sel) {
		return config.overlaycolor;
	} else if (c->ismaximizescreen && selmon && c == selmon->sel) {
		return config.maximizescreencolor;
	} else if (selmon && c == selmon->sel) {
		return config.focuscolor;
	} else {
		return config.bordercolor;
	}
}

int32_t is_single_bit_set(uint32_t x) { return x && !(x & (x - 1)); }

bool client_only_in_one_tag(Client *c) {
	uint32_t masked = c->tags & TAGMASK;
	if (is_single_bit_set(masked)) {
		return true;
	} else {
		return false;
	}
}

bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc) {
	if (!sc || !tc)
		return false;

	uint32_t id = sc->mon->pertag->ltidxs[sc->mon->pertag->curtag]->id;

	if (id != SCROLLER && id != VERTICAL_SCROLLER && id != TILE &&
		id != VERTICAL_TILE && id != DECK && id != VERTICAL_DECK &&
		id != CENTER_TILE && id != RIGHT_TILE)
		return false;

	if (id == SCROLLER || id == VERTICAL_SCROLLER) {
		Client *source_stack_head = scroll_get_stack_head_client(sc);
		Client *target_stack_head = scroll_get_stack_head_client(tc);
		Client *fc_head = fc ? scroll_get_stack_head_client(fc) : NULL;

		if (fc && fc_head == source_stack_head)
			return false;
		if (source_stack_head == target_stack_head)
			return true;
		else
			return false;
	}

	if (id == TILE || id == VERTICAL_TILE || id == DECK ||
		id == VERTICAL_DECK || id == RIGHT_TILE) {
		if (tc->ismaster ^ sc->ismaster)
			return false;
		if (fc && !(fc->ismaster ^ sc->ismaster))
			return false;
		else
			return true;
	}

	if (id == CENTER_TILE) {
		if (tc->ismaster ^ sc->ismaster)
			return false;
		if (fc && !(fc->ismaster ^ sc->ismaster))
			return false;
		if (sc->geom.x == tc->geom.x)
			return true;
		else
			return false;
	}

	return false;
}

Client *get_focused_stack_client(Client *sc, Client *custom_focus_client) {
	if (!sc || sc->isfloating)
		return sc;

	Client *tc = NULL;
	Client *fc = custom_focus_client ? custom_focus_client : focustop(sc->mon);

	if (fc->isfloating || sc->isfloating)
		return sc;

	wl_list_for_each(tc, &fstack, flink) {
		if (tc->iskilling || tc->isunglobal || tc->is_monocle_hide)
			continue;
		if (!VISIBLEON(tc, sc->mon))
			continue;
		if (tc == fc)
			continue;

		if (client_is_in_same_stack(sc, tc, fc)) {
			return tc;
		}
	}
	return sc;
}