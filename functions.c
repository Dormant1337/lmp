#include "functions.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <mpg123.h>

#include "main.h"
#include "config.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>

/* Initial capacities for dynamic arrays (backward-compatible defaults) */
#define LMP_INIT_LIBRARY_CAP	100
#define LMP_INIT_PLAYLISTS_CAP	20
#define LMP_INIT_PL_TRACK_CAP	100

/* =========================
 * Dynamic array helpers
 * ========================= */

int ensure_library_capacity(struct AppState *state, int additional)
{
	size_t need, newcap;
	Track *tmp;

	if (!state)
		return -1;
	if (additional < 0)
		additional = 0;

	if ((size_t)state->library_cap >=
	    (size_t)state->track_count + (size_t)additional)
		return 0;

	newcap = state->library_cap ? state->library_cap : LMP_INIT_LIBRARY_CAP;
	need = (size_t)state->track_count + (size_t)additional;
	while (newcap < need) {
		if (newcap > SIZE_MAX / 2)
			return -1;
		newcap *= 2;
	}

	tmp = realloc(state->library, newcap * sizeof(*state->library));
	if (!tmp)
		return -1;

	state->library = tmp;
	state->library_cap = (int)newcap;
	return 0;
}

int ensure_playlists_capacity(struct AppState *state, int additional)
{
	size_t need, newcap;
	Playlist *tmp;

	if (!state)
		return -1;
	if (additional < 0)
		additional = 0;

	if ((size_t)state->playlists_cap >=
	    (size_t)state->playlist_count + (size_t)additional)
		return 0;

	newcap = state->playlists_cap ? state->playlists_cap
				      : LMP_INIT_PLAYLISTS_CAP;
	need = (size_t)state->playlist_count + (size_t)additional;
	while (newcap < need) {
		if (newcap > SIZE_MAX / 2)
			return -1;
		newcap *= 2;
	}

	tmp = realloc(state->playlists, newcap * sizeof(*state->playlists));
	if (!tmp)
		return -1;

	/* Zero-init only the newly added tail to keep fields sane */
	if (newcap > (size_t)state->playlists_cap) {
		size_t old = (size_t)state->playlists_cap;
		memset(tmp + old, 0, (newcap - old) * sizeof(*tmp));
	}

	state->playlists = tmp;
	state->playlists_cap = (int)newcap;
	return 0;
}

int ensure_playlist_tracks_capacity(struct Playlist *pl, int additional)
{
	size_t need, newcap;
	int *tmp;

	if (!pl)
		return -1;
	if (additional < 0)
		additional = 0;

	if ((size_t)pl->track_capacity >=
	    (size_t)pl->track_count + (size_t)additional)
		return 0;

	newcap = pl->track_capacity ? pl->track_capacity : LMP_INIT_PL_TRACK_CAP;
	need = (size_t)pl->track_count + (size_t)additional;
	while (newcap < need) {
		if (newcap > SIZE_MAX / 2)
			return -1;
		newcap *= 2;
	}

	tmp = realloc(pl->track_indices, newcap * sizeof(*pl->track_indices));
	if (!tmp)
		return -1;

	pl->track_indices = tmp;
	pl->track_capacity = (int)newcap;
	return 0;
}

void free_app_state(struct AppState *state)
{
	int i;

	if (!state)
		return;

	for (i = 0; i < state->playlists_cap; i++) {
		free(state->playlists[i].track_indices);
		state->playlists[i].track_indices = NULL;
		state->playlists[i].track_capacity = 0;
		state->playlists[i].track_count = 0;
	}

	free(state->playlists);
	state->playlists = NULL;
	state->playlists_cap = 0;
	state->playlist_count = 0;

	free(state->library);
	state->library = NULL;
	state->library_cap = 0;
	state->track_count = 0;
}

/* =========================
 * Audio backend (SDL_mixer + mpg123)
 * ========================= */

/* Global variables to hold the music track and timing */
static Mix_Music *g_music;
static Uint32 g_start_ticks;
static Uint32 g_paused_ticks;

void player_set_volume(int involume)
{
	int volume;

	volume = (involume * 128) / 100;

	if (volume < 0)
		volume = 0;
	if (volume > MIX_MAX_VOLUME)
		volume = MIX_MAX_VOLUME;

	Mix_VolumeMusic(volume);
}

int player_init(void)
{
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL: %s\n",
			SDL_GetError());
		return -1;
	}

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		fprintf(stderr, "Failed to initialize SDL_mixer: %s\n",
			Mix_GetError());
		SDL_Quit();
		return -1;
	}

	g_music = NULL;
	g_start_ticks = 0;
	g_paused_ticks = 0;

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
		fprintf(stderr, "Failed to load MP3 file '%s': %s\n", filename,
			Mix_GetError());
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
			g_start_ticks = SDL_GetTicks() - g_paused_ticks;
			g_paused_ticks = 0;
		} else {
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
	if (Mix_PausedMusic())
		return PLAYER_PAUSED;
	if (Mix_PlayingMusic())
		return PLAYER_PLAYING;
	return PLAYER_STOPPED;
}

