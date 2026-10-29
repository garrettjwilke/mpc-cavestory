#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "input.h"
#include "ui.h"

#include "audio_engine.h"
#include "config.h"
#include "cs_runner.h"
#include "input_map.h"
#include "ui_overlay.h"

struct app_state {
    struct cs_runner *runner;
    struct input_map map;
    struct app_settings settings;
    struct ui_overlay overlay;
    struct audio_engine *audio;
    bool audio_active;

    int64_t next_tick_ns;
    int encoder_accum;
    int pad_latched[16];
    int pad_latch_vel[16];

    int shown_mode;
    int shown_bank;
    int shown_toast;
};

static int64_t mono_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000000LL + (int64_t)t.tv_nsec;
}

static void app_audio_fill(void *ctx, int16_t *dst, int frames) {
    struct app_state *app = (struct app_state *)ctx;
    if (app && app->runner) {
        cs_runner_fill_audio(app->runner, dst, frames);
    } else if (dst && frames > 0) {
        memset(dst, 0, (size_t)frames * 2 * sizeof(int16_t));
    }
}

static void audio_set_active(struct app_state *app, bool active) {
    if (!app->audio || app->audio_active == active) return;
    audio_engine_set_active(app->audio, active);
    app->audio_active = active;
}

static void clear_game_edges(struct app_state *app) {
    app->encoder_accum = 0;
    app->next_tick_ns = 0;
    memset(app->pad_latched, 0, sizeof(app->pad_latched));
}

static int ticks_due(int64_t now, int64_t *next, int tps) {
    int64_t tick_ns = 1000000000LL / (tps > 0 ? tps : 50);
    if (*next <= 0) {
        *next = now + tick_ns;
        return 1;
    }
    int n = 0;
    while (now >= *next && n < 2) {
        *next += tick_ns;
        n++;
    }
    if (now >= *next) {
        *next = now + tick_ns;
    }
    return n;
}

static void latch_game_input(struct app_state *app, const struct input *in) {
    app->encoder_accum += in->encoder;
    for (int p = 0; p < 16; p++) {
        if (in->pad_hit[p] >= 0) {
            app->pad_latched[p] = 1;
            app->pad_latch_vel[p] = in->pad_hit[p];
        }
    }
}

static void tick_game(struct app_state *app, const struct input *in, int n) {
    bool in_menu = cs_runner_is_in_menu(app->runner);
    for (int i = 0; i < n; i++) {
        struct input tick_in = *in;
        if (i == 0) {
            tick_in.encoder = app->encoder_accum;
            for (int p = 0; p < 16; p++) {
                if (app->pad_latched[p] && !tick_in.pad_down[p]) {
                    tick_in.pad_down[p] = 1;
                    if (tick_in.pad_vel[p] <= 0) {
                        tick_in.pad_vel[p] = app->pad_latch_vel[p];
                    }
                }
            }
        } else {
            tick_in.encoder = 0;
            tick_in.encoder_press = 0;
        }
        struct cs_input_state cs_in;
        input_map_translate(&app->map, &tick_in, in_menu, &cs_in);
        cs_runner_tick(app->runner, &cs_in);
    }
    app->encoder_accum = 0;
    memset(app->pad_latched, 0, sizeof(app->pad_latched));
}

static int toast_on(const struct app_state *app, int64_t now_ms) {
    return app->overlay.toast_msg[0] && now_ms < app->overlay.toast_until_ms;
}

static unsigned chrome_mask(const struct app_state *app, int64_t now_ms) {
    int bank = input_map_get_bank(&app->map);
    int toast = toast_on(app, now_ms);
    if (app->shown_mode != (int)UI_MODE_GAMEPLAY
        || app->shown_bank != bank
        || app->shown_toast != toast) {
        unsigned mask = UI_OVERLAY_CHROME;
        if (app->shown_toast != toast) {
            mask |= UI_OVERLAY_VIEWPORT;
        }
        return mask;
    }
    return 0;
}

