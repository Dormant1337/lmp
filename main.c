#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "functions.h"

#include <SDL2/SDL_mixer.h> 


typedef struct {
	char	name[50];  
	char	path[256];
} Track;

typedef struct {
	char name[50];
	int track_indices[100];
	int track_count;
} Playlist;

typedef struct {
	Track	 library[100]; 
	Playlist playlists[20];
	int      playlist_count;
	int	 track_count;    
	int	 is_running;
	int      current_volume;
	char	 current_track[256];
	char	 command_buffer[256];
	char	 message[256];
	double   track_duration;
	int      playing_playlist_index;
    int      playing_track_index_in_playlist;
} AppState;


void handle_command(AppState *state);
void draw_ui(AppState *state);

void play_track(AppState *state, const char *track_path)
{
	if (access(track_path, F_OK) != 0) {
		snprintf(state->message, sizeof(state->message), "Error: File not found: %s", track_path);
	} else {
		player_load_file(track_path);
		player_play();
		state->track_duration = get_mp3_duration(track_path);
		strncpy(state->current_track, track_path, sizeof(state->current_track) - 1);
		state->current_track[sizeof(state->current_track) - 1] = '\0';
		snprintf(state->message, sizeof(state->message), "Started playing: %s", state->current_track);
	}
}

int main(int argc, char *argv[])
{
	
	AppState state = {0};
	int	ch, rows, cols;

	if (player_init() != 0) {
                fprintf(stderr, "Failed to initialize the audio player. Exiting.\n");
                return 1;
        }

	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	timeout(100);
	
	state.current_volume = 100;
	player_set_volume(state.current_volume);
	state.is_running = 1;
	state.playing_playlist_index = -1;
	strncpy(state.message, "Welcome to lmp!", sizeof(state.message) -1);

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
				strncpy(state.command_buffer, "", sizeof(state.command_buffer) -1);

				attron(A_REVERSE);
				mvprintw(rows - 1, 0, ":");
				for (int i = 1; i < cols; i++) {
					printw(" ");
				}
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
		
		
		if (state.current_track[0] != '\0' && !player_is_playing()) {
			if (state.playing_playlist_index != -1) {
				Playlist *playlist = &state.playlists[state.playing_playlist_index];
				
				state.playing_track_index_in_playlist++;
				
				if (state.playing_track_index_in_playlist < playlist->track_count) {
					int library_track_index = playlist->track_indices[state.playing_track_index_in_playlist];
					play_track(&state, state.library[library_track_index].path);
					snprintf(state.message, sizeof(state.message), "Now playing next track in '%s'", playlist->name);
				} else {
					state.playing_playlist_index = -1;
					strncpy(state.current_track, "", sizeof(state.current_track) - 1);
					state.track_duration = 0.0;
					snprintf(state.message, sizeof(state.message), "Playlist '%s' finished.", playlist->name);
				}
			} else {
				strncpy(state.current_track, "", sizeof(state.current_track) - 1);
				state.track_duration = 0.0;
				snprintf(state.message, sizeof(state.message), "Playback Finished.");
			}
		}
	}

	endwin();
	player_shutdown();
	return 0;
}

void draw_ui(AppState *state)
{
	int	rows, cols;
	char	display_buffer[256];
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
		
		if (state->playing_playlist_index != -1) {
			snprintf(temp_buffer, sizeof(temp_buffer), "Playlist: %s | ", 
				 state->playlists[state->playing_playlist_index].name);
		}

		const char *filename = strrchr(state->current_track, '/');
		if (filename) {
			filename++;
		} else {
			filename = state->current_track;
		}
		
		char track_vol_buffer[200];
		snprintf(track_vol_buffer, sizeof(track_vol_buffer), "%s - \"%s\" | Volume: %d", 
			 status_text, filename, state->current_volume);

		strncat(temp_buffer, track_vol_buffer, sizeof(temp_buffer) - strlen(temp_buffer) - 1);
		
		int text_len = strlen(temp_buffer);
		if (cols > text_len + 2) {
			mvprintw(0, cols - text_len - 2, "%s", temp_buffer);
		}
	}

	mvprintw(2, 2, "Message: %s", state->message);

	if (player_is_playing() && state->track_duration > 0) {
		double current_position = player_get_current_position();
		double duration = state->track_duration;
		char time_str[64];
		int bar_width, progress_width;

		if (current_position > duration) current_position = duration;

		snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d",
			(int)(current_position) / 60, (int)(current_position) % 60,
			(int)(duration) / 60, (int)(duration) % 60);

		bar_width = cols - 5 - (int)strlen(time_str);
		if (bar_width < 10) bar_width = 10;

		progress_width = (duration > 0.0) ? (int)((current_position / duration) * bar_width) : 0;
		if (progress_width > bar_width) progress_width = bar_width;

		mvprintw(rows - 3, 0, "  [");
		attron(A_REVERSE);
		for (int i = 0; i < progress_width; i++) printw(" ");
		attroff(A_REVERSE);
		for (int i = progress_width; i < bar_width; i++) printw("-");
		printw("] %s", time_str);
	}


	attron(A_REVERSE);
	mvprintw(rows - 1, 0, "Press ':' to enter command mode | 'q' to quit");
	attroff(A_REVERSE);

	refresh();
}