double player_get_current_position(void)
{
	if (!player_is_playing() && !Mix_PausedMusic())
		return 0.0;

	if (Mix_PausedMusic())
		return (double)g_paused_ticks / 1000.0;

	return (double)(SDL_GetTicks() - g_start_ticks) / 1000.0;
}

double get_mp3_duration(const char *filename)
{
	mpg123_handle *mh = NULL;
	double duration = -1.0;
	long rate;
	int err = MPG123_OK;
	int channels, encoding;
	off_t num_samples;

	static int mpg123_initialized;

	if (!mpg123_initialized) {
		err = mpg123_init();
		if (err != MPG123_OK) {
			fprintf(stderr, "Failed to initialize mpg123: %s\n",
				mpg123_plain_strerror(err));
			return -1.0;
		}
		mpg123_initialized = 1;
	}

	mh = mpg123_new(NULL, &err);
	if (!mh) {
		fprintf(stderr, "Failed to create mpg123 handle: %s\n",
			mpg123_plain_strerror(err));
		return -1.0;
	}

	if (mpg123_open(mh, filename) != MPG123_OK) {
		fprintf(stderr, "Failed to open file '%s': %s\n",
			filename, mpg123_strerror(mh));
		goto out_del;
	}

	if (mpg123_scan(mh) != MPG123_OK) {
		fprintf(stderr, "Failed to scan file '%s'\n", filename);
		goto out_close;
	}

	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
		fprintf(stderr, "Failed to get audio format.\n");
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
	return duration;
}

/* =========================
 * addfolder implementation
 * ========================= */

/* Return non-zero if name ends with ".mp3" (case-insensitive). */
static int has_mp3_ext(const char *name)
{
	const char *dot = strrchr(name, '.');

	if (!dot)
		return 0;
	return strcasecmp(dot, ".mp3") == 0;
}

/* Copy fname without trailing ".mp3" into out. */
static void strip_mp3_ext(const char *fname, char *out, size_t outlen)
{
	const char *dot = strrchr(fname, '.');
	size_t n = dot ? (size_t)(dot - fname) : strlen(fname);

	if (!outlen)
		return;
	if (n >= outlen)
		n = outlen - 1;

	memcpy(out, fname, n);
	out[n] = '\0';
}

/* Join dir and name into dst. */
static void join_path(char *dst, size_t dstsz, const char *dir, const char *name)
{
	size_t len = strlen(dir);

	if (len > 0 && dir[len - 1] == '/')
		snprintf(dst, dstsz, "%s%s", dir, name);
	else
		snprintf(dst, dstsz, "%s/%s", dir, name);
}

static int track_name_exists(const AppState *state, const char *name)
{
	int i;

	for (i = 0; i < state->track_count; i++) {
		if (strcmp(state->library[i].name, name) == 0)
			return 1;
	}
	return 0;
}

static int track_path_exists(const AppState *state, const char *path)
{
	int i;

	for (i = 0; i < state->track_count; i++) {
		if (strcmp(state->library[i].path, path) == 0)
			return 1;
	}
	return 0;
}

/**
 * addfolder() - add all *.mp3 files from a directory into library.
 */
void addfolder(AppState *state, const char *dirpath)
{
	DIR *dir;
	struct dirent *de;
	int added = 0, skipped_exists = 0, skipped_cap = 0, skipped_invalid = 0;

	if (!dirpath || !*dirpath) {
		snprintf(state->message, sizeof(state->message),
			 "Usage: addfolder <directory_path>");
		return;
	}

	dir = opendir(dirpath);
	if (!dir) {
		snprintf(state->message, sizeof(state->message),
			 "addfolder: cannot open '%s': %s",
			 dirpath, strerror(errno));
		return;
	}

	while ((de = readdir(dir)) != NULL) {
		char fullpath[512];
		struct stat st;
		char track_name[50];

		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;

		if (!has_mp3_ext(de->d_name))
			continue;

		join_path(fullpath, sizeof(fullpath), dirpath, de->d_name);

		if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) {
			skipped_invalid++;
			continue;
		}

		if (ensure_library_capacity(state, 1) != 0) {
			/* ENOMEM or overflow */
			skipped_cap++;
			break;
		}

		strip_mp3_ext(de->d_name, track_name, sizeof(track_name));
		if (track_name[0] == '\0') {
			skipped_invalid++;
			continue;
		}

		if (track_name_exists(state, track_name) ||
		    track_path_exists(state, fullpath)) {
			skipped_exists++;
			continue;
		}

		strncpy(state->library[state->track_count].name, track_name,
			sizeof(state->library[state->track_count].name) - 1);
		state->library[state->track_count].name[
			sizeof(state->library[state->track_count].name) - 1] = '\0';

		strncpy(state->library[state->track_count].path, fullpath,
			sizeof(state->library[state->track_count].path) - 1);
		state->library[state->track_count].path[
			sizeof(state->library[state->track_count].path) - 1] = '\0';

		state->track_count++;
		added++;
	}
	closedir(dir);

	config_save(state);

	snprintf(state->message, sizeof(state->message),
		 "addfolder: added %d, skipped (exists %d, alloc_fail %d, invalid %d)",
		 added, skipped_exists, skipped_cap, skipped_invalid);
}