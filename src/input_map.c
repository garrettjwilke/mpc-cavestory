#include "input_map.h"
#include <stdio.h>
#include <string.h>

static const char *s_preset_names[PRESET_COUNT] = {
    "Arcade (Classic)",
    "Ergonomic (2-Hand)",
    "Action Platformer",
    "2-Player Co-Op"
};

const char *input_map_preset_name(enum input_preset_id preset) {
    if (preset < 0 || preset >= PRESET_COUNT) return "Custom";
    return s_preset_names[preset];
}

void input_map_set_bank(struct input_map *map, int bank) {
    if (!map) return;
    map->active_bank = (bank != 0) ? 1 : 0;
}

int input_map_get_bank(const struct input_map *map) {
    return map ? map->active_bank : 0;
}

void input_map_toggle_bank(struct input_map *map) {
    if (!map) return;
    map->active_bank = (map->active_bank == 0) ? 1 : 0;
}

const char *input_map_action_short_name(uint16_t code) {
    switch (code) {
        case CS_ACT_LEFT:      return "LEFT";
        case CS_ACT_DOWN:      return "DOWN";
        case CS_ACT_RIGHT:     return "RIGHT";
        case CS_ACT_UP:        return "UP";
        case CS_ACT_JUMP:      return "JUMP";
        case CS_ACT_SHOOT:     return "SHOOT";
        case CS_ACT_PREV_WPN:  return "WPN-";
        case CS_ACT_NEXT_WPN:  return "WPN+";
        case CS_ACT_INVENTORY: return "ITEM";
        case CS_ACT_MAP:       return "MAP";
        case CS_ACT_STRAFE:    return "STRF";
        case CS_ACT_SKIP:      return "SKIP";
        case CS_ACT_PAUSE:     return "PAUSE";
        case CS_ACT_OK:        return "OK";
        default:               return "-";
    }
}

const char *input_map_action_full_name(uint16_t code) {
    switch (code) {
        case CS_ACT_LEFT:      return "Move Left";
        case CS_ACT_DOWN:      return "Look Down / Interact";
        case CS_ACT_RIGHT:     return "Move Right";
        case CS_ACT_UP:        return "Look Up";
        case CS_ACT_JUMP:      return "Jump / Confirm";
        case CS_ACT_SHOOT:     return "Shoot / Cancel";
        case CS_ACT_PREV_WPN:  return "Prev Weapon";
        case CS_ACT_NEXT_WPN:  return "Next Weapon";
        case CS_ACT_INVENTORY: return "Inventory Screen";
        case CS_ACT_MAP:       return "Minimap System";
        case CS_ACT_STRAFE:    return "Strafe Lock";
        case CS_ACT_SKIP:      return "Fast-Forward Cutscene";
        case CS_ACT_PAUSE:     return "In-Game Pause Menu";
        case CS_ACT_OK:        return "Menu Confirm (Enter)";
        default:               return "None (Unmap)";
    }
}

uint16_t input_map_action_to_mask(uint16_t code) {
    switch (code) {
        case CS_ACT_LEFT:      return CS_BTN_LEFT;
        case CS_ACT_DOWN:      return CS_BTN_DOWN;
        case CS_ACT_RIGHT:     return CS_BTN_RIGHT;
        case CS_ACT_UP:        return CS_BTN_UP;
        case CS_ACT_JUMP:      return CS_BTN_JUMP;
        case CS_ACT_SHOOT:     return CS_BTN_SHOOT;
        case CS_ACT_PREV_WPN:  return CS_BTN_PREV_WPN;
        case CS_ACT_NEXT_WPN:  return CS_BTN_NEXT_WPN;
        case CS_ACT_INVENTORY: return CS_BTN_INVENTORY;
        case CS_ACT_MAP:       return CS_BTN_MAP;
        case CS_ACT_STRAFE:    return CS_BTN_STRAFE;
        case CS_ACT_SKIP:      return CS_BTN_SKIP;
        case CS_ACT_PAUSE:     return CS_BTN_PAUSE;
        case CS_ACT_OK:        return CS_BTN_OK;
        default:               return 0;
    }
}

