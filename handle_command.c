
#include "config.h"
#include "main.h"
#include "functions.h" 
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <strings.h>   
#include <sys/stat.h>
#include <unistd.h>    


void cmd_add(AppState *state, const char *argument) {
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
}

void cmd_addfolder(AppState *state, const char *argument) {
    addfolder(state, argument);
}



void cmd_webdownload(AppState *state, const char *argument) {
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
}

void cmd_help(AppState *state, const char *argument) {
    const char *msg = "Help:\n"
                      "add \"<name>\" <path>       - Add track to library\n"
                      "rename <index> <new_name> - Rename a track in library by its index\n"
                      "remove / rm <name>        - Remove track from library\n"
                      "addfolder <dir>           - Add all *.mp3 from dir (name=file sans .mp3)\n"
                      "webdownload <track_name>  - Download via spotdl into LMP and import\n"
                      "library / lib             - Show library & playlists\n"
                      "search <promt>           - Search for tracks in library\n"
                      "play <name>               - Play a track from library\n"
                      "pause                     - Toggle pause/resume\n"
                      "stop                      - Stop playback\n"
                      "volume                    - Show current volume\n"
                      "setvolume <0-100>         - Set the volume\n"
                      "setmode <mode>            - Set playback mode (no-repeat, repeat-one, repeat-all, shuffle)\n"
                      "listnew <name>            - Create a new playlist\n"
                      "createlist <name>         - Alias to listnew\n"
                      "deletelist <pl>           - Delete an entire playlist\n"
                      "listadd \"<pl>\" \"<track>\"  - Add track to a playlist\n"
                      "listremove <pl> <idx>     - Remove track from playlist by its 1-based index\n"
                      "listaddmulti <pl> <id>..  - Add multiple tracks to playlist by ID from library.\n"
                      "listview <name>           - View tracks in a playlist\n"
                      "listplay <name>           - Play a playlist\n"
                      "next                      - Skips current track in playlist"
                      "author                    - Show authors\n"
                      "quit                      - Exit the player";
    snprintf(state->message, sizeof(state->message), "%s", msg);
}

void cmd_library(AppState *state, const char *argument) {
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
}

void cmd_rename(AppState *state, char *argument) {
        if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message), "Usage: rename <track_index> <new_name>");
      return;
    }

    char *index_str = strtok(argument, " ");
    char *new_name_arg = strtok(NULL, " ");

    if (!index_str || !new_name_arg) {
      snprintf(state->message, sizeof(state->message), "Usage: rename <track_index> <new_name>");
      return;
    }

    char *endptr;
    long idx_long = strtol(index_str, &endptr, 10);

    if (*endptr != '\0' || idx_long <= 0 || idx_long > state->track_count) {
      snprintf(state->message, sizeof(state->message), "Error: Invalid track index. Must be 1-%d.", state->track_count);
      return;
    }
    int track_index = (int)idx_long - 1;

    if (strlen(new_name_arg) >= sizeof(state->library[track_index].name)) {
      snprintf(state->message, sizeof(state->message), "Error: New name '%s' is too long (max %zu chars).", new_name_arg, sizeof(state->library[track_index].name) - 1);
      return;
    }

    char old_name[sizeof(state->library[track_index].name)];
    strncpy(old_name, state->library[track_index].name, sizeof(old_name) - 1);
    old_name[sizeof(old_name) - 1] = '\0';

    strncpy(state->library[track_index].name, new_name_arg, sizeof(state->library[track_index].name) - 1);
    state->library[track_index].name[sizeof(state->library[track_index].name) - 1] = '\0';

    snprintf(state->message, sizeof(state->message), "Renamed track '%s' to '%s'.", old_name, new_name_arg);
    config_save(state); 
}

void cmd_play(AppState *state, char *argument) {
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
}

void cmd_pause(AppState *state) {
    player_pause_toggle();
    snprintf(state->message, sizeof(state->message), "Toggled pause/resume.");
}

void cmd_volume(AppState *state, char *argument) {
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

}

void cmd_setvolume(AppState *state, const char *argument) {
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
}

void cmd_show_authors(AppState *state) {
        snprintf(state->message, sizeof(state->message),
             "---Authors---\n dormant1337: https://github.com/Zer0Flux86\n "
             "Syn4pse: https://github.com/BoLIIIoi\n");
}

void cmd_remove(AppState *state, const char *argument) {
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
}

void cmd_setmode(AppState *state, const char *argument) {
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
}

void cmd_mode(AppState *state) {
        sprintf(state->message, "Use setmode to change the playback mode.");
}

void cmd_listaddmulti(AppState *state, char *argument) {
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
}

