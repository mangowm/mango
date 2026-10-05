#include "mango/config/internal.h"
#include "mango/config/parse.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mango/common/log.h"

typedef struct {
	char name[256];
	char keymode[28];
	char saved_keymode[28];
	char bindtype[16];
	char keyword[24];
	char pairs[8192];
	size_t pairs_len;
	int line_number;
	int kind;
	bool active;
	bool skip;
} TomlSection;

enum {
	TOML_SECTION_NONE = 0,
	TOML_SECTION_GLOBAL,
	TOML_SECTION_ENV,
	TOML_SECTION_VAR,
	TOML_SECTION_BIND,
	TOML_SECTION_RULE,
};

static void remove_toml_comment(char *str) {
	char quote = '\0';
	for (char *p = str; *p != '\0'; p++) {
		if (quote == '"') {
			if (*p == '\\' && p[1] != '\0') {
				p++;
				continue;
			}
			if (*p == '"')
				quote = '\0';
		} else if (quote == '\'') {
			if (*p == '\'')
				quote = '\0';
		} else if (*p == '"' || *p == '\'') {
			quote = *p;
		} else if (*p == '#') {
			*p = '\0';
			return;
		}
	}
}

static void toml_scalar(const char *raw, char *out, size_t out_size,
						bool map_bool) {
	size_t n = 0;
	out[0] = '\0';

	while (*raw != '\0' && isspace((unsigned char)*raw))
		raw++;
	size_t len = strlen(raw);
	while (len > 0 && isspace((unsigned char)raw[len - 1]))
		len--;

	if (len >= 2 && raw[0] == '"' && raw[len - 1] == '"') {
		for (size_t i = 1; i + 1 < len && n + 1 < out_size; i++) {
			char c = raw[i];
			if (c == '\\' && i + 2 < len) {
				char esc = raw[++i];
				switch (esc) {
				case 'n':
					c = '\n';
					break;
				case 't':
					c = '\t';
					break;
				case 'r':
					c = '\r';
					break;
				case '"':
				case '\\':
					c = esc;
					break;
				default:
					if (n + 2 < out_size) {
						out[n++] = '\\';
						out[n++] = esc;
					}
					continue;
				}
			}
			out[n++] = c;
		}
		out[n] = '\0';
		return;
	}

	if (len >= 2 && raw[0] == '\'' && raw[len - 1] == '\'') {
		for (size_t i = 1; i + 1 < len && n + 1 < out_size; i++)
			out[n++] = raw[i];
		out[n] = '\0';
		return;
	}

	if (map_bool) {
		if (len == 4 && strncmp(raw, "true", 4) == 0) {
			snprintf(out, out_size, "1");
			return;
		}
		if (len == 5 && strncmp(raw, "false", 5) == 0) {
			snprintf(out, out_size, "0");
			return;
		}
	}

	for (size_t i = 0; i < len && n + 1 < out_size; i++)
		out[n++] = raw[i];
	out[n] = '\0';
}

static void toml_value_to_string(const char *raw, char *out, size_t out_size) {
	toml_scalar(raw, out, out_size, true);
}

static void toml_value_as_string(const char *raw, char *out, size_t out_size) {
	toml_scalar(raw, out, out_size, false);
}

static bool toml_foreach_array_element(const char *p,
									   bool (*fn)(void *, const char *),
									   void *ctx) {
	char elem[2048];
	size_t el = 0;
	int depth = 0;
	char quote = '\0';

	for (; *p != '\0'; p++) {
		char c = *p;
		if (quote != '\0') {
			if (quote == '"' && c == '\\' && p[1] != '\0') {
				if (el + 1 < sizeof(elem))
					elem[el++] = c;
				p++;
			}
			if (el + 1 < sizeof(elem))
				elem[el++] = *p;
			if (*p == quote)
				quote = '\0';
			continue;
		}

		if (c == '"' || c == '\'') {
			quote = c;
			if (el + 1 < sizeof(elem))
				elem[el++] = c;
		} else if (c == '[') {
			depth++;
			if (el + 1 < sizeof(elem))
				elem[el++] = c;
		} else if (c == ']') {
			if (depth == 0) {
				elem[el] = '\0';
				return fn(ctx, elem);
			}
			depth--;
			if (el + 1 < sizeof(elem))
				elem[el++] = c;
		} else if (c == ',' && depth == 0) {
			elem[el] = '\0';
			if (!fn(ctx, elem))
				return false;
			el = 0;
		} else if (el + 1 < sizeof(elem)) {
			elem[el++] = c;
		}
	}

	elem[el] = '\0';
	if (el > 0)
		return fn(ctx, elem);
	return true;
}