uint32_t input_map_default_color(enum bind_type type, uint16_t code) {
    if (type == BIND_TYPE_NONE || code == CS_ACT_NONE) {
        return COLOR_DIM;
    }
    if (type == BIND_TYPE_P2_BTN) {
        switch (code) {
            case CS_ACT_LEFT:
            case CS_ACT_DOWN:
            case CS_ACT_RIGHT:
            case CS_ACT_UP:
                return COLOR_PURPLE;
            case CS_ACT_JUMP:  return COLOR_CYAN;
            case CS_ACT_SHOOT: return COLOR_PINK;
            default:           return COLOR_AMBER;
        }
    }
    switch (code) {
        case CS_ACT_LEFT:
        case CS_ACT_DOWN:
        case CS_ACT_RIGHT:
        case CS_ACT_UP:
            return COLOR_BLUE;
        case CS_ACT_JUMP:      return COLOR_GREEN;
        case CS_ACT_SHOOT:     return COLOR_RED;
        case CS_ACT_PREV_WPN:
        case CS_ACT_NEXT_WPN:  return COLOR_AMBER;
        case CS_ACT_INVENTORY: return COLOR_PURPLE;
        case CS_ACT_MAP:       return COLOR_TEAL;
        case CS_ACT_STRAFE:    return COLOR_CYAN;
        case CS_ACT_SKIP:      return COLOR_YELLOW;
        case CS_ACT_PAUSE:
        case CS_ACT_OK:        return COLOR_WHITE;
        default:               return COLOR_DIM;
    }
}

void input_map_clear_binding(struct input_map *map, int pad_idx) {
    if (!map || pad_idx < 0 || pad_idx >= PAD_SLOT_COUNT) return;
    struct pad_binding *b = &map->pads[pad_idx];
    b->type = BIND_TYPE_NONE;
    b->code = CS_ACT_NONE;
    b->color = COLOR_DIM;
    snprintf(b->label, sizeof(b->label), "-");
}

void input_map_set_binding(struct input_map *map, int pad_idx,
                           enum bind_type type, uint16_t code,
                           uint32_t color, const char *label) {
    if (!map || pad_idx < 0 || pad_idx >= PAD_SLOT_COUNT) return;
    if (type == BIND_TYPE_NONE || code == CS_ACT_NONE) {
        input_map_clear_binding(map, pad_idx);
        return;
    }
    struct pad_binding *b = &map->pads[pad_idx];
    b->type = type;
    b->code = code;
    b->color = color ? color : input_map_default_color(type, code);
    if (label && label[0]) {
        snprintf(b->label, sizeof(b->label), "%s", label);
    } else {
        const char *short_name = input_map_action_short_name(code);
        if (type == BIND_TYPE_P2_BTN) {
            snprintf(b->label, sizeof(b->label), "2:%s", short_name);
        } else {
            snprintf(b->label, sizeof(b->label), "%s", short_name);
        }
    }
}

static void clear_all_pads(struct input_map *map) {
    for (int i = 0; i < PAD_SLOT_COUNT; i++) {
        input_map_clear_binding(map, i);
    }
}

