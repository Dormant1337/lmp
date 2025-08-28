#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "config.h"
#include "main.h"
#include "functions.h"
#include "cJSON.h"

#define CONFIG_VERSION		1
#define CONFIG_PATH_MAX		512
#define VOLUME_MIN		0
#define VOLUME_MAX		100

static void ensure_dir_exists(void)
{
	const char *home = getenv("HOME");
	char path[CONFIG_PATH_MAX];
	struct stat st;

	if (!home)
		return;

	snprintf(path, sizeof(path), "%s/.config", home);
	if (stat(path, &st) == -1) {
		if (errno == ENOENT)
			mkdir(path, 0755);
	}

	snprintf(path, sizeof(path), "%s/.config/LMP", home);
	if (stat(path, &st) == -1) {
		if (errno == ENOENT)
			mkdir(path, 0755);
	}
}

static void get_config_path(char *buf, size_t size)
{
	const char *home;

	if (!buf || size == 0)
		return;

	home = getenv("HOME");
	if (!home) {
		snprintf(buf, size, "config.json");
		return;
	}

	snprintf(buf, size, "%s/.config/LMP/config.json", home);
}

static int find_track_index_by_name(const AppState *state, const char *name)
{
	int i;

	if (!state || !name)
		return -1;

	for (i = 0; i < state->track_count; i++) {
		if (strcmp(state->library[i].name, name) == 0)
			return i;
	}
	return -1;
}

static void save_library(cJSON *root, const AppState *state)
{
	cJSON *lib, *t;
	int i;

	lib = cJSON_CreateArray();
	if (!lib)
		return;

	for (i = 0; i < state->track_count; i++) {
		t = cJSON_CreateObject();
		if (!t)
			continue;

		cJSON_AddStringToObject(t, "name", state->library[i].name);
		cJSON_AddStringToObject(t, "path", state->library[i].path);
		cJSON_AddItemToArray(lib, t);
	}
	cJSON_AddItemToObject(root, "library", lib);
}

static void save_playlists(cJSON *root, const AppState *state)
{
	cJSON *pls, *plj, *tracks;
	int i, j, idx;

	pls = cJSON_CreateArray();
	if (!pls)
		return;

	for (i = 0; i < state->playlist_count; i++) {
		const Playlist *pl = &state->playlists[i];

		plj = cJSON_CreateObject();
		tracks = cJSON_CreateArray();

		if (!plj || !tracks) {
			cJSON_Delete(plj);
			cJSON_Delete(tracks);
			continue;
		}

		cJSON_AddStringToObject(plj, "name", pl->name);

		for (j = 0; j < pl->track_count; j++) {
			idx = pl->track_indices[j];
			if (idx >= 0 && idx < state->track_count) {
				cJSON_AddItemToArray(tracks,
						    cJSON_CreateString(state->library[idx].name));
			}
		}

		cJSON_AddItemToObject(plj, "tracks", tracks);
		cJSON_AddItemToArray(pls, plj);
	}
	cJSON_AddItemToObject(root, "playlists", pls);
}

void config_save(const AppState *state)
{
	char path[CONFIG_PATH_MAX];
	cJSON *root = NULL;
	char *json = NULL;
	FILE *fp = NULL;

	if (!state)
		return;

	get_config_path(path, sizeof(path));
	ensure_dir_exists();

	root = cJSON_CreateObject();
	if (!root)
		return;

	cJSON_AddNumberToObject(root, "version", CONFIG_VERSION);
	cJSON_AddNumberToObject(root, "volume", state->current_volume);

	if (state->current_track[0] != '\0')
		cJSON_AddStringToObject(root, "last_track_path",
					state->current_track);

	save_library(root, state);
	save_playlists(root, state);

	json = cJSON_Print(root);
	if (!json)
		goto cleanup;

	fp = fopen(path, "wb");
	if (!fp)
		goto cleanup;

	if (fprintf(fp, "%s\n", json) < 0)
		goto cleanup;

cleanup:
	if (fp)
		fclose(fp);
	free(json);
	cJSON_Delete(root);
}

static void load_volume(cJSON *root, AppState *state)
{
	cJSON *vol = cJSON_GetObjectItem(root, "volume");

	if (cJSON_IsNumber(vol)) {
		state->current_volume = vol->valueint;
		if (state->current_volume < VOLUME_MIN)
			state->current_volume = VOLUME_MIN;
		if (state->current_volume > VOLUME_MAX)
			state->current_volume = VOLUME_MAX;
		player_set_volume(state->current_volume);
	}
}

