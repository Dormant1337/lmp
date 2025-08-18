#include "functions.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <mpg123.h>

<<<<<<< HEAD
// Global variables to hold the music track and timing
static Mix_Music *g_music = NULL;
static Uint32 g_start_ticks = 0;
static Uint32 g_paused_ticks = 0;
=======
/* Global variables to hold the music track and timing */
static Mix_Music *g_music;
static Uint32 g_start_ticks;
static Uint32 g_paused_ticks;
>>>>>>> 90c055f (Add audioplayer source files)

void player_set_volume(int involume)
{
	int volume;
<<<<<<< HEAD
	
	volume = (involume * 128) / 100;

	if(volume < 0) {
		volume = 0;
	}
	if(volume > MIX_MAX_VOLUME) {
		volume = MIX_MAX_VOLUME;
	}
=======

	volume = (involume * 128) / 100;

	if (volume < 0)
		volume = 0;
	if (volume > MIX_MAX_VOLUME)
		volume = MIX_MAX_VOLUME;

>>>>>>> 90c055f (Add audioplayer source files)
	Mix_VolumeMusic(volume);
}

int player_init(void)
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
		return -1;
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
<<<<<<< HEAD
		fprintf(stderr, "Failed to initialize SDL_mixer: %s\n", Mix_GetError());
=======
		fprintf(stderr, "Failed to initialize SDL_mixer: %s\n",
			Mix_GetError());
>>>>>>> 90c055f (Add audioplayer source files)
		SDL_Quit();
		return -1;
	}

<<<<<<< HEAD
=======
	g_music = NULL;
	g_start_ticks = 0;
	g_paused_ticks = 0;

>>>>>>> 90c055f (Add audioplayer source files)
	return 0;
}

void player_shutdown(void)
{
	if (g_music) {
		Mix_FreeMusic(g_music);
		g_music = NULL;
	}
	Mix_CloseAudio();
	SDL_Quit();
}

int player_load_file(const char *filename)
{
	if (g_music) {
		Mix_FreeMusic(g_music);
		g_music = NULL;
	}

	g_music = Mix_LoadMUS(filename);
	if (!g_music) {
<<<<<<< HEAD
		fprintf(stderr, "Failed to load MP3 file '%s': %s\n", filename, Mix_GetError());
=======
		fprintf(stderr, "Failed to load MP3 file '%s': %s\n", filename,
			Mix_GetError());
>>>>>>> 90c055f (Add audioplayer source files)
		return -1;
	}

	return 0;
}

void player_play(void)
{
	if (g_music) {
		Mix_PlayMusic(g_music, 1);
		g_start_ticks = SDL_GetTicks();
		g_paused_ticks = 0;
	}
}

void player_pause_toggle(void)
{
	if (Mix_PlayingMusic() || Mix_PausedMusic()) {
		if (Mix_PausedMusic()) {
			Mix_ResumeMusic();
<<<<<<< HEAD
			// Adjust start time to account for the time spent paused
			g_start_ticks = SDL_GetTicks() - g_paused_ticks;
			g_paused_ticks = 0;
		} else {
			// Record how long the music has been playing
=======
			/* Adjust start time to account for the time spent paused */
			g_start_ticks = SDL_GetTicks() - g_paused_ticks;
			g_paused_ticks = 0;
		} else {
			/* Record how long the music has been playing */
>>>>>>> 90c055f (Add audioplayer source files)
			g_paused_ticks = SDL_GetTicks() - g_start_ticks;
			Mix_PauseMusic();
		}
	}
}

void player_stop(void)
{
	Mix_HaltMusic();
	g_start_ticks = 0;
	g_paused_ticks = 0;
}

int player_is_playing(void)
{
	return Mix_PlayingMusic() || Mix_PausedMusic();
}

