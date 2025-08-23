/* main.c - full file with addholder command, config integration, playlists */
/* help prints multi-line into message using \n */

#include <ctype.h>
#include <dirent.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "functions.h"
#include "main.h"

/* Prototypes */
static void handle_command(AppState *state);
static void draw_ui(AppState *state);
static void play_track(AppState *state, const char *track_path);

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



static void play_track(AppState *state, const char *track_path) {
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

static void draw_ui(AppState *state) {
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

  if (strcmp(command, "add") == 0) {
    char name[50] = {0};
    char path[256] = {0};

    if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message), "Usage: add <track_name> <file_path>");
      return;
    }


    const char *args = argument;

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

        {
          const char *path_start = name_end + 1;

          while (*path_start == ' ')
            path_start++;
          strncpy(path, path_start, sizeof(path) - 1);
          path[sizeof(path) - 1] = '\0';
        }
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

        {
          const char *path_start = name_end + 1;

          while (*path_start == ' ')
            path_start++;
          strncpy(path, path_start, sizeof(path) - 1);
          path[sizeof(path) - 1] = '\0';
        }
      }
    }

    if (name[0] == '\0' || path[0] == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: add \"<name>\" <path>  OR  add <name> <path>");
      return;
    }

    if (ensure_library_capacity(state, 1) != 0) {
      snprintf(state->message, sizeof(state->message),
               "Error: cannot grow library (out of memory).");
      return;
    }
    if (access(path, F_OK) != 0) {
      snprintf(state->message, sizeof(state->message),
               "Error: File not found: %s", path);
      return;
    }

    strncpy(state->library[state->track_count].name, name,
            sizeof(state->library[state->track_count].name) - 1);
    state->library[state->track_count]
        .name[sizeof(state->library[state->track_count].name) - 1] = '\0';
    strncpy(state->library[state->track_count].path, path,
            sizeof(state->library[state->track_count].path) - 1);
    state->library[state->track_count]
        .path[sizeof(state->library[state->track_count].path) - 1] = '\0';
    state->track_count++;

    snprintf(state->message, sizeof(state->message), "Added '%s' to library.",
             name);

    config_save(state);

  } else if (strcmp(command, "addfolder") == 0) {
    addfolder(state, argument);
  } else if (strcmp(command, "webdownload") == 0) {
    if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: webdownload <track_name>");
    } else {
      char dl_command[1024];
      char download_target_dir[512];
      char safe_argument[256];
      const char *home = getenv("HOME");

      int j = 0;
      for (int i = 0; argument[i] != '\0' && j < sizeof(safe_argument) - 1; i++) {
        if (isalnum((unsigned char)argument[i]) || isspace((unsigned char)argument[i]) ||
                argument[i] == '-' || argument[i] == '_' || argument[i] == '.') {
                safe_argument[j++] = argument[i];
            }
      }
      safe_argument[j] = '\0';

      if (safe_argument[0] == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: webdownload <track_name>");
      return;
      }

      if (home) {
        snprintf(download_target_dir, sizeof(download_target_dir), "%s/LMP/downloads", home);

      {
        char tmp_path[512];
        snprintf(tmp_path, sizeof(tmp_path), "%s/LMP", home); mkdir(tmp_path, 0755);
        snprintf(tmp_path, sizeof(tmp_path), "%s/LMP/downloads", home); mkdir(tmp_path, 0755);
        mkdir(download_target_dir, 0755);
      }
        snprintf(dl_command, sizeof(dl_command),
                 "spotdl download \"%s\" --output \"%s\"", safe_argument, download_target_dir);

        snprintf(state->message, sizeof(state->message),
                 "Downloading '%s'... please wait.", safe_argument);
        draw_ui(state);
        refresh();

        fprintf(stderr, "DEBUG: Executing command: %s\n", dl_command);
        
        int result = system(dl_command);

        if (result == 0) {
          addfolder(state, download_target_dir);
          snprintf(state->message, sizeof(state->message),
                   "Finished downloading. '%s' added to library.", safe_argument);
        } else {
          snprintf(state->message, sizeof(state->message),
                   "Error downloading track. Is spotdl installed and "
                   "configured correctly?");
        }
      } else {
        snprintf(state->message, sizeof(state->message),
                 "Could not find HOME directory.");
      }
    }
  } else if (strcmp(command, "help") == 0) {
    const char *msg = "Help:\n"
                      "add \"<name>\" <path>       - Add track to library\n"
                      "addfolder <dir>           - Add all *.mp3 from dir (name=file sans .mp3)\n"
                      "webdownload <track_name>  - Download via spotdl into LMP and import\n"
                      "library / lib             - Show library & playlists\n"
                      "play <name>               - Play a track from library\n"
                      "pause                     - Toggle pause/resume\n"
                      "stop                      - Stop playback\n"
                      "volume                    - Show current volume\n"
                      "setvolume <0-100>         - Set the volume\n"
                      "setmode <mode>            - Set playback mode (no-repeat, repeat-one, repeat-all, shuffle)\n"
                      "listnew <name>            - Create a new playlist\n"
                      "createlist <name>         - Alias to listnew\n"
                      "listadd \"<pl>\" \"<track>\"  - Add track to a playlist\n"
                      "listaddmulti <pl> <id>..  - Add multiple tracks to playlist by ID from library.\n"
                      "listview <name>           - View tracks in a playlist\n"
                      "listplay <name>           - Play a playlist\n"
                      "remove / rm <name>        - Remove track from library\n"
                      "author                    - Show authors\n"
                      "quit                      - Exit the player";
    snprintf(state->message, sizeof(state->message), "%s", msg);

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

        snprintf(left, sizeof(left), "%d: %s", i + 1, state->library[i].name);
        left[mid - 4] = '\0';
        mvprintw(i + 2, 2, "%s", left);
      }
      if (i < state->playlist_count) {
        char right[256];

        snprintf(right, sizeof(right), "%d: %s", i + 1,
                 state->playlists[i].name);
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
    snprintf(state->message, sizeof(state->message), "Toggled pause/resume.");

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
        snprintf(state->message, sizeof(state->message), "Volume set to %d", v);
        config_save(state);
      }
    }

  } else if (strcmp(command, "author") == 0) {
    snprintf(state->message, sizeof(state->message),
             "---Authors---\n dormant1337: https://github.com/Zer0Flux86\n "
             "Syn4pse: https://github.com/BoLIIIoi\n");

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
        snprintf(state->message, sizeof(state->message), "Track not found...");
      } else {
        for (int p = 0; p < state->playlist_count; p++) {
          Playlist *pl = &state->playlists[p];
          int w = 0;

          if (!pl->track_indices || pl->track_count <= 0) {
            pl->track_count = 0;
            continue;
          }

          for (int r = 0; r < pl->track_count; r++) {
            int t = pl->track_indices[r];

            if (t == idx)
              continue;
            if (t > idx)
              t--;
            pl->track_indices[w++] = t;
          }
          pl->track_count = w;
        }

        for (i = idx; i < state->track_count - 1; i++)
          state->library[i] = state->library[i + 1];
        state->track_count--;

        snprintf(state->message, sizeof(state->message), "Removed track: '%s'",
                 argument);

        config_save(state);
      }
    }

  } else if (strcmp(command, "setmode") == 0) {
    if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: setmode <mode>\nValid modes are: no-repeat, repeat-one, "
               "repeat-all, shuffle.");
    } else {
      if (strcasecmp(argument, "no-repeat") == 0 ||
          strcasecmp(argument, "repeat-one") == 0 ||
          strcasecmp(argument, "repeat-all") == 0 ||
          strcasecmp(argument, "shuffle") == 0) {
        strncpy(state->mode, argument, sizeof(state->mode) - 1);
        state->mode[sizeof(state->mode) - 1] = '\0';
        snprintf(state->message, sizeof(state->message), "Mode set to: %s",
                 argument);
      } else {
        snprintf(state->message, sizeof(state->message), "Invalid mode: %s",
                 argument);
      }
    }
  } else if (strcmp(command, "mode") == 0) {
    sprintf(state->message, "Use setmode to change the playback mode.");
  } else if (strcmp(command, "listaddmulti") == 0) {
    char *pl_name;
    char *token;
    int pidx = -1;
    int added_count = 0;
    Playlist *pl;

    if (!argument) {
      snprintf(state->message, sizeof(state->message),
               "Usage: listaddmulti <playlist> <id1> <id2> ...");
      return;
    }

    pl_name = strtok(argument, " ");
    if (!pl_name) {
      snprintf(state->message, sizeof(state->message),
               "Usage: listaddmulti <playlist> <id1> <id2> ...");
      return;
    }

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

    pl = &state->playlists[pidx];

    while ((token = strtok(NULL, " ")) != NULL) {
      int track_id = atoi(token);

      if (track_id <= 0 || track_id > state->track_count) {
        continue;
      }

      if (ensure_playlist_tracks_capacity(pl, 1) != 0) {
        snprintf(state->message, sizeof(state->message),
                 "Playlist '%s': cannot grow (out of memory). Added %d so far.",
                 pl->name, added_count);
        if (added_count > 0)
          config_save(state);
        return;
      }

      pl->track_indices[pl->track_count] = track_id - 1;
      pl->track_count++;
      added_count++;
    }

    if (added_count > 0) {
      snprintf(state->message, sizeof(state->message),
               "Added %d tracks to playlist '%s'.", added_count, pl->name);
      config_save(state);
    } else {
      snprintf(state->message, sizeof(state->message),
               "No valid tracks added. Check track IDs.");
    }
  } else if (strcmp(command, "listnew") == 0 ||
             strcmp(command, "createlist") == 0) {
    if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: %s <playlist_name>",
               strcmp(command, "listnew") == 0 ? "listnew" : "createlist");
    } else {
      if (ensure_playlists_capacity(state, 1) != 0) {
        snprintf(state->message, sizeof(state->message),
                 "Error: cannot grow playlists (out of memory).");
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

          if (!pl->track_indices) {
            pl->track_indices = (int *)malloc(100 * sizeof(int));
            if (!pl->track_indices) {
              snprintf(state->message, sizeof(state->message),
                       "Out of memory (new playlist indices).");
              return;
            }
            pl->track_capacity = 100;
          }
          pl->track_count = 0;

          strncpy(pl->name, argument, sizeof(pl->name) - 1);
          pl->name[sizeof(pl->name) - 1] = '\0';

          state->playlist_count++;

          snprintf(state->message, sizeof(state->message),
                   "Created playlist '%s'.", argument);

          config_save(state);
        }
      }
    }

  } else if (strcmp(command, "listadd") == 0) {
    char pl_name[50] = {0};
    char tr_name[50] = {0};

    if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message),
    "Error: No playlist name given.");
    return;
    }


    const char *args = argument;

    while (*args == ' ')
      args++;

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

    if (ensure_playlist_tracks_capacity(&state->playlists[pidx], 1) != 0) {
      snprintf(state->message, sizeof(state->message),
               "Error: Playlist '%s' cannot grow (out of memory).",
               state->playlists[pidx].name);
    } else {
      Playlist *pl = &state->playlists[pidx];

      pl->track_indices[pl->track_count] = tidx;
      pl->track_count++;

      snprintf(state->message, sizeof(state->message),
               "Added '%s' to playlist '%s'.", tr_name, pl_name);

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

      if (!pl->track_indices || pl->track_count == 0) {
        mvprintw(line++, 4, "Empty.");
      } else {
        for (int i = 0; i < pl->track_count; i++) {
          if (line >= rows - 2) {
            mvprintw(line, 4, "...");
            break;
          }
          int t = pl->track_indices[i];

          if (t >= 0 && t < state->track_count)
            mvprintw(line++, 4, "%d: %s", i + 1, state->library[t].name);
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

    if (!state->playlists[pidx].track_indices ||
        state->playlists[pidx].track_count == 0) {
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
                 "Playing playlist '%s'", state->playlists[pidx].name);
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
    snprintf(state->message, sizeof(state->message), "Unknown command: %s",
             command);
  }
}