#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "functions.h"
#include "config.h"
#include "main.h"

/* Prototypes */
static void handle_command(AppState *state);
static void draw_ui(AppState *state);

static void play_track(AppState *state, const char *track_path)
{
	if (access(track_path, F_OK) != 0) {
		snprintf(state->message, sizeof(state->message),
			 "Error: File not found: %s", track_path);
	} else {
		player_load_file(track_path);
		player_play();
		state->track_duration = get_mp3_duration(track_path);
		strncpy(state->current_track, track_path,
			sizeof(state->current_track) - 1);
		state->current_track[sizeof(state->current_track) - 1] = '\0';
		snprintf(state->message, sizeof(state->message),
			 "Started playing: %s", state->current_track);

		/* Persist last track path, etc. */
		config_save(state);
	}
}

int main(int argc, char *argv[])
{
	AppState state = {0};
	int ch, rows, cols;

	if (player_init() != 0) {
		fprintf(stderr, "Failed to initialize the audio player. Exiting.\n");
		return 1;
	}

	/* Defaults before load */
	state.current_volume = 100;
	state.is_running = 1;
	state.playing_playlist_index = -1;
	state.playing_track_index_in_playlist = 0;

	/* Load config (volume, library, playlists, last track) */
	config_load(&state);

	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	timeout(100);

	if (state.current_volume < 0 || state.current_volume > 100)
		state.current_volume = 100;
	player_set_volume(state.current_volume);

	strncpy(state.message, "Welcome to lmp!", sizeof(state.message) - 1);

	while (state.is_running) {
		draw_ui(&state);
		ch = getch();

		if (ch != ERR) {
			switch (ch) {
			case 'q':
				state.is_running = 0;
				break;
			case ':':
				getmaxyx(stdscr, rows, cols);
				echo();
				strncpy(state.command_buffer, "",
					sizeof(state.command_buffer) - 1);

				attron(A_REVERSE);
				mvprintw(rows - 1, 0, ":");
				for (int i = 1; i < cols; i++)
					printw(" ");
				move(rows - 1, 1);
				attroff(A_REVERSE);

				timeout(-1);
				getstr(state.command_buffer);
				timeout(100);

				noecho();
				clear();
				refresh();

				handle_command(&state);
				break;
			}
		}

		/* Auto-advance for playlists */
		if (state.current_track[0] != '\0' && !player_is_playing()) {
			if (state.playing_playlist_index != -1) {
				Playlist *pl = &state.playlists[state.playing_playlist_index];

				state.playing_track_index_in_playlist++;
				if (state.playing_track_index_in_playlist < pl->track_count) {
					int lib_idx = pl->track_indices[state.playing_track_index_in_playlist];

					if (lib_idx >= 0 && lib_idx < state.track_count) {
						play_track(&state, state.library[lib_idx].path);
						snprintf(state.message, sizeof(state.message),
							 "Now playing next track in '%s'",
							 pl->name);
					} else {
						state.playing_playlist_index = -1;
						state.playing_track_index_in_playlist = 0;
						state.current_track[0] = '\0';
						state.track_duration = 0.0;
						snprintf(state.message, sizeof(state.message),
							 "Playlist '%s' finished (invalid index).",
							 pl->name);
					}
				} else {
					state.playing_playlist_index = -1;
					state.playing_track_index_in_playlist = 0;
					state.current_track[0] = '\0';
					state.track_duration = 0.0;
					snprintf(state.message, sizeof(state.message),
						 "Playlist '%s' finished.", pl->name);
				}
			} else {
				state.current_track[0] = '\0';
				state.track_duration = 0.0;
				snprintf(state.message, sizeof(state.message),
					 "Playback Finished.");
			}
		}
	}

	/* Persist on exit */
	config_save(&state);

	endwin();
	player_shutdown();
	return 0;
}

