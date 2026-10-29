#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "cs_runner.h"
#include "input.h"
#include "input_map.h"
#include "ui.h"
#include "ui_overlay.h"

static void test_input_map_and_delete_bind(void) {
    printf("[TEST] input_map presets, translation, and deleting a bind...\n");
    struct input_map map;
    input_map_init(&map);

    /* Pad 0 in Arcade preset is Left, Pad 3 is Jump */
    assert(map.pads[0].type == BIND_TYPE_P1_BTN);
    assert(map.pads[0].code == CS_ACT_LEFT);
    assert(map.pads[3].type == BIND_TYPE_P1_BTN);
    assert(map.pads[3].code == CS_ACT_JUMP);

    struct input in;
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < 16; i++) in.pad_hit[i] = -1;
    in.pad_down[0] = 1;
    in.pad_down[3] = 1;

    struct cs_input_state cs_in;
    input_map_translate(&map, &in, false, &cs_in);
    assert((cs_in.p1_buttons & CS_BTN_LEFT) != 0);
    assert((cs_in.p1_buttons & CS_BTN_JUMP) != 0);
    assert((cs_in.p1_buttons & CS_BTN_SHOOT) == 0);

    /* Delete bind on Pad 3 using input_map_clear_binding */
    input_map_clear_binding(&map, 3);
    assert(map.pads[3].type == BIND_TYPE_NONE);
    assert(map.pads[3].code == CS_ACT_NONE);
    assert(strcmp(map.pads[3].label, "-") == 0);

    /* Holding Pad 3 now produces no button press */
    input_map_translate(&map, &in, false, &cs_in);
    assert((cs_in.p1_buttons & CS_BTN_LEFT) != 0);
    assert((cs_in.p1_buttons & CS_BTN_JUMP) == 0);

    /* Delete bind on Pad 0 using input_map_set_binding(..., CS_ACT_NONE) */
    input_map_set_binding(&map, 0, BIND_TYPE_P1_BTN, CS_ACT_NONE, 0, NULL);
    assert(map.pads[0].type == BIND_TYPE_NONE);
    assert(map.pads[0].code == CS_ACT_NONE);
    input_map_translate(&map, &in, false, &cs_in);
    assert(cs_in.p1_buttons == 0);

    /* Wheel in gameplay cycles weapons; wheel in menu navigates Up/Down */
    memset(&in, 0, sizeof(in));
    in.encoder = 1;
    input_map_translate(&map, &in, false, &cs_in);
    assert((cs_in.p1_buttons & CS_BTN_NEXT_WPN) != 0);

    in.encoder = 1;
    input_map_translate(&map, &in, true, &cs_in);
    assert((cs_in.p1_buttons & CS_BTN_DOWN) != 0);
}

static void test_config_roundtrip(void) {
    printf("[TEST] config save/load roundtrip (including deleted binds)...\n");
    const char *tmp_bind = "/tmp/mpc_cs_test_bindings.json";
    const char *tmp_sett = "/tmp/mpc_cs_test_settings.json";

    struct app_settings s_out, s_in;
    config_defaults(&s_out);
    s_out.volume = 65;
    s_out.speaker_on = false;
    s_out.preset = PRESET_ERGONOMIC;
    assert(config_save_settings(tmp_sett, &s_out));

    config_defaults(&s_in);
    assert(config_load_settings(tmp_sett, &s_in));
    assert(s_in.volume == 65);
    assert(s_in.speaker_on == false);
    assert(s_in.preset == PRESET_ERGONOMIC);

    struct input_map m_out, m_in;
    input_map_init(&m_out);
    input_map_clear_binding(&m_out, 3); /* Delete Pad 4 bind */
    input_map_set_binding(&m_out, 5, BIND_TYPE_P1_BTN, CS_ACT_MAP, COLOR_TEAL, "MAP");
    assert(config_save_bindings(tmp_bind, &m_out));

    input_map_init(&m_in);
    assert(config_load_bindings(tmp_bind, &m_in));
    assert(m_in.pads[3].type == BIND_TYPE_NONE);
    assert(m_in.pads[3].code == CS_ACT_NONE);
    assert(strcmp(m_in.pads[3].label, "-") == 0);
    assert(m_in.pads[5].type == BIND_TYPE_P1_BTN);
    assert(m_in.pads[5].code == CS_ACT_MAP);

    unlink(tmp_bind);
    unlink(tmp_sett);
}