static void remember_chrome(struct app_state *app, int64_t now_ms) {
    app->shown_mode = UI_MODE_GAMEPLAY;
    app->shown_bank = input_map_get_bank(&app->map);
    app->shown_toast = toast_on(app, now_ms);
}

static int wake_for(int64_t now_ns, int64_t next_ns, bool playing) {
    if (!playing) return 16;
    if (next_ns <= now_ns) return 1;
    int64_t ms = (next_ns - now_ns + 999999LL) / 1000000LL;
    if (ms < 1) return 1;
    if (ms > 16) return 16;
    return (int)ms;
}

static int app_frame(struct ui *u, const struct input *in, void *state) {
    struct app_state *app = (struct app_state *)state;
    if (!app || !u || !in) return 0;

    int64_t now_ns = mono_ns();
    u->now_ms = now_ns / 1000000LL;

    if (in->down & (1u << IN_PAD_BANK)) {
        input_map_toggle_bank(&app->map);
        ui_overlay_show_toast(&app->overlay,
            (input_map_get_bank(&app->map) == 0) ? "Pad Bank A (1-16)" : "Pad Bank B (17-32)",
            u->now_ms, 1200);
    }

    if (in->down & (1u << IN_PLAY)) {
        if (in->held & (1u << IN_SHIFT)) {
            enum input_preset_id next_p = (enum input_preset_id)((app->map.current_preset + 1) % PRESET_COUNT);
            input_map_apply_preset(&app->map, next_p);
            ui_overlay_show_toast(&app->overlay, input_map_preset_name(next_p), u->now_ms, 1400);
        } else {
            cs_runner_set_paused(app->runner, false);
            ui_overlay_set_mode(&app->overlay, UI_MODE_GAMEPLAY);
        }
    }
    if (in->down & (1u << IN_STOP)) {
        cs_runner_set_paused(app->runner, true);
        ui_overlay_set_mode(&app->overlay, UI_MODE_GAME_MENU);
    }

    if (in->main_moved && app->audio) {
        audio_engine_set_volume(app->audio, in->main);
        app->settings.volume = (in->main * 100) / 127;
    }

    UiMode mode0 = app->overlay.mode;
    if (mode0 != UI_MODE_GAMEPLAY) {
        ui_clear(u, 0x080A0E);
    }

    int req = ui_overlay_draw(&app->overlay, u, in, app->runner, &app->map, &app->settings,
                              mode0 == UI_MODE_GAMEPLAY ? UI_OVERLAY_INPUT : 0);
    if (req || cs_runner_should_quit(app->runner)) {
        return 1;
    }

    if (mode0 == UI_MODE_GAMEPLAY && app->overlay.mode != UI_MODE_GAMEPLAY) {
        app->shown_mode = -1;
        clear_game_edges(app);
    }

    bool in_game = (app->overlay.mode == UI_MODE_GAMEPLAY);
    if (in_game && cs_runner_is_paused(app->runner)) {
        cs_runner_set_paused(app->runner, false);
    } else if (!in_game && !cs_runner_is_paused(app->runner)) {
        cs_runner_set_paused(app->runner, true);
    }

    bool playing = in_game && !cs_runner_is_paused(app->runner);
    int ticked = 0;
    if (playing) {
        latch_game_input(app, in);
        int tps = cs_runner_get_tps(app->runner);
        int n = ticks_due(now_ns, &app->next_tick_ns, tps);
        if (n > 0) {
            tick_game(app, in, n);
            ticked = 1;
        }
    } else {
        clear_game_edges(app);
    }

    if (in_game) {
        unsigned paint = chrome_mask(app, u->now_ms);
        if (ticked || app->shown_mode != (int)UI_MODE_GAMEPLAY) {
            paint |= UI_OVERLAY_VIEWPORT;
        }
        if (paint) {
            ui_overlay_draw(&app->overlay, u, in, app->runner, &app->map, &app->settings, paint);
            if (paint & UI_OVERLAY_CHROME) {
                remember_chrome(app, u->now_ms);
            }
        }
    } else {
        app->shown_mode = -1;
    }

    audio_set_active(app, playing);

    for (int p = 0; p < 16; p++) {
        uint8_t r = 0, g = 0, b = 0;
        input_map_get_pad_led(&app->map, in, p, &r, &g, &b);
        ui_led(u, p, r, g, b);
    }

    if (app->audio) {
        int l_mask = 0, r_mask = 0;
        audio_engine_get_meter_masks(app->audio, &l_mask, &r_mask);
        ui_meter(u, l_mask, r_mask);
    }

    ui_button_led(u, IN_PLAY, playing ? 2 : 0);
    ui_button_led(u, IN_STOP, playing ? 0 : 2);
    ui_button_led(u, IN_PAD_BANK, (input_map_get_bank(&app->map) == 0) ? 1 : 2);
    ui_button_led(u, IN_ERASE, (app->overlay.mode == UI_MODE_REBIND) ? 2 : 0);

    ui_wake(u, wake_for(now_ns, app->next_tick_ns, playing));
    return 0;
}

