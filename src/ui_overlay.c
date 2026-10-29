#include "ui_overlay.h"
#include <stdio.h>
#include <string.h>

static const char *CATEGORY_NAMES[REBIND_CAT_COUNT] = {
    "All P1 Actions",
    "Movement & Jump",
    "Combat & Weapons",
    "UI & Utility",
    "Player 2 (Co-Op)",
    "Clear / Unmap"
};

static const uint16_t CAT_ALL_P1_ITEMS[] = {
    CS_ACT_LEFT, CS_ACT_DOWN, CS_ACT_RIGHT, CS_ACT_UP,
    CS_ACT_JUMP, CS_ACT_SHOOT, CS_ACT_PREV_WPN, CS_ACT_NEXT_WPN,
    CS_ACT_INVENTORY, CS_ACT_MAP, CS_ACT_STRAFE, CS_ACT_SKIP,
    CS_ACT_PAUSE, CS_ACT_OK, CS_ACT_NONE
};
#define NUM_CAT_ALL_P1 (sizeof(CAT_ALL_P1_ITEMS) / sizeof(CAT_ALL_P1_ITEMS[0]))

static const uint16_t CAT_MOVEMENT_ITEMS[] = {
    CS_ACT_LEFT, CS_ACT_DOWN, CS_ACT_RIGHT, CS_ACT_UP,
    CS_ACT_JUMP, CS_ACT_STRAFE, CS_ACT_NONE
};
#define NUM_CAT_MOVEMENT (sizeof(CAT_MOVEMENT_ITEMS) / sizeof(CAT_MOVEMENT_ITEMS[0]))

static const uint16_t CAT_COMBAT_ITEMS[] = {
    CS_ACT_SHOOT, CS_ACT_JUMP, CS_ACT_PREV_WPN, CS_ACT_NEXT_WPN,
    CS_ACT_STRAFE, CS_ACT_NONE
};
#define NUM_CAT_COMBAT (sizeof(CAT_COMBAT_ITEMS) / sizeof(CAT_COMBAT_ITEMS[0]))

static const uint16_t CAT_UTILITY_ITEMS[] = {
    CS_ACT_INVENTORY, CS_ACT_MAP, CS_ACT_PAUSE, CS_ACT_SKIP,
    CS_ACT_OK, CS_ACT_NONE
};
#define NUM_CAT_UTILITY (sizeof(CAT_UTILITY_ITEMS) / sizeof(CAT_UTILITY_ITEMS[0]))

static struct ui_list_state s_game_ls;
static struct ui_list_state s_settings_ls;

void ui_overlay_init(struct ui_overlay *o, const char *bindings_path, const char *settings_path) {
    if (!o) return;
    memset(o, 0, sizeof(*o));
    o->mode = UI_MODE_GAMEPLAY;
    o->prev_mode = UI_MODE_GAMEPLAY;
    o->rebind_selected_pad = 0;
    o->rebind_category = REBIND_CAT_ALL_P1;
    o->rebind_item_idx = 0;
    if (bindings_path) {
        snprintf(o->bindings_path, sizeof(o->bindings_path), "%s", bindings_path);
    }
    if (settings_path) {
        snprintf(o->settings_path, sizeof(o->settings_path), "%s", settings_path);
    }
}

void ui_overlay_set_mode(struct ui_overlay *o, UiMode mode) {
    if (!o) return;
    if (o->mode != mode) {
        if (mode == UI_MODE_GAME_MENU) s_game_ls.cursor = 0;
        if (mode == UI_MODE_SETTINGS_MENU) s_settings_ls.cursor = 0;
    }
    o->prev_mode = o->mode;
    o->mode = mode;
}

UiMode ui_overlay_get_mode(const struct ui_overlay *o) {
    return o ? o->mode : UI_MODE_GAMEPLAY;
}

void ui_overlay_show_toast(struct ui_overlay *o, const char *msg, int64_t now_ms, int duration_ms) {
    if (!o || !msg) return;
    snprintf(o->toast_msg, sizeof(o->toast_msg), "%s", msg);
    o->toast_until_ms = now_ms + ((duration_ms > 0) ? duration_ms : 1800);
}

