/* main.c - full file with addholder command, config integration, playlists */
/* help prints multi-line into message using \n */

// #include <ctype.h>
#include <dirent.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "handle_command.h"
#include "config.h"
#include "functions.h"
#include "main.h"

/* Prototypes */
static void handle_command(AppState *state);
void draw_ui(AppState *state);
void play_track(AppState *state, const char *track_path);

/* Helpers for addholder */
static int has_mp3_ext(const char *name) {
  const char *dot = strrchr(name, '.');

  if (!dot)
    return 0;
  return strcasecmp(dot, ".mp3") == 0;
}

static void strip_mp3_ext(const char *fname, char *out, size_t outlen) {
  const char *dot = strrchr(fname, '.');
  size_t n = dot ? (size_t)(dot - fname) : strlen(fname);

  if (!outlen)
    return;
  if (n >= outlen)
    n = outlen - 1;

  memcpy(out, fname, n);
  out[n] = '\0';
}

static void join_path(char *dst, size_t dstsz, const char *dir,
                      const char *name) {
  size_t len = strlen(dir);

  if (len > 0 && dir[len - 1] == '/')
    snprintf(dst, dstsz, "%s%s", dir, name);
  else
    snprintf(dst, dstsz, "%s/%s", dir, name);
}

static int track_name_exists(const AppState *state, const char *name) {
  int i;

  for (i = 0; i < state->track_count; i++) {
    if (strcmp(state->library[i].name, name) == 0)
      return 1;
  }
  return 0;
}

static int track_path_exists(const AppState *state, const char *path) {
  int i;

  for (i = 0; i < state->track_count; i++) {
    if (strcmp(state->library[i].path, path) == 0)
      return 1;
  }
  return 0;
}



void play_track(AppState *state, const char *track_path) {
  if (access(track_path, F_OK) != 0) {
    snprintf(state->message, sizeof(state->message),
             "Error: File not found: %s", track_path);
  } else {
    const char *track_display_name = NULL;
    int i;

    player_load_file(track_path);
    player_play();
    state->track_duration = get_mp3_duration(track_path);
    strncpy(state->current_track, track_path, sizeof(state->current_track) - 1);
    state->current_track[sizeof(state->current_track) - 1] = '\0';

    for (i = 0; i < state->track_count; i++) {
      if (strcmp(state->library[i].path, state->current_track) == 0) {
        track_display_name = state->library[i].name;
        break;
      }
    }

    if (!track_display_name)
      track_display_name = state->current_track;

    snprintf(state->message, sizeof(state->message), "Started playing: %s",
             track_display_name);

    /* Persist last track path, etc. */
    config_save(state);
  }
}

int main(int argc, char *argv[]) {
  AppState state = {0};
  int ch, rows, cols;

  srand(time(NULL));

  if (player_init() != 0) {
    fprintf(stderr, "Failed to initialize the audio player. Exiting.\n");
    return 1;
  }

  /* Defaults before load */
  state.current_volume = 100;
  state.is_running = 1;
  state.playing_playlist_index = -1;
  state.playing_track_index_in_playlist = 0;
  strncpy(state.mode, "no-repeat", sizeof(state.mode) - 1);

  /* Pre-allocate enough capacity for compatibility with old config_load()
   * that assumes static arrays. */
  if (ensure_library_capacity(&state, 100) != 0 ||
      ensure_playlists_capacity(&state, 20) != 0) {
    fprintf(stderr, "Out of memory.\n");
    player_shutdown();
    return 1;
  }

  /* CRITICAL: pre-allocate track_indices for all playlist slots,
   * so legacy config_load that writes into pl->track_indices[i]
   * won't crash on NULL. */
  for (int i = 0; i < state.playlists_cap; i++) {
    if (!state.playlists[i].track_indices) {
      state.playlists[i].track_indices =
          (int *)malloc(100 * sizeof(int)); /* legacy cap */
      if (!state.playlists[i].track_indices) {
        fprintf(stderr, "Out of memory (playlist indices).\n");
        player_shutdown();
        return 1;
      }
      state.playlists[i].track_capacity = 100;
      state.playlists[i].track_count = 0;
      state.playlists[i].name[0] = '\0';
    }
  }

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
        strncpy(state.command_buffer, "", sizeof(state.command_buffer) - 1);

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

    if (state.current_track[0] != '\0' && !player_is_playing()) {
      if (strcmp(state.mode, "repeat-one") == 0) {
        play_track(&state, state.current_track);
        snprintf(state.message, sizeof(state.message), "Repeating track.");
      } else if (strcmp(state.mode, "shuffle") == 0) {
        if (state.track_count > 0) {
          int next_track_index = rand() % state.track_count;
          play_track(&state, state.library[next_track_index].path);
          snprintf(state.message, sizeof(state.message),
                   "Shuffling to next track.");
        }
      } else if (state.playing_playlist_index != -1) {
        Playlist *pl = &state.playlists[state.playing_playlist_index];

        if (!pl->track_indices || pl->track_count <= 0) {
          state.playing_playlist_index = -1;
          state.current_track[0] = '\0';
          state.track_duration = 0.0;
          snprintf(state.message, sizeof(state.message),
                   "Playlist finished or invalid.");
          continue;
        }

        if (strcmp(state.mode, "repeat-all") == 0 &&
            state.playing_track_index_in_playlist >= pl->track_count - 1) {
          state.playing_track_index_in_playlist = 0;
        } else {
          state.playing_track_index_in_playlist++;
        }

        if (state.playing_track_index_in_playlist < pl->track_count) {
          int lib_idx =
              pl->track_indices[state.playing_track_index_in_playlist];
          if (lib_idx >= 0 && lib_idx < state.track_count) {
            play_track(&state, state.library[lib_idx].path);
            snprintf(state.message, sizeof(state.message),
                     "Now playing next track in '%s'", pl->name);
          } else {
            state.playing_playlist_index = -1;
            state.current_track[0] = '\0';
            state.track_duration = 0.0;
            snprintf(state.message, sizeof(state.message),
                     "Playlist '%s' finished (invalid index).", pl->name);
          }
        } else {
          state.playing_playlist_index = -1;
          state.current_track[0] = '\0';
          state.track_duration = 0.0;
          snprintf(state.message, sizeof(state.message),
                   "Playlist '%s' finished.", pl->name);
        }
      } else {
        state.current_track[0] = '\0';
        state.track_duration = 0.0;
        snprintf(state.message, sizeof(state.message), "Playback Finished.");
      }
    }
  }

  /* Persist on exit */
  config_save(&state);

  endwin();
  free_app_state(&state);
  player_shutdown();
  return 0;
}

