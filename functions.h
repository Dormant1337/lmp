#ifndef FUNCTIONS_H
#define FUNCTIONS_H

typedef enum {
	PLAYER_STOPPED,
	PLAYER_PLAYING,
	PLAYER_PAUSED
} PlayerStatus;

int player_init(void);
void player_shutdown(void);
int player_load_file(const char *filename);
void player_play(void);
void player_set_volume(int volume);
void player_pause_toggle(void);
void player_stop(void);
int player_is_playing(void);
PlayerStatus player_get_status(void);
double player_get_current_position(void);
double get_mp3_duration(const char *filename);

struct AppState;
void addfolder(struct AppState *state, const char *dirpath);

/* Dynamic array helpers */
int ensure_library_capacity(struct AppState *state, int additional);
int ensure_playlists_capacity(struct AppState *state, int additional);
void free_app_state(struct AppState *state);

#endif /* FUNCTIONS_H */