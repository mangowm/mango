#include "mango/config/internal.h"
#include "mango/config/parse.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mango/common/log.h"
#include "mango/manage/client.h"
#include <wlr/types/wlr_keyboard.h>

int compare_keybind_by_key_only(const void *a, const void *b) {
	const KeyBinding *ka = (const KeyBinding *)a;
	const KeyBinding *kb = (const KeyBinding *)b;

	if (ka->mod != kb->mod)
		return (ka->mod > kb->mod) ? 1 : -1;

	if (ka->keysymcode.type != kb->keysymcode.type)
		return (ka->keysymcode.type > kb->keysymcode.type) ? 1 : -1;

	if (ka->keysymcode.type == KEY_TYPE_SYM) {
		if (ka->keysymcode.keysym != kb->keysymcode.keysym)
			return (ka->keysymcode.keysym > kb->keysymcode.keysym) ? 1 : -1;
	} else {
		if (ka->keysymcode.keycode.keycode1 != kb->keysymcode.keycode.keycode1)
			return (ka->keysymcode.keycode.keycode1 >
					kb->keysymcode.keycode.keycode1)
					   ? 1
					   : -1;
	}
	return 0;
}

bool same_key(const KeyBinding *a, const KeyBinding *b) {
	return compare_keybind_by_key_only(a, b) == 0;
}

bool same_mousebind_key(const void *a, const void *b) {
	const MouseBinding *ma = (const MouseBinding *)a;
	const MouseBinding *mb = (const MouseBinding *)b;
	return ma->mod == mb->mod && ma->button == mb->button;
}

bool same_axisbind_key(const void *a, const void *b) {
	const AxisBinding *aa = (const AxisBinding *)a;
	const AxisBinding *ab = (const AxisBinding *)b;
	return aa->mod == ab->mod &&
		   (aa->dir == ALLDIR || ab->dir == ALLDIR || aa->dir == ab->dir);
}

bool same_switchbind_key(const void *a, const void *b) {
	const SwitchBinding *sa = (const SwitchBinding *)a;
	const SwitchBinding *sb = (const SwitchBinding *)b;
	return sa->fold == sb->fold;
}

bool same_gesturebind_key(const void *a, const void *b) {
	const GestureBinding *ga = (const GestureBinding *)a;
	const GestureBinding *gb = (const GestureBinding *)b;
	return ga->mod == gb->mod && ga->fingers_count == gb->fingers_count &&
		   (ga->motion == ALLDIR || gb->motion == ALLDIR ||
			ga->motion == gb->motion);
}

void get_mousebind_meta(const void *elem, BindingConflictMeta *meta) {
	const MouseBinding *b = (const MouseBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

void get_axisbind_meta(const void *elem, BindingConflictMeta *meta) {
	const AxisBinding *b = (const AxisBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

void get_switchbind_meta(const void *elem, BindingConflictMeta *meta) {
	const SwitchBinding *b = (const SwitchBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

void get_gesturebind_meta(const void *elem, BindingConflictMeta *meta) {
	const GestureBinding *b = (const GestureBinding *)elem;
	meta->mode = b->mode;
	meta->iscommonmode = b->iscommonmode;
	meta->file_index = b->file_index;
	meta->line_number = b->line_number;
}

bool check_mouse_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->mouse_bindings, config->mouse_bindings_count,
		sizeof(MouseBinding), same_mousebind_key, get_mousebind_meta,
		"mousebind");
}

bool check_axis_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->axis_bindings, config->axis_bindings_count, sizeof(AxisBinding),
		same_axisbind_key, get_axisbind_meta, "axisbind");
}

bool check_switch_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->switch_bindings, config->switch_bindings_count,
		sizeof(SwitchBinding), same_switchbind_key, get_switchbind_meta,
		"switchbind");
}

