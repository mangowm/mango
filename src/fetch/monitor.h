Monitor *dirtomon(enum wlr_direction dir) {
	struct wlr_output *next;
	if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
		return selmon;
	if ((next = wlr_output_layout_adjacent_output(output_layout, 1 << dir,
												  selmon->wlr_output,
												  selmon->m.x, selmon->m.y)))
		return next->data;
	if ((next = wlr_output_layout_farthest_output(
			 output_layout,
			 dir ^ (WLR_DIRECTION_LEFT | WLR_DIRECTION_RIGHT |
					WLR_DIRECTION_UP | WLR_DIRECTION_DOWN),
			 selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	return selmon;
}

bool is_scroller_layout(Monitor *m) {

	if (m->pertag->ltidxs[m->pertag->curtag]->id == SCROLLER)
		return true;

	if (m->pertag->ltidxs[m->pertag->curtag]->id == VERTICAL_SCROLLER)
		return true;

	return false;
}

bool is_centertile_layout(Monitor *m) {

	if (m->pertag->ltidxs[m->pertag->curtag]->id == CENTER_TILE)
		return true;

	return false;
}

uint32_t get_tag_status(uint32_t tag, Monitor *m) {
	Client *c = NULL;
	uint32_t status = 0;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && c->tags & 1 << (tag - 1) & TAGMASK) {
			if (c->isurgent) {
				status = 2;
				break;
			}
			status = 1;
		}
	}
	return status;
}

uint32_t get_tags_first_tag_num(uint32_t source_tags) {
	uint32_t i, tag;
	tag = 0;

	if (!source_tags) {
		return selmon->pertag->curtag;
	}

	for (i = 0; !(tag & 1) && source_tags != 0 && i < LENGTH(tags); i++) {
		tag = source_tags >> i;
	}

	if (i == 1) {
		return 1;
	} else if (i >= LENGTH(tags)) {
		return LENGTH(tags);
	} else {
		return i;
	}
}

// 获取tags中最前面的tag的tagmask
uint32_t get_tags_first_tag(uint32_t source_tags) {
	uint32_t i, tag;
	tag = 0;

	if (!source_tags) {
		return selmon->pertag->curtag;
	}

	for (i = 0; !(tag & 1) && source_tags != 0 && i < LENGTH(tags); i++) {
		tag = source_tags >> i;
	}

	if (i == 1) {
		return 1;
	} else if (i > LENGTH(tags)) {
		return 1 << (LENGTH(tags) - 1);
	} else {
		return 1 << (i - 1);
	}
}

Monitor *xytomon(double x, double y) {
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

Monitor *get_monitor_nearest_to(int32_t lx, int32_t ly) {
	double closest_x, closest_y;
	wlr_output_layout_closest_point(output_layout, NULL, lx, ly, &closest_x,
									&closest_y);

	return output_from_wlr_output(
		wlr_output_layout_output_at(output_layout, closest_x, closest_y));
}

bool match_monitor_spec(char *spec, Monitor *m) {
	if (!spec || !m)
		return false;

	// if the spec does not contain a colon, treat it as a match on the monitor
	// name
	if (strchr(spec, ':') == NULL) {
		return regex_match(spec, m->wlr_output->name);
	}

	char *spec_copy = strdup(spec);
	if (!spec_copy)
		return false;

	char *name_rule = NULL;
	char *make_rule = NULL;
	char *model_rule = NULL;
	char *serial_rule = NULL;

	char *token = strtok(spec_copy, "&&");
	while (token) {
		char *colon = strchr(token, ':');
		if (colon) {
			*colon = '\0';
			char *key = token;
			char *value = colon + 1;

			if (strcmp(key, "name") == 0)
				name_rule = strdup(value);
			else if (strcmp(key, "make") == 0)
				make_rule = strdup(value);
			else if (strcmp(key, "model") == 0)
				model_rule = strdup(value);
			else if (strcmp(key, "serial") == 0)
				serial_rule = strdup(value);
		}
		token = strtok(NULL, "&&");
	}

	bool match = true;

	if (name_rule) {
		if (!regex_match(name_rule, m->wlr_output->name))
			match = false;
	}
	if (make_rule) {
		if (!m->wlr_output->make || strcmp(make_rule, m->wlr_output->make) != 0)
			match = false;
	}
	if (model_rule) {
		if (!m->wlr_output->model ||
			strcmp(model_rule, m->wlr_output->model) != 0)
			match = false;
	}
	if (serial_rule) {
		if (!m->wlr_output->serial ||
			strcmp(serial_rule, m->wlr_output->serial) != 0)
			match = false;
	}

	free(spec_copy);
	free(name_rule);
	free(make_rule);
	free(model_rule);
	free(serial_rule);

	return match;
}