static void populate_bank_b_p2(struct input_map *map) {
    /* Bank B (16..31): Player 2 Co-Op + extra utility shortcuts */
    input_map_set_binding(map, 16, BIND_TYPE_P2_BTN, CS_ACT_LEFT,      COLOR_PURPLE, "2:LEFT");
    input_map_set_binding(map, 17, BIND_TYPE_P2_BTN, CS_ACT_DOWN,      COLOR_PURPLE, "2:DOWN");
    input_map_set_binding(map, 18, BIND_TYPE_P2_BTN, CS_ACT_RIGHT,     COLOR_PURPLE, "2:RGHT");
    input_map_set_binding(map, 19, BIND_TYPE_P2_BTN, CS_ACT_JUMP,      COLOR_CYAN,   "2:JUMP");
    input_map_set_binding(map, 20, BIND_TYPE_P2_BTN, CS_ACT_STRAFE,    COLOR_AMBER,  "2:STRF");
    input_map_set_binding(map, 21, BIND_TYPE_P2_BTN, CS_ACT_UP,        COLOR_PURPLE, "2:UP");
    input_map_set_binding(map, 22, BIND_TYPE_P2_BTN, CS_ACT_SHOOT,     COLOR_PINK,   "2:SHOT");
    input_map_set_binding(map, 23, BIND_TYPE_P2_BTN, CS_ACT_JUMP,      COLOR_CYAN,   "2:JUMP");
    input_map_set_binding(map, 24, BIND_TYPE_P2_BTN, CS_ACT_INVENTORY, COLOR_AMBER,  "2:ITEM");
    input_map_set_binding(map, 25, BIND_TYPE_P2_BTN, CS_ACT_MAP,       COLOR_TEAL,   "2:MAP");
    input_map_set_binding(map, 26, BIND_TYPE_P2_BTN, CS_ACT_PREV_WPN,  COLOR_AMBER,  "2:WPN-");
    input_map_set_binding(map, 27, BIND_TYPE_P2_BTN, CS_ACT_NEXT_WPN,  COLOR_AMBER,  "2:WPN+");
    input_map_set_binding(map, 28, BIND_TYPE_P1_BTN, CS_ACT_PAUSE,     COLOR_WHITE,  "PAUSE");
    input_map_set_binding(map, 29, BIND_TYPE_P1_BTN, CS_ACT_SKIP,      COLOR_YELLOW, "SKIP");
    input_map_set_binding(map, 30, BIND_TYPE_P1_BTN, CS_ACT_INVENTORY, COLOR_PURPLE, "ITEM");
    input_map_set_binding(map, 31, BIND_TYPE_P1_BTN, CS_ACT_OK,        COLOR_WHITE,  "OK");
}

