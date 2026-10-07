#include "mango/config/internal.h"
#include "mango/config/parse.h"

#include <ctype.h>
#include <libgen.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mango/animation/common.h"
#include "mango/common/input-event-codes.h"
#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/common/util.h"
#include "mango/config/error_nag.h"
#include "mango/config/error_store.h"
#include "mango/config/watcher.h"
#include "mango/dispatch/bind.h"
#include "mango/ext-protocol/hdr.h"
#include "mango/input/device.h"
#include "mango/input/keyboard.h"
#include "mango/input/pointer.h"
#include "mango/ipc/ipc.h"
#include "mango/layout/arrange.h"
#include "mango/layout/layout.h"
#include "mango/manage/client.h"
#include "mango/manage/layer.h"
#include "mango/manage/misc.h"
#include "mango/manage/monitor.h"
#include "mango/manage/tab.h"
#include "mango/switcher/switcher.h"
#include <unistd.h>
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>

void set_env_without_display() {
	for (int32_t i = 0; i < config.env_count; i++) {
		if (strcmp(config.env[i]->type, "DISPLAY") == 0) {
			continue; // Skip setting DISPLAY
		}
		setenv(config.env[i]->type, config.env[i]->value, 1);
	}
}

void set_env_display() {
	for (int32_t i = 0; i < config.env_count; i++) {
		if (strcmp(config.env[i]->type, "DISPLAY") == 0) {
			setenv("DISPLAY", config.env[i]->value, 1);
		}
	}
}

void run_exec() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_count; i++) {
		arg.v = config.exec[i];
		spawn_shell(&arg);
	}
}

void run_exec_once() {
	Arg arg;

	for (int32_t i = 0; i < config.exec_once_count; i++) {
		arg.v = config.exec_once[i];
		spawn_shell(&arg);
	}
}

static void apply_explicit_xcursor_env(void) {
	for (int32_t i = 0; i < config.env_count; i++) {
		if (config.env[i]->type &&
			(strcmp(config.env[i]->type, "XCURSOR_SIZE") == 0 ||
			 strcmp(config.env[i]->type, "XCURSOR_THEME") == 0)) {
			setenv(config.env[i]->type, config.env[i]->value, 1);
		}
	}
}

void set_xcursor_env() {
	if (config.cursor_size > 0) {
		char size_str[16];
		snprintf(size_str, sizeof(size_str), "%d", config.cursor_size);
		setenv("XCURSOR_SIZE", size_str, 1);
	} else {
		setenv("XCURSOR_SIZE", "24", 1);
	}

	if (config.cursor_theme) {
		setenv("XCURSOR_THEME", config.cursor_theme, 1);
	}

	/* Explicit env=XCURSOR_SIZE/XCURSOR_THEME entries take precedence over
	 * the values derived from cursor_size/cursor_theme above. */
	apply_explicit_xcursor_env();
}

void reapply_rootbg(void) {
	wlr_scene_rect_set_color(server.root_bg, config.rootcolor);
}

void reapply_property(void) {
	Client *c = NULL;

	// reset border width when config change
	wl_list_for_each(c, &server.clients, link) {
		if (c && !c->iskilling) {
			if (!c->no_border && !c->isfullscreen) {
				c->bw = config.borderpx;
			}
			client_set_group_config(c);
			client_apply_tab_bar_config(c);
		}
	}
}

void reapply_pointer(void) {
	InputDevice *id;
	struct libinput_device *device;
	wl_list_for_each(id, &server.input_devices, link) {

		if (id->wlr_device->type != WLR_INPUT_DEVICE_POINTER) {
			continue;
		}

		device = id->libinput_device;
		if (wlr_input_device_is_libinput(id->wlr_device) && device) {
			configure_pointer(id->wlr_device, device);
		}
	}
}

void reapply_tagrule(void) {
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (!m->wlr_output->enabled) {
			continue;
		}
		parse_tagrule(m);
	}
}

void reset_option(void) {
	init_baked_points();
	pointer_cursor_activity();
	reset_keyboard_layout();
	set_env_without_display();
	set_env_display();
	run_exec();

	reapply_cursor_style();
	reapply_property();
	reapply_rootbg();
	reapply_keyboard();
	reapply_pointer();
	reapply_master();

	reapply_tagrule();
	reapply_monitor_rules();

	/* A global reload must re-apply every monitor, not just the selected one:
	 * layout-affecting options such as always_show_group_bar reserve or release
	 * space on all outputs. */
	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		if (m->wlr_output->enabled)
			arrange(m, false, false);
	}
}