static void load_last_track(cJSON *root, AppState *state)
{
	cJSON *lt = cJSON_GetObjectItem(root, "last_track_path");

	if (cJSON_IsString(lt) && lt->valuestring) {
		strncpy(state->current_track, lt->valuestring,
			sizeof(state->current_track) - 1);
		state->current_track[sizeof(state->current_track) - 1] = '\0';
	}
}

static void load_library(cJSON *root, AppState *state)
{
	cJSON *lib, *t, *nm, *pth;
	int i, n;

	lib = cJSON_GetObjectItem(root, "library");
	state->track_count = 0;

	if (!cJSON_IsArray(lib))
		return;

	n = cJSON_GetArraySize(lib);
	if (n > 100)
		n = 100;

	for (i = 0; i < n; i++) {
		t = cJSON_GetArrayItem(lib, i);
		if (!cJSON_IsObject(t))
			continue;

		nm = cJSON_GetObjectItem(t, "name");
		pth = cJSON_GetObjectItem(t, "path");

		if (!cJSON_IsString(nm) || !cJSON_IsString(pth))
			continue;
		if (!nm->valuestring || !pth->valuestring)
			continue;

		strncpy(state->library[state->track_count].name,
			nm->valuestring,
			sizeof(state->library[state->track_count].name) - 1);
		state->library[state->track_count].name[
			sizeof(state->library[state->track_count].name) - 1] = '\0';

		strncpy(state->library[state->track_count].path,
			pth->valuestring,
			sizeof(state->library[state->track_count].path) - 1);
		state->library[state->track_count].path[
			sizeof(state->library[state->track_count].path) - 1] = '\0';

		state->track_count++;
	}
}

static void load_playlists(cJSON *root, AppState *state)
{
	cJSON *pls, *pl, *nm, *tr_arr, *tn;
	int i, j, n, m, idx;
	Playlist *current_pl;

	pls = cJSON_GetObjectItem(root, "playlists");
	state->playlist_count = 0;

	if (!cJSON_IsArray(pls))
		return;

	n = cJSON_GetArraySize(pls);
	if (n > 20)
		n = 20;

	for (i = 0; i < n; i++) {
		pl = cJSON_GetArrayItem(pls, i);
		if (!cJSON_IsObject(pl))
			continue;

		nm = cJSON_GetObjectItem(pl, "name");
		if (!cJSON_IsString(nm) || !nm->valuestring)
			continue;

		current_pl = &state->playlists[state->playlist_count];

		strncpy(current_pl->name, nm->valuestring,
			sizeof(current_pl->name) - 1);
		current_pl->name[sizeof(current_pl->name) - 1] = '\0';
		current_pl->track_count = 0;

		tr_arr = cJSON_GetObjectItem(pl, "tracks");
		if (cJSON_IsArray(tr_arr)) {
			m = cJSON_GetArraySize(tr_arr);
			if (m > 100)
				m = 100;

			for (j = 0; j < m && current_pl->track_count < 100; j++) {
				tn = cJSON_GetArrayItem(tr_arr, j);
				if (!cJSON_IsString(tn) || !tn->valuestring)
					continue;

				idx = find_track_index_by_name(state, tn->valuestring);
				if (idx >= 0) {
					current_pl->track_indices[current_pl->track_count] = idx;
					current_pl->track_count++;
				}
			}
		}

		state->playlist_count++;
	}
}

void config_load(AppState *state)
{
	char path[CONFIG_PATH_MAX];
	FILE *fp = NULL;
	char *buf = NULL;
	cJSON *root = NULL;
	long sz;
	size_t read_size;

	if (!state)
		return;

	get_config_path(path, sizeof(path));

	fp = fopen(path, "rb");
	if (!fp)
		return;

	if (fseek(fp, 0, SEEK_END) != 0)
		goto cleanup;

	sz = ftell(fp);
	if (sz <= 0 || sz > 5 * 1024 * 1024)  /* Max 5MB config file */
		goto cleanup;

	if (fseek(fp, 0, SEEK_SET) != 0)
		goto cleanup;

	buf = malloc(sz + 1);
	if (!buf)
		goto cleanup;

	read_size = fread(buf, 1, sz, fp);
	if (read_size != (size_t)sz)
		goto cleanup;

	buf[sz] = '\0';

	root = cJSON_Parse(buf);
	if (!root)
		goto cleanup;

	load_volume(root, state);
	load_last_track(root, state);
	load_library(root, state);
	load_playlists(root, state);

cleanup:
	cJSON_Delete(root);
	free(buf);
	if (fp)
		fclose(fp);
}