void handle_command(AppState *state)
{
	char	buffer_copy[256];
	char	*command;
	char	*argument;

	strncpy(buffer_copy, state->command_buffer, sizeof(buffer_copy) -1);
	command = strtok(buffer_copy, " ");

	if (command == NULL) {
		return;
	}
	
	argument = strtok(NULL, "");

	if (argument) {
		while (*argument && *argument == ' ') {
			argument++;
		}
	}


	if (strcmp(command, "add") == 0) {
		char name[50] = {0};
		char path[256] = {0};
		const char *args = state->command_buffer + strlen(command);

		while (*args == ' ') {
			args++;
		}

		if (*args == '"') {
			const char *name_start = args + 1;
			const char *name_end = strchr(name_start, '"');

			if (name_end) {
				size_t name_len = name_end - name_start;
				if (name_len > sizeof(name) - 1) name_len = sizeof(name) - 1;
				strncpy(name, name_start, name_len);
				name[name_len] = '\0';

				const char *path_start = name_end + 1;
				while (*path_start == ' ') {
					path_start++;
				}
				strncpy(path, path_start, sizeof(path) - 1);
			}
		} else {
			const char *name_start = args;
			const char *name_end = strchr(name_start, ' ');

			if (name_end) {
				size_t name_len = name_end - name_start;
				if (name_len > sizeof(name) - 1) name_len = sizeof(name) - 1;
				strncpy(name, name_start, name_len);
				name[name_len] = '\0';

				const char *path_start = name_end + 1;
				while (*path_start == ' ') {
					path_start++;
				}
				strncpy(path, path_start, sizeof(path) - 1);
			}
		}

		if (name[0] == '\0' || path[0] == '\0') {
			sprintf(state->message, "Usage: add \"<name>\" <path>  OR  add <name> <path>");
		} else {
			if (state->track_count >= 100) {
				sprintf(state->message, "Library is full.");
			} else {
				Track *new_track = &state->library[state->track_count];
				strncpy(new_track->name, name, sizeof(new_track->name) - 1);
				strncpy(new_track->path, path, sizeof(new_track->path) - 1);
				state->track_count++;
				sprintf(state->message, "Added: '%s'", name);
			}
		}
	} else if (strcmp(command, "help") == 0) {
		const char *help_message = "Commands:\n"
			"  add \"<name>\" <path>    - Add track to library\n"
			"  library / lib            - Show library & playlists\n"
			"  play <name>              - Play a track from library\n"
			"  pause                    - Toggle pause/resume\n"
			"  stop                     - Stop playback\n"
			"  setvolume <0-100>        - Set the volume\n"
			"  createlist <name>        - Create a new playlist\n"
			"  listadd \"<pl>\" \"<track>\"   - Add track to a playlist\n"
			"  listview <name>          - View tracks in a playlist\n"
			"  listplay <name>          - Play a playlist\n"
			"  remove / rm <name>       - Remove track from library\n"
			"  quit                     - Exit the player";
		sprintf(state->message, "%s", help_message);
	} else if (strcmp(command, "library") == 0 || strcmp(command, "lib") == 0) {
		int	rows, cols;
		getmaxyx(stdscr, rows, cols);
		clear();

		int mid_col = cols / 2;
		int max_items = (state->track_count > state->playlist_count) ? state->track_count : state->playlist_count;
		int content_rows = rows - 4;

		mvprintw(0, 2, "--- Library ---");
		mvprintw(0, mid_col + 2, "--- Playlists ---");

		for (int i = 0; i < content_rows; i++) {
			if (i >= max_items) {
				break;
			}

			if (i < state->track_count) {
				char track_str[256];
				sprintf(track_str, "%d: %s", i + 1, state->library[i].name);
				track_str[mid_col - 4] = '\0';
				mvprintw(i + 2, 2, "%s", track_str);
			}

			if (i < state->playlist_count) {
				char playlist_str[256];
				sprintf(playlist_str, "%d: %s", i + 1, state->playlists[i].name);
				playlist_str[cols - mid_col - 4] = '\0';
				mvprintw(i + 2, mid_col + 2, "%s", playlist_str);
			}
		}
		
		if (max_items > content_rows) {
			mvprintw(rows - 2, 2, "...");
		}

		attron(A_REVERSE);
		mvprintw(rows - 1, 0, "Press any key to return");
		attroff(A_REVERSE);

		refresh();
		timeout(-1);
		getch(); 
		timeout(100);
		sprintf(state->message, "Returned from library view.");
	} else if (strcmp(command, "quit") == 0) {
		state->is_running = 0;
	} else if (strcmp(command, "play") == 0) {
        if (!argument || *argument == '\0') {
                snprintf(state->message, sizeof(state->message), "Usage: play <track_name>");
        } else {
                int i;
                int found = 0; 

                for (i = 0; i < state->track_count; i++) {
                        if (strcmp(state->library[i].name, argument) == 0) {
				state->playing_playlist_index = -1;
                                play_track(state, state->library[i].path);
                                found = 1; 
                                break;   
                        }
                }

                if (!found) {
                        snprintf(state->message, sizeof(state->message), "Error: Track '%s' not found in library.", argument);
                }
        }
	} else if (strcmp(command, "pause") == 0) {
		player_pause_toggle();
		sprintf(state->message, "Toggled pause/resume.");
	} else if (strcmp(command, "volume") == 0) {
		int rows, cols;
		getmaxyx(stdscr, rows, cols);
		
		mvprintw(0, 4, "L-M-P");
		mvprintw(1, 2, "Current Volume: %d/100", state->current_volume);
		mvprintw(2, 2, "Write <setvolume> to set volume. (0-100)");

		attron(A_REVERSE);
		mvprintw(4, cols - 1, "Press any key to return");
		attroff(A_REVERSE);

		refresh();
		timeout(-1);
		getch(); 
		timeout(100);
		sprintf(state->message, "Returned from volume view.");

	} else if (strcmp(command, "setvolume") == 0) {
		if (!argument || *argument == '\0') {
			sprintf(state->message, "Usage: setvolume <volume>");
		} else {
			int volume = atoi(argument);
			if (volume < 0 || volume > 100) {
				sprintf(state->message, "Please, enter a valid volume value (0-100)");
			} else {
				state->current_volume = volume;
				player_set_volume(volume);
				sprintf(state->message, "Volume set to %d", state->current_volume);
			}
		}
	} else if (strcmp(command, "author") == 0) {
		sprintf(state->message, "---Authors---\n dormant1337: https://github.com/Zer0Flux86\n Syn4pse: https://github.com/BoLIIIoi\n");
	} else if (strcmp(command, "remove") == 0 || strcmp(command, "rm") == 0) {
		if (!argument || *argument == '\0') {
			sprintf(state->message, "Usage: remove <track_name>");
		} else {
			int i;
			int found_index = -1;
			for (i = 0; i < state->track_count; i++) {
				if (strcmp(state->library[i].name, argument) == 0) {
					found_index = i;
					break;
				}
			}
			if(found_index == -1) {
				sprintf(state->message, "Track not found...");
			} else {
				for (i = found_index; i < state->track_count - 1; i++) {
					state->library[i] = state->library[i + 1];
				}
				state->track_count--;
				sprintf(state->message, "Removed track: '%s'", argument);
			}
		}


	} else if (strcmp(command, "createlist") == 0) {
		if (!argument || *argument == '\0') {
			sprintf(state->message, "Usage: createlist <list_name>");
		} else {
			if (state->playlist_count >= 20) {
				sprintf(state->message, "Error: Maximum number of lists reached");
			} else {
				int found = 0;
				for (int i = 0; i < state->playlist_count; i++) {
					if (strcmp(state->playlists[i].name, argument) == 0) {
						found = 1;
						break;
					}
				}
				if (found) {
					sprintf(state->message, "Error: List already exists");
				} else {
					Playlist *new_playlist = &state->playlists[state->playlist_count];
					strncpy(new_playlist->name, argument, sizeof(new_playlist->name) - 1);
					new_playlist->name[sizeof(new_playlist->name) - 1] = '\0';
					new_playlist->track_count = 0;
					state->playlist_count++;
					sprintf(state->message, "List '%s' succesfully created.", argument);

				}
			}
		}
	} else if (strcmp(command, "listadd") == 0) {
		char playlist_name[50] = {0};
		char track_name[50] = {0};
		const char *args = state->command_buffer + strlen(command);

		while (*args == ' ') {
			args++;
		}

		if (*args == '"') {
			const char *name_start = args + 1;
			const char *name_end = strchr(name_start, '"');
			if (name_end) {
				size_t len = name_end - name_start;
				if (len > sizeof(playlist_name) - 1) len = sizeof(playlist_name) - 1;
				strncpy(playlist_name, name_start, len);
				playlist_name[len] = '\0';
				args = name_end + 1;
			}
		} else {
			const char *name_start = args;
			const char *name_end = strchr(name_start, ' ');
			if (name_end) {
				size_t len = name_end - name_start;
				if (len > sizeof(playlist_name) - 1) len = sizeof(playlist_name) - 1;
				strncpy(playlist_name, name_start, len);
				playlist_name[len] = '\0';
				args = name_end;
			}
		}

		while (*args == ' ') {
			args++;
		}

		if (*args == '"') {
			const char *name_start = args + 1;
			const char *name_end = strchr(name_start, '"');
			if (name_end) {
				size_t len = name_end - name_start;
				if (len > sizeof(track_name) - 1) len = sizeof(track_name) - 1;
				strncpy(track_name, name_start, len);
				track_name[len] = '\0';
			}
		} else {
			strncpy(track_name, args, sizeof(track_name) - 1);
			track_name[sizeof(track_name) - 1] = '\0';
		}


		if (playlist_name[0] == '\0' || track_name[0] == '\0') {
			sprintf(state->message, "Usage: listadd \"<playlist>\" \"<track>\"");
			return;
		}

		int playlist_index = -1;
		for (int i = 0; i < state->playlist_count; i++) {
			if (strcmp(state->playlists[i].name, playlist_name) == 0) {
				playlist_index = i;
				break;
			}
		}

		if (playlist_index == -1) {
			sprintf(state->message, "Error: Playlist '%s' not found.", playlist_name);
			return;
		}

		int track_index = -1;
		for (int i = 0; i < state->track_count; i++) {
			if (strcmp(state->library[i].name, track_name) == 0) {
				track_index = i;
				break;
			}
		}

		if (track_index == -1) {
			sprintf(state->message, "Error: Track '%s' not found in library.", track_name);
			return;
		}

		Playlist *playlist = &state->playlists[playlist_index];
		if (playlist->track_count >= 100) {
			sprintf(state->message, "Error: Playlist '%s' is full.", playlist->name);
		} else {
			playlist->track_indices[playlist->track_count] = track_index;
			playlist->track_count++;
			sprintf(state->message, "Added '%s' to playlist '%s'.", track_name, playlist_name);
		}
	} else if (strcmp(command, "listview") == 0) {
		if (!argument || *argument == '\0') {
			sprintf(state->message, "Usage: listview <playlist_name>");
			return;
		}

		int playlist_index = -1;
		for (int i = 0; i < state->playlist_count; i++) {
			if (strcmp(state->playlists[i].name, argument) == 0) {
				playlist_index = i;
				break;
			}
		}

		if (playlist_index == -1) {
			sprintf(state->message, "Error: Playlist '%s' not found.", argument);
			return;
		}

		Playlist *playlist = &state->playlists[playlist_index];
		int rows, cols;
		getmaxyx(stdscr, rows, cols);
		clear();
		mvprintw(0, 2, "--- Playlist: %s ---", playlist->name);

		int last_line = 2;
		if (playlist->track_count == 0) {
			mvprintw(last_line, 4, "Empty.");
		} else {
			for (int i = 0; i < playlist->track_count; i++) {
				if (last_line >= rows - 2) {
					mvprintw(last_line, 4, "...");
					break;
				}
				int track_idx = playlist->track_indices[i];
				mvprintw(last_line, 4, "%d: %s", i + 1, state->library[track_idx].name);
				last_line++;
			}
		}

		attron(A_REVERSE);
		mvprintw(rows - 1, 0, "Press any key to return");
		attroff(A_REVERSE);

		refresh();
		timeout(-1);
		getch();
		timeout(100);
		sprintf(state->message, "Returned from playlist view.");
	} else if (strcmp(command, "listplay") == 0) {
		if (!argument || *argument == '\0') {
			sprintf(state->message, "Usage: listplay <playlist_name>");
			return;
		}

		int playlist_index = -1;
		for (int i = 0; i < state->playlist_count; i++) {
			if (strcmp(state->playlists[i].name, argument) == 0) {
				playlist_index = i;
				break;
			}
		}

		if (playlist_index == -1) {
			sprintf(state->message, "Error: Playlist '%s' not found.", argument);
			return;
		}

		Playlist *playlist = &state->playlists[playlist_index];
		if (playlist->track_count == 0) {
			sprintf(state->message, "Error: Playlist '%s' is empty.", argument);
			return;
		}

		state->playing_playlist_index = playlist_index;
		state->playing_track_index_in_playlist = 0;

		int library_track_index = playlist->track_indices[0];

		play_track(state, state->library[library_track_index].path);
		sprintf(state->message, "Playing playlist '%s'", playlist->name);
	} else if (strcmp(command, "stop") == 0) {
		player_stop();
		state->playing_playlist_index = -1;
		strncpy(state->current_track, "", sizeof(state->current_track) -1);
		state->track_duration = 0.0;
		sprintf(state->message, "Playback stopped.");
	} else {
			sprintf(state->message, "Unknown command: %s", command);
	}
}
