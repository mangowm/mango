void save_old_size_per(Monitor *m) {
	Client *c = NULL;

	wl_list_for_each(c, &clients, link) {
		if (VISIBLEON(c, m) && ISTILED(c)) {
			c->old_master_inner_per = c->master_inner_per;
			c->old_stack_inner_per = c->stack_inner_per;
		}
	}
}

void restore_size_per(Monitor *m, Client *c) {
	Client *fc = NULL;

	if (!m || !c)
		return;

	wl_list_for_each(fc, &clients, link) {
		if (VISIBLEON(fc, m) && ISTILED(fc)) {
			fc->old_ismaster = fc->ismaster;
		}
	}

	c->old_master_inner_per = c->master_inner_per;
	c->old_stack_inner_per = c->stack_inner_per;

	pre_caculate_before_arrange(m, false, false, true);

	const Layout *current_layout = m->pertag->ltidxs[m->pertag->curtag];

	if (current_layout->id == SCROLLER ||
		current_layout->id == VERTICAL_SCROLLER || current_layout->id == GRID ||
		current_layout->id == VERTICAL_GRID || current_layout->id == DECK ||
		current_layout->id == VERTICAL_DECK || current_layout->id == MONOCLE) {
		return;
	}

	if (current_layout->id == CENTER_TILE) {
		wl_list_for_each(fc, &clients, link) {
			if (VISIBLEON(fc, m) && ISTILED(fc) && !c->ismaster) {
				set_size_per(m, fc);
			}
		}
		return;
	}

	// it is possible that the current floating window is moved to another tag,
	// but the tag has not executed save_old_size_per
	// so it must be judged whether their old size values are initial values

	if (!c->ismaster && c->old_stack_inner_per < 1.0 &&
		c->old_stack_inner_per > 0.0f && c->stack_inner_per < 1.0 &&
		c->stack_inner_per > 0.0f) {
		c->stack_inner_per = (1.0 - c->stack_inner_per) *
							 c->old_stack_inner_per /
							 (1.0 - c->old_stack_inner_per);
	}

	if (c->ismaster && c->old_master_inner_per < 1.0 &&
		c->old_master_inner_per > 0.0f && c->master_inner_per < 1.0 &&
		c->master_inner_per > 0.0f) {
		c->master_inner_per = (1.0 - c->master_inner_per) *
							  c->old_master_inner_per /
							  (1.0 - c->old_master_inner_per);
	}

	wl_list_for_each(fc, &clients, link) {
		if (VISIBLEON(fc, m) && ISTILED(fc) && fc != c && !fc->ismaster &&
			fc->old_ismaster && fc->old_stack_inner_per < 1.0 &&
			fc->old_stack_inner_per > 0.0f && fc->stack_inner_per < 1.0 &&
			fc->stack_inner_per > 0.0f) {
			fc->stack_inner_per = (1.0 - fc->stack_inner_per) *
								  fc->old_stack_inner_per /
								  (1.0 - fc->old_stack_inner_per);
			fc->old_ismaster = false;
		}
	}
}

void set_size_per(Monitor *m, Client *c) {
	Client *fc = NULL;
	bool found = false;

	if (!m || !c)
		return;

	const Layout *current_layout = m->pertag->ltidxs[m->pertag->curtag];

	wl_list_for_each(fc, &clients, link) {
		if (VISIBLEON(fc, m) && ISTILED(fc) && fc != c) {
			if (current_layout->id == CENTER_TILE &&
				!(fc->isleftstack ^ c->isleftstack))
				continue;
			c->master_mfact_per = fc->master_mfact_per;
			c->master_inner_per = fc->master_inner_per;
			c->stack_inner_per = fc->stack_inner_per;
			found = true;
			break;
		}
	}

	if (!found) {
		c->master_mfact_per = m->pertag->mfacts[m->pertag->curtag];
		c->master_inner_per = 1.0f;
		c->stack_inner_per = 1.0f;
	}
}