void draw_ui(AppState *state) {
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
    const char *track_display_name = NULL;
    int i;

    for (i = 0; i < state->track_count; i++) {
      if (strcmp(state->library[i].path, state->current_track) == 0) {
        track_display_name = state->library[i].name;
        break;
      }
    }

    if (!track_display_name) {
      const char *filename = strrchr(state->current_track, '/');
      if (filename)
        track_display_name = filename + 1;
      else
        track_display_name = state->current_track;
    }

    if (state->playing_playlist_index != -1) {
      sprintf(temp_buffer, "Playlist: %s | ",
              state->playlists[state->playing_playlist_index].name);
    }

    sprintf(display_buffer, "%s - \"%s\" | Volume: %d", status_text,
            track_display_name, state->current_volume);
    strncat(temp_buffer, display_buffer,
            sizeof(temp_buffer) - strlen(temp_buffer) - 1);

    if ((int)strlen(temp_buffer) + 2 < cols)
      mvprintw(0, cols - (int)strlen(temp_buffer) - 2, "%s", temp_buffer);
  }
  mvprintw(1, cols - (int)strlen(state->mode) - 8, "Mode: %s", state->mode);

  mvprintw(2, 2, "Message: %s", state->message);

  if (player_is_playing() && state->track_duration > 0) {
    double pos = player_get_current_position();
    double dur = state->track_duration;
    char time_str[64];
    int bar_w, prog_w;

    if (pos > dur)
      pos = dur;

    sprintf(time_str, "%02d:%02d / %02d:%02d", (int)pos / 60, (int)pos % 60,
            (int)dur / 60, (int)dur % 60);

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

static void handle_command(AppState *state) {
  char buffer_copy[256];
  char *command;
  char *argument;

  strncpy(buffer_copy, state->command_buffer, sizeof(buffer_copy) - 1);
  buffer_copy[sizeof(buffer_copy) - 1] = '\0';
  command = strtok(buffer_copy, " ");
  if (!command)
    return;

  argument = strtok(NULL, "");
  if (argument) {
    while (*argument == ' ')
      argument++;
  }

  if (strcmp(command, "add") == 0) {                   cmd_add(state, argument);
  } else if (strcmp(command, "addfolder") == 0) {      cmd_addfolder(state, argument);
  } else if (strcmp(command, "webdownload") == 0) {    cmd_webdownload(state, argument);
  } else if (strcmp(command, "help") == 0) {           cmd_help(state, argument);
  } else if (strcmp(command, "rename") == 0) {         cmd_rename(state, argument);
  } else if (strcmp(command, "play") == 0) {           cmd_play(state, argument);
  } else if (strcmp(command, "pause") == 0) {          cmd_pause(state);
  } else if (strcmp(command, "volume") == 0) {         cmd_volume(state, argument);
  } else if (strcmp(command, "setvolume") == 0) {      cmd_setvolume(state, argument);
  } else if (strcmp(command, "author") == 0) {         cmd_show_authors(state);
  } else if (strcmp(command, "setmode") == 0) {        cmd_setmode(state, argument);
  } else if (strcmp(command, "mode") == 0) {           cmd_mode(state);
  } else if (strcmp(command, "listaddmulti") == 0) {   cmd_listaddmulti(state, argument);
  } else if (strcmp(command, "deletelist") == 0) {     cmd_deletelist(state, argument);
  } else if (strcmp(command, "listadd") == 0) {        cmd_listadd(state, argument);
  } else if (strcmp(command, "listview") == 0) {       cmd_listview(state, argument);
  } else if (strcmp(command, "listplay") == 0) {       cmd_listplay(state, argument);
  } else if (strcmp(command, "stop") == 0) {           cmd_stop(state);
  } else if (strcmp(command, "search") == 0) {         cmd_search(state, argument);
  //          //          //          //          //          //          //          //          //          //
  } else if (strcmp(command, "quit") == 0) {   state->is_running = 0;
  } else if (strcmp(command, "remove") == 0 || strcmp(command, "rm") == 0) { cmd_remove(state,argument);
  } else if (strcmp(command, "listnew") == 0 || strcmp(command, "createlist") == 0) { cmd_listnew(state, argument, command);
  } else if (strcmp(command, "listremove") == 0 || strcmp(command, "listrem") == 0) { cmd_listremove(state, argument);
  } else if (strcmp(command, "library") == 0 || strcmp(command, "lib") == 0) { cmd_library(state, argument);
  } else {
    snprintf(state->message, sizeof(state->message), "Unknown command: %s",
             command);
  }
}