static void draw_top_bar(struct ui *u, const char *b1_lbl, const char *b2_lbl, const char *b3_lbl,
                         const char *center_title) {
    ui_box(u, 0, 0, UI_W, 22, (struct ui_paint){
        .fill_set = 1, .fill = 0x141820,
        .border_set = 0
    });
    ui_box(u, 0, 22, UI_W, 2, (struct ui_paint){
        .fill_set = 1, .fill = 0x2A3244,
        .border_set = 0
    });

    ui_box(u, 8, 2, 74, 18, (struct ui_paint){
        .fill_set = 1, .fill = 0x222834,
        .border_set = 1, .border = 0x3E485C, .border_px = 1
    });
    ui_text(u, 14, 5, b1_lbl, 0x00E5FF, 1);

    ui_box(u, 88, 2, 74, 18, (struct ui_paint){
        .fill_set = 1, .fill = 0x222834,
        .border_set = 1, .border = 0x3E485C, .border_px = 1
    });
    ui_text(u, 92, 5, b2_lbl, 0xFFD600, 1);

    ui_box(u, 238, 2, 74, 18, (struct ui_paint){
        .fill_set = 1, .fill = 0x222834,
        .border_set = 1, .border = 0x3E485C, .border_px = 1
    });
    ui_text(u, 246, 5, b3_lbl, 0x00E676, 1);

    if (center_title && center_title[0]) {
        ui_text(u, 168, 5, center_title, 0xE8ECF0, 1);
    }
}

static void draw_app_button(struct ui *u, int x, const char *label, uint32_t color, int active) {
    ui_box(u, x, 2, 74, 18, (struct ui_paint){
        .fill_set = 1,
        .fill = active ? 0x2C384C : 0x222834,
        .border_set = 1,
        .border = active ? color : 0x3E485C,
        .border_px = active ? 2 : 1
    });
    int tw = (int)strlen(label) * 6;
    int tx = x + (74 - tw) / 2;
    ui_text(u, tx, 5, label, color, 1);
}

static void draw_app_bar(struct ui *u, int active) {
    ui_box(u, 0, 0, UI_W, 22, (struct ui_paint){
        .fill_set = 1, .fill = 0x141820, .border_set = 0
    });
    ui_box(u, 0, 22, UI_W, 2, (struct ui_paint){
        .fill_set = 1, .fill = 0x2A3244, .border_set = 0
    });
    ui_text(u, 168, 5, "CAVE STORY", 0xE8ECF0, 1);
    draw_app_button(u, 8, "GAME", 0x00E5FF, active == 0);
    draw_app_button(u, 88, "REBIND", 0xFFD600, active == 1);
    draw_app_button(u, 238, "SETTINGS", 0x00E676, active == 2);
}

static void draw_toast(struct ui *u, struct ui_overlay *o, int y) {
    if (o->toast_msg[0] && u->now_ms < o->toast_until_ms) {
        int w = 236, h = 24;
        int x = (UI_W - w) / 2;
        ui_box(u, x, y, w, h, (struct ui_paint){
            .fill_set = 1, .fill = 0x1A2230,
            .border_set = 1, .border = 0x00E5FF, .border_px = 2
        });
        ui_text(u, x + 10, y + 6, o->toast_msg, 0xFFFFFF, 1);
    }
}

static void draw_dimmed_background(struct ui_overlay *o, struct ui *u) {
    if (o->has_game_fb && u->px) {
        for (int i = 0; i < UI_W * UI_H; i++) {
            uint32_t c = o->game_fb[i];
            uint32_t r = ((c >> 16) & 0xFF) >> 2;
            uint32_t g = ((c >> 8) & 0xFF) >> 2;
            uint32_t b = (c & 0xFF) >> 2;
            u->px[i] = (r << 16) | (g << 8) | b;
        }
    } else {
        ui_box(u, 0, 0, UI_W, UI_H, (struct ui_paint){
            .fill_set = 1, .fill = 0x080A0E, .border_set = 0
        });
    }
}