void input_map_apply_preset(struct input_map *map, enum input_preset_id preset) {
    if (!map) return;
    clear_all_pads(map);
    map->current_preset = preset;

    switch (preset) {
        case PRESET_ARCADE:
        default:
            /* Row 1 (Bottom: Pad 1..4): Left, Down, Right, Jump */
            input_map_set_binding(map, 0,  BIND_TYPE_P1_BTN, CS_ACT_LEFT,      COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_P1_BTN, CS_ACT_DOWN,      COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 2,  BIND_TYPE_P1_BTN, CS_ACT_RIGHT,     COLOR_BLUE,   "RIGHT");
            input_map_set_binding(map, 3,  BIND_TYPE_P1_BTN, CS_ACT_JUMP,      COLOR_GREEN,  "JUMP");
            /* Row 2 (Pad 5..8): Strafe, Up, Shoot, Jump */
            input_map_set_binding(map, 4,  BIND_TYPE_P1_BTN, CS_ACT_STRAFE,    COLOR_CYAN,   "STRF");
            input_map_set_binding(map, 5,  BIND_TYPE_P1_BTN, CS_ACT_UP,        COLOR_BLUE,   "UP");
            input_map_set_binding(map, 6,  BIND_TYPE_P1_BTN, CS_ACT_SHOOT,     COLOR_RED,    "SHOOT");
            input_map_set_binding(map, 7,  BIND_TYPE_P1_BTN, CS_ACT_JUMP,      COLOR_GREEN,  "JUMP");
            /* Row 3 (Pad 9..12): Item, Map, Prev Wpn, Next Wpn */
            input_map_set_binding(map, 8,  BIND_TYPE_P1_BTN, CS_ACT_INVENTORY, COLOR_PURPLE, "ITEM");
            input_map_set_binding(map, 9,  BIND_TYPE_P1_BTN, CS_ACT_MAP,       COLOR_TEAL,   "MAP");
            input_map_set_binding(map, 10, BIND_TYPE_P1_BTN, CS_ACT_PREV_WPN,  COLOR_AMBER,  "WPN-");
            input_map_set_binding(map, 11, BIND_TYPE_P1_BTN, CS_ACT_NEXT_WPN,  COLOR_AMBER,  "WPN+");
            /* Row 4 (Top: Pad 13..16): Pause, Skip, Shoot, OK */
            input_map_set_binding(map, 12, BIND_TYPE_P1_BTN, CS_ACT_PAUSE,     COLOR_WHITE,  "PAUSE");
            input_map_set_binding(map, 13, BIND_TYPE_P1_BTN, CS_ACT_SKIP,      COLOR_YELLOW, "SKIP");
            input_map_set_binding(map, 14, BIND_TYPE_P1_BTN, CS_ACT_SHOOT,     COLOR_RED,    "SHOOT");
            input_map_set_binding(map, 15, BIND_TYPE_P1_BTN, CS_ACT_OK,        COLOR_WHITE,  "OK");
            populate_bank_b_p2(map);
            break;

        case PRESET_ERGONOMIC:
            /* Left hand on cols 1-2, Right hand on cols 3-4 */
            input_map_set_binding(map, 0,  BIND_TYPE_P1_BTN, CS_ACT_LEFT,      COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_P1_BTN, CS_ACT_RIGHT,     COLOR_BLUE,   "RIGHT");
            input_map_set_binding(map, 2,  BIND_TYPE_P1_BTN, CS_ACT_SHOOT,     COLOR_RED,    "SHOOT");
            input_map_set_binding(map, 3,  BIND_TYPE_P1_BTN, CS_ACT_JUMP,      COLOR_GREEN,  "JUMP");

            input_map_set_binding(map, 4,  BIND_TYPE_P1_BTN, CS_ACT_DOWN,      COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 5,  BIND_TYPE_P1_BTN, CS_ACT_UP,        COLOR_BLUE,   "UP");
            input_map_set_binding(map, 6,  BIND_TYPE_P1_BTN, CS_ACT_PREV_WPN,  COLOR_AMBER,  "WPN-");
            input_map_set_binding(map, 7,  BIND_TYPE_P1_BTN, CS_ACT_NEXT_WPN,  COLOR_AMBER,  "WPN+");

            input_map_set_binding(map, 8,  BIND_TYPE_P1_BTN, CS_ACT_STRAFE,    COLOR_CYAN,   "STRF");
            input_map_set_binding(map, 9,  BIND_TYPE_P1_BTN, CS_ACT_MAP,       COLOR_TEAL,   "MAP");
            input_map_set_binding(map, 10, BIND_TYPE_P1_BTN, CS_ACT_INVENTORY, COLOR_PURPLE, "ITEM");
            input_map_set_binding(map, 11, BIND_TYPE_P1_BTN, CS_ACT_SKIP,      COLOR_YELLOW, "SKIP");

            input_map_set_binding(map, 12, BIND_TYPE_P1_BTN, CS_ACT_PAUSE,     COLOR_WHITE,  "PAUSE");
            input_map_set_binding(map, 13, BIND_TYPE_NONE,   CS_ACT_NONE,      COLOR_DIM,    "-");
            input_map_set_binding(map, 14, BIND_TYPE_NONE,   CS_ACT_NONE,      COLOR_DIM,    "-");
            input_map_set_binding(map, 15, BIND_TYPE_P1_BTN, CS_ACT_OK,        COLOR_WHITE,  "OK");
            populate_bank_b_p2(map);
            break;

        case PRESET_PLATFORMER:
            /* Fast platforming with doubled Jump/Shoot pads */
            input_map_set_binding(map, 0,  BIND_TYPE_P1_BTN, CS_ACT_LEFT,      COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_P1_BTN, CS_ACT_DOWN,      COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 2,  BIND_TYPE_P1_BTN, CS_ACT_RIGHT,     COLOR_BLUE,   "RIGHT");
            input_map_set_binding(map, 3,  BIND_TYPE_P1_BTN, CS_ACT_JUMP,      COLOR_GREEN,  "JUMP");

            input_map_set_binding(map, 4,  BIND_TYPE_P1_BTN, CS_ACT_LEFT,      COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 5,  BIND_TYPE_P1_BTN, CS_ACT_UP,        COLOR_BLUE,   "UP");
            input_map_set_binding(map, 6,  BIND_TYPE_P1_BTN, CS_ACT_SHOOT,     COLOR_RED,    "SHOOT");
            input_map_set_binding(map, 7,  BIND_TYPE_P1_BTN, CS_ACT_JUMP,      COLOR_GREEN,  "JUMP");

            input_map_set_binding(map, 8,  BIND_TYPE_P1_BTN, CS_ACT_PREV_WPN,  COLOR_AMBER,  "WPN-");
            input_map_set_binding(map, 9,  BIND_TYPE_P1_BTN, CS_ACT_NEXT_WPN,  COLOR_AMBER,  "WPN+");
            input_map_set_binding(map, 10, BIND_TYPE_P1_BTN, CS_ACT_SHOOT,     COLOR_RED,    "SHOOT");
            input_map_set_binding(map, 11, BIND_TYPE_P1_BTN, CS_ACT_STRAFE,    COLOR_CYAN,   "STRF");

            input_map_set_binding(map, 12, BIND_TYPE_P1_BTN, CS_ACT_INVENTORY, COLOR_PURPLE, "ITEM");
            input_map_set_binding(map, 13, BIND_TYPE_P1_BTN, CS_ACT_MAP,       COLOR_TEAL,   "MAP");
            input_map_set_binding(map, 14, BIND_TYPE_P1_BTN, CS_ACT_SKIP,      COLOR_YELLOW, "SKIP");
            input_map_set_binding(map, 15, BIND_TYPE_P1_BTN, CS_ACT_PAUSE,     COLOR_WHITE,  "PAUSE");
            populate_bank_b_p2(map);
            break;

        case PRESET_TWOPLAYER:
            /* Split pad grid: Bottom 2 rows = P1, Top 2 rows = P2 */
            input_map_set_binding(map, 0,  BIND_TYPE_P1_BTN, CS_ACT_LEFT,      COLOR_BLUE,   "LEFT");
            input_map_set_binding(map, 1,  BIND_TYPE_P1_BTN, CS_ACT_DOWN,      COLOR_BLUE,   "DOWN");
            input_map_set_binding(map, 2,  BIND_TYPE_P1_BTN, CS_ACT_RIGHT,     COLOR_BLUE,   "RIGHT");
            input_map_set_binding(map, 3,  BIND_TYPE_P1_BTN, CS_ACT_JUMP,      COLOR_GREEN,  "JUMP");

            input_map_set_binding(map, 4,  BIND_TYPE_P1_BTN, CS_ACT_PREV_WPN,  COLOR_AMBER,  "WPN-");
            input_map_set_binding(map, 5,  BIND_TYPE_P1_BTN, CS_ACT_UP,        COLOR_BLUE,   "UP");
            input_map_set_binding(map, 6,  BIND_TYPE_P1_BTN, CS_ACT_NEXT_WPN,  COLOR_AMBER,  "WPN+");
            input_map_set_binding(map, 7,  BIND_TYPE_P1_BTN, CS_ACT_SHOOT,     COLOR_RED,    "SHOOT");

            input_map_set_binding(map, 8,  BIND_TYPE_P2_BTN, CS_ACT_LEFT,      COLOR_PURPLE, "2:LEFT");
            input_map_set_binding(map, 9,  BIND_TYPE_P2_BTN, CS_ACT_DOWN,      COLOR_PURPLE, "2:DOWN");
            input_map_set_binding(map, 10, BIND_TYPE_P2_BTN, CS_ACT_RIGHT,     COLOR_PURPLE, "2:RGHT");
            input_map_set_binding(map, 11, BIND_TYPE_P2_BTN, CS_ACT_JUMP,      COLOR_CYAN,   "2:JUMP");

            input_map_set_binding(map, 12, BIND_TYPE_P2_BTN, CS_ACT_PREV_WPN,  COLOR_AMBER,  "2:WPN-");
            input_map_set_binding(map, 13, BIND_TYPE_P2_BTN, CS_ACT_UP,        COLOR_PURPLE, "2:UP");
            input_map_set_binding(map, 14, BIND_TYPE_P2_BTN, CS_ACT_NEXT_WPN,  COLOR_AMBER,  "2:WPN+");
            input_map_set_binding(map, 15, BIND_TYPE_P2_BTN, CS_ACT_SHOOT,     COLOR_PINK,   "2:SHOT");
            populate_bank_b_p2(map);
            break;
    }
}