static void test_ui_overlay_rebind_and_delete(void) {
    printf("[TEST] ui_overlay rebind modal (assign, ERASE delete, and wheel unmap)...\n");
    uint32_t fb_px[UI_W * UI_H];
    memset(fb_px, 0, sizeof(fb_px));

    struct ui u;
    memset(&u, 0, sizeof(u));
    u.px = fb_px;
    u.now_ms = 1000;

    struct input_map map;
    input_map_init(&map);

    struct app_settings settings;
    config_defaults(&settings);

    struct ui_overlay overlay;
    ui_overlay_init(&overlay, "/tmp/mpc_cs_ui_bind.json", "/tmp/mpc_cs_ui_set.json");

    struct input in;
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < 16; i++) in.pad_hit[i] = -1;

    /* Press B2 in gameplay to enter UI_MODE_REBIND */
    in.down = (1u << IN_B2);
    ui_overlay_draw(&overlay, &u, &in, NULL, &map, &settings, UI_OVERLAY_INPUT);
    assert(ui_overlay_get_mode(&overlay) == UI_MODE_REBIND);

    /* Hit Pad 7 (index 6, currently SHOOT) to select it, then press ERASE to delete bind */
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < 16; i++) in.pad_hit[i] = -1;
    in.pad_hit[6] = 100;
    in.down = (1u << IN_ERASE);
    ui_overlay_draw(&overlay, &u, &in, NULL, &map, &settings, 0);

    assert(overlay.rebind_selected_pad == 6);
    assert(map.pads[6].type == BIND_TYPE_NONE);
    assert(map.pads[6].code == CS_ACT_NONE);
    assert(strcmp(map.pads[6].label, "-") == 0);

    /* Now assign JUMP to Pad 7 via wheel + encoder_press */
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < 16; i++) in.pad_hit[i] = -1;
    overlay.rebind_category = REBIND_CAT_ALL_P1;
    overlay.rebind_item_idx = 4; /* CS_ACT_JUMP in CAT_ALL_P1_ITEMS */
    in.encoder_press = 1;
    ui_overlay_draw(&overlay, &u, &in, NULL, &map, &settings, 0);
    assert(map.pads[6].type == BIND_TYPE_P1_BTN);
    assert(map.pads[6].code == CS_ACT_JUMP);

    /* Now switch to REBIND_CAT_CLEAR and click wheel to delete bind */
    memset(&in, 0, sizeof(in));
    for (int i = 0; i < 16; i++) in.pad_hit[i] = -1;
    overlay.rebind_category = REBIND_CAT_CLEAR;
    in.encoder_press = 1;
    ui_overlay_draw(&overlay, &u, &in, NULL, &map, &settings, 0);
    assert(map.pads[6].type == BIND_TYPE_NONE);
    assert(map.pads[6].code == CS_ACT_NONE);
    assert(strcmp(map.pads[6].label, "-") == 0);
}

static void test_cs_runner_integration(void) {
    printf("[TEST] cs_runner standalone embedded gamedata (zero external files, 320x240 render, audio)...\n");
    const char *user_dir = "/tmp/mpc_cs_test_user";
    mkdir(user_dir, 0755);

    /* Pass NULL for data_dir to verify 100% embedded single-binary execution */
    struct cs_runner *runner = cs_runner_create(NULL, user_dir, 44100);
    assert(runner != NULL);

    struct cs_input_state empty_in = {0, 0};
    uint32_t fb[CS_SCREEN_WIDTH * CS_SCREEN_HEIGHT];
    memset(fb, 0, sizeof(fb));

    /* Tick 40 times to complete LoadingScene and enter intro/title from embedded VFS */
    for (int i = 0; i < 40; i++) {
        cs_runner_tick(runner, &empty_in);
    }
    assert(cs_runner_get_frame_count(runner) >= 40);

    /* Render 320x240 frame and verify non-black pixels are drawn */
    cs_runner_render(runner, fb);
    int non_zero_pixels = 0;
    for (int i = 0; i < CS_SCREEN_WIDTH * CS_SCREEN_HEIGHT; i++) {
        if ((fb[i] & 0x00FFFFFF) != 0) {
            non_zero_pixels++;
        }
    }
    printf("       Rendered 320x240 frame from embedded VFS: %d / %d non-black pixels\n",
           non_zero_pixels, CS_SCREEN_WIDTH * CS_SCREEN_HEIGHT);
    assert(non_zero_pixels > 100);

    /* Test audio mixing */
    int16_t audio_buf[1024 * 2];
    memset(audio_buf, 0, sizeof(audio_buf));
    cs_runner_fill_audio(runner, audio_buf, 1024);

    cs_runner_destroy(runner);
}

int main(void) {
    printf("=== mpc-cavestory Test Suite ===\n");
    test_input_map_and_delete_bind();
    test_config_roundtrip();
    test_ui_overlay_rebind_and_delete();
    test_cs_runner_integration();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}

