#ifndef FUNCTIONS_H
#define FUNCTIONS_H

// Forward declaration to avoid circular dependency
struct AppState;

/*
 * Represents the current state of the audio player.
 */
typedef enum {
	PLAYER_STOPPED,
	PLAYER_PLAYING,
	PLAYER_PAUSED
} PlayerStatus;

/**
 * @brief Initializes the audio player.
 */
int player_init(void);

/**
 * @brief Shuts down the audio player.
 */
void player_shutdown(void);

/**
 * @brief Loads an MP3 file for playback.
 */
int player_load_file(const char *filename);

/**
 * @brief Plays the currently loaded music.
 */
void player_play(void);

/**
 * @brief Sets the music volume.
 */
void player_set_volume(int volume);

/**
 * @brief Toggles the pause/resume state of the player.
 */
void player_pause_toggle(void);

/**
 * @brief Stops the currently playing music.
 */
void player_stop(void);

/**
 * @brief Checks if the player is currently playing or paused.
 */
int player_is_playing(void);

/**
 * @brief Gets the current status of the player.
 */
PlayerStatus player_get_status(void);

/**
 * @brief Get the current playback position in seconds.
 */
double player_get_current_position(void);

/**
 * @brief Get the duration of an MP3 file in seconds.
 */
double get_mp3_duration(const char *filename);

#endif /* FUNCTIONS_H */