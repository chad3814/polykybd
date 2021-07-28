#include QMK_KEYBOARD_H
#include "print.h"
#include "kdisp.h"

enum { LAYER_1 = 0, LAYER_2 = 1, LAYER_3 = 2 };

enum { KC_LAYER_1 = SAFE_RANGE, KC_LAYER_2, KC_LAYER_3 };

uint8_t g_rgb_matrix_mode = RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS;

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    [LAYER_1] = LAYOUT(KC_LAYER_2, KC_2, KC_1, KC_3),

    [LAYER_2] = LAYOUT(KC_LAYER_3, KC_5, KC_4, KC_6),

    [LAYER_3] = LAYOUT(KC_LAYER_1, KC_8, KC_7, KC_9)};

led_config_t g_led_config = {{// Key Matrix to LED Index
                              {6, 4},
                              {5, 3}},
                             {// LED Index to Physical Position
                              {230, 32}, {220, 32}, {210, 32}, {200, 32}, {190, 32}, {180, 32}, {170, 32}, {160, 32}, {150, 32}, {140, 32}//, {139, 32}, {138, 32}, {137, 32}, {136, 32}, {135, 32}, {134, 32}, {133, 32}, {132, 32}, {131, 32}, {130, 32}, {129, 32}, {128, 32}, {127, 32}, {126, 32}, {125, 32}, {124, 32}, {123, 32}, {122, 32}, {121, 32}, {120, 32}, {119, 32}, {118, 32}, {117, 32}, {116, 32}, {115, 32}, {114, 32}, {113, 32}, {112, 32}, {111, 32}, {110, 32}, {109, 32}, {108, 32}, {107, 32}, {106, 32}, {105, 32}, {104, 32}, {103, 32}, {102, 32}, {101, 32}, {100, 32}, {99, 32}, {98, 32}, {97, 32}, {96, 32}, {95, 32}, {94, 32}, {93, 32}, {92, 32}, {91, 32}, {90, 32}
                              },
                             {// LED Index to Flag
                              0, 0, 2, 4, 4, 4, 4, 2, 0, 0
                              //, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
                              }};

// void suspend_power_down_kb(void)
// {
//     rgb_matrix_set_suspend_state(true);
//     suspend_power_down_user();
// }

// void suspend_wakeup_init_kb(void)
// {
//     rgb_matrix_set_suspend_state(false);
//     suspend_wakeup_init_user();
// }

uint32_t startup_timer;
uint16_t last_key = 0;
bool finished_timer = false;

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    startup_timer = timer_read32();
    print("OLED initialized.");
    return OLED_ROTATION_180;
}

static void render_logo(void) {
    static const char PROGMEM raw_logo[] = {0x80, 0xc0, 0xc0, 0x40, 0x40, 0x40, 0xc0, 0xc0, 0xc0, 0x80, 0x00, 0x80, 0xc0, 0xc0, 0x40, 0x40, 0x40, 0x40, 0xc0, 0xc0, 0x80, 0x00, 0x80, 0xc0, 0x40, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x80, 0x00, 0x80, 0xc0, 0x40, 0xc0, 0xc0, 0xc0, 0xc0, 0x40, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0xff, 0xff, 0x00, 0xef, 0xef, 0xef, 0xf0, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xf8, 0xf7, 0xf7, 0x07, 0xf7, 0xf8, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
                                            0x01, 0x03, 0x02, 0x03, 0x73, 0x73, 0x63, 0x03, 0x73, 0x61, 0x30, 0x01, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x01, 0x00, 0x01, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x01, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x02, 0x03, 0x03, 0xff, 0xff, 0x00, 0xcf, 0xdf, 0x2f, 0xf0, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x01, 0xee, 0xee, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0x00, 0xff, 0xff, 0xf0, 0xef, 0xef, 0x0f, 0xef, 0xf0, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xee, 0xce, 0x31, 0xff, 0xff, 0x00, 0xff, 0xff, 0x01, 0xfe, 0xfe, 0xfe, 0xfe, 0x01, 0xff, 0xff, 0x00, 0xff, 0xff, 0x07, 0xb9, 0xbe, 0xbe, 0xb9, 0x07, 0xff, 0xff, 0x00, 0xff, 0xff, 0x01, 0xde, 0xde, 0x9e, 0x61, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfd, 0x03, 0xff, 0xff,
                                            0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x27, 0x21, 0x20, 0x27, 0x22, 0x27, 0x20, 0x27, 0x23, 0x23, 0x20, 0x27, 0x25, 0x27, 0x20, 0x27, 0x27, 0x24, 0x20, 0x27, 0x27, 0x24, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x23, 0x27, 0x24, 0x27, 0x27, 0x27, 0x24, 0x27, 0x27, 0x23, 0x20, 0x23, 0x27, 0x26, 0x25, 0x25, 0x25, 0x25, 0x25, 0x27, 0x23, 0x20, 0x23, 0x27, 0x27, 0x27, 0x27, 0x24, 0x27, 0x27, 0x27, 0x23, 0x20, 0x23, 0x27, 0x24, 0x25, 0x25, 0x25, 0x25, 0x26, 0x27, 0x23, 0x20, 0x23, 0x27, 0x26, 0x25, 0x25, 0x25, 0x25, 0x26, 0x27, 0x23, 0x20, 0x23, 0x27, 0x24, 0x27, 0x27, 0x27, 0x27, 0x24, 0x27, 0x03, 0x20, 0x03, 0x27, 0x04, 0x27, 0x07, 0x67, 0x34, 0x77, 0x07, 0x13, 0x70, 0x13, 0x07, 0x74, 0x55, 0x75, 0x05, 0x76, 0x37, 0x77, 0x03};
    oled_write_raw_P(raw_logo, sizeof(raw_logo));
}

