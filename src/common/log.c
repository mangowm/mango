#define _GNU_SOURCE
#include "mango/common/log.h"
#include "mango/common/util.h"
#include "mango/config/error_store.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void mango_log_init(enum wlr_log_importance verbosity,
					wlr_log_func_t callback) {
	wlr_log_init(verbosity, callback);
}

static const char *mango_tag_color(enum wlr_log_importance verbosity) {
	switch (verbosity) {
	case WLR_ERROR:
		return "\033[1m\033[31m[ERROR]:\033[0m ";
	case WLR_INFO:
		return "\033[1m\033[33m[WARN]:\033[0m ";
	case WLR_DEBUG:
		return "\033[1;32m[DEBUG]:\033[0m ";
	default:
		return "";
	}
}

static const char *mango_tag_plain(enum wlr_log_importance verbosity) {
	switch (verbosity) {
	case WLR_ERROR:
		return "[ERROR]: ";
	case WLR_INFO:
		return "[WARN]: ";
	case WLR_DEBUG:
		return "[DEBUG]: ";
	default:
		return "";
	}
}

static void mango_emit(bool log, bool tagged, enum wlr_log_importance verbosity,
					   const char *file, int line, const char *fmt,
					   va_list args) {
	if (!log) {
		bool capture = config_error_store_active();
		bool printable = verbosity == WLR_ERROR || verbosity == WLR_INFO ||
						 verbosity == WLR_DEBUG;
		const char *color_tag = tagged ? mango_tag_color(verbosity) : "";

		va_list copy;
		va_copy(copy, args);

		if (printable) {
			if (color_tag[0] != '\0')
				fputs(color_tag, stderr);
			vfprintf(stderr, fmt, args);
		}

		if (capture) {
			char buf[4096];
			if (vsnprintf(buf, sizeof(buf), fmt, copy) >= 0) {
				const char *plain_tag =
					tagged ? mango_tag_plain(verbosity) : "";
				if (plain_tag[0] != '\0') {
					char full[4096 + 32];
					snprintf(full, sizeof(full), "%s%s", plain_tag, buf);
					config_error_store_record(full);
				} else {
					config_error_store_record(buf);
				}
			}
		}

		va_end(copy);
	} else {
		char *prefixed = string_printf("[%s:%d] %s", file, line, fmt);
		if (prefixed) {
			_wlr_vlog(verbosity, prefixed, args);
			free(prefixed);
		} else {
			_wlr_vlog(verbosity, fmt, args);
		}
	}
}

void mango_error_impl(bool log, enum wlr_log_importance verbosity,
					  const char *file, int line, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	mango_emit(log, true, verbosity, file, line, fmt, args);
	va_end(args);
}

void mango_error_untagged(enum wlr_log_importance verbosity, const char *fmt,
						  ...) {
	va_list args;

	va_start(args, fmt);
	mango_emit(false, false, verbosity, NULL, 0, fmt, args);
	va_end(args);
}