void cmd_listremove(AppState *state, const char *argument) {
        char pl_name[50] = {0};
    char *track_idx_str;
    const char *args = argument;
    int pidx = -1;
    int track_idx_to_remove = -1;

    if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message), "Usage: listremove <playlist_name> <index>");
      return;
    }
    
    if (*args == '"') {
      const char *start = args + 1;
      const char *end = strchr(start, '"');
      if (end) {
        size_t len = end - start;
        if (len > sizeof(pl_name) - 1) len = sizeof(pl_name) - 1;
        strncpy(pl_name, start, len);
        pl_name[len] = '\0';
        args = end + 1;
      }
    } else {
      const char *start = args;
      const char *end = strchr(start, ' ');
      if (end) {
        size_t len = end - start;
        if (len > sizeof(pl_name) - 1) len = sizeof(pl_name) - 1;
        strncpy(pl_name, start, len);
        pl_name[len] = '\0';
        args = end;
      }
    }

    while (*args == ' ') {
      args++;
    }
    track_idx_str = (char *)args;

    if (pl_name[0] == '\0' || track_idx_str[0] == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: listremove <playlist_name> <index>");
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
               "Playlist '%s' not found.", pl_name);
      return;
    }

    track_idx_to_remove = atoi(track_idx_str);
    Playlist *pl = &state->playlists[pidx];

    if (track_idx_to_remove <= 0 || track_idx_to_remove > pl->track_count) {
      snprintf(state->message, sizeof(state->message),
               "Error: Invalid track index %d for playlist '%s'. (1-%d)",
               track_idx_to_remove, pl->name, pl->track_count);
      return;
    }

    for (int i = track_idx_to_remove - 1; i < pl->track_count - 1; i++) {
      pl->track_indices[i] = pl->track_indices[i + 1];
    }
    pl->track_count--;

    if (state->playing_playlist_index == pidx &&
        state->playing_track_index_in_playlist >= track_idx_to_remove - 1) {
      if (state->playing_track_index_in_playlist >= pl->track_count) {
        state->playing_track_index_in_playlist = pl->track_count > 0 ? pl->track_count - 1 : 0;
        if (pl->track_count == 0) {
            player_stop();
            state->playing_playlist_index = -1;
            state->current_track[0] = '\0';
            state->track_duration = 0.0;
        }
      }
    }
    snprintf(state->message, sizeof(state->message),
             "Removed track at index %d from playlist '%s'.",
             track_idx_to_remove, pl->name);
    config_save(state);
}

void cmd_deletelist(AppState *state, const char *argument) {
        if (!argument || *argument == '\0') {
      snprintf(state->message, sizeof(state->message),
               "Usage: deletelist <playlist_name>");
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

    free(state->playlists[pidx].track_indices);
    state->playlists[pidx].track_indices = NULL;
    state->playlists[pidx].track_capacity = 0;
    state->playlists[pidx].track_count = 0;

    for (int i = pidx; i < state->playlist_count - 1; i++) {
      state->playlists[i] = state->playlists[i + 1];
    }
    state->playlist_count--;

    if (state->playing_playlist_index == pidx) {
        player_stop();
        state->playing_playlist_index = -1;
        state->current_track[0] = '\0';
        state->track_duration = 0.0;
    } else if (state->playing_playlist_index > pidx) {
        state->playing_playlist_index--;
    }

    snprintf(state->message, sizeof(state->message),
             "Playlist '%s' deleted.", argument);
    config_save(state);
}

void cmd_listnew(AppState *state, const char *argument, const char *command) {
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
}

void cmd_listadd(AppState *state, const char *argument) {
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
}

void cmd_listview(AppState *state, const char *argument) {
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
}

void cmd_listplay(AppState *state, const char *argument) {
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
}

void cmd_stop(AppState *state) {
            player_stop();
    state->playing_playlist_index = -1;
    state->playing_track_index_in_playlist = 0;
    state->current_track[0] = '\0';
    state->track_duration = 0.0;
    snprintf(state->message, sizeof(state->message), "Playback stopped.");

}

void cmd_search(AppState *state, const char *argument) {
  if (!argument || *argument == '\0') {
        snprintf(state->message, sizeof(state->message), "Usage: search <query>");
        return;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();
    mvprintw(0, 2, "--- Search Results for '%s' ---", argument);

    int line = 2;
    int found_count = 0;
    char query_lower[256];
    char track_name_lower[256];
    int i;

    for (i = 0; argument[i] && i < sizeof(query_lower) - 1; i++) {
        query_lower[i] = tolower((unsigned char)argument[i]);
    }
    query_lower[i] = '\0';

    for (i = 0; i < state->track_count; i++) {
        int j;
        for (j = 0; state->library[i].name[j] && j < sizeof(track_name_lower) - 1; j++) {
            track_name_lower[j] = tolower((unsigned char)state->library[i].name[j]);
        }
        track_name_lower[j] = '\0';

        if (strstr(track_name_lower, query_lower)) {
            found_count++;
            if (line >= rows - 2) {
                mvprintw(line, 4, "...");
                break; 
            }
            mvprintw(line++, 4, "%d: %s", i + 1, state->library[i].name);
        }
    }

    if (found_count == 0) {
        mvprintw(line, 4, "No tracks found matching '%s'.", argument);
    }

    attron(A_REVERSE);
    mvprintw(rows - 1, 0, "Press any key to return");
    attroff(A_REVERSE);
    refresh();

    timeout(-1); 
    getch();
    timeout(100); 

    snprintf(state->message, sizeof(state->message), "Returned from search.");
}