#ifndef MANGO_CONFIG_ERROR_STORE_H
#define MANGO_CONFIG_ERROR_STORE_H 1

#include <stdbool.h>

const char *config_error_store_path(void);
void config_error_store_begin(void);
void config_error_store_end(void);
bool config_error_store_active(void);
void config_error_store_record(const char *message);

#endif
