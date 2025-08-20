#ifndef CONFIG_H
#define CONFIG_H

#include "main.h"

void config_load(AppState *state);
void config_save(const AppState *state);

#endif /* CONFIG_H */