void reapply_monitor_rules(void) {
	Monitor *m = NULL;
	int32_t ji;

	wl_list_for_each(m, &server.monitors, link) {
		ConfigMonitorRule *mr = NULL;
		bool was_enabled = m->wlr_output->enabled;

		/* Match disabled outputs too, so a rule can re-enable them. */
		for (ji = config.monitor_rules_count - 1; ji >= 0; ji--) {
			if (monitor_matches_rule(m, &config.monitor_rules[ji])) {
				mr = &config.monitor_rules[ji];
				break;
			}
		}

		if (mr)
			apply_rule_to_state(m, mr, &m->pending);
		else
			/* No rule matches: same default as a new output, enabled. */
			wlr_output_state_set_enabled(&m->pending, true);

		/* Enable the output first so it can safely enter the layout:
		 * adding a still-disabled output re-enters
		 * handle_output_layout_change and crashes. */
		if (m->pending.enabled && !m->wlr_output->enabled &&
			!mango_scene_output_commit(m->scene_output, &m->pending))
			continue;

		if (mr && m->wlr_output->enabled) {
			if (mr->x == INT32_MAX && mr->y == INT32_MAX) {
				if (!was_enabled)
					wlr_output_layout_add_auto(server.output_layout,
											   m->wlr_output);
			} else {
				int32_t mx = mr->x == INT32_MAX ? m->m.x : mr->x;
				int32_t my = mr->y == INT32_MAX ? m->m.y : mr->y;
				wlr_output_layout_add(server.output_layout, m->wlr_output, mx,
									  my);
			}
		}

		/* Image descriptions only take effect in a scene commit that
		 * carries a buffer, so HDR is applied after the output is in the
		 * layout. */
		bool was_hdr = m->is_hdr_enabling;
		if (m->hdr_enable) {
			output_state_setup_hdr(m, false, &m->pending);
		} else {
			output_enable_hdr(m, &m->pending, false, false);
		}

		if (m->is_hdr_enabling != was_hdr) {
			m->pending.allow_reconfiguration = true;
			if (m->scene_output)
				wlr_damage_ring_add_whole(&m->scene_output->damage_ring);
		}

		if (!(mango_scene_output_commit(m->scene_output, &m->pending))) {
			if (m->hdr_enable) {
				output_state_setup_hdr(m, true, &m->pending);
			}
		}

		/* After scale/mode changes, force one frame so wl_output events are
		 * sent. */
		wlr_output_schedule_frame(m->wlr_output);
		wlr_output_effective_resolution(m->wlr_output, &m->m.width,
										&m->m.height);
	}

	handle_output_layout_change(NULL, NULL);
}

void reapply_cursor_style(void) {
	if (server.hide_cursor_source) {
		wl_event_source_timer_update(server.hide_cursor_source, 0);
		wl_event_source_remove(server.hide_cursor_source);
		server.hide_cursor_source = NULL;
	}

	wlr_cursor_unset_image(server.cursor);

	wlr_cursor_set_surface(server.cursor, NULL, 0, 0);

	if (server.cursor_manager) {
		wlr_xcursor_manager_destroy(server.cursor_manager);
		server.cursor_manager = NULL;
	}

	set_xcursor_env();

	server.cursor_manager =
		wlr_xcursor_manager_create(config.cursor_theme, config.cursor_size);

	Monitor *m = NULL;
	wl_list_for_each(m, &server.monitors, link) {
		wlr_xcursor_manager_load(server.cursor_manager, m->wlr_output->scale);
	}

	wlr_cursor_set_xcursor(server.cursor, server.cursor_manager, "left_ptr");

	server.hide_cursor_source =
		wl_event_loop_add_timer(wl_display_get_event_loop(server.display),
								pointer_hide_cursor, server.cursor);
	if (server.cursor_hidden) {
		wlr_cursor_unset_image(server.cursor);
	} else {
		wl_event_source_timer_update(server.hide_cursor_source,
									 config.cursor_hide_timeout * 1000);
	}
}