static bool is_dir(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void resolve_paths(int argc, char **argv, char *data_dir, size_t data_len, char *user_dir, size_t user_len) {
    char exe_dir[512] = ".";
    if (argc > 0 && argv[0]) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s", argv[0]);
        char *slash = strrchr(tmp, '/');
        if (slash) {
            *slash = '\0';
            snprintf(exe_dir, sizeof(exe_dir), "%s", tmp);
        }
    }

    if (argc > 1 && argv[1] && is_dir(argv[1])) {
        snprintf(data_dir, data_len, "%s", argv[1]);
    } else {
        char cand1[512], cand2[512];
        snprintf(cand1, sizeof(cand1), "%s/gamedata/data", exe_dir);
        snprintf(cand2, sizeof(cand2), "%s/../gamedata/data", exe_dir);
        if (is_dir(cand1)) {
            snprintf(data_dir, data_len, "%s", cand1);
        } else if (is_dir(cand2)) {
            snprintf(data_dir, data_len, "%s", cand2);
        } else if (is_dir("gamedata/data")) {
            snprintf(data_dir, data_len, "gamedata/data");
        } else if (is_dir("/media/MPC/mpc-cavestory/gamedata/data")) {
            snprintf(data_dir, data_len, "/media/MPC/mpc-cavestory/gamedata/data");
        } else {
            snprintf(data_dir, data_len, "gamedata/data");
        }
    }

    snprintf(user_dir, user_len, "%s/user", exe_dir);
    mkdir(user_dir, 0755);
}

int main(int argc, char **argv) {
    struct app_state app;
    memset(&app, 0, sizeof(app));
    app.shown_mode = -1;

    char data_dir[512];
    char user_dir[512];
    resolve_paths(argc, argv, data_dir, sizeof(data_dir), user_dir, sizeof(user_dir));

    char bindings_path[512];
    char settings_path[512];
    snprintf(bindings_path, sizeof(bindings_path), "%s/bindings.json", user_dir);
    snprintf(settings_path, sizeof(settings_path), "%s/settings.json", user_dir);

    config_defaults(&app.settings);
    config_load_settings(settings_path, &app.settings);

    input_map_init(&app.map);
    input_map_apply_preset(&app.map, (enum input_preset_id)app.settings.preset);
    config_load_bindings(bindings_path, &app.map);

    ui_overlay_init(&app.overlay, bindings_path, settings_path);

    app.runner = cs_runner_create(data_dir, user_dir, 44100);
    if (!app.runner) {
        fprintf(stderr, "Failed to create Cave Story runner (data_dir=%s)\n", data_dir);
        return 1;
    }

    app.audio = audio_engine_create(44100, 512);
    if (app.audio) {
        audio_engine_set_source(app.audio, app_audio_fill, &app);
        audio_engine_set_volume(app.audio, (app.settings.volume * 127) / 100);
        audio_engine_start(app.audio);
    }

    int rc = app_run(app_frame, &app);

    if (app.audio) {
        audio_engine_destroy(app.audio);
        app.audio = NULL;
    }

    if (app.runner) {
        cs_runner_destroy(app.runner);
        app.runner = NULL;
    }

    return rc;
}