static void draw_ui(AppState *state)
{
	int rows, cols;
	char display_buffer[256];
	const char *status_text = NULL;
	PlayerStatus status = player_get_status();

	clear();
	getmaxyx(stdscr, rows, cols);

	mvprintw(0, 2, "L-M-P Player");

	switch (status) {
	case PLAYER_PLAYING:
		status_text = "Playing";
		break;
	case PLAYER_PAUSED:
		status_text = "Paused";
		break;
	case PLAYER_STOPPED:
	default:
		status_text = NULL;
		break;
	}

	if (status_text && state->current_track[0] != '\0') {
		char temp_buffer[256] = {0};
		const char *filename = strrchr(state->current_track, '/');

		if (state->playing_playlist_index != -1) {
			snprintf(temp_buffer, sizeof(temp_buffer), "Playlist: %s | ",
				 state->playlists[state->playing_playlist_index].name);
		}

		if (filename)
			filename++;
		else
			filename = state->current_track;

		snprintf(display_buffer, sizeof(display_buffer),
			 "%s - \"%s\" | Volume: %d", status_text, filename,
			 state->current_volume);
		strncat(temp_buffer, display_buffer,
			sizeof(temp_buffer) - strlen(temp_buffer) - 1);

		if ((int)strlen(temp_buffer) + 2 < cols)
			mvprintw(0, cols - (int)strlen(temp_buffer) - 2, "%s",
				 temp_buffer);
	}

	mvprintw(2, 2, "Message: %s", state->message);

	/* Progress bar */
	if (player_is_playing() && state->track_duration > 0) {
		double pos = player_get_current_position();
		double dur = state->track_duration;
		char time_str[64];
		int bar_w, prog_w;

		if (pos > dur)
			pos = dur;

		snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d",
			 (int)pos / 60, (int)pos % 60, (int)dur / 60, (int)dur % 60);

		bar_w = cols - 5 - (int)strlen(time_str);
		if (bar_w < 10)
			bar_w = 10;

		prog_w = dur > 0.0 ? (int)((pos / dur) * bar_w) : 0;
		if (prog_w > bar_w)
			prog_w = bar_w;

		mvprintw(rows - 3, 0, "  [");
		attron(A_REVERSE);
		for (int i = 0; i < prog_w; i++)
			printw(" ");
		attroff(A_REVERSE);
		for (int i = prog_w; i < bar_w; i++)
			printw("-");
		printw("] %s", time_str);
	}

	attron(A_REVERSE);
	mvprintw(rows - 1, 0, "Press ':' for commands, 'q' to quit");
	attroff(A_REVERSE);

	refresh();
}

