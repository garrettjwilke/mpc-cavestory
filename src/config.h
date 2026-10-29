#ifndef MPC_CONFIG_H
#define MPC_CONFIG_H

#include "input_map.h"
#include <stdbool.h>

struct app_settings {
    int volume;          /* 0 .. 100 */
    bool speaker_on;     /* Builtin speaker enabled */
    int preset;          /* enum input_preset_id */
};

void config_defaults(struct app_settings *settings);

bool config_load_settings(const char *path, struct app_settings *settings);
bool config_save_settings(const char *path, const struct app_settings *settings);

bool config_load_bindings(const char *path, struct input_map *map);
bool config_save_bindings(const char *path, const struct input_map *map);

#endif /* MPC_CONFIG_H */

