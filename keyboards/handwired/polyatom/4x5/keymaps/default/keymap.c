#include QMK_KEYBOARD_H
#include "print.h"
#include "base/disp_array.h"
#include "base/shift_reg.h"
#include "fonts/FreeSans12pt7b.h"

#include "quantum/quantum_keycodes.h"

enum kb_layers { _LAYER0 = 0, _LAYER1 = 1, _LAYER2 = 2, _LAYER3 = 3, NUM_LAYERS = 4 };

enum my_keycodes {
  KC_NEXT_LAYER = SAFE_RANGE,
  RGB_NEXT,
  RGB_PREV
};

static bool rotarySwitchPressed = false;

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_LAYER0] = LAYOUT( KC_NUM_LOCK,        KC_KP_SLASH,        KC_KP_ASTERISK,     KC_KP_MINUS,
                        KC_KP_7,            KC_KP_8,            KC_KP_9,            KC_KP_PLUS,
                        KC_KP_4,            KC_KP_5,            KC_KP_6,            KC_KP_EQUAL,
                        KC_KP_1,            KC_KP_2,            KC_KP_3,            KC_KP_ENTER,
                        KC_KP_0,            KC_KP_COMMA,        KC_KP_DOT,          KC_NEXT_LAYER
                        ),
    [_LAYER1] = LAYOUT( KC_A,               KC_B,               KC_C,               KC_D,
                        KC_E,               KC_F,               KC_G,               KC_H,
                        KC_I,               KC_J,               KC_K,               KC_L,
                        KC_M,               KC_N,               KC_O,               KC_P,
                        KC_Q,               KC_R,               KC_S,               KC_NEXT_LAYER
                        ),
    [_LAYER2] = LAYOUT( KC_T,               KC_U,               KC_V,               KC_W,
                        KC_X,               KC_Y,               KC_Z,               KC_0,
                        KC_1,               KC_2,               KC_3,               KC_SPACE,
                        KC_4,               KC_5,               KC_6,               KC_BACKSPACE,
                        KC_7,               KC_8,               KC_9,               KC_NEXT_LAYER
                        ),
    [_LAYER3] = LAYOUT( QK_BOOTLOADER,      QK_DEBUG_TOGGLE,    QK_CLEAR_EEPROM,    RGB_PREV,
                        KC_MS_ACCEL0,       KC_MS_ACCEL1,       KC_MS_ACCEL2,       RGB_TOG,
                        KC_MS_BTN1,         KC_MS_UP,           KC_MS_BTN2,         RGB_NEXT,
                        KC_MS_LEFT,         KC_MS_BTN3,         KC_MS_RIGHT,        KC_AUDIO_MUTE,
                        KC_AUDIO_VOL_DOWN,  KC_MS_DOWN,         KC_AUDIO_VOL_UP,    KC_NEXT_LAYER
                        )
};

led_config_t g_led_config = {{// Key Matrix to LED Index
                              {0, 1, 2, 3},
                              {4, 5, 6, 7},
                              {8, 9, 10, 11},
                              {12, 13, 14, 15},
                              {16, 17, 18, 19}
                             },
                             {
                                 // LED Index to Physical Position
                                 {100, 40}, {120, 40}, {140, 40}, {160, 40},
                                 {100, 60}, {120, 60}, {140, 60}, {160, 60},
                                 {100, 80}, {120, 80}, {140, 80}, {160, 80},
                                 {100, 100}, {120, 100}, {140, 100}, {160, 100},
                                 {100, 120}, {120, 120}, {140, 120}, {160, 120}
                             },
                             {
                                 // LED Index to Flag
                                 4, 4, 4, 4,
                                 4, 4, 4, 4,
                                 4, 4, 4, 4,
                                 4, 4, 4, 4,
                                 4, 4, 4, 4
                             }};

void process_layer_switch_user(uint16_t new_layer);

void matrix_init_user(void) {
    OLED_INIT;
}

void matrix_scan_user(void) {
    if (readPin(ENCODERS_SWITCH)) {
        if(rotarySwitchPressed) {
            register_code(KC_CAPS_LOCK);
            unregister_code(KC_CAPS_LOCK);
        }
        rotarySwitchPressed = false;
    } else if(!rotarySwitchPressed) {
        rotarySwitchPressed = true;
        //force_layer_switch();
        //update_performed();
    }
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (!clockwise) {
        prev_layer(NUM_LAYERS);
    } else {
        next_layer(NUM_LAYERS);
    }

    update_performed();
    return true;
}

struct disp_status {
    uint8_t bitmask[NUM_SHIFT_REGISTERS];
};