static void handle_command(AppState *state)
{
	char buffer_copy[256];
	char *command;
	char *argument;

	strncpy(buffer_copy, state->command_buffer, sizeof(buffer_copy) - 1);
	command = strtok(buffer_copy, " ");
	if (!command)
		return;

	argument = strtok(NULL, "");
	if (argument) {
		while (*argument == ' ')
			argument++;
	}

	if (strcmp(command, "add") == 0) {
		char name[50] = {0};
		char path[256] = {0};
		const char *args = state->command_buffer + strlen(command);

		while (*args == ' ')
			args++;

		if (*args == '"') {
			const char *name_start = args + 1;
			const char *name_end = strchr(name_start, '"');

			if (name_end) {
				size_t len = name_end - name_start;
				if (len > sizeof(name) - 1)
					len = sizeof(name) - 1;
				strncpy(name, name_start, len);
				name[len] = '\0';

				const char *path_start = name_end + 1;
				while (*path_start == ' ')
					path_start++;
				strncpy(path, path_start, sizeof(path) - 1);
				path[sizeof(path) - 1] = '\0';
			}
		} else {
			const char *name_start = args;
			const char *name_end = strchr(name_start, ' ');

			if (name_end) {
				size_t len = name_end - name_start;
				if (len > sizeof(name) - 1)
					len = sizeof(name) - 1;
				strncpy(name, name_start, len);
				name[len] = '\0';

				const char *path_start = name_end + 1;
				while (*path_start == ' ')
					path_start++;
				strncpy(path, path_start, sizeof(path) - 1);
				path[sizeof(path) - 1] = '\0';
			}
		}

		if (name[0] == '\0' || path[0] == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: add \"<name>\" <path>  OR  add <name> <path>");
			return;
		}
		if (state->track_count >= 100) {
			snprintf(state->message, sizeof(state->message),
				 "Library is full.");
			return;
		}
		if (access(path, F_OK) != 0) {
			snprintf(state->message, sizeof(state->message),
				 "Error: File not found: %s", path);
			return;
		}

		strncpy(state->library[state->track_count].name, name,
			sizeof(state->library[state->track_count].name) - 1);
		strncpy(state->library[state->track_count].path, path,
			sizeof(state->library[state->track_count].path) - 1);
		state->track_count++;

		snprintf(state->message, sizeof(state->message),
			 "Added '%s' to library.", name);

		/* Persist library */
		config_save(state);

	} else if (strcmp(command, "help") == 0) {
		int rows, cols, line = 2;
		const char *help_lines[] = {
			"Commands:",
			"  add \"<name>\" <path>        - Add track to library",
			"  library / lib               - Show library & playlists",
			"  play <name>                 - Play a track from library",
			"  pause                       - Toggle pause/resume",
			"  stop                        - Stop playback",
			"  setvolume <0-100>           - Set the volume",
			"  volume                      - Show current volume",
			"  listnew <name>              - Create a new playlist",
			"  createlist <name>           - Create a new playlist (alias)",
			"  listadd \"<pl>\" \"<track>\"   - Add track to a playlist",
			"  listview <name>             - View tracks in a playlist",
			"  listplay <name>             - Play a playlist",
			"  remove / rm <name>          - Remove track from library",
			"  author                      - Show authors",
			"  quit                        - Exit the player",
			NULL
		};

		getmaxyx(stdscr, rows, cols);
		clear();
		mvprintw(0, 2, "--- Help ---");

		for (int i = 0; help_lines[i]; i++) {
			if (line >= rows - 2) {
				mvprintw(rows - 2, 2, "...");
				break;
			}
			mvprintw(line++, 2, "%s", help_lines[i]);
		}

		attron(A_REVERSE);
		mvprintw(rows - 1, 0, "Press any key to return");
		attroff(A_REVERSE);

		refresh();
		timeout(-1);
		getch();
		timeout(100);

		snprintf(state->message, sizeof(state->message),
			 "Returned from help.");

	} else if (strcmp(command, "library") == 0 || strcmp(command, "lib") == 0) {
		int rows, cols, line = 2, mid;

		getmaxyx(stdscr, rows, cols);
		clear();
		mid = cols / 2;

		mvprintw(0, 2, "--- Library ---");
		mvprintw(0, mid + 2, "--- Playlists ---");

		for (int i = 0; i < rows - 4; i++) {
			if (i < state->track_count) {
				char left[256];

				snprintf(left, sizeof(left), "%d: %s",
					 i + 1, state->library[i].name);
				left[mid - 4] = '\0';
				mvprintw(i + 2, 2, "%s", left);
			}
			if (i < state->playlist_count) {
				char right[256];

				snprintf(right, sizeof(right), "%d: %s",
					 i + 1, state->playlists[i].name);
				right[cols - mid - 4] = '\0';
				mvprintw(i + 2, mid + 2, "%s", right);
			}
		}

		attron(A_REVERSE);
		mvprintw(rows - 1, 0, "Press any key to return");
		attroff(A_REVERSE);
		refresh();

		timeout(-1);
		getch();
		timeout(100);

		snprintf(state->message, sizeof(state->message),
			 "Returned from library view.");

	} else if (strcmp(command, "quit") == 0) {
		state->is_running = 0;

	} else if (strcmp(command, "play") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: play <track_name>");
		} else {
			int i, found = 0;

			state->playing_playlist_index = -1;
			state->playing_track_index_in_playlist = 0;

			for (i = 0; i < state->track_count; i++) {
				if (strcmp(state->library[i].name, argument) == 0) {
					play_track(state, state->library[i].path);
					found = 1;
					break;
				}
			}
			if (!found) {
				snprintf(state->message, sizeof(state->message),
					 "Error: Track '%s' not found in library.", argument);
			}
		}

	} else if (strcmp(command, "pause") == 0) {
		player_pause_toggle();
		snprintf(state->message, sizeof(state->message),
			 "Toggled pause/resume.");

	} else if (strcmp(command, "volume") == 0) {
		int rows, cols;

		getmaxyx(stdscr, rows, cols);
		clear();

		mvprintw(0, 4, "L-M-P");
		mvprintw(1, 2, "Current Volume: %d/100", state->current_volume);
		mvprintw(2, 2, "Use: setvolume <0-100>");

		attron(A_REVERSE);
		mvprintw(4, cols - 1, "Press any key to return");
		attroff(A_REVERSE);

		refresh();
		timeout(-1);
		getch();
		timeout(100);

		snprintf(state->message, sizeof(state->message),
			 "Returned from volume view.");

	} else if (strcmp(command, "setvolume") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: setvolume <volume>");
		} else {
			int v = atoi(argument);

			if (v < 0 || v > 100) {
				snprintf(state->message, sizeof(state->message),
					 "Please, enter a valid volume value (0-100)");
			} else {
				state->current_volume = v;
				player_set_volume(v);
				snprintf(state->message, sizeof(state->message),
					 "Volume set to %d", v);
				/* Persist volume */
				config_save(state);
			}
		}

	} else if (strcmp(command, "author") == 0) {
		snprintf(state->message, sizeof(state->message),
			 "---Authors---\n dormant1337: https://github.com/Zer0Flux86\n Syn4pse: https://github.com/BoLIIIoi\n");

	} else if (strcmp(command, "remove") == 0 || strcmp(command, "rm") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: remove <track_name>");
		} else {
			int i, idx = -1;

			for (i = 0; i < state->track_count; i++) {
				if (strcmp(state->library[i].name, argument) == 0) {
					idx = i;
					break;
				}
			}
			if (idx == -1) {
				snprintf(state->message, sizeof(state->message),
					 "Track not found...");
			} else {
				/* Update playlists: remove occurrences and shift indices */
				for (int p = 0; p < state->playlist_count; p++) {
					Playlist *pl = &state->playlists[p];
					int w = 0;

					for (int r = 0; r < pl->track_count; r++) {
						int t = pl->track_indices[r];

						if (t == idx)
							continue; /* skip removed track */
						if (t > idx)
							t--; /* shift indices down */
						pl->track_indices[w++] = t;
					}
					pl->track_count = w;
				}

				for (i = idx; i < state->track_count - 1; i++)
					state->library[i] = state->library[i + 1];
				state->track_count--;

				snprintf(state->message, sizeof(state->message),
					 "Removed track: '%s'", argument);

				/* Persist library and playlists */
				config_save(state);
			}
		}

	} else if (strcmp(command, "listnew") == 0 ||
		   strcmp(command, "createlist") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: %s <playlist_name>",
				 strcmp(command, "listnew") == 0 ? "listnew" : "createlist");
		} else {
			if (state->playlist_count >= 20) {
				snprintf(state->message, sizeof(state->message),
					 "Maximum playlists reached.");
			} else {
				int exists = 0;

				for (int i = 0; i < state->playlist_count; i++) {
					if (strcmp(state->playlists[i].name, argument) == 0) {
						exists = 1;
						break;
					}
				}
				if (exists) {
					snprintf(state->message, sizeof(state->message),
						 "Error: Playlist '%s' already exists.", argument);
				} else {
					Playlist *pl = &state->playlists[state->playlist_count];

					strncpy(pl->name, argument, sizeof(pl->name) - 1);
					pl->name[sizeof(pl->name) - 1] = '\0';
					pl->track_count = 0;
					state->playlist_count++;

					snprintf(state->message, sizeof(state->message),
						 "Created playlist '%s'.", argument);

					/* Persist playlists */
					config_save(state);
				}
			}
		}

	} else if (strcmp(command, "listadd") == 0) {
		char pl_name[50] = {0};
		char tr_name[50] = {0};
		const char *args = state->command_buffer + strlen(command);

		while (*args == ' ')
			args++;

		/* Parse playlist name (allow quotes) */
		if (*args == '"') {
			const char *start = args + 1;
			const char *end = strchr(start, '"');

			if (end) {
				size_t len = end - start;

				if (len > sizeof(pl_name) - 1)
					len = sizeof(pl_name) - 1;
				strncpy(pl_name, start, len);
				pl_name[len] = '\0';
				args = end + 1;
			}
		} else {
			const char *start = args;
			const char *end = strchr(start, ' ');

			if (end) {
				size_t len = end - start;

				if (len > sizeof(pl_name) - 1)
					len = sizeof(pl_name) - 1;
				strncpy(pl_name, start, len);
				pl_name[len] = '\0';
				args = end;
			}
		}

		while (*args == ' ')
			args++;

		/* Parse track name (allow quotes) */
		if (*args == '"') {
			const char *start = args + 1;
			const char *end = strchr(start, '"');

			if (end) {
				size_t len = end - start;

				if (len > sizeof(tr_name) - 1)
					len = sizeof(tr_name) - 1;
				strncpy(tr_name, start, len);
				tr_name[len] = '\0';
			}
		} else {
			strncpy(tr_name, args, sizeof(tr_name) - 1);
			tr_name[sizeof(tr_name) - 1] = '\0';
		}

		if (pl_name[0] == '\0' || tr_name[0] == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: listadd \"<playlist>\" \"<track>\"");
			return;
		}

		/* Find playlist */
		int pidx = -1, tidx = -1;

		for (int i = 0; i < state->playlist_count; i++) {
			if (strcmp(state->playlists[i].name, pl_name) == 0) {
				pidx = i;
				break;
			}
		}
		if (pidx == -1) {
			snprintf(state->message, sizeof(state->message),
				 "Error: Playlist '%s' not found.", pl_name);
			return;
		}

		/* Find track by name */
		for (int i = 0; i < state->track_count; i++) {
			if (strcmp(state->library[i].name, tr_name) == 0) {
				tidx = i;
				break;
			}
		}
		if (tidx == -1) {
			snprintf(state->message, sizeof(state->message),
				 "Error: Track '%s' not found in library.", tr_name);
			return;
		}

		if (state->playlists[pidx].track_count >= 100) {
			snprintf(state->message, sizeof(state->message),
				 "Error: Playlist '%s' is full.", state->playlists[pidx].name);
		} else {
			Playlist *pl = &state->playlists[pidx];

			pl->track_indices[pl->track_count] = tidx;
			pl->track_count++;

			snprintf(state->message, sizeof(state->message),
				 "Added '%s' to playlist '%s'.", tr_name, pl_name);

			/* Persist playlists */
			config_save(state);
		}

	} else if (strcmp(command, "listview") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: listview <playlist_name>");
			return;
		}

		int pidx = -1;

		for (int i = 0; i < state->playlist_count; i++) {
			if (strcmp(state->playlists[i].name, argument) == 0) {
				pidx = i;
				break;
			}
		}
		if (pidx == -1) {
			snprintf(state->message, sizeof(state->message),
				 "Error: Playlist '%s' not found.", argument);
			return;
		}

		{
			Playlist *pl = &state->playlists[pidx];
			int rows, cols, line = 2;

			getmaxyx(stdscr, rows, cols);
			clear();
			mvprintw(0, 2, "--- Playlist: %s ---", pl->name);

			if (pl->track_count == 0) {
				mvprintw(line++, 4, "Empty.");
			} else {
				for (int i = 0; i < pl->track_count; i++) {
					if (line >= rows - 2) {
						mvprintw(line, 4, "...");
						break;
					}
					int t = pl->track_indices[i];

					if (t >= 0 && t < state->track_count)
						mvprintw(line++, 4, "%d: %s",
							 i + 1, state->library[t].name);
				}
			}

			attron(A_REVERSE);
			mvprintw(rows - 1, 0, "Press any key to return");
			attroff(A_REVERSE);

			refresh();
			timeout(-1);
			getch();
			timeout(100);

			snprintf(state->message, sizeof(state->message),
				 "Returned from playlist view.");
		}

	} else if (strcmp(command, "listplay") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message),
				 "Usage: listplay <playlist_name>");
			return;
		}

		int pidx = -1;

		for (int i = 0; i < state->playlist_count; i++) {
			if (strcmp(state->playlists[i].name, argument) == 0) {
				pidx = i;
				break;
			}
		}
		if (pidx == -1) {
			snprintf(state->message, sizeof(state->message),
				 "Error: Playlist '%s' not found.", argument);
			return;
		}

		if (state->playlists[pidx].track_count == 0) {
			snprintf(state->message, sizeof(state->message),
				 "Error: Playlist '%s' is empty.", argument);
			return;
		}

		state->playing_playlist_index = pidx;
		state->playing_track_index_in_playlist = 0;

		{
			int lib_idx = state->playlists[pidx].track_indices[0];

			if (lib_idx >= 0 && lib_idx < state->track_count) {
				play_track(state, state->library[lib_idx].path);
				snprintf(state->message, sizeof(state->message),
					 "Playing playlist '%s'",
					 state->playlists[pidx].name);
			} else {
				snprintf(state->message, sizeof(state->message),
					 "Invalid track index in playlist.");
			}
		}

	} else if (strcmp(command, "stop") == 0) {
		player_stop();
		state->playing_playlist_index = -1;
		state->playing_track_index_in_playlist = 0;
		state->current_track[0] = '\0';
		state->track_duration = 0.0;
		snprintf(state->message, sizeof(state->message), "Playback stopped.");

	} else {
		snprintf(state->message, sizeof(state->message),
			 "Unknown command: %s", command);
	}
}