typedef struct {
	Config *config;
	char key[256];
	int line_number;
	char joined[4096];
	size_t joined_len;
} TomlApplyCtx;

static bool toml_apply_element(void *ctx, const char *elem) {
	TomlApplyCtx *apply = ctx;
	char value[2048];
	toml_value_to_string(elem, value, sizeof(value));
	if (value[0] == '\0')
		return true;
	return apply_option_expanded(apply->config, apply->key, value,
								 apply->line_number);
}

static bool toml_join_element(void *ctx, const char *elem) {
	TomlApplyCtx *apply = ctx;
	char value[2048];
	toml_value_to_string(elem, value, sizeof(value));
	if (value[0] == '\0')
		return true;

	size_t len = strlen(value);
	if (apply->joined_len + len + 2 >= sizeof(apply->joined))
		return false;
	if (apply->joined_len > 0)
		apply->joined[apply->joined_len++] = ',';
	memcpy(apply->joined + apply->joined_len, value, len + 1);
	apply->joined_len += len;
	return true;
}

static bool toml_key_joins_array(const char *key) {
	return strncmp(key, "animation_curve_", 16) == 0 ||
		   strcmp(key, "scroller_proportion_preset") == 0 ||
		   strcmp(key, "circle_layout") == 0;
}

static bool toml_apply_value(Config *config, const char *key, const char *raw,
							 int line_number) {
	const char *p = raw;
	while (*p != '\0' && isspace((unsigned char)*p))
		p++;

	TomlApplyCtx ctx;
	snprintf(ctx.key, sizeof(ctx.key), "%.255s", key);
	ctx.config = config;
	ctx.line_number = line_number;

	if (*p == '[') {
		if (toml_key_joins_array(key)) {
			ctx.joined[0] = '\0';
			ctx.joined_len = 0;
			if (!toml_foreach_array_element(p + 1, toml_join_element, &ctx))
				return false;
			return apply_option_expanded(config, ctx.key, ctx.joined,
										 line_number);
		}
		return toml_foreach_array_element(p + 1, toml_apply_element, &ctx);
	}

	char value[4096];
	toml_value_to_string(raw, value, sizeof(value));
	if (value[0] == '\0')
		return true;
	return apply_option_expanded(config, ctx.key, value, line_number);
}

static bool toml_is_rule_type(const char *name) {
	return strcmp(name, "monitor_rule") == 0 || strcmp(name, "tag_rule") == 0 ||
		   strcmp(name, "layer_rule") == 0 ||
		   strcmp(name, "window_rule") == 0 ||
		   strcmp(name, "window_rule_once") == 0 ||
		   strcmp(name, "device_rule") == 0;
}

static bool toml_section_add(TomlSection *section, const char *subkey,
							 const char *raw_value) {
	char value[2048];
	toml_value_to_string(raw_value, value, sizeof(value));
	if (value[0] == '\0')
		return true;

	char *expanded = expand_config_variables(value);
	const char *v = expanded ? expanded : value;
	size_t subkey_len = strlen(subkey);
	size_t value_len = strlen(v);

	if (section->pairs_len + subkey_len + value_len + 2 >=
		sizeof(section->pairs)) {
		free(expanded);
		mango_error(false, WLR_ERROR,
					"Toml section is too long: "
					"\033[1m\033[31m%s\033[0m\n",
					section->name);
		return false;
	}

	if (section->pairs_len > 0)
		section->pairs[section->pairs_len++] = ',';
	memcpy(section->pairs + section->pairs_len, subkey, subkey_len);
	section->pairs_len += subkey_len;
	section->pairs[section->pairs_len++] = ':';
	memcpy(section->pairs + section->pairs_len, v, value_len);
	section->pairs_len += value_len;
	section->pairs[section->pairs_len] = '\0';

	free(expanded);
	return true;
}

static int toml_bind_flag_char(const char *word) {
	if (strcmp(word, "sym") == 0)
		return 's';
	if (strcmp(word, "lock") == 0)
		return 'l';
	if (strcmp(word, "release") == 0)
		return 'r';
	if (strcmp(word, "pass") == 0)
		return 'p';
	if (strcmp(word, "conflict") == 0)
		return 'c';
	return 0;
}