struct disp_status key_display[] = {
        {.bitmask = {~0x00, ~0x00, ~0x00, ~0x00, ~0x01}}, {.bitmask = {~0x00, ~0x00, ~0x00, ~0x00, ~0x02}}, {.bitmask = {~0x00, ~0x00, ~0x00, ~0x00, ~0x04}}, {.bitmask = {~0x00, ~0x00, ~0x00, ~0x00, ~0x08}},
        {.bitmask = {~0x00, ~0x00, ~0x00, ~0x01, ~0x00}}, {.bitmask = {~0x00, ~0x00, ~0x00, ~0x02, ~0x00}}, {.bitmask = {~0x00, ~0x00, ~0x00, ~0x04, ~0x00}}, {.bitmask = {~0x00, ~0x00, ~0x00, ~0x08, ~0x00}},
        {.bitmask = {~0x00, ~0x00, ~0x01, ~0x00, ~0x00}}, {.bitmask = {~0x00, ~0x00, ~0x02, ~0x00, ~0x00}}, {.bitmask = {~0x00, ~0x00, ~0x04, ~0x00, ~0x00}}, {.bitmask = {~0x00, ~0x00, ~0x08, ~0x00, ~0x00}},
        {.bitmask = {~0x00, ~0x01, ~0x00, ~0x00, ~0x00}}, {.bitmask = {~0x00, ~0x02, ~0x00, ~0x00, ~0x00}}, {.bitmask = {~0x00, ~0x04, ~0x00, ~0x00, ~0x00}}, {.bitmask = {~0x00, ~0x08, ~0x00, ~0x00, ~0x00}},
        {.bitmask = {~0x01, ~0x00, ~0x00, ~0x00, ~0x00}}, {.bitmask = {~0x02, ~0x00, ~0x00, ~0x00, ~0x00}}, {.bitmask = {~0x04, ~0x00, ~0x00, ~0x00, ~0x00}}, {.bitmask = {~0x08, ~0x00, ~0x00, ~0x00, ~0x00}}
    };

const char* keycode_to_disp_text(uint16_t keycode, led_t state) {
    bool shift = ((get_mods() & MOD_MASK_SHIFT) == MOD_MASK_SHIFT) || state.caps_lock;
    switch (keycode) {
        case KC_0:
            return shift ? ")" : "0";
        case KC_1:
            return shift ? "!" : "1";
        case KC_2:
            return shift ? "@" : "2";
        case KC_3:
            return shift ? "#" : "3";
        case KC_4:
            return shift ? "$" : "4";
        case KC_5:
            return shift ? "%" : "5";
        case KC_6:
            return shift ? "^" : "6";
        case KC_7:
            return shift ? "&" : "7";
        case KC_8:
            return shift ? "*" : "8";
        case KC_9:
            return shift ? "(" : "9";
        case KC_EQUAL:
            return shift ? "+" : "=";
        case KC_MINUS:
            return shift ? "-" : "_";
        case KC_SLASH:
            return shift ? "/" : "?";
        case KC_TILDE:
            return shift ? "`" : "~";
        case KC_COMMA:
            return shift ? "<" : ",";
        case KC_DOT:
            return shift ? ">" : ",";
        case KC_LBRACKET:
            return shift ? "{" : "[";
        case KC_RBRACKET:
            return shift ? "}" : "]";
        case KC_SPACE:
            return "[    ]";
        case KC_DELETE:
            return "Del";
        case KC_BACKSPACE:
            return "<--";
        case KC_A:
            return shift ? "A" : "a";
        case KC_B:
            return shift ? "B" : "b";
        case KC_C:
            return shift ? "C" : "c";
        case KC_D:
            return shift ? "D" : "d";
        case KC_E:
            return shift ? "E" : "e";
        case KC_F:
            return shift ? "F" : "f";
        case KC_G:
            return shift ? "G" : "g";
        case KC_H:
            return shift ? "H" : "h";
        case KC_I:
            return shift ? "I" : "i";
        case KC_J:
            return shift ? "J" : "j";
        case KC_K:
            return shift ? "K" : "k";
        case KC_L:
            return shift ? "L" : "l";
        case KC_M:
            return shift ? "M" : "m";
        case KC_N:
            return shift ? "N" : "n";
        case KC_O:
            return shift ? "O" : "o";
        case KC_P:
            return shift ? "P" : "p";
        case KC_Q:
            return shift ? "Q" : "q";
        case KC_R:
            return shift ? "R" : "r";
        case KC_S:
            return shift ? "S" : "s";
        case KC_T:
            return shift ? "T" : "t";
        case KC_U:
            return shift ? "U" : "u";
        case KC_V:
            return shift ? "V" : "v";
        case KC_W:
            return shift ? "W" : "w";
        case KC_X:
            return shift ? "X" : "x";
        case KC_Y:
            return shift ? "Y" : "y";
        case KC_Z:
            return shift ? "Z" : "z";
        case KC_NUM_LOCK:
            return !state.num_lock ? " Num " : "[Num]";
        case KC_KP_SLASH:
            return "/";
        case KC_KP_ASTERISK:
            return "*";
        case KC_KP_MINUS:
            return "-";
        case KC_KP_7:
            return !state.num_lock ? "Home" : "7";
        case KC_KP_8:
         return !state.num_lock ? "^" : "8";
        case KC_KP_9:
         return !state.num_lock ? "PgUp" : "9";
        case KC_KP_PLUS:
            return "+";
        case KC_KP_4:
            return !state.num_lock ? "<" : "4";
        case KC_KP_5:
            return !state.num_lock ? "" : "5";
        case KC_KP_6:
            return !state.num_lock ? ">" : "6";
        case KC_KP_EQUAL:
            return "=";
        case KC_KP_1:
            return !state.num_lock ? "End" : "1";
        case KC_KP_2:
            return !state.num_lock ? "v" : "2";
        case KC_KP_3:
            return !state.num_lock ? "PgDn" : "3";
        case KC_CALCULATOR:
            return "Calc";
        case KC_KP_0:
            return !state.num_lock ? "Ins" : "0";
        case KC_KP_COMMA:
            return !state.num_lock ? "Del" : ",";
        case KC_KP_DOT:
            return !state.num_lock ? "Del" : ".";
        case KC_KP_ENTER:
            return "Enter";
        case QK_BOOTLOADER:
            return "BootL";
        case QK_DEBUG_TOGGLE:
            return "Debug";
        case QK_CLEAR_EEPROM:
            return "Clr EE";
        case RGB_PREV:
            return "Prev <";
        case RGB_TOG:
            return rgb_matrix_is_enabled() ? "[RGB]" : " RGB ";
        case RGB_NEXT:
            return "Next >";
        case KC_MEDIA_NEXT_TRACK:
            return ">>";
        case KC_MEDIA_PLAY_PAUSE:
            return "|| |>";
        case KC_MEDIA_PREV_TRACK:
            return "<<";
        case KC_MS_ACCEL0:
            return ">>";
        case KC_MS_ACCEL1:
            return ">>>";
        case KC_MS_ACCEL2:
            return ">>>>";
        case KC_MS_BTN1:
            return "LClick";
        case KC_MS_BTN2:
            return "RClick";
        case KC_MS_BTN3:
            return "MClick";
        case KC_MS_UP:
            return "^";
        case KC_MS_LEFT:
            return "<";
        case KC_MS_RIGHT:
            return ">";
        case KC_AUDIO_MUTE:
            return "Mute";
        case KC_AUDIO_VOL_DOWN:
            return "Vol )>.";
        case KC_AUDIO_VOL_UP:
            return "Vol .>)";
        case KC_NEXT_LAYER:
            return "Next L";
        default:
            break;
    }
    return "?";
}

