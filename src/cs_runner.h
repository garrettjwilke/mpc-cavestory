#ifndef MPC_CS_RUNNER_H
#define MPC_CS_RUNNER_H

#include <stdbool.h>
#include <stdint.h>

#define CS_SCREEN_WIDTH  320
#define CS_SCREEN_HEIGHT 240

/* Cave Story logical button bitmasks */
#define CS_BTN_LEFT      (1u << 0)
#define CS_BTN_DOWN      (1u << 1)
#define CS_BTN_RIGHT     (1u << 2)
#define CS_BTN_UP        (1u << 3)
#define CS_BTN_JUMP      (1u << 4)
#define CS_BTN_SHOOT     (1u << 5)
#define CS_BTN_PREV_WPN  (1u << 6)
#define CS_BTN_NEXT_WPN  (1u << 7)
#define CS_BTN_INVENTORY (1u << 8)
#define CS_BTN_MAP       (1u << 9)
#define CS_BTN_STRAFE    (1u << 10)
#define CS_BTN_SKIP      (1u << 11)
#define CS_BTN_PAUSE     (1u << 12)
#define CS_BTN_OK        (1u << 13)

struct cs_input_state {
    uint16_t p1_buttons;
    uint16_t p2_buttons;
};

struct cs_runner;

struct cs_runner *cs_runner_create(const char *data_dir, const char *user_dir, int samplerate);
void cs_runner_destroy(struct cs_runner *r);
void cs_runner_reset(struct cs_runner *r);

void cs_runner_tick(struct cs_runner *r, const struct cs_input_state *input);
void cs_runner_render(struct cs_runner *r, uint32_t *dst_fb_320x240);
void cs_runner_fill_audio(struct cs_runner *r, int16_t *dst_stereo, int frames);

bool cs_runner_is_paused(const struct cs_runner *r);
void cs_runner_set_paused(struct cs_runner *r, bool paused);
bool cs_runner_is_in_menu(struct cs_runner *r);
bool cs_runner_should_quit(struct cs_runner *r);
int  cs_runner_get_tps(struct cs_runner *r);
uint64_t cs_runner_get_frame_count(const struct cs_runner *r);

#endif /* MPC_CS_RUNNER_H */

