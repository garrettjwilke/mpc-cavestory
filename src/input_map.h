#ifndef MPC_INPUT_MAP_H
#define MPC_INPUT_MAP_H

#include "cs_runner.h"
#include "input.h"
#include "ui.h"
#include <stdbool.h>
#include <stdint.h>

#define PAD_BANK_SIZE  16
#define PAD_BANK_COUNT 2
#define PAD_SLOT_COUNT (PAD_BANK_SIZE * PAD_BANK_COUNT)

/* Pad & UI color palette (0x00RRGGBB) */
#define COLOR_DIM    0x202634
#define COLOR_BLUE   0x00B0FF
#define COLOR_GREEN  0x00E676
#define COLOR_RED    0xFF1744
#define COLOR_AMBER  0xFFB300
#define COLOR_PURPLE 0xD500F9
#define COLOR_TEAL   0x00BFA5
#define COLOR_CYAN   0x00E5FF
#define COLOR_YELLOW 0xFFD600
#define COLOR_PINK   0xFF4081
#define COLOR_WHITE  0xE8ECF0

enum bind_type {
    BIND_TYPE_NONE = 0,
    BIND_TYPE_P1_BTN,
    BIND_TYPE_P2_BTN,
};

enum cs_action_id {
    CS_ACT_NONE = 0,
    CS_ACT_LEFT,
    CS_ACT_DOWN,
    CS_ACT_RIGHT,
    CS_ACT_UP,
    CS_ACT_JUMP,
    CS_ACT_SHOOT,
    CS_ACT_PREV_WPN,
    CS_ACT_NEXT_WPN,
    CS_ACT_INVENTORY,
    CS_ACT_MAP,
    CS_ACT_STRAFE,
    CS_ACT_SKIP,
    CS_ACT_PAUSE,
    CS_ACT_OK,
    CS_ACT_COUNT
};

struct pad_binding {
    enum bind_type type;
    uint16_t code;      /* enum cs_action_id */
    uint32_t color;     /* 0x00RRGGBB LED & UI color */
    char label[12];     /* Short display label */
};

enum input_preset_id {
    PRESET_ARCADE = 0,
    PRESET_ERGONOMIC,
    PRESET_PLATFORMER,
    PRESET_TWOPLAYER,
    PRESET_COUNT
};

struct input_map {
    struct pad_binding pads[PAD_SLOT_COUNT];
    int active_bank; /* 0 = Bank A (0..15), 1 = Bank B (16..31) */
    enum input_preset_id current_preset;
    int wheel_pending_dir;  /* <0 = CCW, >0 = CW */
    int wheel_hold_ticks;
    int wheel_click_ticks;
};

void input_map_init(struct input_map *map);
void input_map_apply_preset(struct input_map *map, enum input_preset_id preset);
const char *input_map_preset_name(enum input_preset_id preset);

void input_map_set_bank(struct input_map *map, int bank);
int  input_map_get_bank(const struct input_map *map);
void input_map_toggle_bank(struct input_map *map);

void input_map_set_binding(struct input_map *map, int pad_idx,
                           enum bind_type type, uint16_t code,
                           uint32_t color, const char *label);
void input_map_clear_binding(struct input_map *map, int pad_idx);

/* Build cs_input_state from current held pads & rotary encoder */
void input_map_translate(struct input_map *map,
                         const struct input *in,
                         bool in_menu,
                         struct cs_input_state *out);

/* Compute RGB color for physical pad LED (0..127 per channel) */
void input_map_get_pad_led(const struct input_map *map,
                           const struct input *in,
                           int pad,
                           uint8_t *r, uint8_t *g, uint8_t *b);

/* Helpers for Rebind UI */
const char *input_map_action_short_name(uint16_t code);
const char *input_map_action_full_name(uint16_t code);
uint16_t    input_map_action_to_mask(uint16_t code);
uint32_t    input_map_default_color(enum bind_type type, uint16_t code);

#endif /* MPC_INPUT_MAP_H */