static bool toml_section_begin(Config *config, TomlSection *section,
							   const char *name, int line_number,
							   const char *full_path, const char *stmt) {
	const char *base = NULL;
	const char *suffix = NULL;

	memset(section, 0, sizeof(*section));
	section->line_number = line_number;
	section->active = true;

	if (strcmp(name, "global") == 0 || strcmp(name, "rule") == 0) {
		section->kind = TOML_SECTION_GLOBAL;
		return true;
	}
	if (strcmp(name, "env") == 0) {
		section->kind = TOML_SECTION_ENV;
		return true;
	}
	if (strcmp(name, "var") == 0) {
		section->kind = TOML_SECTION_VAR;
		return true;
	}

	if (strncmp(name, "keybind", 7) == 0 &&
		(name[7] == '\0' || name[7] == '.')) {
		base = "bind";
		suffix = name[7] == '.' ? name + 8 : "";
	} else if (strncmp(name, "bind", 4) == 0 &&
			   (name[4] == '\0' || name[4] == '.')) {
		base = "bind";
		suffix = name[4] == '.' ? name + 5 : "";
	} else if (strncmp(name, "mousebind", 9) == 0 &&
			   (name[9] == '\0' || name[9] == '.')) {
		base = "mousebind";
		suffix = name[9] == '.' ? name + 10 : "";
	} else if (strncmp(name, "axisbind", 8) == 0 &&
			   (name[8] == '\0' || name[8] == '.')) {
		base = "axisbind";
		suffix = name[8] == '.' ? name + 9 : "";
	} else if (strncmp(name, "gesturebind", 11) == 0 &&
			   (name[11] == '\0' || name[11] == '.')) {
		base = "gesturebind";
		suffix = name[11] == '.' ? name + 12 : "";
	} else if (strncmp(name, "switchbind", 10) == 0 &&
			   (name[10] == '\0' || name[10] == '.')) {
		base = "switchbind";
		suffix = name[10] == '.' ? name + 11 : "";
	} else if (strncmp(name, "rule.", 5) == 0) {
		const char *type = name + 5;
		if (!toml_is_rule_type(type)) {
			mango_error(false, WLR_ERROR,
						"Unknown toml rule type: "
						"\033[1m\033[31m%s\033[0m\n",
						type);
			report_config_line_error(full_path, line_number, stmt);
			section->skip = true;
			return false;
		}
		section->kind = TOML_SECTION_RULE;
		snprintf(section->name, sizeof(section->name), "%.255s", type);
	} else if (toml_is_rule_type(name)) {
		section->kind = TOML_SECTION_RULE;
		snprintf(section->name, sizeof(section->name), "%.255s", name);
	} else {
		mango_error(false, WLR_ERROR,
					"Unknown toml section: \033[1m\033[31m%s\033[0m\n", name);
		report_config_line_error(full_path, line_number, stmt);
		section->skip = true;
		return false;
	}

	if (base == NULL)
		return true;

	section->kind = TOML_SECTION_BIND;
	snprintf(section->bindtype, sizeof(section->bindtype), "%.15s", base);
	snprintf(section->keymode, sizeof(section->keymode), "default");

	char letters[8];
	size_t letter_len = 0;
	letters[0] = '\0';

	char suffix_buf[128];
	snprintf(suffix_buf, sizeof(suffix_buf), "%.127s", suffix ? suffix : "");
	char *save = NULL;
	int depth = 0;
	for (char *tok = strtok_r(suffix_buf, ".", &save); tok != NULL;
		 tok = strtok_r(NULL, ".", &save)) {
		trim_whitespace(tok);
		if (*tok == '\0')
			continue;

		if (depth == 0) {
			snprintf(section->keymode, sizeof(section->keymode), "%.27s", tok);
		} else {
			int flag = toml_bind_flag_char(tok);
			if (flag == 0) {
				mango_error(false, WLR_ERROR,
							"Unknown bind flag: "
							"\033[1m\033[31m%s\033[0m\n",
							tok);
				report_config_line_error(full_path, line_number, stmt);
				section->skip = true;
				return false;
			}
			if (letter_len + 1 < sizeof(letters))
				letters[letter_len++] = (char)flag;
			letters[letter_len] = '\0';
		}
		depth++;
	}

	if (depth == 0) {
		mango_error(false, WLR_ERROR,
					"Bind keymode is required, e.g. "
					"\033[1m\033[31m[%s.default]\033[0m\n",
					base);
		report_config_line_error(full_path, line_number, stmt);
		section->skip = true;
		return false;
	}

	snprintf(section->keyword, sizeof(section->keyword), "%.15s%.7s", base,
			 letters);
	snprintf(section->saved_keymode, sizeof(section->saved_keymode), "%.27s",
			 config->keymode);
	snprintf(config->keymode, sizeof(config->keymode), "%.27s",
			 section->keymode);
	return true;
}

typedef struct {
	Config *config;
	char keyword[24];
	char prefix[1024];
	int line_number;
} TomlBindCtx;

