#ifndef MPC_UI_OVERLAY_H
#define MPC_UI_OVERLAY_H

#include "config.h"
#include "cs_runner.h"
#include "input.h"
#include "input_map.h"
#include "ui.h"
#include "widgets.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_MODE_GAMEPLAY = 0,
    UI_MODE_GAME_MENU,
    UI_MODE_REBIND,
    UI_MODE_SETTINGS_MENU,
} UiMode;

typedef enum {
    REBIND_CAT_ALL_P1 = 0,
    REBIND_CAT_MOVEMENT,
    REBIND_CAT_COMBAT,
    REBIND_CAT_UTILITY,
    REBIND_CAT_PLAYER2,
    REBIND_CAT_CLEAR,
    REBIND_CAT_COUNT
} RebindCategory;

struct ui_overlay {
    UiMode mode;
    UiMode prev_mode;

    /* Rebind modal state */
    int rebind_selected_pad; /* 0..31 */
    int rebind_category;     /* RebindCategory */
    int rebind_item_idx;

    /* Cached 320x240 game frame for background under pause/settings menus */
    uint32_t game_fb[CS_SCREEN_WIDTH * CS_SCREEN_HEIGHT];
    bool has_game_fb;

    /* Status / Message toast */
    char toast_msg[64];
    int64_t toast_until_ms;

    /* Config paths */
    char bindings_path[512];
    char settings_path[512];
};

enum {
    UI_OVERLAY_INPUT = 1,    /* soft keys and gameplay button edges */
    UI_OVERLAY_CHROME = 2,   /* toast notifications */
    UI_OVERLAY_VIEWPORT = 4  /* Cave Story 320x240 framebuffer blit */
};

void ui_overlay_init(struct ui_overlay *o, const char *bindings_path, const char *settings_path);
void ui_overlay_set_mode(struct ui_overlay *o, UiMode mode);
UiMode ui_overlay_get_mode(const struct ui_overlay *o);
void ui_overlay_show_toast(struct ui_overlay *o, const char *msg, int64_t now_ms, int duration_ms);

/* Draws the active mode. Returns 1 if the user requested exit, 0 otherwise. */
int ui_overlay_draw(struct ui_overlay *o,
                    struct ui *u,
                    const struct input *in,
                    struct cs_runner *runner,
                    struct input_map *map,
                    struct app_settings *settings,
                    unsigned draw_mask);

#endif /* MPC_UI_OVERLAY_H */
