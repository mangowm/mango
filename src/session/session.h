#ifndef SESSION_H
#define SESSION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Client Client;

typedef struct {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
} SessionRect;

typedef struct {
	char app_id[256];
	char title[512];
	char monitor[128];
	char launch_command[1024];
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