void input_map_init(struct input_map *map) {
    if (!map) return;
    memset(map, 0, sizeof(*map));
    map->active_bank = 0;
    input_map_apply_preset(map, PRESET_ARCADE);
}

void input_map_translate(struct input_map *map,
                         const struct input *in,
                         bool in_menu,
                         struct cs_input_state *out) {
    if (!map || !out) return;
    memset(out, 0, sizeof(*out));
    if (!in) return;

    int bank_offset = map->active_bank * PAD_BANK_SIZE;
    for (int i = 0; i < PAD_BANK_SIZE; i++) {
        if (!in->pad_down[i]) continue;

        const struct pad_binding *b = &map->pads[bank_offset + i];
        uint16_t mask = input_map_action_to_mask(b->code);
        if (b->type == BIND_TYPE_P1_BTN) {
            out->p1_buttons |= mask;
        } else if (b->type == BIND_TYPE_P2_BTN) {
            out->p2_buttons |= mask;
        }
    }

    /* Hardware button shortcuts during gameplay */
    if (!(in->held & (1u << IN_SHIFT))) {
        if (in->held & (1u << IN_MINUS)) out->p1_buttons |= CS_BTN_PREV_WPN;
        if (in->held & (1u << IN_PLUS))  out->p1_buttons |= CS_BTN_NEXT_WPN;
    }

    if (in->encoder != 0) {
        map->wheel_pending_dir = (in->encoder > 0) ? 1 : -1;
        map->wheel_hold_ticks = 2;
    }
    if (in->encoder_press) {
        map->wheel_click_ticks = 2;
    }

    /* Rotary encoder: navigates Up/Down in menus, or cycles weapons in gameplay */
    if (map->wheel_hold_ticks > 0) {
        if (in_menu) {
            if (map->wheel_pending_dir > 0) out->p1_buttons |= CS_BTN_DOWN;
            else if (map->wheel_pending_dir < 0) out->p1_buttons |= CS_BTN_UP;
        } else {
            if (map->wheel_pending_dir > 0) out->p1_buttons |= CS_BTN_NEXT_WPN;
            else if (map->wheel_pending_dir < 0) out->p1_buttons |= CS_BTN_PREV_WPN;
        }
        map->wheel_hold_ticks--;
        if (map->wheel_hold_ticks == 0) {
            map->wheel_pending_dir = 0;
        }
    }

    if (map->wheel_click_ticks > 0) {
        out->p1_buttons |= CS_BTN_OK;
        map->wheel_click_ticks--;
    }
}

