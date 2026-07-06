#ifndef TAG_SCRUB_H
#define TAG_SCRUB_H
#include "tag_scrub_math.h"

static inline uint32_t tag_scrub_occupied_mask(Monitor *m) {
	uint32_t mask = 0;
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->mon == m && !c->iskilling)
			mask |= (c->tags & TAGMASK);
	}
	return mask;
}

static inline void tag_scrub_unstage(Monitor *m) {
	if (!m->scrub_incoming_tag) {
		m->scrub_dir = 0;
		m->scrub_rubberband = false;
		return;
	}
	Client *c;
	uint32_t inbit = 1u << (m->scrub_incoming_tag - 1);
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m || !(c->tags & inbit))
			continue;
		c->animation.tagining = false;
		c->animation.running = false;
		c->is_clip_to_hide = false;
		wlr_scene_node_set_enabled(&c->scene->node, false);
	}
	m->scrub_incoming_tag = 0;
	m->scrub_dir = 0;
	m->scrub_rubberband = false;
}

static inline void tag_scrub_stage(Monitor *m, int dir) {
	int curtag = (int)m->pertag->curtag;
	int ntags = LENGTH(tags);
	uint32_t mask = tag_scrub_occupied_mask(m);
	int target = tag_scrub_neighbor(curtag, dir, ntags, mask,
									m->scrub_have_client, config.tag_carousel);

	m->scrub_dir = (int8_t)dir;
	m->carousel_anim_dir = (int8_t)dir;
	if (target == 0) {
		m->scrub_rubberband = true;
		m->scrub_incoming_tag = 0;
		return;
	}
	m->scrub_rubberband = false;
	m->scrub_incoming_tag = (uint32_t)target;

	/* Mango lays out tags lazily: a tag you moved a window off of
	 * keeps stale window geometry until it's viewed. This is very annoying
	 * Here i try to predict the arrangement of the tag just so the tags don't
	 * appear miss-aligned in the scrub.
	 * */
	{
		uint32_t saved_curtag = m->pertag->curtag;
		uint32_t saved_tagset = m->tagset[m->seltags];
		m->pertag->curtag = (uint32_t)target;
		m->tagset[m->seltags] = (1u << (target - 1));
		pre_caculate_before_arrange(m, false, false, true);
		m->pertag->ltidxs[target]->arrange(m);
		m->pertag->curtag = saved_curtag;
		m->tagset[m->seltags] = saved_tagset;
		pre_caculate_before_arrange(m, false, false, true);
	}

	Client *c;
	uint32_t inbit = 1u << (target - 1);
	wl_list_for_each(c, &clients, link) {
		if (c->mon != m || !(c->tags & inbit) || !ISTILED(c))
			continue;
		if (c->isglobal || c->isunglobal)
			continue;
		wlr_scene_node_set_enabled(&c->scene->node, true);
		c->animation.tagining = true;
		c->animation.tagouting = false;
		c->animation.running = false;
		c->animation.current = c->geom;
		set_tagin_animation(m, c);
		c->animation.current = c->animainit_geom;
	}
}

