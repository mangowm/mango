#include "mango/config/parse.h"
#include "mango/config/preset.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mango/common/log.h"
#include "mango/common/server.h"
#include "mango/manage/client.h"

// Helper function to trim whitespace from start and end of a string
void trim_whitespace(char *str) {
	if (str == NULL || *str == '\0')
		return;

	// Trim leading space
	char *start = str;
	while (isspace((unsigned char)*start)) {
		start++;
	}

	// Trim trailing space
	char *end = str + strlen(str) - 1;
	while (end > start && isspace((unsigned char)*end)) {
		end--;
	}

	// Null-terminate the trimmed string
	*(end + 1) = '\0';

	// Move the trimmed part to the beginning if needed
	if (start != str) {
		memmove(str, start, end - start + 2); // +2 to include null terminator
	}
}

void strip_quotes(char *str) {
	if (str == NULL || *str == '\0')
		return;

	size_t len = strlen(str);
	if (len >= 2 && ((str[0] == '"' && str[len - 1] == '"') ||
					 (str[0] == '\'' && str[len - 1] == '\''))) {
		str[len - 1] = '\0';
		memmove(str, str + 1, len - 1);
		trim_whitespace(str);
	}
}

// remove comment, support double quote inside "#xxx" or '#xxx' not be treated
// as comment
void remove_comment(char *str) {
	if (str == NULL || *str == '\0')
		return;

	char quote_char = '\0';

	for (char *p = str; *p != '\0'; p++) {
		if (quote_char == '\0') {
			if (*p == '\'' || *p == '"') {
				quote_char = *p;
			} else if (*p == '#' && p > str &&
					   isspace((unsigned char)*(p - 1))) {
				*p = '\0';
				return;
			}
		} else {
			if (*p == quote_char) {
				quote_char = '\0';
			}
		}
	}
}

int32_t parse_double_array(const char *input, double *output,
						   int32_t max_count) {
	char *dup = strdup(input);
	char *token;
	int32_t count = 0;

	// Clear the whole array first.
	memset(output, 0, max_count * sizeof(double));

	token = strtok(dup, ",");
	while (token != NULL && count < max_count) {
		trim_whitespace(token);
		char *endptr;
		double val = strtod(token, &endptr);
		if (endptr == token || *endptr != '\0') {
			mango_error(false, WLR_ERROR,
						"Invalid number in array: \033[1m\033[31m%s\033[0m\n",
						token);
			free(dup);
			return -1;
		}
		if (val < 0.0) {
			mango_error(false, WLR_ERROR,
						"Invalid number in array (must be non-negative): "
						"\033[1m\033[31m%s\033[0m\n",
						token);
			free(dup);
			return -1;
		}
		output[count] = val; // Assign at the current count position.
		count++;			 // Then increment.
		token = strtok(NULL, ",");
	}

	free(dup);
	return count;
}

// Removes invisible characters from the string (including \r, \n, spaces).
char *sanitize_string(char *str) {
	// Removes leading invisible characters.
	while (*str != '\0' && !isprint((unsigned char)*str))
		str++;
	// Removes trailing invisible characters.
	char *end = str + strlen(str) - 1;
	while (end > str && !isprint((unsigned char)*end))
		end--;
	*(end + 1) = '\0';
	return str;
}

// Helper: checks whether a string starts with the given prefix
// (case-insensitive).
bool starts_with_ignore_case(const char *str, const char *prefix) {
	while (*prefix) {
		if (tolower(*str) != tolower(*prefix)) {
			return false;
		}
		str++;
		prefix++;
	}
	return true;
}

void report_config_line_error(const char *full_path, int line_number,
							  const char *line) {
	mango_error_untagged(WLR_INFO,
						 "\033[1;31m╰─\033[1;33m[Index]\033[0m "
						 "\033[1;36m%s\033[0m:\033[1;35m%d\033[0m\n"
						 "   \033[1;36m╰─\033[0;33m%s\033[0m\n\n",
						 full_path, line_number, line);
}

void convert_hex_to_rgba(float *color, uint32_t hex) {
	color[0] = ((hex >> 24) & 0xFF) / 255.0f;
	color[1] = ((hex >> 16) & 0xFF) / 255.0f;
	color[2] = ((hex >> 8) & 0xFF) / 255.0f;
	color[3] = (hex & 0xFF) / 255.0f;
}

uint32_t parse_num_type(char *str) {
	switch (str[0]) {
	case '-':
		return NUM_TYPE_MINUS;
	case '+':
		return NUM_TYPE_PLUS;
	default:
		return NUM_TYPE_DEFAULT;
	}
}