void resize_tile_master_horizontal(Client *grabc, bool isdrag, int32_t offsetx,
								   int32_t offsety, uint32_t time,
								   int32_t type) {
	Client *tc = NULL;
	float delta_x, delta_y;
	Client *next = NULL;
	Client *prev = NULL;
	Client *nextnext = NULL;
	Client *prevprev = NULL;
	struct wl_list *node;
	bool begin_find_nextnext = false;
	bool begin_find_prevprev = false;

	// 从当前节点的下一个开始遍历
	for (node = grabc->link.next; node != &clients; node = node->next) {
		tc = wl_container_of(node, tc, link);
		if (begin_find_nextnext && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			nextnext = tc;
			break;
		}

		if (!begin_find_nextnext && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			next = tc;
			begin_find_nextnext = true;
			continue;
		}
	}

	// 从当前节点的上一个开始遍历
	for (node = grabc->link.prev; node != &clients; node = node->prev) {
		tc = wl_container_of(node, tc, link);

		if (begin_find_prevprev && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			prevprev = tc;
			break;
		}

		if (!begin_find_prevprev && VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			prev = tc;
			begin_find_prevprev = true;
			continue;
		}
	}

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;
		// 记录初始状态
		grabc->old_master_mfact_per = grabc->master_mfact_per;
		grabc->old_master_inner_per = grabc->master_inner_per;
		grabc->old_stack_inner_per = grabc->stack_inner_per;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		// 记录初始几何信息
		grabc->drag_begin_geom = grabc->geom;
	} else {
		// 计算相对于屏幕尺寸的比例变化
		if (isdrag) {

			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->old_master_mfact_per = grabc->master_mfact_per;
			grabc->old_master_inner_per = grabc->master_inner_per;
			grabc->old_stack_inner_per = grabc->stack_inner_per;
			grabc->drag_begin_geom = grabc->geom;
			grabc->cursor_in_upper_half = true;
			grabc->cursor_in_left_half = false;
		}

		if (grabc->ismaster) {
			delta_x = (float)(offsetx) * (grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_master_inner_per) /
					  grabc->drag_begin_geom.height;
		} else {
			delta_x = (float)(offsetx) * (1 - grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_stack_inner_per) /
					  grabc->drag_begin_geom.height;
		}
		bool moving_up;
		bool moving_down;

		if (!isdrag) {
			moving_up = offsety < 0 ? true : false;
			moving_down = offsety > 0 ? true : false;
		} else {
			moving_up = cursor->y < drag_begin_cursory;
			moving_down = cursor->y > drag_begin_cursory;
		}

		if (grabc->ismaster && !prev) {
			if (moving_up) {
				delta_y = -fabsf(delta_y);
			} else {
				delta_y = fabsf(delta_y);
			}
		} else if (grabc->ismaster && next && !next->ismaster) {
			if (moving_up) {
				delta_y = fabsf(delta_y);
			} else {
				delta_y = -fabsf(delta_y);
			}
		} else if (!grabc->ismaster && prev && prev->ismaster) {
			if (moving_up) {
				delta_y = -fabsf(delta_y);
			} else {
				delta_y = fabsf(delta_y);
			}
		} else if (!grabc->ismaster && !next) {
			if (moving_up) {
				delta_y = fabsf(delta_y);
			} else {
				delta_y = -fabsf(delta_y);
			}
		} else if (type == CENTER_TILE && !grabc->ismaster && !nextnext) {
			if (moving_up) {
				delta_y = fabsf(delta_y);
			} else {
				delta_y = -fabsf(delta_y);
			}
		} else if (type == CENTER_TILE && !grabc->ismaster && prevprev &&
				   prevprev->ismaster) {
			if (moving_up) {
				delta_y = -fabsf(delta_y);
			} else {
				delta_y = fabsf(delta_y);
			}
		} else if ((grabc->cursor_in_upper_half && moving_up) ||
				   (!grabc->cursor_in_upper_half && moving_down)) {
			// 光标在窗口上方且向上移动，或在窗口下方且向下移动 → 增加高度
			delta_y = fabsf(delta_y);
			delta_y = delta_y * 2;
		} else {
			// 其他情况 → 减小高度
			delta_y = -fabsf(delta_y);
			delta_y = delta_y * 2;
		}

		if (!grabc->ismaster && grabc->isleftstack && type == CENTER_TILE) {
			delta_x = delta_x * -1.0f;
		}

		if (grabc->ismaster && type == CENTER_TILE &&
			grabc->cursor_in_left_half) {
			delta_x = delta_x * -1.0f;
		}

		if (grabc->ismaster && type == CENTER_TILE) {
			delta_x = delta_x * 2;
		}

		if (type == RIGHT_TILE) {
			delta_x = delta_x * -1.0f;
		}

		// 直接设置新的比例，基于初始值 + 变化量
		float new_master_mfact_per = grabc->old_master_mfact_per + delta_x;
		float new_master_inner_per = grabc->old_master_inner_per + delta_y;
		float new_stack_inner_per = grabc->old_stack_inner_per + delta_y;

		// 应用限制，确保比例在合理范围内
		new_master_mfact_per = fmaxf(0.1f, fminf(0.9f, new_master_mfact_per));
		new_master_inner_per = fmaxf(0.1f, fminf(0.9f, new_master_inner_per));
		new_stack_inner_per = fmaxf(0.1f, fminf(0.9f, new_stack_inner_per));

		// 应用到所有平铺窗口
		wl_list_for_each(tc, &clients, link) {
			if (VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {

				if (!isdrag && tc != grabc && type != CENTER_TILE) {
					if (!tc->ismaster && new_stack_inner_per != 1.0f &&
						grabc->old_stack_inner_per != 1.0f)
						tc->stack_inner_per = (1 - new_stack_inner_per) /
											  (1 - grabc->old_stack_inner_per) *
											  tc->stack_inner_per;
					if (tc->ismaster && new_master_inner_per != 1.0f &&
						grabc->old_master_inner_per != 1.0f)
						tc->master_inner_per =
							(1.0f - new_master_inner_per) /
							(1.0f - grabc->old_master_inner_per) *
							tc->master_inner_per;
				}

				tc->master_mfact_per = new_master_mfact_per;
			}
		}

		grabc->master_inner_per = new_master_inner_per;
		grabc->stack_inner_per = new_stack_inner_per;

		if (!isdrag) {
			arrange(grabc->mon, false, false);
			return;
		}

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > drag_tile_refresh_interval) {
			arrange(grabc->mon, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_master_vertical(Client *grabc, bool isdrag, int32_t offsetx,
								 int32_t offsety, uint32_t time, int32_t type) {
	Client *tc = NULL;
	float delta_x, delta_y;
	Client *next = NULL;
	Client *prev = NULL;
	struct wl_list *node;

	// 从当前节点的下一个开始遍历
	for (node = grabc->link.next; node != &clients; node = node->next) {
		tc = wl_container_of(node, tc, link);

		if (VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			next = tc;
			break;
		}
	}

	// 从当前节点的上一个开始遍历
	for (node = grabc->link.prev; node != &clients; node = node->prev) {
		tc = wl_container_of(node, tc, link);

		if (VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
			prev = tc;
			break;
		}
	}

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;

		// 记录初始状态
		grabc->old_master_mfact_per = grabc->master_mfact_per;
		grabc->old_master_inner_per = grabc->master_inner_per;
		grabc->old_stack_inner_per = grabc->stack_inner_per;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		// 记录初始几何信息
		grabc->drag_begin_geom = grabc->geom;
	} else {
		// 计算相对于屏幕尺寸的比例变化
		// 计算相对于屏幕尺寸的比例变化
		if (isdrag) {

			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->old_master_mfact_per = grabc->master_mfact_per;
			grabc->old_master_inner_per = grabc->master_inner_per;
			grabc->old_stack_inner_per = grabc->stack_inner_per;
			grabc->drag_begin_geom = grabc->geom;
			grabc->cursor_in_upper_half = true;
			grabc->cursor_in_left_half = false;
		}

		if (grabc->ismaster) {
			// 垂直版本：左右移动调整高度比例，上下移动调整宽度比例
			delta_x = (float)(offsetx) * (grabc->old_master_inner_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.height;
		} else {
			delta_x = (float)(offsetx) * (grabc->old_stack_inner_per) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (1 - grabc->old_master_mfact_per) /
					  grabc->drag_begin_geom.height;
		}

		bool moving_left;
		bool moving_right;

		if (!isdrag) {
			moving_left = offsetx < 0 ? true : false;
			moving_right = offsetx > 0 ? true : false;
		} else {
			moving_left = cursor->x < drag_begin_cursorx;
			moving_right = cursor->x > drag_begin_cursorx;
		}

		// 调整主区域和栈区域的高度比例（垂直分割）
		if (grabc->ismaster && !prev) {
			if (moving_left) {
				delta_x = -fabsf(delta_x); // 向上移动减少主区域高度
			} else {
				delta_x = fabsf(delta_x); // 向下移动增加主区域高度
			}
		} else if (grabc->ismaster && next && !next->ismaster) {
			if (moving_left) {
				delta_x = fabsf(delta_x); // 向上移动增加主区域高度
			} else {
				delta_x = -fabsf(delta_x); // 向下移动减少主区域高度
			}
		} else if (!grabc->ismaster && prev && prev->ismaster) {
			if (moving_left) {
				delta_x = -fabsf(delta_x); // 向上移动减少栈区域高度
			} else {
				delta_x = fabsf(delta_x); // 向下移动增加栈区域高度
			}
		} else if (!grabc->ismaster && !next) {
			if (moving_left) {
				delta_x = fabsf(delta_x); // 向上移动增加栈区域高度
			} else {
				delta_x = -fabsf(delta_x); // 向下移动减少栈区域高度
			}
		} else if ((grabc->cursor_in_left_half && moving_left) ||
				   (!grabc->cursor_in_left_half && moving_right)) {
			// 光标在窗口左侧且向左移动，或在窗口右侧且向右移动 → 增加宽度
			delta_x = fabsf(delta_x);
			delta_x = delta_x * 2;
		} else {
			// 其他情况 → 减小宽度
			delta_x = -fabsf(delta_x);
			delta_x = delta_x * 2;
		}

		// 直接设置新的比例，基于初始值 + 变化量
		float new_master_mfact_per = grabc->old_master_mfact_per +
									 delta_y; // 垂直：delta_y调整主区域高度
		float new_master_inner_per = grabc->old_master_inner_per +
									 delta_x; // 垂直：delta_x调整主区域内部宽度
		float new_stack_inner_per = grabc->old_stack_inner_per +
									delta_x; // 垂直：delta_x调整栈区域内部宽度

		// 应用限制，确保比例在合理范围内
		new_master_mfact_per = fmaxf(0.1f, fminf(0.9f, new_master_mfact_per));
		new_master_inner_per = fmaxf(0.1f, fminf(0.9f, new_master_inner_per));
		new_stack_inner_per = fmaxf(0.1f, fminf(0.9f, new_stack_inner_per));

		// 应用到所有平铺窗口
		wl_list_for_each(tc, &clients, link) {
			if (VISIBLEON(tc, grabc->mon) && ISTILED(tc)) {
				if (!isdrag && tc != grabc && type != CENTER_TILE) {
					if (!tc->ismaster && new_stack_inner_per != 1.0f &&
						grabc->old_stack_inner_per != 1.0f)
						tc->stack_inner_per = (1 - new_stack_inner_per) /
											  (1 - grabc->old_stack_inner_per) *
											  tc->stack_inner_per;
					if (tc->ismaster && new_master_inner_per != 1.0f &&
						grabc->old_master_inner_per != 1.0f)
						tc->master_inner_per =
							(1.0f - new_master_inner_per) /
							(1.0f - grabc->old_master_inner_per) *
							tc->master_inner_per;
				}

				tc->master_mfact_per = new_master_mfact_per;
			}
		}

		grabc->master_inner_per = new_master_inner_per;
		grabc->stack_inner_per = new_stack_inner_per;

		if (!isdrag) {
			arrange(grabc->mon, false, false);
			return;
		}

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > drag_tile_refresh_interval) {
			arrange(grabc->mon, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_scroller(Client *grabc, bool isdrag, int32_t offsetx,
						  int32_t offsety, uint32_t time, bool isvertical) {
	Client *tc = NULL;
	float delta_x, delta_y;
	float new_scroller_proportion;
	float new_stack_proportion;
	Client *stack_head = get_scroll_stack_head(grabc);

	if (grabc && grabc->mon->visible_tiling_clients == 1 &&
		!scroller_ignore_proportion_single)
		return;

	if (!start_drag_window && isdrag) {
		drag_begin_cursorx = cursor->x;
		drag_begin_cursory = cursor->y;
		start_drag_window = true;

		// 记录初始状态
		stack_head->old_scroller_pproportion = stack_head->scroller_proportion;
		grabc->old_stack_proportion = grabc->stack_proportion;

		grabc->cursor_in_left_half =
			cursor->x < grabc->geom.x + grabc->geom.width / 2;
		grabc->cursor_in_upper_half =
			cursor->y < grabc->geom.y + grabc->geom.height / 2;
		// 记录初始几何信息
		grabc->drag_begin_geom = grabc->geom;
	} else {
		// 计算相对于屏幕尺寸的比例变化
		// 计算相对于屏幕尺寸的比例变化
		if (isdrag) {

			offsetx = cursor->x - drag_begin_cursorx;
			offsety = cursor->y - drag_begin_cursory;
		} else {
			grabc->old_master_mfact_per = grabc->master_mfact_per;
			grabc->old_master_inner_per = grabc->master_inner_per;
			grabc->old_stack_inner_per = grabc->stack_inner_per;
			grabc->drag_begin_geom = grabc->geom;
			stack_head->old_scroller_pproportion =
				stack_head->scroller_proportion;
			grabc->old_stack_proportion = grabc->stack_proportion;
			grabc->cursor_in_upper_half = false;
			grabc->cursor_in_left_half = false;
		}

		if (isvertical) {
			delta_y = (float)(offsety) *
					  (stack_head->old_scroller_pproportion) /
					  grabc->drag_begin_geom.height;
			delta_x = (float)(offsetx) * (grabc->old_stack_proportion) /
					  grabc->drag_begin_geom.width;
		} else {
			delta_x = (float)(offsetx) *
					  (stack_head->old_scroller_pproportion) /
					  grabc->drag_begin_geom.width;
			delta_y = (float)(offsety) * (grabc->old_stack_proportion) /
					  grabc->drag_begin_geom.height;
		}

		bool moving_up;
		bool moving_down;
		bool moving_left;
		bool moving_right;

		if (!isdrag) {
			moving_up = offsety < 0 ? true : false;
			moving_down = offsety > 0 ? true : false;
			moving_left = offsetx < 0 ? true : false;
			moving_right = offsetx > 0 ? true : false;
		} else {
			moving_up = cursor->y < drag_begin_cursory;
			moving_down = cursor->y > drag_begin_cursory;
			moving_left = cursor->x < drag_begin_cursorx;
			moving_right = cursor->x > drag_begin_cursorx;
		}

		if ((grabc->cursor_in_upper_half && moving_up) ||
			(!grabc->cursor_in_upper_half && moving_down)) {
			// 光标在窗口上方且向上移动，或在窗口下方且向下移动 → 增加高度
			delta_y = fabsf(delta_y);
		} else {
			// 其他情况 → 减小高度
			delta_y = -fabsf(delta_y);
		}

		if ((grabc->cursor_in_left_half && moving_left) ||
			(!grabc->cursor_in_left_half && moving_right)) {
			delta_x = fabsf(delta_x);
		} else {
			delta_x = -fabsf(delta_x);
		}

		if (isvertical) {
			if (!grabc->next_in_stack && grabc->prev_in_stack && !isdrag) {
				delta_x = delta_x * -1.0f;
			}
			if (!grabc->next_in_stack && grabc->prev_in_stack && isdrag) {
				if (moving_right) {
					delta_x = -fabsf(delta_x);
				} else {
					delta_x = fabsf(delta_x);
				}
			}
			if (!grabc->prev_in_stack && grabc->next_in_stack && isdrag) {
				if (moving_left) {
					delta_x = -fabsf(delta_x);
				} else {
					delta_x = fabsf(delta_x);
				}
			}

			if (isdrag) {
				if (moving_up) {
					delta_y = -fabsf(delta_y);
				} else {
					delta_y = fabsf(delta_y);
				}
			}

		} else {
			if (!grabc->next_in_stack && grabc->prev_in_stack && !isdrag) {
				delta_y = delta_y * -1.0f;
			}
			if (!grabc->next_in_stack && grabc->prev_in_stack && isdrag) {
				if (moving_down) {
					delta_y = -fabsf(delta_y);
				} else {
					delta_y = fabsf(delta_y);
				}
			}
			if (!grabc->prev_in_stack && grabc->next_in_stack && isdrag) {
				if (moving_up) {
					delta_y = -fabsf(delta_y);
				} else {
					delta_y = fabsf(delta_y);
				}
			}

			if (isdrag) {
				if (moving_left) {
					delta_x = -fabsf(delta_x);
				} else {
					delta_x = fabsf(delta_x);
				}
			}
		}

		// 直接设置新的比例，基于初始值 + 变化量
		if (isvertical) {
			new_scroller_proportion =
				stack_head->old_scroller_pproportion + delta_y;
			new_stack_proportion = grabc->old_stack_proportion + delta_x;

		} else {
			new_scroller_proportion =
				stack_head->old_scroller_pproportion + delta_x;
			new_stack_proportion = grabc->old_stack_proportion + delta_y;
		}

		// 应用限制，确保比例在合理范围内
		new_scroller_proportion =
			fmaxf(0.1f, fminf(1.0f, new_scroller_proportion));
		new_stack_proportion = fmaxf(0.1f, fminf(0.9f, new_stack_proportion));

		grabc->stack_proportion = new_stack_proportion;

		stack_head->scroller_proportion = new_scroller_proportion;

		wl_list_for_each(tc, &clients, link) {
			if (new_stack_proportion != 1.0f &&
				grabc->old_stack_proportion != 1.0f && tc != grabc &&
				ISTILED(tc) && get_scroll_stack_head(tc) == stack_head) {
				tc->stack_proportion = (1.0f - new_stack_proportion) /
									   (1.0f - grabc->old_stack_proportion) *
									   tc->stack_proportion;
			}
		}

		if (!isdrag) {
			arrange(grabc->mon, false, false);
			return;
		}

		if (last_apply_drap_time == 0 ||
			time - last_apply_drap_time > drag_tile_refresh_interval) {
			arrange(grabc->mon, false, false);
			last_apply_drap_time = time;
		}
	}
}

void resize_tile_client(Client *grabc, bool isdrag, int32_t offsetx,
						int32_t offsety, uint32_t time) {

	if (!grabc || grabc->isfullscreen || grabc->ismaximizescreen)
		return;

	if (grabc->mon->isoverview)
		return;

	const Layout *current_layout =
		grabc->mon->pertag->ltidxs[grabc->mon->pertag->curtag];
	if (current_layout->id == TILE || current_layout->id == DECK ||
		current_layout->id == CENTER_TILE || current_layout->id == RIGHT_TILE ||
		(current_layout->id == TGMIX && grabc->mon->visible_tiling_clients <= 3)

	) {
		resize_tile_master_horizontal(grabc, isdrag, offsetx, offsety, time,
									  current_layout->id);
	} else if (current_layout->id == VERTICAL_TILE ||
			   current_layout->id == VERTICAL_DECK) {
		resize_tile_master_vertical(grabc, isdrag, offsetx, offsety, time,
									current_layout->id);
	} else if (current_layout->id == SCROLLER) {
		resize_tile_scroller(grabc, isdrag, offsetx, offsety, time, false);
	} else if (current_layout->id == VERTICAL_SCROLLER) {
		resize_tile_scroller(grabc, isdrag, offsetx, offsety, time, true);
	}
}

/* If there are no calculation omissions,
these two functions will never be triggered.
Just in case to facilitate the final investigation*/

void check_size_per_valid(Client *c) {
	if (c->ismaster) {
		assert(c->master_inner_per > 0.0f && c->master_inner_per <= 1.0f);
	} else {
		assert(c->stack_inner_per > 0.0f && c->stack_inner_per <= 1.0f);
	}
}

void reset_size_per_mon(Monitor *m, int32_t tile_cilent_num,
						double total_left_stack_hight_percent,
						double total_right_stack_hight_percent,
						double total_stack_hight_percent,
						double total_master_inner_percent, int32_t master_num,
						int32_t stack_num) {
	Client *c = NULL;
	int32_t i = 0;
	uint32_t stack_index = 0;
	uint32_t nmasters = m->pertag->nmasters[m->pertag->curtag];

	if (m->pertag->ltidxs[m->pertag->curtag]->id != CENTER_TILE) {

		wl_list_for_each(c, &clients, link) {
			if (VISIBLEON(c, m) && ISTILED(c)) {

				if (total_master_inner_percent > 0.0 && i < nmasters) {
					c->ismaster = true;
					c->stack_inner_per = stack_num ? 1.0f / stack_num : 1.0f;
					c->master_inner_per =
						c->master_inner_per / total_master_inner_percent;
				} else {
					c->ismaster = false;
					c->master_inner_per =
						master_num > 0 ? 1.0f / master_num : 1.0f;
					c->stack_inner_per =
						total_stack_hight_percent
							? c->stack_inner_per / total_stack_hight_percent
							: 1.0f;
				}
				i++;

				check_size_per_valid(c);
			}
		}
	} else {
		wl_list_for_each(c, &clients, link) {
			if (VISIBLEON(c, m) && ISTILED(c)) {

				if (total_master_inner_percent > 0.0 && i < nmasters) {
					c->ismaster = true;
					if ((stack_index % 2) ^ (tile_cilent_num % 2 == 0)) {
						c->stack_inner_per =
							stack_num > 1 ? 1.0f / ((stack_num - 1) / 2.0f)
										  : 1.0f;
					} else {
						c->stack_inner_per =
							stack_num > 1 ? 2.0f / stack_num : 1.0f;
					}

					c->master_inner_per =
						c->master_inner_per / total_master_inner_percent;
				} else {
					stack_index = i - nmasters;

					c->ismaster = false;
					c->master_inner_per =
						master_num > 0 ? 1.0f / master_num : 1.0f;
					if ((stack_index % 2) ^ (tile_cilent_num % 2 == 0)) {
						c->stack_inner_per =
							total_right_stack_hight_percent
								? c->stack_inner_per /
									  total_right_stack_hight_percent
								: 1.0f;
					} else {
						c->stack_inner_per =
							total_left_stack_hight_percent
								? c->stack_inner_per /
									  total_left_stack_hight_percent
								: 1.0f;
					}
				}
				i++;

				check_size_per_valid(c);
			}
		}
	}
}

void pre_caculate_before_arrange(Monitor *m, bool want_animation,
								 bool from_view, bool only_caculate) {
	Client *c = NULL;
	double total_stack_inner_percent = 0;
	double total_master_inner_percent = 0;
	double total_right_stack_hight_percent = 0;
	double total_left_stack_hight_percent = 0;
	int32_t i = 0;
	int32_t nmasters = 0;
	int32_t stack_index = 0;
	int32_t master_num = 0;
	int32_t stack_num = 0;

	if (!m)
		return;

	if (!m->wlr_output->enabled)
		return;
	m->visible_clients = 0;
	m->visible_tiling_clients = 0;
	m->visible_scroll_tiling_clients = 0;

	wl_list_for_each(c, &clients, link) {

		if (!client_only_in_one_tag(c) || c->isglobal || c->isunglobal) {
			exit_scroller_stack(c);
		}

		if (from_view && (c->isglobal || c->isunglobal)) {
			set_size_per(m, c);
		}

		if (c->mon == m && (c->isglobal || c->isunglobal)) {
			c->tags = m->tagset[m->seltags];
		}

		if (from_view && m->sel == NULL && c->isglobal && VISIBLEON(c, m)) {
			focusclient(c, 1);
		}

		if (VISIBLEON(c, m)) {
			if (from_view && !client_only_in_one_tag(c)) {
				set_size_per(m, c);
			}

			if (!c->isunglobal)
				m->visible_clients++;

			if (ISTILED(c)) {
				m->visible_tiling_clients++;
			}

			if (ISSCROLLTILED(c) && !c->prev_in_stack) {
				m->visible_scroll_tiling_clients++;
			}
		}
	}

	nmasters = m->pertag->nmasters[m->pertag->curtag];

	wl_list_for_each(c, &clients, link) {
		if (c->iskilling)
			continue;

		if (c->mon == m) {
			if (VISIBLEON(c, m)) {
				if (ISTILED(c)) {

					if (i < nmasters) {
						master_num++;
						total_master_inner_percent += c->master_inner_per;
					} else {
						stack_num++;
						total_stack_inner_percent += c->stack_inner_per;
						stack_index = i - nmasters;
						if ((stack_index % 2) ^
							(m->visible_tiling_clients % 2 == 0)) {
							c->isleftstack = false;
							total_right_stack_hight_percent +=
								c->stack_inner_per;
						} else {
							c->isleftstack = true;
							total_left_stack_hight_percent +=
								c->stack_inner_per;
						}
					}

					i++;
				}

				if (!only_caculate)
					set_arrange_visible(m, c, want_animation);
			} else {
				if (!only_caculate)
					set_arrange_hidden(m, c, want_animation);
			}
		}

		if (!only_caculate && c->mon == m && c->ismaximizescreen &&
			!c->animation.tagouted && !c->animation.tagouting &&
			VISIBLEON(c, m)) {
			reset_maximizescreen_size(c);
		}
	}

	reset_size_per_mon(
		m, m->visible_tiling_clients, total_left_stack_hight_percent,
		total_right_stack_hight_percent, total_stack_inner_percent,
		total_master_inner_percent, master_num, stack_num);
}

void // 17
arrange(Monitor *m, bool want_animation, bool from_view) {

	pre_caculate_before_arrange(m, want_animation, from_view, false);

	if (m->isoverview) {
		overviewlayout.arrange(m);
	} else {
		m->pertag->ltidxs[m->pertag->curtag]->arrange(m);
	}

	if (!start_drag_window) {
		motionnotify(0, NULL, 0, 0, 0, 0);
		checkidleinhibitor(NULL);
	}

	printstatus();
}