static bool toml_bind_element(void *ctx, const char *elem) {
	TomlBindCtx *bind = ctx;
	char action[2048];
	toml_value_to_string(elem, action, sizeof(action));
	if (action[0] == '\0')
		return true;
	char buf[3200];
	snprintf(buf, sizeof(buf), "%.950s%.2047s", bind->prefix, action);
	return apply_option_expanded(bind->config, bind->keyword, buf,
								 bind->line_number);
}

static bool toml_bind_prefix(const char *base, const char *combo, char *prefix,
							 size_t size) {
	char tmp[256];
	snprintf(tmp, sizeof(tmp), "%.255s", combo);

	char *segs[16];
	int n = 0;
	char *save = NULL;
	for (char *tok = strtok_r(tmp, "+", &save); tok != NULL && n < 16;
		 tok = strtok_r(NULL, "+", &save)) {
		trim_whitespace(tok);
		if (*tok != '\0')
			segs[n++] = tok;
	}
	if (n == 0)
		return false;

	if (strcmp(base, "switchbind") == 0) {
		snprintf(prefix, size, "%.255s,", segs[0]);
		return true;
	}

	if (strcmp(base, "gesturebind") == 0) {
		if (n < 3)
			return false;
		snprintf(prefix, size, "%.255s,%.255s,%.255s,", segs[0], segs[1],
				 segs[2]);
		return true;
	}

	char mod[256];
	mod[0] = '\0';
	if (n >= 2) {
		size_t len = 0;
		for (int i = 0; i < n - 1; i++) {
			len += snprintf(mod + len, sizeof(mod) - len, "%s%s", i ? "+" : "",
							segs[i]);
		}
	} else {
		snprintf(mod, sizeof(mod), "none");
	}
	snprintf(prefix, size, "%.255s,%.255s,", mod, segs[n - 1]);
	return true;
}

static bool toml_section_leaf(Config *config, TomlSection *section,
							  const char *key, const char *raw_value,
							  int line_number) {
	if (section->kind == TOML_SECTION_ENV) {
		char value[2048];
		toml_value_as_string(raw_value, value, sizeof(value));
		if (value[0] == '\0')
			return true;
		char buf[2304];
		snprintf(buf, sizeof(buf), "%.255s,%.2047s", key, value);
		return apply_option_expanded(config, "env", buf, line_number);
	}
	if (section->kind == TOML_SECTION_VAR) {
		char value[2048];
		toml_value_as_string(raw_value, value, sizeof(value));
		char buf[2304];
		snprintf(buf, sizeof(buf), "%.255s,%.2047s", key, value);
		return apply_option_expanded(config, "var", buf, line_number);
	}
	if (section->kind == TOML_SECTION_RULE)
		return toml_section_add(section, key, raw_value);
	if (section->kind == TOML_SECTION_BIND) {
		TomlBindCtx bind;
		snprintf(bind.keyword, sizeof(bind.keyword), "%.23s", section->keyword);
		bind.config = config;
		bind.line_number = line_number;

		char *expanded_key = expand_config_variables(key);
		bool prefix_ok = toml_bind_prefix(section->bindtype,
										  expanded_key ? expanded_key : key,
										  bind.prefix, sizeof(bind.prefix));
		free(expanded_key);
		if (!prefix_ok)
			return false;

		const char *p = raw_value;
		while (*p != '\0' && isspace((unsigned char)*p))
			p++;
		if (*p == '[')
			return toml_foreach_array_element(p + 1, toml_bind_element, &bind);

		char action[2048];
		toml_value_to_string(raw_value, action, sizeof(action));
		if (action[0] == '\0')
			return true;
		char buf[3200];
		snprintf(buf, sizeof(buf), "%.950s%.2047s", bind.prefix, action);
		return apply_option_expanded(config, bind.keyword, buf, line_number);
	}
	return toml_apply_value(config, key, raw_value, line_number);
}

static bool toml_section_flush(Config *config, TomlSection *section,
							   const char *full_path) {
	if (!section->active)
		return true;

	bool ok = true;
	if (section->kind == TOML_SECTION_RULE && section->pairs_len > 0) {
		if (!parse_option(config, section->name, section->pairs,
						  section->line_number)) {
			ok = false;
			report_config_line_error(full_path, section->line_number,
									 section->name);
		}
	}

	if (section->kind == TOML_SECTION_BIND) {
		snprintf(config->keymode, sizeof(config->keymode), "%.27s",
				 section->saved_keymode);
	}

	section->active = false;
	section->pairs_len = 0;
	section->pairs[0] = '\0';
	return ok;
}

