#ifndef MANGO_LOG_H
#define MANGO_LOG_H

#include <wlr/util/log.h>


/**
 * This is just a wrapper around wlr_log_init.
 */
void mango_log_init(enum wlr_log_importance verbosity, wlr_log_func_t callback);

/** Error function : Write a error on the good place
 * This function is a go to for all the error who need to be display and/or log
 * @param log if it's something to log or display to the user
 * @param verbosity the importance of the error
 * @param fmt the string to display, work as the same as printf
 */
void mango_error(bool log,enum wlr_log_importance verbosity,const char *fmt, ...);

#endif