static inline void tag_scrub_apply(Monitor *m, double progress) {
	if (progress < 0.0)
		progress = 0.0;
	if (progress > 1.0)
		progress = 1.0;
	m->scrub_progress = progress;

	double eff = m->scrub_rubberband ? progress * 0.2 : progress;
	Client *c;
	uint32_t inbit =
		m->scrub_incoming_tag ? (1u << (m->scrub_incoming_tag - 1)) : 0;
	uint32_t curbit = 1u << (m->pertag->curtag - 1);

	wl_list_for_each(c, &clients, link) {
		if (c->mon != m || !ISTILED(c))
			continue;
		struct wlr_box g;
		if (inbit && (c->tags & inbit) && !(c->isglobal || c->isunglobal)) {
			g.x = (int)(c->animainit_geom.x +
						(c->geom.x - c->animainit_geom.x) * eff);
			g.y = (int)(c->animainit_geom.y +
						(c->geom.y - c->animainit_geom.y) * eff);
			g.width = c->geom.width;
			g.height = c->geom.height;
			c->animation.current = g;
			wlr_scene_node_set_position(&c->scene->node, g.x, g.y);
			client_apply_clip(c, 1.0);
			c->is_clip_to_hide = false;
			wlr_scene_node_set_enabled(&c->scene->node, true);
			c->animation.running = false;
			c->need_output_flush = false;
		} else if (c->tags & curbit) {
			int off = (int)(-(int)m->scrub_dir *
							(config.tag_animation_direction == HORIZONTAL
								 ? m->m.width
								 : m->m.height) *
							eff);
			g = c->geom;
			if (config.tag_animation_direction == HORIZONTAL)
				g.x = c->geom.x + off;
			else
				g.y = c->geom.y + off;
			c->animation.tagouting = (eff > 0.0);
			c->animation.current = g;
			wlr_scene_node_set_position(&c->scene->node, g.x, g.y);
			client_apply_clip(c, 1.0);
			c->animation.running = false;
			c->need_output_flush = false;
		}
	}
	request_fresh_all_monitors();
}

static inline bool tag_scrub_active(Monitor *m) { return m && m->scrub_active; }

static inline bool tag_scrub_engaged(Monitor *m) {
	return m && m->scrub_active && m->scrub_axis_locked;
}

static inline void tag_scrub_abort(Monitor *m) {
	if (!m)
		return;
	m->scrub_active = false;
	m->scrub_axis_locked = false;
	m->scrub_dir = 0;
	m->scrub_progress = 0.0;
	m->scrub_accum = 0.0;
	m->scrub_velocity = 0.0;
	m->scrub_incoming_tag = 0;
	m->scrub_rubberband = false;
}

static inline bool tag_scrub_arm(Monitor *m, const GestureBinding *g) {
	if (!m || m->isoverview || m->pertag->curtag == 0)
		return false;
	m->scrub_active = true;
	m->scrub_axis = g->axis;
	m->scrub_have_client = g->have_client;
	m->scrub_dir = 0;
	m->scrub_incoming_tag = 0;
	m->scrub_rubberband = false;
	m->scrub_accum = 0.0;
	m->scrub_progress = 0.0;
	m->scrub_velocity = 0.0;
	m->scrub_last_delta = 0.0;
	m->scrub_last_time = 0;
	m->scrub_axis_locked = false;
	return true;
}

static inline void tag_scrub_feed(Monitor *m, double dx, double dy,
								  uint32_t time_msec) {
	if (!m->scrub_active)
		return;

	if (!m->scrub_axis_locked) {
		double along = (m->scrub_axis == HORIZONTAL) ? swipe_dx : swipe_dy;
		double perp = (m->scrub_axis == HORIZONTAL) ? swipe_dy : swipe_dx;
		double a = along < 0 ? -along : along;
		double p = perp < 0 ? -perp : perp;
		if (a + p < (double)config.gesture_axis_lock)
			return;
		if (p > a) {
			m->scrub_active = false;
			return;
		}
		m->scrub_axis_locked = true;
	}

	double delta = (m->scrub_axis == HORIZONTAL) ? dx : dy;
	if (config.trackpad_natural_scrolling)
		delta = -delta;
	m->scrub_accum += delta;
	double dim = (double)config.gesture_swipe_distance;
	uint32_t dt = (m->scrub_last_time && time_msec > m->scrub_last_time)
					  ? (time_msec - m->scrub_last_time)
					  : 1;
	double inst_v = (dim > 0) ? (delta / dim) / (double)dt : 0.0;
	m->scrub_velocity = m->scrub_velocity * 0.5 + inst_v * 0.5;
	m->scrub_last_delta = delta;
	m->scrub_last_time = time_msec;

	int dir = (m->scrub_accum > 0) ? +1 : (m->scrub_accum < 0 ? -1 : 0);
	double signed_p = tag_scrub_progress(m->scrub_accum, dim);
	double mag_p = signed_p < 0 ? -signed_p : signed_p;

	if (!config.animations) {
		m->scrub_dir = (int8_t)dir;
		uint32_t mask = tag_scrub_occupied_mask(m);
		int target =
			tag_scrub_neighbor((int)m->pertag->curtag, dir, LENGTH(tags), mask,
							   m->scrub_have_client, config.tag_carousel);
		m->scrub_rubberband = (dir != 0 && target == 0);
		m->scrub_incoming_tag = 0;
		tag_scrub_apply(m, mag_p);
		return;
	}

	if (dir != 0 && dir != m->scrub_dir) {
		if (m->scrub_incoming_tag || m->scrub_rubberband)
			tag_scrub_unstage(m);
		tag_scrub_stage(m, dir);
	}
	tag_scrub_apply(m, mag_p);
}