PlayerStatus player_get_status(void)
{
<<<<<<< HEAD
	if (Mix_PausedMusic()) {
		return PLAYER_PAUSED;
	}
	if (Mix_PlayingMusic()) {
		return PLAYER_PLAYING;
	}
=======
	if (Mix_PausedMusic())
		return PLAYER_PAUSED;
	if (Mix_PlayingMusic())
		return PLAYER_PLAYING;
>>>>>>> 90c055f (Add audioplayer source files)
	return PLAYER_STOPPED;
}

double player_get_current_position(void)
{
<<<<<<< HEAD
	if (!player_is_playing() && !Mix_PausedMusic()) {
		return 0.0;
	}
	if (Mix_PausedMusic()) {
		return (double)g_paused_ticks / 1000.0;
	} else {
		return (double)(SDL_GetTicks() - g_start_ticks) / 1000.0;
	}
=======
	if (!player_is_playing() && !Mix_PausedMusic())
		return 0.0;

	if (Mix_PausedMusic())
		return (double)g_paused_ticks / 1000.0;

	return (double)(SDL_GetTicks() - g_start_ticks) / 1000.0;
>>>>>>> 90c055f (Add audioplayer source files)
}

double get_mp3_duration(const char *filename)
{
	mpg123_handle *mh = NULL;
	double duration = -1.0;
	long rate;
	int err = MPG123_OK;
	int channels, encoding;
	off_t num_samples;

<<<<<<< HEAD
	static int mpg123_initialized = 0;
	if (!mpg123_initialized) {
		err = mpg123_init();
		if (err != MPG123_OK) {
			fprintf(stderr, "Failed to initialize mpg123: %s\n", mpg123_plain_strerror(err));
=======
	static int mpg123_initialized;

	if (!mpg123_initialized) {
		err = mpg123_init();
		if (err != MPG123_OK) {
			fprintf(stderr, "Failed to initialize mpg123: %s\n",
				mpg123_plain_strerror(err));
>>>>>>> 90c055f (Add audioplayer source files)
			return -1.0;
		}
		mpg123_initialized = 1;
	}

	mh = mpg123_new(NULL, &err);
<<<<<<< HEAD
	if (mh == NULL) {
		fprintf(stderr, "Failed to create mpg123 handle: %s\n", mpg123_plain_strerror(err));
=======
	if (!mh) {
		fprintf(stderr, "Failed to create mpg123 handle: %s\n",
			mpg123_plain_strerror(err));
>>>>>>> 90c055f (Add audioplayer source files)
		return -1.0;
	}

	if (mpg123_open(mh, filename) != MPG123_OK) {
<<<<<<< HEAD
		fprintf(stderr, "Failed to open file '%s': %s\n", filename, mpg123_strerror(mh));
		goto cleanup_handle;
=======
		fprintf(stderr, "Failed to open file '%s': %s\n",
			filename, mpg123_strerror(mh));
		goto out_del;
>>>>>>> 90c055f (Add audioplayer source files)
	}

	if (mpg123_scan(mh) != MPG123_OK) {
		fprintf(stderr, "Failed to scan file '%s'\n", filename);
<<<<<<< HEAD
		goto cleanup_close;
=======
		goto out_close;
>>>>>>> 90c055f (Add audioplayer source files)
	}

	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		fprintf(stderr, "Failed to get audio format.\n");
<<<<<<< HEAD
		goto cleanup_close;
	}

	num_samples = mpg123_length(mh);
	if (num_samples <= 0) {
		fprintf(stderr, "File length is invalid or zero.\n");
	} else {
		duration = (double)num_samples / rate;
	}

cleanup_close:
	mpg123_close(mh);
cleanup_handle:
	mpg123_delete(mh);

=======
		goto out_close;
	}

	num_samples = mpg123_length(mh);
	if (num_samples > 0)
		duration = (double)num_samples / rate;
	else
		fprintf(stderr, "File length is invalid or zero.\n");

out_close:
	mpg123_close(mh);
out_del:
	mpg123_delete(mh);
>>>>>>> 90c055f (Add audioplayer source files)
	return duration;
}