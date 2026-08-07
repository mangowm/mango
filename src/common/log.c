#include "log.h"
#include "wlr/util/log.h"
#include <stdarg.h>
#include <stdio.h>

void mango_log_init(enum wlr_log_importance verbosity,
					wlr_log_func_t callback) {
	wlr_log_init(verbosity, callback);
}

#include <stdarg.h>

void mango_error(bool log, enum wlr_log_importance verbosity, const char *fmt,
				 ...) {
	va_list args;
	va_start(args, fmt);
	if (!log) {
		if (verbosity == WLR_ERROR) {
			fprintf(stderr, "\033[1m\033[31m[ERROR]:\033[33m ");
			vfprintf(stderr, fmt, args);
		} else if (verbosity == WLR_INFO) {
			vfprintf(stderr, fmt, args);
		} else if (verbosity == WLR_DEBUG) {
			fprintf(stderr, "\033[1;32m[DEBUG]:\033[0m ");
			vfprintf(stderr, fmt, args);
		}
	} else {
		_wlr_vlog(verbosity, fmt, args);
	}
	va_end(args);
}