static bool toml_statement_complete(const char *stmt) {
	if (stmt[0] == '[') {
		size_t n = strlen(stmt);
		while (n > 0 && isspace((unsigned char)stmt[n - 1]))
			n--;
		return n > 0 && stmt[n - 1] == ']';
	}

	const char *eq = strchr(stmt, '=');
	if (eq == NULL)
		return true;

	int depth = 0;
	char quote = '\0';
	for (const char *p = eq + 1; *p != '\0'; p++) {
		char c = *p;
		if (quote != '\0') {
			if (quote == '"' && c == '\\' && p[1] != '\0') {
				p++;
				continue;
			}
			if (c == quote)
				quote = '\0';
			continue;
		}
		if (c == '"' || c == '\'')
			quote = c;
		else if (c == '[')
			depth++;
		else if (c == ']' && depth > 0)
			depth--;
	}
	return depth == 0 && quote == '\0';
}

static bool toml_process_statement(Config *config, const char *stmt,
								   int line_number, const char *full_path,
								   TomlSection *section) {
	if (stmt[0] == '[') {
		bool ok = toml_section_flush(config, section, full_path);

		const char *begin = stmt;
		while (*begin == '[')
			begin++;
		size_t len = strlen(begin);
		while (len > 0 && begin[len - 1] == ']')
			len--;

		char name[256];
		size_t n = len < sizeof(name) - 1 ? len : sizeof(name) - 1;
		memcpy(name, begin, n);
		name[n] = '\0';
		trim_whitespace(name);

		bool begun = toml_section_begin(config, section, name, line_number,
										full_path, stmt);
		return ok && begun;
	}

	if (section->skip)
		return true;

	const char *eq = strchr(stmt, '=');
	if (eq == NULL) {
		mango_error(false, WLR_ERROR,
					"Invalid toml line format: "
					"\033[1m\033[31m%s\033[0m\n",
					stmt);
		report_config_line_error(full_path, line_number, stmt);
		return false;
	}

	char key[256];
	size_t key_len = (size_t)(eq - stmt);
	if (key_len >= sizeof(key))
		key_len = sizeof(key) - 1;
	memcpy(key, stmt, key_len);
	key[key_len] = '\0';
	trim_whitespace(key);

	size_t key_quoted = strlen(key);
	if (key_quoted >= 2 && ((key[0] == '"' && key[key_quoted - 1] == '"') ||
							(key[0] == '\'' && key[key_quoted - 1] == '\''))) {
		memmove(key, key + 1, key_quoted - 2);
		key[key_quoted - 2] = '\0';
	}

	const char *value = eq + 1;
	while (*value != '\0' && isspace((unsigned char)*value))
		value++;

	if (section->active) {
		bool ok = toml_section_leaf(config, section, key, value, line_number);
		if (!ok && section->kind != TOML_SECTION_RULE)
			report_config_line_error(full_path, line_number, stmt);
		return ok;
	}

	if (!toml_apply_value(config, key, value, line_number)) {
		report_config_line_error(full_path, line_number, stmt);
		return false;
	}
	return true;
}

bool parse_toml_stream(Config *config, FILE *file, const char *full_path) {
	char raw[4096];
	char stmt[8192];
	size_t stmt_len = 0;
	int line_count = 0;
	int stmt_line = 0;
	bool parse_correct = true;
	TomlSection section = {0};

	stmt[0] = '\0';

	while (fgets(raw, sizeof(raw), file)) {
		line_count++;
		remove_toml_comment(raw);
		trim_whitespace(raw);
		if (raw[0] == '\0')
			continue;

		if (stmt_len == 0)
			stmt_line = line_count;

		size_t raw_len = strlen(raw);
		if (stmt_len + raw_len + 2 >= sizeof(stmt)) {
			mango_error(false, WLR_ERROR,
						"Config line too long in "
						"\033[1m\033[31m%s\033[0m\n",
						full_path);
			return false;
		}
		if (stmt_len > 0)
			stmt[stmt_len++] = ' ';
		memcpy(stmt + stmt_len, raw, raw_len + 1);
		stmt_len += raw_len;

		if (!toml_statement_complete(stmt))
			continue;

		if (!toml_process_statement(config, stmt, stmt_line, full_path,
									&section)) {
			parse_correct = false;
		}
		stmt_len = 0;
		stmt[0] = '\0';
	}

	if (stmt_len > 0 &&
		!toml_process_statement(config, stmt, stmt_line, full_path, &section)) {
		parse_correct = false;
	}

	if (!toml_section_flush(config, &section, full_path)) {
		parse_correct = false;
	}

	return parse_correct;
}
