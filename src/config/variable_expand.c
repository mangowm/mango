#include "mango/config/internal.h"
#include "mango/config/parse.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mango/common/log.h"

static ConfigVar *find_config_var(const char *name) {
	for (int32_t i = 0; i < config.vars_count; i++) {
		if (config.vars[i] && config.vars[i]->name &&
			strcmp(config.vars[i]->name, name) == 0) {
			return config.vars[i];
		}
	}
	return NULL;
}

bool is_valid_var_name(const char *name) {
	if (!name || *name == '\0')
		return false;
	if (!isalpha((unsigned char)*name) && *name != '_')
		return false;
	for (const char *p = name + 1; *p != '\0'; p++) {
		if (!isalnum((unsigned char)*p) && *p != '_')
			return false;
	}
	return true;
}

void set_config_var(const char *name, const char *value) {
	ConfigVar *var = find_config_var(name);
	if (var) {
		char *dup = strdup(value);
		if (!dup)
			return;
		free(var->value);
		var->value = dup;
		return;
	}

	ConfigVar *new_var = calloc(1, sizeof(ConfigVar));
	if (!new_var)
		return;
	new_var->name = strdup(name);
	new_var->value = strdup(value);
	if (!new_var->name || !new_var->value) {
		free(new_var->name);
		free(new_var->value);
		free(new_var);
		return;
	}

	ConfigVar **new_vars =
		realloc(config.vars, (config.vars_count + 1) * sizeof(ConfigVar *));
	if (!new_vars) {
		free(new_var->name);
		free(new_var->value);
		free(new_var);
		return;
	}
	config.vars = new_vars;
	config.vars[config.vars_count++] = new_var;
}

static bool var_buf_append(char **buf, size_t *cap, size_t *len, const char *s,
						   size_t n) {
	if (*len + n + 1 > *cap) {
		size_t new_cap = *cap * 2 + n + 1;
		char *new_buf = realloc(*buf, new_cap);
		if (!new_buf)
			return false;
		*buf = new_buf;
		*cap = new_cap;
	}
	memcpy(*buf + *len, s, n);
	*len += n;
	(*buf)[*len] = '\0';
	return true;
}

char *expand_config_variables(const char *value) {
	size_t cap = strlen(value) + 1;
	size_t len = 0;
	char *out = malloc(cap);
	if (!out)
		return NULL;
	out[0] = '\0';

	const char *p = value;
	while (*p != '\0') {
		const char *name = NULL;
		size_t name_len = 0;
		const char *next = NULL;

		if (p[0] == '$' && p[1] == '{') {
			const char *close = strchr(p + 2, '}');
			if (close) {
				name = p + 2;
				name_len = (size_t)(close - (p + 2));
				next = close + 1;
			}
		} else if (p[0] == '$' &&
				   (isalpha((unsigned char)p[1]) || p[1] == '_')) {
			const char *q = p + 1;
			while (isalnum((unsigned char)*q) || *q == '_')
				q++;
			name = p + 1;
			name_len = (size_t)(q - (p + 1));
			next = q;
		}

		if (name && name_len > 0 && name_len < 128) {
			char var_name[128];
			memcpy(var_name, name, name_len);
			var_name[name_len] = '\0';

			ConfigVar *var = find_config_var(var_name);
			if (var) {
				if (!var_buf_append(&out, &cap, &len, var->value,
									strlen(var->value))) {
					free(out);
					return NULL;
				}
				p = next;
				continue;
			}
			if (!var_buf_append(&out, &cap, &len, p, (size_t)(next - p))) {
				free(out);
				return NULL;
			}
			p = next;
			continue;
		}

		if (!var_buf_append(&out, &cap, &len, p, 1)) {
			free(out);
			return NULL;
		}
		p++;
	}
	return out;
}