void reapply_keyboard(void) {
	InputDevice *id;
	KeyboardGroup *g;
	ConfigDeviceRule *rule;
	bool want_standalone;

	wlr_keyboard_set_repeat_info(&server.keyboard_group->wlr_group->keyboard,
								 config.repeat_rate, config.repeat_delay);
	wl_list_for_each(id, &server.input_devices, link) {
		if (id->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}

		rule = find_device_rule(id->wlr_device);
		want_standalone = rule && device_rule_has_keyboard_settings(rule);

		if (want_standalone && !id->standalone) {
			/* Rule matched: split it out as a standalone keyboard. */
			struct wlr_keyboard *kb = (struct wlr_keyboard *)id->device_data;
			if (!kb)
				continue;
			wlr_keyboard_group_remove_keyboard(server.keyboard_group->wlr_group,
											   kb);
			id->standalone = true;
			create_standalone_keyboard(id, kb, rule);
		} else if (!want_standalone && id->standalone) {
			/* Rule no longer matches: merge back into the default keyboard
			 * group. */
			g = (KeyboardGroup *)id->device_data;
			struct wlr_keyboard *kb = g ? g->keyboard : NULL;
			if (g)
				handle_standalone_keyboard_destroy(&g->destroy, NULL);
			id->standalone = false;
			id->device_data = kb;
			if (kb) {
				wlr_keyboard_set_keymap(
					kb, server.keyboard_group->keyboard->keymap);
				wlr_keyboard_notify_modifiers(kb, 0, 0, server.locked_modifiers,
											  0);
				wlr_keyboard_group_add_keyboard(
					server.keyboard_group->wlr_group, kb);
				wlr_keyboard_set_repeat_info(kb, config.repeat_rate,
											 config.repeat_delay);
			}
		} else if (want_standalone) {
			g = (KeyboardGroup *)id->device_data;
			if (g)
				standalone_keyboard_apply_config(g, rule);
		} else {
			wlr_keyboard_set_repeat_info((struct wlr_keyboard *)id->device_data,
										 config.repeat_rate,
										 config.repeat_delay);
		}
	}
}

void reapply_master(void) {

	int32_t i;
	Monitor *m = NULL;
	for (i = 0; i < PERTAG_SLOTS; i++) {
		wl_list_for_each(m, &server.monitors, link) {
			if (!m->wlr_output->enabled) {
				continue;
			}
			m->pertag->nmasters[i] = config.default_nmaster;
			m->pertag->mfacts[i] = config.default_mfact;
			m->gappih = config.gappih;
			m->gappiv = config.gappiv;
			m->gappoh = config.gappoh;
			m->gappov = config.gappov;
			m->special_gappih = config.special_gappih;
			m->special_gappiv = config.special_gappiv;
			m->special_gappoh = config.special_gappoh;
			m->special_gappov = config.special_gappov;
		}
	}
}

// Reset a pertag slot to defaults.
void tag_slot_set_defaults(Monitor *m, uint32_t tag) {
	m->pertag->nmasters[tag] = config.default_nmaster;
	m->pertag->mfacts[tag] = config.default_mfact;
	m->pertag->config_ltidxs[tag] = &layouts[0];
	m->pertag->scroller_default_proportion[tag] =
		config.scroller_default_proportion;
	m->pertag->scroller_default_proportion_single[tag] =
		config.scroller_default_proportion_single;
	m->pertag->scroller_ignore_proportion_single[tag] =
		config.scroller_ignore_proportion_single;
}

// Does this tag rule match the monitor?
bool tag_rule_matches_monitor(const ConfigTagRule *tr, Monitor *m) {
	if (tr->monitor_name != NULL &&
		!regex_match(tr->monitor_name, m->wlr_output->name))
		return false;
	if (tr->monitor_make != NULL &&
		(m->wlr_output->make == NULL ||
		 strcmp(tr->monitor_make, m->wlr_output->make) != 0))
		return false;
	if (tr->monitor_model != NULL &&
		(m->wlr_output->model == NULL ||
		 strcmp(tr->monitor_model, m->wlr_output->model) != 0))
		return false;
	if (tr->monitor_serial != NULL &&
		(m->wlr_output->serial == NULL ||
		 strcmp(tr->monitor_serial, m->wlr_output->serial) != 0))
		return false;
	return true;
}