void render_info(void) {
    oled_write_P(PSTR("Layer:"), false);

    switch (get_highest_layer(layer_state)) {
        case LAYER_1:
            oled_write_ln_P(PSTR("1 "), false);
            break;
        case LAYER_2:
            oled_write_ln_P(PSTR("2 "), false);
            break;
        case LAYER_3:
            oled_write_ln_P(PSTR("3 "), false);
            break;
        default:
            // Or use the write_ln shortcut over adding '\n' to the end of your string
            oled_write_ln_P(PSTR("? "), false);
    }

    // Host Keyboard Layer Status
    // Host Keyboard LED Status
    led_t led_state = host_keyboard_led_state();

    oled_write_P(led_state.num_lock ? PSTR("NUM ") : PSTR("[ ] "), false);
    oled_write_P(led_state.caps_lock ? PSTR("CAP ") : PSTR("[ ] "), false);
    oled_write_P(led_state.scroll_lock ? PSTR("SCR  ") : PSTR("[ ]  "), false);

    char     buffer[32];
    uint32_t uptime = timer_elapsed32(startup_timer);
    snprintf(buffer, sizeof(buffer), "Mode:%2d %d:%02d v%d.%d\n", g_rgb_matrix_mode, uptime / 60000, (uptime / 1000) % 60, (char)(DEVICE_VER >> 8), (char)DEVICE_VER);
    oled_write(buffer, false);

    snprintf(buffer, sizeof(buffer), "Key: 0x%04x %s", last_key, finished_timer ? "r" : "x");
    oled_write(buffer, false);
}

void oled_task_user(void) {

    if (!finished_timer && (timer_elapsed32(startup_timer) < 2000)) {
        oled_set_cursor(0, 4);
        render_logo();
    } else {
        if (!finished_timer) {
            oled_clear();
            finished_timer = true;
            print("Ready.");
        }
        /*if(timer_elapsed32(startup_timer) > 60000) {

        }*/
        oled_set_cursor(0, 4);
        render_logo();
        oled_set_cursor(0, 0);
        render_info();
    }
}

void keyboard_pre_init_user(void) {
}


void keyboard_post_init_user(void) {
    rgb_matrix_set_color_all(0, 4, 4);
    rgb_matrix_mode_noeeprom(g_rgb_matrix_mode);

    // Customise these values to desired behaviour
    debug_enable   = true;
    debug_matrix   = false;
    debug_keyboard = false;
    debug_mouse    = false;

    kdisp_init();
}

bool process_record_user(uint16_t keycode, keyrecord_t* record) {
    if (record->event.pressed) {
        last_key = keycode;
    }
    switch (keycode) {
        case KC_LAYER_1:
            if (record->event.pressed) {
                layer_move(LAYER_1);
                print("Activated Layer 1");
                rgb_matrix_step_noeeprom();
                g_rgb_matrix_mode = (g_rgb_matrix_mode + 1) % RGB_MATRIX_EFFECT_MAX;
            }
            return false;

        case KC_LAYER_2:
            if (record->event.pressed) {
                layer_move(LAYER_2);
                print("Activated Layer 2");
                rgb_matrix_step_noeeprom();
                g_rgb_matrix_mode = (g_rgb_matrix_mode + 1) % RGB_MATRIX_EFFECT_MAX;
            }
            return false;

        case KC_LAYER_3:
            if (record->event.pressed) {
                layer_move(LAYER_3);
                print("Activated Layer 3");
                rgb_matrix_step_noeeprom();
                g_rgb_matrix_mode = (g_rgb_matrix_mode + 1) % RGB_MATRIX_EFFECT_MAX;
            }
            return false;

        default:
            return true;
    }
};