int32_t animation_type_from_string(const char *value) {
	if (!value || !value[0])
		return ANIM_TYPE_UNSET;
	if (strcmp(value, "none") == 0)
		return ANIM_TYPE_NONE;
	if (strcmp(value, "fade") == 0)
		return ANIM_TYPE_FADE;
	if (strcmp(value, "slide") == 0)
		return ANIM_TYPE_SLIDE;
	if (strcmp(value, "zoom") == 0)
		return ANIM_TYPE_ZOOM;
	return ANIM_TYPE_UNKNOWN;
}

int32_t parse_circle_direction(const char *str) {
	// Converts the input string to lowercase.

	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// Returns the matching enum value from the lowercased string.
	if (strcmp(lowerStr, "next") == 0) {
		return NEXT;
	} else {
		return PREV;
	}
}

int32_t parse_overcircle_direction(const char *str) {
	char lowerStr[16];
	int32_t i = 0;
	while (str[i] && i < 15) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	if (strcmp(lowerStr, "next") == 0)
		return OVERCIRCLE_NEXT;
	if (strcmp(lowerStr, "current_next") == 0)
		return OVERCIRCLE_CURRENT_NEXT;
	if (strcmp(lowerStr, "current_prev") == 0)
		return OVERCIRCLE_CURRENT_PREV;
	return OVERCIRCLE_PREV;
}

int32_t parse_direction(const char *str) {
	// Converts the input string to lowercase.
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// Returns the matching enum value from the lowercased string.
	if (strcmp(lowerStr, "up") == 0) {
		return UP;
	} else if (strcmp(lowerStr, "down") == 0) {
		return DOWN;
	} else if (strcmp(lowerStr, "left") == 0) {
		return LEFT;
	} else if (strcmp(lowerStr, "right") == 0) {
		return RIGHT;
	} else if (strcmp(lowerStr, "alldir") == 0) {
		return ALLDIR;
	} else {
		return UNDIR;
	}
}

int32_t parse_monitor_arg(const char *str) {
	int32_t dir = parse_direction(str);

	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	if (strcmp(lowerStr, "next") == 0) {
		return MON_NEXT;
	} else if (strcmp(lowerStr, "prev") == 0) {
		return MON_PREV;
	}
	return dir;
}

int64_t parse_color(const char *hex_str) {
	char *endptr;
	int64_t hex_num = strtol(hex_str, &endptr, 16);
	if (*endptr != '\0') {
		return -1;
	}
	return hex_num;
}

int32_t parse_force(const char *str) {
	// Converts the input string to lowercase.
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// Returns the matching enum value from the lowercased string.
	if (strcmp(lowerStr, "unforce") == 0) {
		return UNFORCE;
	} else if (strcmp(lowerStr, "force") == 0) {
		return FORCE;
	} else {
		return UNFORCE;
	}
}

int32_t parse_fold_state(const char *str) {
	// Converts the input string to lowercase.
	char lowerStr[10];
	int32_t i = 0;
	while (str[i] && i < 9) {
		lowerStr[i] = tolower(str[i]);
		i++;
	}
	lowerStr[i] = '\0';

	// Returns the matching enum value from the lowercased string.
	if (strcmp(lowerStr, "fold") == 0) {
		return FOLD;
	} else if (strcmp(lowerStr, "unfold") == 0) {
		return UNFOLD;
	} else {
		return INVALIDFOLD;
	}
}

uint32_t parse_tag_mask(char *str) {
	uint32_t mask = 0;
	char *token;
	char *arg_copy = strdup(str);

	if (arg_copy != NULL) {
		char *saveptr = NULL;
		token = strtok_r(arg_copy, "|", &saveptr);

		while (token != NULL) {
			trim_whitespace(token);
			int32_t num = atoi(token);
			if (num == 0 && strcmp(token, "0") == 0) {
				mask |= TAG0_MASK;
			} else if (num > 0 && num <= tag_num_MAX) {
				mask |= (1 << (num - 1));
			}
			token = strtok_r(NULL, "|", &saveptr);
		}

		free(arg_copy);
	}

	// tag0 and normal tags are exclusive; keep tag0 when mixed
	if ((mask & TAG0_MASK) && (mask & TAGMASK))
		mask = TAG0_MASK;

	uint32_t result = 0;

	if (mask) {
		result = mask;
	} else {
		result = atoi(str);
	}

	return result;
}
