#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "filebrowser.h"
#include "input.h"
#include "ui.h"
#include "usb_sd.h"
#include "widgets.h"

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
    /* Set by the toolkit autorun launcher. The binary runs from /run. */
    const char *home = getenv("MPC_APP_DIR");
    if (home && home[0] && strlen(home) < sizeof(exe_dir)) {
        snprintf(exe_dir, sizeof(exe_dir), "%s", home);
    } else if (argc > 0 && argv[0]) {
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

int has_game_data(const char *path) {
    if (!path || !path[0]) return 0;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;

    DIR *d = opendir(path);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        struct stat st2;
        if (stat(full, &st2) == 0 && S_ISREG(st2.st_mode)) {
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

struct data_dir_picker {
    int phase; /* 0 menu, 1 browse, 2 usb */
    char data_dir[512];
    struct ui_filebrowser fb;
    int fb_ready;
    struct ui_usb_sd usb;
    int usb_ready;
    struct ui_list_state list_state;
    int selected;
};

int data_dir_picker_frame(struct ui *u, const struct input *in, void *state) {
    struct data_dir_picker *p = (struct data_dir_picker *)state;
    if (!p || !u) return 0;

    if (p->selected) {
        ui_quit(u);
        return 1;
    }

    if (p->phase == 2) {
        if (!p->usb_ready) {
            ui_usb_sd_init(&p->usb);
            p->usb_ready = 1;
        }
        struct ui_usb_sd_style ust = ui_usb_sd_style_default();
        int rc = ui_usb_sd(u, &p->usb, ust, in);
        if (rc == UI_USB_SD_DONE || rc == UI_USB_SD_FAILED) {
            p->phase = 0;
            p->usb_ready = 0;
        }
        return 0;
    }

    if (p->phase == 1) {
        if (!p->fb_ready) {
            struct ui_filebrowser_config cfg;
            memset(&cfg, 0, sizeof(cfg));
            cfg.initial_path = NULL;
            cfg.title = "SELECT DATA FOLDER";
            cfg.action_label = "SELECT";
            cfg.options = UI_FB_OPT_HIDE_DOTFILES | UI_FB_OPT_TOGGLE_DOTFILES |
                          UI_FB_OPT_DIRS_ONLY | UI_FB_OPT_SELECT_DIR |
                          UI_FB_OPT_SHOW_SIZE;
            ui_filebrowser_init(&p->fb, &cfg);
            p->fb_ready = 1;
        }
        struct ui_filebrowser_style fst = ui_filebrowser_style_default();
        int res = ui_filebrowser(u, 0, 0, UI_W, UI_H, &p->fb, fst, in);
        if (res == UI_FB_ACTIVE && p->fb.cursor >= 0 && p->fb.cursor < p->fb.count) {
            const struct ui_fb_entry *e = &p->fb.entries[p->fb.cursor];
            char full[UI_FB_MAX_PATH];
            if (e->is_dir) {
                snprintf(full, sizeof(full), "%s/%s", p->fb.cwd[0] ? p->fb.cwd : "", e->name);
                /* normalize leading slash */
                if (full[0] == '/' && full[1] == '/') {
                    /* collapse accidental double slash not needed; has_game_data handles stat */
                }
                if (has_game_data(full)) {
                    snprintf(p->fb.status_msg, sizeof(p->fb.status_msg),
                             "Data directory found. Use this directory?");
                    p->fb.status_is_error = 0;
                } else if (has_game_data(p->fb.cwd)) {
                    snprintf(p->fb.status_msg, sizeof(p->fb.status_msg),
                             "Data directory found. Press Select to use this directory.");
                    p->fb.status_is_error = 0;
                }
            } else if (has_game_data(p->fb.cwd)) {
                snprintf(p->fb.status_msg, sizeof(p->fb.status_msg),
                         "Data directory found. Press Select to use this directory.");
                p->fb.status_is_error = 0;
            }
        }
        if (res == UI_FB_SELECT) {
            if (has_game_data(p->fb.selected_path)) {
                snprintf(p->data_dir, sizeof(p->data_dir), "%s", p->fb.selected_path);
                p->selected = 1;
                ui_quit(u);
            } else {
                snprintf(p->fb.status_msg, sizeof(p->fb.status_msg), "No game data in folder");
                p->fb.status_is_error = 1;
            }
        } else if (res == UI_FB_CANCEL) {
            p->phase = 0;
            p->fb_ready = 0;
        }
        return 0;
    }

    ui_clear(u, UI_BG);
    ui_text(u, 12, 16, "Game data required", UI_HL, 2);
    ui_text(u, 12, 40, "No game data directory was found.", UI_TEXT, 1);
    ui_text(u, 12, 56, "Provide one to continue, or quit.", 0x9aa0a6, 1);

    const char *rows[] = {"Browse for folder", "Add data directory with USB", "Quit"};
    struct ui_list_style lst = {
        .text = UI_TEXT,
        .highlight = UI_HL,
        .row_h = 24,
        .highlight_px = 2,
    };
    int hit = ui_list(u, 12, 84, 296, 100, rows, 3, lst, in, &p->list_state);
    if (hit == 0) {
        p->phase = 1;
        p->fb_ready = 0;
    } else if (hit == 1) {
        p->phase = 2;
        p->usb_ready = 0;
    } else if (hit == 2) {
        ui_quit(u);
    }

    /* Also offer an explicit on-screen Quit button. Only activate it when the
       list did not consume this frame's encoder press, otherwise selecting a
       row above (e.g. "Add data directory with USB") would also trigger quit. */
    struct ui_button qst = {
        .fill = UI_BOX,
        .text = UI_TEXT,
        .border_set = 1,
        .border = UI_HL,
        .border_px = 1,
        .highlighted = 1,
        .highlight = UI_HL,
        .highlight_px = 1,
    };
    if (hit < 0 && ui_button(u, 12, 200, 80, 24, "Quit", qst, in)) {
        ui_quit(u);
    }

    return 0;
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

    /* Use a remembered data directory if the resolved one is missing. */
    if (!has_game_data(data_dir) && has_game_data(app.settings.data_dir)) {
        snprintf(data_dir, sizeof(data_dir), "%s", app.settings.data_dir);
    }

    if (!has_game_data(data_dir)) {
        struct data_dir_picker picker;
        memset(&picker, 0, sizeof(picker));
        picker.phase = 0;
        picker.data_dir[0] = '\0';
        picker.selected = 0;
        app_run(data_dir_picker_frame, &picker);
        if (!picker.selected || !has_game_data(picker.data_dir)) {
            return 0;
        }
        snprintf(data_dir, sizeof(data_dir), "%s", picker.data_dir);
        snprintf(app.settings.data_dir, sizeof(app.settings.data_dir), "%s", picker.data_dir);
        config_save_settings(settings_path, &app.settings);
    }

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