static int handle_app_buttons(struct ui_overlay *o, const struct input *in) {
    if (in->down & (1u << IN_B1)) {
        ui_overlay_set_mode(o, o->mode == UI_MODE_GAME_MENU ? UI_MODE_GAMEPLAY : UI_MODE_GAME_MENU);
        return 1;
    }
    if (in->down & (1u << IN_B2)) {
        ui_overlay_set_mode(o, o->mode == UI_MODE_REBIND ? UI_MODE_GAMEPLAY : UI_MODE_REBIND);
        return 1;
    }
    if (in->down & (1u << IN_B3)) {
        ui_overlay_set_mode(o, o->mode == UI_MODE_SETTINGS_MENU ? UI_MODE_GAMEPLAY : UI_MODE_SETTINGS_MENU);
        return 1;
    }
    return 0;
}

static struct ui_list_style menu_style(void) {
    struct ui_list_style st = {
        .text = 0xD0D8E8,
        .highlight = 0x00E5FF,
        .row_h = 22,
        .highlight_px = 2,
        .current_fill_set = 1,
        .current_fill = 0x1E2638
    };
    return st;
}

/* -------------------------------------------------------------------------
 * GAMEPLAY MODE (Full-screen 320x240)
 * ------------------------------------------------------------------------- */
static int draw_gameplay(struct ui_overlay *o, struct ui *u, const struct input *in,
                         struct cs_runner *runner, struct input_map *map, unsigned mask) {
    (void)map;
    if ((mask & UI_OVERLAY_INPUT) && handle_app_buttons(o, in))
        return 0;

    if (mask & UI_OVERLAY_VIEWPORT) {
        if (runner && u->px) {
            cs_runner_render(runner, o->game_fb);
            o->has_game_fb = true;
            memcpy(u->px, o->game_fb, sizeof(o->game_fb));
        } else if (o->has_game_fb && u->px) {
            memcpy(u->px, o->game_fb, sizeof(o->game_fb));
        } else {
            ui_box(u, 0, 0, UI_W, UI_H, (struct ui_paint){
                .fill_set = 1, .fill = 0x000000, .border_set = 0
            });
        }
    }

    if ((mask & UI_OVERLAY_CHROME) || (mask & UI_OVERLAY_VIEWPORT)) {
        draw_toast(u, o, 206);
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * GAME / PAUSE MENU
 * ------------------------------------------------------------------------- */
static int draw_game_menu(struct ui_overlay *o, struct ui *u, const struct input *in,
                          struct cs_runner *runner, struct input_map *map) {
    if (handle_app_buttons(o, in)) {
        return 0;
    }

    draw_dimmed_background(o, u);
    draw_app_bar(u, 0);

    char preset_row[44];
    char bank_row[32];
    snprintf(preset_row, sizeof(preset_row), "Preset: %s", input_map_preset_name(map->current_preset));
    snprintf(bank_row, sizeof(bank_row), "Pad Bank: %s", (input_map_get_bank(map) == 0) ? "Bank A (1-16)" : "Bank B (17-32)");

    const char *rows[] = {
        "Resume Game",
        "Rebind Controls",
        preset_row,
        bank_row,
        "Restart Game",
        "Quit Cave Story"
    };
    int n = 6;
    int x = 36, y = 34, w = UI_W - 72, h = 8 + n * 22;

    ui_box(u, x, y, w, h, (struct ui_paint){
        .fill_set = 1, .fill = 0x10141E,
        .border_set = 1, .border = 0x2A3448, .border_px = 1
    });

    int chosen = ui_list(u, x + 4, y + 4, w - 8, n * 22, rows, n, menu_style(), in, &s_game_ls);
    if (chosen == 0) {
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
    } else if (chosen == 1) {
        ui_overlay_set_mode(o, UI_MODE_REBIND);
    } else if (chosen == 2) {
        enum input_preset_id next_p = (enum input_preset_id)((map->current_preset + 1) % PRESET_COUNT);
        input_map_apply_preset(map, next_p);
        ui_overlay_show_toast(o, input_map_preset_name(next_p), u->now_ms, 1500);
    } else if (chosen == 3) {
        input_map_toggle_bank(map);
        ui_overlay_show_toast(o, (input_map_get_bank(map) == 0) ? "Switched to Bank A" : "Switched to Bank B", u->now_ms, 1200);
    } else if (chosen == 4) {
        if (runner) cs_runner_reset(runner);
        ui_overlay_show_toast(o, "Game Restarted", u->now_ms, 1500);
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
    } else if (chosen == 5) {
        return 1;
    }

    draw_toast(u, o, 198);
    return 0;
}

/* -------------------------------------------------------------------------
 * SETTINGS MENU
 * ------------------------------------------------------------------------- */
static int draw_settings_menu(struct ui_overlay *o, struct ui *u, const struct input *in,
                              struct input_map *map, struct app_settings *settings) {
    if (handle_app_buttons(o, in)) {
        return 0;
    }

    draw_dimmed_background(o, u);
    draw_app_bar(u, 2);

    char vol_row[40];
    char spk_row[40];
    snprintf(vol_row, sizeof(vol_row), "Volume: %d%%", settings ? settings->volume : 80);
    snprintf(spk_row, sizeof(spk_row), "Builtin Speaker: %s", (settings && settings->speaker_on) ? "On" : "Off");

    const char *rows[] = {
        vol_row,
        spk_row,
        "Save Bindings & Settings",
        "Reset Default Controls",
        "Back to Game"
    };
    int n = 5;
    int x = 36, y = 38, w = UI_W - 72, h = 8 + n * 22;

    ui_box(u, x, y, w, h, (struct ui_paint){
        .fill_set = 1, .fill = 0x10141E,
        .border_set = 1, .border = 0x2A3448, .border_px = 1
    });

    int chosen = ui_list(u, x + 4, y + 4, w - 8, n * 22, rows, n, menu_style(), in, &s_settings_ls);
    if (chosen == 0 && settings) {
        settings->volume = (settings->volume + 10) > 100 ? 0 : (settings->volume + 10);
        if (o->settings_path[0]) config_save_settings(o->settings_path, settings);
    } else if (chosen == 1 && settings) {
        settings->speaker_on = !settings->speaker_on;
        if (o->settings_path[0]) config_save_settings(o->settings_path, settings);
        ui_overlay_show_toast(o, settings->speaker_on ? "Speaker Enabled" : "Speaker Muted", u->now_ms, 1200);
    } else if (chosen == 2) {
        bool ok_b = o->bindings_path[0] ? config_save_bindings(o->bindings_path, map) : false;
        bool ok_s = (o->settings_path[0] && settings) ? config_save_settings(o->settings_path, settings) : false;
        ui_overlay_show_toast(o, (ok_b || ok_s) ? "Configuration Saved!" : "Save Failed", u->now_ms, 1800);
    } else if (chosen == 3) {
        input_map_apply_preset(map, PRESET_ARCADE);
        ui_overlay_show_toast(o, "Reset to Arcade Preset", u->now_ms, 1500);
    } else if (chosen == 4) {
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
    }

    draw_toast(u, o, 198);
    return 0;
}

/* -------------------------------------------------------------------------
 * REBIND CONTROLS MODAL (4x4 Pad Grid + Delete Bind via ERASE or Wheel)
 * ------------------------------------------------------------------------- */
static uint16_t current_rebind_action(int category, int item_idx, enum bind_type *out_type) {
    if (out_type) *out_type = BIND_TYPE_P1_BTN;
    if (item_idx < 0) item_idx = 0;

    switch (category) {
        case REBIND_CAT_ALL_P1:
        default: {
            uint16_t act = CAT_ALL_P1_ITEMS[item_idx % NUM_CAT_ALL_P1];
            if (act == CS_ACT_NONE && out_type) *out_type = BIND_TYPE_NONE;
            return act;
        }
        case REBIND_CAT_MOVEMENT: {
            uint16_t act = CAT_MOVEMENT_ITEMS[item_idx % NUM_CAT_MOVEMENT];
            if (act == CS_ACT_NONE && out_type) *out_type = BIND_TYPE_NONE;
            return act;
        }
        case REBIND_CAT_COMBAT: {
            uint16_t act = CAT_COMBAT_ITEMS[item_idx % NUM_CAT_COMBAT];
            if (act == CS_ACT_NONE && out_type) *out_type = BIND_TYPE_NONE;
            return act;
        }
        case REBIND_CAT_UTILITY: {
            uint16_t act = CAT_UTILITY_ITEMS[item_idx % NUM_CAT_UTILITY];
            if (act == CS_ACT_NONE && out_type) *out_type = BIND_TYPE_NONE;
            return act;
        }
        case REBIND_CAT_PLAYER2: {
            uint16_t act = CAT_ALL_P1_ITEMS[item_idx % NUM_CAT_ALL_P1];
            if (out_type) *out_type = (act == CS_ACT_NONE) ? BIND_TYPE_NONE : BIND_TYPE_P2_BTN;
            return act;
        }
        case REBIND_CAT_CLEAR:
            if (out_type) *out_type = BIND_TYPE_NONE;
            return CS_ACT_NONE;
    }
}

static int draw_rebind(struct ui_overlay *o, struct ui *u, const struct input *in,
                       struct input_map *map, struct app_settings *settings) {
    if (in->down & (1u << IN_B1)) {
        ui_overlay_set_mode(o, UI_MODE_GAMEPLAY);
        return 0;
    }
    if (in->down & (1u << IN_B2)) {
        enum input_preset_id next_p = (enum input_preset_id)((map->current_preset + 1) % PRESET_COUNT);
        input_map_apply_preset(map, next_p);
        if (settings) settings->preset = (int)next_p;
        ui_overlay_show_toast(o, input_map_preset_name(next_p), u->now_ms, 1500);
    }
    if (in->down & (1u << IN_B3)) {
        bool saved = o->bindings_path[0] ? config_save_bindings(o->bindings_path, map) : false;
        if (settings && o->settings_path[0]) {
            settings->preset = (int)map->current_preset;
            config_save_settings(o->settings_path, settings);
        }
        ui_overlay_show_toast(o, saved ? "Saved Bindings!" : "Could not save bindings", u->now_ms, 1800);
    }

    draw_top_bar(u, "B1:DONE", "B2:PRESET", "B3:SAVE", "REBIND PADS");

    /* Hit any physical pad to select it in the active bank */
    int bank_offset = input_map_get_bank(map) * PAD_BANK_SIZE;
    if (o->rebind_selected_pad < bank_offset || o->rebind_selected_pad >= bank_offset + PAD_BANK_SIZE) {
        o->rebind_selected_pad = bank_offset + (o->rebind_selected_pad % PAD_BANK_SIZE);
    }

    for (int p = 0; p < PAD_BANK_SIZE; p++) {
        if (in->pad_hit[p] >= 0) {
            o->rebind_selected_pad = bank_offset + p;
        }
    }

    /* Turn wheel (or SHIFT + wheel to select pad) */
    if (in->encoder != 0) {
        if (in->held & (1u << IN_SHIFT)) {
            int next_pad = (o->rebind_selected_pad + in->encoder) % PAD_SLOT_COUNT;
            if (next_pad < 0) next_pad += PAD_SLOT_COUNT;
            o->rebind_selected_pad = next_pad;
            input_map_set_bank(map, o->rebind_selected_pad / PAD_BANK_SIZE);
            bank_offset = input_map_get_bank(map) * PAD_BANK_SIZE;
        } else {
            o->rebind_item_idx += in->encoder;
            if (o->rebind_item_idx < 0) o->rebind_item_idx = 0;
        }
    }

    /* - / + buttons switch action category */
    if (in->down & (1u << IN_MINUS)) {
        o->rebind_category = (o->rebind_category - 1 + REBIND_CAT_COUNT) % REBIND_CAT_COUNT;
        o->rebind_item_idx = 0;
    }
    if (in->down & (1u << IN_PLUS)) {
        o->rebind_category = (o->rebind_category + 1) % REBIND_CAT_COUNT;
        o->rebind_item_idx = 0;
    }

    /* Physical ERASE button immediately deletes/clears the selected pad's bind */
    if (in->down & (1u << IN_ERASE)) {
        input_map_clear_binding(map, o->rebind_selected_pad);
        ui_overlay_show_toast(o, "Bind Deleted (Unmapped)", u->now_ms, 1400);
    }

    /* Draw 4x4 Pad Grid */
    int pad_w = 46, pad_h = 28, pad_gap = 4;
    int base_x = 60, base_y = 28;

    for (int row = 0; row < 4; row++) {
        int phy_row = 3 - row;
        for (int col = 0; col < 4; col++) {
            int pad_local = phy_row * 4 + col;
            int pad_global = bank_offset + pad_local;
            bool is_selected = (o->rebind_selected_pad == pad_global);

            int px = base_x + col * (pad_w + pad_gap);
            int py = base_y + row * (pad_h + pad_gap);

            const struct pad_binding *b = &map->pads[pad_global];
            ui_box(u, px, py, pad_w, pad_h, (struct ui_paint){
                .fill_set = 1, .fill = b->color,
                .border_set = 1,
                .border = is_selected ? 0xFFFFFF : 0x303848,
                .border_px = is_selected ? 2 : 1
            });

            char num_str[8];
            snprintf(num_str, sizeof(num_str), "%d", pad_local + 1);
            ui_text(u, px + 2, py + 2, num_str, 0x90A0B8, 1);
            ui_text(u, px + 8, py + 11, b->label, 0xFFFFFF, 1);
        }
    }

    enum bind_type target_type = BIND_TYPE_P1_BTN;
    uint16_t target_code = current_rebind_action(o->rebind_category, o->rebind_item_idx, &target_type);

    /* Click wheel to assign (or clear if target is CS_ACT_NONE / REBIND_CAT_CLEAR) */
    if (in->encoder_press) {
        int sel = o->rebind_selected_pad;
        if (target_type == BIND_TYPE_NONE || target_code == CS_ACT_NONE) {
            input_map_clear_binding(map, sel);
            ui_overlay_show_toast(o, "Bind Deleted (Unmapped)", u->now_ms, 1200);
        } else {
            uint32_t col = input_map_default_color(target_type, target_code);
            input_map_set_binding(map, sel, target_type, target_code, col, NULL);
            ui_overlay_show_toast(o, "Assigned!", u->now_ms, 1000);
        }
    }

    /* Bottom Inspector Panel */
    ui_box(u, 14, 156, UI_W - 28, 80, (struct ui_paint){
        .fill_set = 1, .fill = 0x141824,
        .border_set = 1, .border = 0x2E384C, .border_px = 1
    });

    int cur_pad = o->rebind_selected_pad;
    const struct pad_binding *cur_b = &map->pads[cur_pad];
    const char *cur_desc = (cur_b->type == BIND_TYPE_NONE || cur_b->code == CS_ACT_NONE)
        ? "None (Unmapped)"
        : input_map_action_full_name(cur_b->code);

    char pad_info[64];
    snprintf(pad_info, sizeof(pad_info), "PAD %d (%s): [%s] %s",
             (cur_pad % 16) + 1,
             (cur_pad < 16) ? "Bank A" : "Bank B",
             cur_b->label,
             cur_desc);
    ui_text(u, 22, 162, pad_info, 0xFFD600, 1);

    char cat_str[64];
    snprintf(cat_str, sizeof(cat_str), "Cat [-/+]: <%s>", CATEGORY_NAMES[o->rebind_category]);
    ui_text(u, 22, 180, cat_str, 0x00E5FF, 1);

    char item_str[64];
    if (o->rebind_category == REBIND_CAT_CLEAR || target_code == CS_ACT_NONE) {
        snprintf(item_str, sizeof(item_str), "Wheel: None (Delete / Unmap Pad)");
    } else if (target_type == BIND_TYPE_P2_BTN) {
        snprintf(item_str, sizeof(item_str), "Wheel: P2 %s", input_map_action_full_name(target_code));
    } else {
        snprintf(item_str, sizeof(item_str), "Wheel: %s", input_map_action_full_name(target_code));
    }
    ui_text(u, 22, 198, item_str, 0xFFFFFF, 1);
    ui_text(u, 22, 216, "Click Wheel: Assign | ERASE: Delete Bind", 0x90A4AE, 1);

    draw_toast(u, o, 180);
    return 0;
}

int ui_overlay_draw(struct ui_overlay *o,
                    struct ui *u,
                    const struct input *in,
                    struct cs_runner *runner,
                    struct input_map *map,
                    struct app_settings *settings,
                    unsigned draw_mask) {
    if (!o || !u || !in) return 0;

    switch (o->mode) {
        case UI_MODE_GAMEPLAY:
            return draw_gameplay(o, u, in, runner, map, draw_mask);
        case UI_MODE_GAME_MENU:
            return draw_game_menu(o, u, in, runner, map);
        case UI_MODE_REBIND:
            return draw_rebind(o, u, in, map, settings);
        case UI_MODE_SETTINGS_MENU:
            return draw_settings_menu(o, u, in, map, settings);
        default:
            return draw_gameplay(o, u, in, runner, map, draw_mask);
    }
}

