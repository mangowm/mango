#ifndef SESSION_H
#define SESSION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Client Client;

#define SESSION_TITLE_MAX 512
#define SESSION_MONITOR_MAX 128
#define SESSION_COMMAND_MAX 1024

typedef struct {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
} SessionRect;

typedef struct {
	char app_id[256];
	char title[SESSION_TITLE_MAX];
	char monitor[SESSION_MONITOR_MAX];
	char launch_command[SESSION_COMMAND_MAX];
	int32_t pid;
	uint32_t tags;
	int32_t is_floating;
	int32_t is_fullscreen;
	int32_t is_minimized;
	SessionRect geom;
	SessionRect float_geom;
} SessionRestoreEntry;

void session_init(void);
void session_shutdown(void);
void session_maybe_restore_startup(void);
void session_handle_client_mapped(Client *c);
void session_handle_client_destroyed(Client *c);
void session_save_now(bool is_final_shutdown);
bool session_is_enabled(void);
bool session_is_restorable_client(Client *c);

#endif