void input_map_get_pad_led(const struct input_map *map,
                           const struct input *in,
                           int pad,
                           uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!r || !g || !b) return;
    *r = 0; *g = 0; *b = 0;
    if (!map || pad < 0 || pad >= PAD_BANK_SIZE) return;

    int bank_offset = map->active_bank * PAD_BANK_SIZE;
    const struct pad_binding *bind = &map->pads[bank_offset + pad];
    uint32_t col = bind->color;
    uint8_t br = (col >> 16) & 0xFF;
    uint8_t bg = (col >> 8) & 0xFF;
    uint8_t bb = col & 0xFF;

    if (bind->type == BIND_TYPE_NONE || bind->code == CS_ACT_NONE) {
        *r = 3;
        *g = 3;
        *b = 5;
        return;
    }

    if (in && in->pad_down[pad]) {
        /* Bright velocity-scaled flash when hit */
        int vel = in->pad_vel[pad] > 0 ? in->pad_vel[pad] : 100;
        int boost = (vel > 60) ? 100 : 80;
        int rr = (br >> 1) + boost;
        int gg = (bg >> 1) + boost;
        int bb_val = (bb >> 1) + boost;
        *r = (uint8_t)(rr > 127 ? 127 : rr);
        *g = (uint8_t)(gg > 127 ? 127 : gg);
        *b = (uint8_t)(bb_val > 127 ? 127 : bb_val);
    } else {
        *r = (br >> 2);
        *g = (bg >> 2);
        *b = (bb >> 2);
    }
}
