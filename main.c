#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "functions.h"

#include <SDL2/SDL_mixer.h> 


/*
 * Describes a single audio track.
 */
typedef struct {
	char	name[50];  
	char	path[256];
} Track;

/*
 * Holds the application's current state.
 */
typedef struct {
	Track	library[100]; 
	int	track_count;    
	int	is_running;
	int     current_volume;
	char	current_track[256];
	char	command_buffer[256];
	char	message[256];
	double  track_duration; /* Duration of the currently playing track */
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
	timeout(100); /* Make getch() non-blocking */
	
	state.current_volume = 100; /* Start with a reasonable volume */
	player_set_volume(state.current_volume);
	state.is_running = 1;
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
				
				timeout(-1); /* Make getstr blocking */
				getstr(state.command_buffer);
				timeout(100); /* Restore non-blocking */

				noecho();
				clear();
				refresh();

				handle_command(&state);
				break;
			}
		}
		
		/* Check if track has finished */
		if (state.current_track[0] != '\0' && !player_is_playing()) {
			strncpy(state.current_track, "", sizeof(state.current_track) -1);
			state.track_duration = 0.0;
			strncpy(state.message, "Playback Finished.", sizeof(state.message) -1);
		}
	}

	endwin();
	player_shutdown();
	return 0;
}

/**
 * draw_ui() - Redraws the entire terminal interface.
 * @state:	A pointer to the current application state.
 */
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
		const char *filename = strrchr(state->current_track, '/');
		if (filename) {
			filename++;
		} else {
			filename = state->current_track;
		}
		
		snprintf(display_buffer, sizeof(display_buffer), "%s - \"%s\"", status_text, filename);
		int text_len = strlen(display_buffer);
		if (cols > text_len + 2) {
			mvprintw(0, cols - text_len - 2, "%s", display_buffer);
		}
	}

	mvprintw(2, 2, "Message: %s", state->message);

	/* Draw progress bar */
	if (player_is_playing() && state->track_duration > 0) {
		double current_position = player_get_current_position();
		double duration = state->track_duration;
		char time_str[64];
		int bar_width, progress_width;

		if (current_position > duration) current_position = duration;

		snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d",
			(int)(current_position) / 60, (int)(current_position) % 60,
			(int)(duration) / 60, (int)(duration) % 60);

		/* Total non-bar space is for "  [] " (5 chars) and the time string */
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

/**
 * handle_command() - Parses and acts on a command from the user.
 * @state:	A pointer to the current application state.
 */
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
			strncpy(state->message, "Usage: add \"<name>\" <path>  OR  add <name> <path>", sizeof(state->message) -1);
		} else {
			if (state->track_count >= 100) {
				strncpy(state->message, "Library is full.", sizeof(state->message) -1);
			} else {
				Track *new_track = &state->library[state->track_count];
				strncpy(new_track->name, name, sizeof(new_track->name) - 1);
				strncpy(new_track->path, path, sizeof(new_track->path) - 1);
				state->track_count++;
				snprintf(state->message, sizeof(state->message), "Added: '%s'", name);
			}
		}
	} else if (strcmp(command, "help") == 0) {
		strncpy(state->message, "---Command List and instructions---\n1.<add> <track_name> <track_path.mp3>\n---Adds track to the library.\n\n2.<library>\n---Shows all tracks in the library.\n\n3.<play> <track_name>\n---Plays the track.\n\n4.<stop>\n---Stop current playing track.\n\n5.<pause>\n---Pauses current track. You can resume playing it by typing <pause> again.\n\n6.<quit>\n---Quit the program.", sizeof(state->message) -1);
	} else if (strcmp(command, "library") == 0 || strcmp(command, "lib") == 0) {
		int	rows, cols;
		getmaxyx(stdscr, rows, cols);
		clear();
		mvprintw(0, 2, "--- Library ---");

		int	last_line = 2;

		if (state->track_count == 0) {
			mvprintw(last_line, 4, "Empty.");
			last_line++;
		} else {
			for (int i = 0; i < state->track_count; i++) {
				if (last_line >= rows -1) {
					mvprintw(last_line, 4, "...");
					last_line++;
					break;
				}
				mvprintw(last_line, 4, "%d: %s {%s}", i + 1, state->library[i].name, state->library[i].path);
				last_line++;
			}
		}

		if (last_line >= rows) {
			last_line = rows - 1;
		}

		attron(A_REVERSE);
		mvprintw(last_line + 1, 0, "Press any key to return");
		attroff(A_REVERSE);

		refresh();
		timeout(-1); /* Wait for user input */
		getch(); 
		timeout(100); /* Restore non-blocking mode */
		strncpy(state->message, "Returned from library view.", sizeof(state->message) -1);
	} else if (strcmp(command, "quit") == 0) {
		state->is_running = 0;
	} else if (strcmp(command, "play") == 0) {
        if (!argument || *argument == '\0') {
                strncpy(state->message, "Usage: play <track_name>", sizeof(state->message) -1);
        } else {
                int i;
                int found = 0; 

                for (i = 0; i < state->track_count; i++) {
                        if (strcmp(state->library[i].name, argument) == 0) {
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
		strncpy(state->message, "Toggled pause/resume.", sizeof(state->message) -1);
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
		timeout(-1); /* Wait for user input */
		getch(); 
		timeout(100); /* Restore non-blocking mode */
		strncpy(state->message, "Returned from volume view.", sizeof(state->message) -1);

	} else if (strcmp(command, "setvolume") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message), "Usage: setvolume <volume>");
		} else {
			int volume = atoi(argument);
			if (volume < 0 || volume > 100) {
				snprintf(state->message, sizeof(state->message), "Please, enter a valid volume value (0-100)");
			} else {
				state->current_volume = volume;
				player_set_volume(volume);
				snprintf(state->message, sizeof(state->message), "Volume set to %d", state->current_volume);
			}
		}
	} else if (strcmp(command, "author") == 0) {
		strncpy(state->message, "---Authors---\n dormant1337: https://github.com/Zer0Flux86\n Syn4pse: https://github.com/BoLIIIoi\n", sizeof(state->message) -1);
	} else if (strcmp(command, "remove") == 0 || strcmp(command, "rm") == 0) {
		if (!argument || *argument == '\0') {
			snprintf(state->message, sizeof(state->message), "Usage: remove <track_name>");
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
				snprintf(state->message, sizeof(state->message), "Track not found...");
			} else {
				for (i = found_index; i < state->track_count - 1; i++) {
					state->library[i] = state->library[i + 1];
				}
				state->track_count--;
				snprintf(state->message, sizeof(state->message), "Removed track: '%s'", argument);
			}
		}


	} else if (strcmp(command, "stop") == 0) {
		player_stop();
		strncpy(state->current_track, "", sizeof(state->current_track) -1);
		state->track_duration = 0.0;
		strncpy(state->message, "Playback stopped.", sizeof(state->message) -1);
	} else {
			snprintf(state->message, sizeof(state->message), "Unknown command: %s", command);
	}
}