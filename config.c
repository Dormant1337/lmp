#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "main.h"
#include "functions.h"
#include "cJSON.h"

static void ensure_dir_exists(void)
{
	const char *home = getenv("HOME");
	char path[512];

	if (!home)
		return;

	snprintf(path, sizeof(path), "%s/.config", home);
	mkdir(path, 0755);

	snprintf(path, sizeof(path), "%s/.config/LMP", home);
	mkdir(path, 0755);
}

static void get_config_path(char *buf, size_t size)
{
	const char *home = getenv("HOME");

	if (!home) {
		snprintf(buf, size, "config.json");
		return;
	}

	snprintf(buf, size, "%s/.config/LMP/config.json", home);
}

static int find_track_index_by_name(const AppState *state, const char *name)
{
	int i;

	for (i = 0; i < state->track_count; i++) {
		if (strcmp(state->library[i].name, name) == 0)
			return i;
	}
	return -1;
}

void config_save(const AppState *state)
{
	char path[512];
	cJSON *root, *lib, *pls;
	char *json;
	FILE *fp;

	get_config_path(path, sizeof(path));
	ensure_dir_exists();

	root = cJSON_CreateObject();
	if (!root)
		return;

	cJSON_AddNumberToObject(root, "version", 1);
	cJSON_AddNumberToObject(root, "volume", state->current_volume);

	if (state->current_track[0] != '\0')
		cJSON_AddStringToObject(root, "last_track_path",
					state->current_track);

	/* Library */
	lib = cJSON_CreateArray();
	if (!lib) {
		cJSON_Delete(root);
		return;
	}
	for (int i = 0; i < state->track_count; i++) {
		cJSON *t = cJSON_CreateObject();

		if (!t)
			continue;

		cJSON_AddStringToObject(t, "name", state->library[i].name);
		cJSON_AddStringToObject(t, "path", state->library[i].path);
		cJSON_AddItemToArray(lib, t);
	}
	cJSON_AddItemToObject(root, "library", lib);

	/* Playlists (store track names) */
	pls = cJSON_CreateArray();
	if (!pls) {
		cJSON_Delete(root);
		return;
	}
	for (int i = 0; i < state->playlist_count; i++) {
		const Playlist *pl = &state->playlists[i];
		cJSON *plj = cJSON_CreateObject();
		cJSON *tracks = cJSON_CreateArray();

		if (!plj || !tracks) {
			if (plj)
				cJSON_Delete(plj);
			if (tracks)
				cJSON_Delete(tracks);
			continue;
		}

		cJSON_AddStringToObject(plj, "name", pl->name);

		for (int j = 0; j < pl->track_count; j++) {
			int idx = pl->track_indices[j];

			if (idx >= 0 && idx < state->track_count)
				cJSON_AddItemToArray(tracks, cJSON_CreateString(
							      state->library[idx].name));
		}

		cJSON_AddItemToObject(plj, "tracks", tracks);
		cJSON_AddItemToArray(pls, plj);
	}
	cJSON_AddItemToObject(root, "playlists", pls);

	json = cJSON_Print(root);
	if (json) {
		fp = fopen(path, "wb");
		if (fp) {
			fprintf(fp, "%s\n", json);
			fclose(fp);
		}
		free(json);
	}

	cJSON_Delete(root);
}

void config_load(AppState *state)
{
	char path[512];
	FILE *fp;
	long sz;
	char *buf;
	cJSON *root;

	get_config_path(path, sizeof(path));

	fp = fopen(path, "rb");
	if (!fp)
		return;

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return;
	}
	sz = ftell(fp);
	if (sz <= 0) {
		fclose(fp);
		return;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return;
	}

	buf = malloc(sz + 1);
	if (!buf) {
		fclose(fp);
		return;
	}
	if (fread(buf, 1, sz, fp) != (size_t)sz) {
		fclose(fp);
		free(buf);
		return;
	}
	fclose(fp);
	buf[sz] = '\0';

	root = cJSON_Parse(buf);
	free(buf);

	if (!root)
		return;

	/* Volume */
	{
		cJSON *vol = cJSON_GetObjectItem(root, "volume");

		if (cJSON_IsNumber(vol)) {
			state->current_volume = vol->valueint;
			if (state->current_volume < 0)
				state->current_volume = 0;
			if (state->current_volume > 100)
				state->current_volume = 100;
			player_set_volume(state->current_volume);
		}
	}

	/* Last track */
	{
		cJSON *lt = cJSON_GetObjectItem(root, "last_track_path");

		if (cJSON_IsString(lt) && lt->valuestring) {
			strncpy(state->current_track, lt->valuestring,
				sizeof(state->current_track) - 1);
			state->current_track[sizeof(state->current_track) - 1] = '\0';
		}
	}

	/* Library */
	{
		cJSON *lib = cJSON_GetObjectItem(root, "library");

		state->track_count = 0;

		if (cJSON_IsArray(lib)) {
			int n = cJSON_GetArraySize(lib);

			for (int i = 0; i < n && i < 100; i++) {
				cJSON *t = cJSON_GetArrayItem(lib, i);
				cJSON *nm, *pth;

				if (!cJSON_IsObject(t))
					continue;

				nm = cJSON_GetObjectItem(t, "name");
				pth = cJSON_GetObjectItem(t, "path");

				if (!cJSON_IsString(nm) || !cJSON_IsString(pth))
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
	}

	/* Playlists */
	{
		cJSON *pls = cJSON_GetObjectItem(root, "playlists");

		state->playlist_count = 0;

		if (cJSON_IsArray(pls)) {
			int n = cJSON_GetArraySize(pls);

			for (int i = 0; i < n && i < 20; i++) {
				cJSON *pl = cJSON_GetArrayItem(pls, i);
				cJSON *nm, *tr_arr;

				if (!cJSON_IsObject(pl))
					continue;

				nm = cJSON_GetObjectItem(pl, "name");
				tr_arr = cJSON_GetObjectItem(pl, "tracks");

				if (!cJSON_IsString(nm))
					continue;

				strncpy(state->playlists[state->playlist_count].name,
					nm->valuestring,
					sizeof(state->playlists[state->playlist_count].name) - 1);
				state->playlists[state->playlist_count].name[
					sizeof(state->playlists[state->playlist_count].name) - 1] = '\0';

				state->playlists[state->playlist_count].track_count = 0;

				if (cJSON_IsArray(tr_arr)) {
					int m = cJSON_GetArraySize(tr_arr);

					for (int j = 0; j < m && j < 100; j++) {
						cJSON *tn = cJSON_GetArrayItem(tr_arr, j);
						int idx;

						if (!cJSON_IsString(tn) || !tn->valuestring)
							continue;

						idx = find_track_index_by_name(state,
									      tn->valuestring);
						if (idx >= 0) {
							state->playlists[state->playlist_count]
								.track_indices[
								state->playlists[state->playlist_count]
									.track_count] = idx;
							state->playlists[state->playlist_count].track_count++;
						}
					}
				}

				state->playlist_count++;
			}
		}
	}

	cJSON_Delete(root);
}