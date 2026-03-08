bool check_hit_no_border(Client *c) {
	int32_t i;
	bool hit_no_border = false;
	if (!render_border) {
		hit_no_border = true;
	}

	for (i = 0; i < config.tag_rules_count; i++) {
		if (c->tags & (1 << (config.tag_rules[i].id - 1)) &&
			config.tag_rules[i].no_render_border) {
			hit_no_border = true;
		}
	}

	if (no_border_when_single && c && c->mon &&
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
		if (!scratchpad_cross_monitor && c->mon != selmon) {
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

	uint32_t cbw = check_hit_no_border(c) ? c->bw : 0;

	if (!c->no_force_center && m) {
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
Client *find_client_by_direction(Client *tc, const Arg *arg, bool findfloating,
								 bool ignore_align) {
	Client *c = NULL;
	Client **tempClients = NULL; // 初始化为 NULL
	int32_t last = -1;

	// 第一次遍历，计算客户端数量
	wl_list_for_each(c, &clients, link) {
		if (c && (findfloating || !c->isfloating) && !c->isunglobal &&
			(focus_cross_monitor || c->mon == tc->mon) &&
			(c->tags & c->mon->tagset[c->mon->seltags])) {
			last++;
		}
	}

	if (last < 0) {
		return NULL; // 没有符合条件的客户端
	}

	// 动态分配内存
	tempClients = malloc((last + 1) * sizeof(Client *));
	if (!tempClients) {
		// 处理内存分配失败的情况
		return NULL;
	}

	// 第二次遍历，填充 tempClients
	last = -1;
	wl_list_for_each(c, &clients, link) {
		if (c && (findfloating || !c->isfloating) && !c->isunglobal &&
			(focus_cross_monitor || c->mon == tc->mon) &&
			(c->tags & c->mon->tagset[c->mon->seltags])) {
			last++;
			tempClients[last] = c;
		}
	}

	int32_t sel_x = tc->geom.x;
	int32_t sel_y = tc->geom.y;
	int64_t distance = LLONG_MAX;
	int64_t same_monitor_distance = LLONG_MAX;
	Client *tempFocusClients = NULL;
	Client *tempSameMonitorFocusClients = NULL;

	switch (arg->i) {
	case UP:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y < sel_y &&
					tempClients[_i]->geom.x == sel_x &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y < sel_y &&
					tempClients[_i]->mon == tc->mon &&
					client_is_in_same_stack(tc, tempClients[_i], NULL)) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y < sel_y) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case DOWN:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y > sel_y &&
					tempClients[_i]->geom.x == sel_x &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y > sel_y &&
					tempClients[_i]->mon == tc->mon &&
					client_is_in_same_stack(tc, tempClients[_i], NULL)) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y > sel_y) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case LEFT:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x < sel_x &&
					tempClients[_i]->geom.y == sel_y &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x < sel_x &&
					tempClients[_i]->mon == tc->mon &&
					client_is_in_same_stack(tc, tempClients[_i], NULL)) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x < sel_x) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case RIGHT:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x > sel_x &&
					tempClients[_i]->geom.y == sel_y &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x > sel_x &&
					tempClients[_i]->mon == tc->mon &&
					client_is_in_same_stack(tc, tempClients[_i], NULL)) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x > sel_x) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
						dis_x * dis_x + dis_y * dis_y; // 计算距离
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	}

	free(tempClients); // 释放内存
	if (tempSameMonitorFocusClients) {
		return tempSameMonitorFocusClients;
	} else {
		return tempFocusClients;
	}
}

Client *direction_select(const Arg *arg) {

	Client *tc = selmon->sel;

	if (!tc)
		return NULL;

	if (tc && (tc->isfullscreen || tc->ismaximizescreen) &&
		(!is_scroller_layout(selmon) || tc->isfloating)) {
		return NULL;
	}

	return find_client_by_direction(
		tc, arg, true,
		(is_scroller_layout(selmon) || is_centertile_layout(selmon)) &&
			!selmon->isoverview);
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
		return bordercolor;
	} else if (c->isurgent) {
		return urgentcolor;
	} else if (c->is_in_scratchpad && selmon && c == selmon->sel) {
		return scratchpadcolor;
	} else if (c->isglobal && selmon && c == selmon->sel) {
		return globalcolor;
	} else if (c->isoverlay && selmon && c == selmon->sel) {
		return overlaycolor;
	} else if (c->ismaximizescreen && selmon && c == selmon->sel) {
		return maximizescreencolor;
	} else if (selmon && c == selmon->sel) {
		return focuscolor;
	} else {
		return bordercolor;
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

Client *get_scroll_stack_head(Client *c) {
	Client *scroller_stack_head = c;

	if (!scroller_stack_head)
		return NULL;

	while (scroller_stack_head->prev_in_stack) {
		scroller_stack_head = scroller_stack_head->prev_in_stack;
	}
	return scroller_stack_head;
}

bool client_is_in_same_stack(Client *sc, Client *tc, Client *fc) {
	if (!sc || !tc)
		return false;

	uint32_t id = sc->mon->pertag->ltidxs[sc->mon->pertag->curtag]->id;

	if (id != SCROLLER && id != VERTICAL_SCROLLER && id != TILE &&
		id != VERTICAL_TILE && id != DECK && id != VERTICAL_DECK &&
		id != CENTER_TILE && id != RIGHT_TILE && id != TGMIX)
		return false;

	if (id == SCROLLER || id == VERTICAL_SCROLLER) {
		Client *source_stack_head = get_scroll_stack_head(sc);
		Client *target_stack_head = get_scroll_stack_head(tc);
		Client *fc_head = fc ? get_scroll_stack_head(fc) : NULL;
		if (fc && fc->prev_in_stack && fc_head == source_stack_head)
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

	if (id == TGMIX) {
		if (tc->ismaster ^ sc->ismaster)
			return false;
		if (fc && !(fc->ismaster ^ sc->ismaster))
			return false;
		if (!sc->ismaster && sc->mon->visible_tiling_clients <= 3)
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

Client *get_focused_stack_client(Client *sc) {
	if (!sc || sc->isfloating)
		return sc;

	Client *tc = NULL;
	Client *fc = focustop(sc->mon);

	if (fc->isfloating || sc->isfloating)
		return sc;

	wl_list_for_each(tc, &fstack, flink) {
		if (tc->iskilling || tc->isunglobal)
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