void process_layer_switch_user(uint16_t new_layer) {
    layer_move(new_layer);

    led_t state = host_keyboard_led_state();

    // keypos_t key;
    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            //key.col           = c;
            //key.row           = r;
            uint16_t keycode  = keymaps[new_layer][r][c];
            uint8_t  disp_idx = LAYOUT_TO_INDEX(r, c);

            if (disp_idx != 255) {
                const char* text = keycode_to_disp_text(keycode, state);
                sr_shift_out_buffer_latch(key_display[disp_idx].bitmask, sizeof(key_display->bitmask));
                kdisp_set_buffer(0x00);
                kdisp_write_gfx_text(&FreeSans12pt7b, 28, 18, text);
                kdisp_send_buffer();
            }
        }
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t* record) {
    uint8_t disp_idx = LAYOUT_TO_INDEX(record->event.key.row, record->event.key.col);
    const uint8_t* bitmask = key_display[disp_idx].bitmask;
    sr_shift_out_buffer_latch(bitmask, sizeof(key_display->bitmask));
    if (record->event.pressed) {
        set_last_key(keycode);
        if (disp_idx != 255) {
            kdisp_invert(true);
        }
    } else {
        if (disp_idx != 255) {
            kdisp_invert(false);
        }
    }
    if(!record->event.pressed) {
        switch(keycode) {
            case KC_NUM_LOCK:
                force_layer_switch();
                break;
            case KC_NEXT_LAYER:
                next_layer(NUM_LAYERS);
                update_performed();
                break;
            case RGB_NEXT:
                rgb_matrix_step_noeeprom();
                break;
            case RGB_PREV:
                rgb_matrix_step_reverse_noeeprom();
                break;
        }
    }

    uprintf("KL: kc: 0x%04X, col: %u, row: %u, pressed: %b, time: %u, interrupt: %b, count: %u display: %d SR bitmask: 0x%02X%02X%02X%02X%02X\n",
        keycode, record->event.key.col, record->event.key.row, record->event.pressed,
        record->event.time, record->tap.interrupted, record->tap.count, disp_idx, bitmask[4], bitmask[3], bitmask[2], bitmask[1], bitmask[0]);

   update_performed();

   return true;
};