const char *mod_to_string(uint32_t mod) {
	static char buf[128];
	buf[0] = '\0';
	if (mod & WLR_MODIFIER_LOGO)
		strcat(buf, "Super+");
	if (mod & WLR_MODIFIER_CTRL)
		strcat(buf, "Ctrl+");
	if (mod & WLR_MODIFIER_ALT)
		strcat(buf, "Alt+");
	if (mod & WLR_MODIFIER_SHIFT)
		strcat(buf, "Shift+");
	if (mod & WLR_MODIFIER_MOD3)
		strcat(buf, "Hyper+");
	size_t len = strlen(buf);
	if (len > 0)
		buf[len - 1] = '\0';
	else
		strcpy(buf, "None");
	return buf;
}

bool check_key_binding_conflicts(Config *config) {
	int n = config->key_bindings_count;
	if (n < 2)
		return false;

	/* Copies user-defined bindings (line number > 0). */
	KeyBinding *binds = malloc(n * sizeof(KeyBinding));
	int count = 0;
	for (int i = 0; i < n; i++) {
		if (config->key_bindings[i].line_number > 0)
			binds[count++] = config->key_bindings[i];
	}
	if (count < 2) {
		free(binds);
		return false;
	}

	/* Sorts only by key so bindings with the same key are grouped together. */
	qsort(binds, count, sizeof(KeyBinding), compare_keybind_by_key_only);

	bool conflict_found = false;

	for (int i = 0; i < count;) {
		int j = i;
		/* Finds all bindings with the same key (range [i, j)). */
		while (j < count && same_key(&binds[i], &binds[j]))
			j++;

		/* Detects conflicts inside that range. */
		for (int a = i; a < j; a++) {
			for (int b = a + 1; b < j; b++) {
				bool same_mode = (strcmp(binds[a].mode, binds[b].mode) == 0);
				bool any_common =
					binds[a].iscommonmode || binds[b].iscommonmode;
				bool allow_conflict =
					binds[a].isallowconflict && binds[b].isallowconflict;

				if ((same_mode || any_common) && !allow_conflict) {

					const char *file_a =
						(binds[a].file_index >= 0)
							? config_file_path_at(binds[a].file_index)
							: "(built-in)";
					const char *file_b =
						(binds[b].file_index >= 0)
							? config_file_path_at(binds[b].file_index)
							: "(built-in)";

					conflict_found = true;
					mango_error(false, WLR_INFO,
								"Key binding conflict in keymode %s:\n"
								"  File \"%s\", line %d\n"
								"  File \"%s\", line %d\n",
								(any_common ? "common" : binds[a].mode), file_a,
								binds[a].line_number, file_b,
								binds[b].line_number);
				}
			}
		}
		i = j; /* Moves to the next key group. */
	}

	free(binds);
	return conflict_found;
}

bool check_simple_binding_conflicts(void *arr, size_t count, size_t elem_size,
									bool (*same_key)(const void *,
													 const void *),
									BindingMetaFunc get_meta,
									const char *kind) {
	bool conflict_found = false;

	for (size_t i = 0; i < count; i++) {
		for (size_t j = i + 1; j < count; j++) {
			if (!same_key((char *)arr + i * elem_size,
						  (char *)arr + j * elem_size))
				continue;

			BindingConflictMeta ma, mb;
			get_meta((char *)arr + i * elem_size, &ma);
			get_meta((char *)arr + j * elem_size, &mb);

			bool same_mode = (strcmp(ma.mode, mb.mode) == 0);
			bool any_common = ma.iscommonmode || mb.iscommonmode;
			if (same_mode || any_common) {

				const char *file_a = (ma.file_index >= 0)
										 ? config_file_path_at(ma.file_index)
										 : "(built-in)";
				const char *file_b = (mb.file_index >= 0)
										 ? config_file_path_at(mb.file_index)
										 : "(built-in)";

				conflict_found = true;
				mango_error(false, WLR_INFO,
							"%s conflict in keymode %s:\n"
							"  File \"%s\", line %d\n"
							"  File \"%s\", line %d\n",
							kind, (any_common ? "common" : ma.mode), file_a,
							ma.line_number, file_b, mb.line_number);
			}
		}
	}
	return conflict_found;
}

bool check_gesture_binding_conflicts(Config *config) {
	return check_simple_binding_conflicts(
		config->gesture_bindings, config->gesture_bindings_count,
		sizeof(GestureBinding), same_gesturebind_key, get_gesturebind_meta,
		"gesturebind");
}