static inline void tag_scrub_release(Monitor *m, bool cancelled) {
	if (!m->scrub_active)
		return;

	double oriented_v = m->scrub_velocity * (double)m->scrub_dir;

	if (!config.animations) {
		bool commit = !cancelled && m->scrub_dir != 0 && !m->scrub_rubberband &&
					  gesture_scrub_should_commit(m->scrub_progress, oriented_v,
												  config.gesture_commit_ratio);
		int target = 0;
		if (commit) {
			uint32_t mask = tag_scrub_occupied_mask(m);
			target = tag_scrub_neighbor(
				(int)m->pertag->curtag, m->scrub_dir, LENGTH(tags), mask,
				m->scrub_have_client, config.tag_carousel);
		}
		if (target != 0)
			view_in_mon(&(Arg){.ui = 1u << (target - 1)}, false, m, true);
		else
			arrange(m, true, true);
	} else {
		bool commit = !cancelled && !m->scrub_rubberband &&
					  m->scrub_incoming_tag &&
					  gesture_scrub_should_commit(m->scrub_progress, oriented_v,
												  config.gesture_commit_ratio);
		if (commit) {
			uint32_t inbit = 1u << (m->scrub_incoming_tag - 1);
			uint32_t curbit = 1u << (m->pertag->curtag - 1);
			Client *c;
			wl_list_for_each(c, &clients, link) {
				if (c->mon == m && ISTILED(c) &&
					((c->tags & inbit) || (c->tags & curbit)))
					c->animation.running = true;
			}
			view_in_mon(&(Arg){.ui = inbit}, true, m, true);
		} else {
			uint32_t curbit = 1u << (m->pertag->curtag - 1);
			uint32_t inbit =
				m->scrub_incoming_tag ? (1u << (m->scrub_incoming_tag - 1)) : 0;
			uint32_t now = get_now_in_ms();
			Client *c;
			wl_list_for_each(c, &clients, link) {
				if (c->mon != m || !ISTILED(c))
					continue;
				bool is_incoming = inbit && (c->tags & inbit) &&
								   !(c->isglobal || c->isunglobal);
				if (!is_incoming && !(c->tags & curbit))
					continue;

				c->animation.initial = c->animation.current;
				c->current = is_incoming ? c->animainit_geom : c->geom;
				c->animation.tagining = false;
				c->animation.tagouting = is_incoming;
				c->animation.action = TAG;
				c->animation.duration = config.animation_duration_tag;
				c->animation.time_started = now;
				c->animation.running = true;
				c->need_output_flush = true;
			}
			request_fresh_all_monitors();
		}
	}

	m->scrub_active = false;
	m->scrub_axis_locked = false;
	m->scrub_dir = 0;
	m->scrub_progress = 0.0;
	m->scrub_accum = 0.0;
	m->scrub_velocity = 0.0;
	m->scrub_incoming_tag = 0;
	m->scrub_rubberband = false;
	m->carousel_anim_dir = 0;
}

#endif /* TAG_SCRUB_H */
