#ifndef __CONFIG_INTERNAL_H__
#define __CONFIG_INTERNAL_H__ 1

#include "mango/config/parse.h"

#include <stdbool.h>
#include <stdio.h>

typedef enum {
	CONFIG_FORMAT_CONF,
	CONFIG_FORMAT_TOML,
} ConfigFileFormat;

int config_current_file_index(void);
const char *config_file_path_at(int index);

char *expand_config_variables(const char *value);
bool is_valid_var_name(const char *name);
void set_config_var(const char *name, const char *value);

bool apply_option_expanded(Config *config, char *key, char *value,
						   int line_number);
void report_config_line_error(const char *full_path, int line_number,
							  const char *line);

void create_config_keymap(void);

void set_binding_keymode(Config *config, char mode[28], bool *iscommonmode,
						 bool *isdefaultmode);

int32_t parse_overcircle_direction(const char *str);

bool parse_toml_stream(Config *config, FILE *file, const char *full_path);

#endif
