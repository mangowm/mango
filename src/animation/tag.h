void set_tagin_animation(Monitor *m, Client *c) {
	if (c->animation.running) {
		c->animainit_geom.x = c->animation.current.x;
		c->animainit_geom.y = c->animation.current.y;
		return;
	}

	if (m->pertag->curtag > m->pertag->prevtag) {

		c->animainit_geom.x = config.tag_animation_direction == VERTICAL
								  ? c->animation.current.x
								  : MAX(c->mon->m.x + c->mon->m.width,
										c->geom.x + c->mon->m.width);
		c->animainit_geom.y = config.tag_animation_direction == VERTICAL
								  ? MAX(c->mon->m.y + c->mon->m.height,
										c->geom.y + c->mon->m.height)
								  : c->animation.current.y;

	} else {

		c->animainit_geom.x =
			config.tag_animation_direction == VERTICAL
				? c->animation.current.x
				: MIN(m->m.x - c->geom.width, c->geom.x - c->mon->m.width);
		c->animainit_geom.y =
			config.tag_animation_direction == VERTICAL
				? MIN(m->m.y - c->geom.height, c->geom.y - c->mon->m.height)
				: c->animation.current.y;
	}
}

void set_arrange_visible(Monitor *m, Client *c, bool want_animation) {

	if (!c->is_clip_to_hide || !ISTILED(c) || !is_scroller_layout(c->mon)) {
		c->is_clip_to_hide = false;
		/* For XWayland clients that were hidden offscreen, just restore
		 * border/shadow — the node stays enabled and position is fixed
		 * by resize() below. For non-XWayland, re-enable normally. */
		if (c->is_xwayland_hidden && config.xwayland_render_unfocused) {
			xwayland_show_from_offscreen(c);
		} else {
			wlr_scene_node_set_enabled(&c->scene->node, true);
			wlr_scene_node_set_enabled(&c->scene_surface->node, true);
		}
	}
	client_set_suspended(c, false);

	if (!c->animation.tag_from_rule && want_animation &&
		m->pertag->prevtag != 0 && m->pertag->curtag != 0 &&
		config.animations) {
		c->animation.tagining = true;
		set_tagin_animation(m, c);
	} else {
		c->animainit_geom.x = c->animation.current.x;
		c->animainit_geom.y = c->animation.current.y;
	}

	c->animation.tag_from_rule = false;
	c->animation.tagouting = false;
	c->animation.tagouted = false;
	resize(c, c->geom, 0);
}

void set_tagout_animation(Monitor *m, Client *c) {
	if (m->pertag->curtag > m->pertag->prevtag) {
		c->pending = c->geom;
		c->pending.x =
			config.tag_animation_direction == VERTICAL
				? c->animation.current.x
				: MIN(c->mon->m.x - c->geom.width, c->geom.x - c->mon->m.width);
		c->pending.y = config.tag_animation_direction == VERTICAL
						   ? MIN(c->mon->m.y - c->geom.height,
								 c->geom.y - c->mon->m.height)
						   : c->animation.current.y;

		resize(c, c->geom, 0);
	} else {
		c->pending = c->geom;
		c->pending.x = config.tag_animation_direction == VERTICAL
						   ? c->animation.current.x
						   : MAX(c->mon->m.x + c->mon->m.width,
								 c->geom.x + c->mon->m.width);
		c->pending.y = config.tag_animation_direction == VERTICAL
						   ? MAX(c->mon->m.y + c->mon->m.height,
								 c->geom.y + c->mon->m.height)
						   : c->animation.current.y;
		resize(c, c->geom, 0);
	}
}

void set_arrange_hidden(Monitor *m, Client *c, bool want_animation) {
	if ((c->tags & (1 << (m->pertag->prevtag - 1))) &&
		m->pertag->prevtag != 0 && m->pertag->curtag != 0 &&
		config.animations) {
		c->animation.tagouting = true;
		c->animation.tagining = false;
		set_tagout_animation(m, c);
	} else {
		/* For XWayland clients, hide offscreen instead of disabling the
		 * scene node to keep frame callbacks flowing. */
		if (client_is_x11(c) && config.xwayland_render_unfocused) {
			xwayland_hide_offscreen(c);
		} else {
			wlr_scene_node_set_enabled(&c->scene->node, false);
			client_set_suspended(c, true);
		}
	}
}
