#ifndef HANDLE_COMMAND_H
#define HANDLE_COMMAND_H

#include "main.h"   
#include "config.h"  
#include "functions.h" 


void cmd_add(AppState *state, const char *argument);
void cmd_addfolder(AppState *state, const char *argument);
void cmd_webdownload(AppState *state, const char *argument);
void cmd_help(AppState *state, const char *argument);
void cmd_library(AppState *state, const char *argument);
void cmd_rename(AppState *state, const char *argument); 
void cmd_play(AppState *state, const char *argument);  
void cmd_pause(AppState *state);
void cmd_volume(AppState *state, const char *argument); 
void cmd_setvolume(AppState *state, const char *argument);
void cmd_show_authors(AppState *state);
void cmd_remove(AppState *state, const char *argument);
void cmd_setmode(AppState *state, const char *argument);
void cmd_mode(AppState *state);
void cmd_listaddmulti(AppState *state, char *argument); 
void cmd_listremove(AppState *state, const char *argument);
void cmd_deletelist(AppState *state, const char *argument);
void cmd_listnew(AppState *state, const char *argument, const char *command_name);
void cmd_listadd(AppState *state, const char *argument);
void cmd_listview(AppState *state, const char *argument);
void cmd_listplay(AppState *state, const char *argument);
void cmd_stop(AppState *state);
void cmd_search(AppState *state, const char *argument);
void cmd_next(AppState *state);
void cmd_skip(AppState *state);
#endif 