// Apply one tag rule to a slot (caller checks coverage).
void tag_rule_apply_to_slot(Monitor *m, const ConfigTagRule *tr, uint32_t tag) {
	int32_t jk;

	for (jk = 0; jk < LENGTH(layouts); jk++) {
		if (tr->layout_name && strcmp(layouts[jk].name, tr->layout_name) == 0)
			m->pertag->config_ltidxs[tag] = &layouts[jk];
	}

	if (tr->no_hide >= 0)
		m->pertag->no_hide[tag] = tr->no_hide;
	if (tr->nmaster >= 1)
		m->pertag->nmasters[tag] = tr->nmaster;
	if (tr->mfact > 0.0f)
		m->pertag->mfacts[tag] = tr->mfact;
	if (tr->no_render_border >= 0)
		m->pertag->no_render_border[tag] = tr->no_render_border;
	if (tr->open_as_floating >= 0)
		m->pertag->open_as_floating[tag] = tr->open_as_floating;
	if (tr->scroller_default_proportion > 0.0f)
		m->pertag->scroller_default_proportion[tag] =
			tr->scroller_default_proportion;
	if (tr->scroller_default_proportion_single > 0.0f)
		m->pertag->scroller_default_proportion_single[tag] =
			tr->scroller_default_proportion_single;
	if (tr->scroller_ignore_proportion_single >= 0)
		m->pertag->scroller_ignore_proportion_single[tag] =
			tr->scroller_ignore_proportion_single;
}

void parse_tagrule(Monitor *m) {
	int32_t i;
	Client *c = NULL;
	const Layout *prev_config_ltidxs[PERTAG_SLOTS];

	for (i = 0; i < PERTAG_SLOTS; i++)
		prev_config_ltidxs[i] = m->pertag->config_ltidxs[i];

	// Set defaults for every tag.
	for (i = 0; i <= config.tag_num; i++)
		tag_slot_set_defaults(m, i);

	for (i = 0; i < config.tag_rules_count; i++) {
		const ConfigTagRule *tr = &config.tag_rules[i];

		if (tag_rule_matches_monitor(tr, m) &&
			(tr->id_wildcard || tr->id <= config.tag_num)) {
			int32_t tag_id_start = tr->id_wildcard ? 0 : tr->id;
			int32_t tag_id_end = tr->id_wildcard ? config.tag_num : tr->id;
			int32_t ti;

			for (ti = tag_id_start; ti <= tag_id_end; ti++)
				tag_rule_apply_to_slot(m, tr, ti);
		}
	}

	for (i = 0; i <= config.tag_num; i++) {
		if (prev_config_ltidxs[i] != m->pertag->config_ltidxs[i])
			m->pertag->ltidxs[i] = m->pertag->config_ltidxs[i];
	}

	for (i = 1; i <= config.tag_num; i++) {
		wl_list_for_each(c, &server.clients, link) {
			if ((c->tags & (1 << (i - 1)) & TAGMASK) && ISTILED(c)) {
				if (m->pertag->mfacts[i] > 0.0f)
					c->master_mfact_per = m->pertag->mfacts[i];
			}
		}
	}
}

void reset_tag(int old_tag_num) {
	if (config.tag_num != old_tag_num) {
		uint32_t last_tag_mask = (uint32_t)1 << (config.tag_num - 1);
		Client *c = NULL;
		Monitor *m = NULL;

		wl_list_for_each(c, &server.clients, link) {
			if (c->tags & ~server.tagmask) {
				c->tags = last_tag_mask;
				c->oldtags = last_tag_mask;
			}
		}

		wl_list_for_each(m, &server.monitors, link) {
			if (!m->wlr_output->enabled)
				continue;
			m->tagset[0] &= server.tagmask;
			m->tagset[1] &= server.tagmask;
			if (m->tagset[m->seltags] == 0)
				m->tagset[m->seltags] = last_tag_mask;
			if (m->pertag->curtag > (uint32_t)config.tag_num)
				m->pertag->curtag = config.tag_num;
			if (m->pertag->prevtag > (uint32_t)config.tag_num)
				m->pertag->prevtag = config.tag_num;
			sync_workspaces_to_tag_num(m);
		}
	}
}

int32_t reload_config(const Arg *arg) {
	int old_tag_num = config.tag_num;
	parse_config();
	reset_tag(old_tag_num);
	reset_option();
	update_seat_capabilities();
	apply_primary_selection();
	printstatus(IPC_WATCH_ARRANGGE);
	config_watcher_update();
	return 1;
}
