#include QMK_KEYBOARD_H

#include "polykybd.h"
#include "split72/split72.h"
#include "split72/keymaps/default/uni.h"

#include "base/disp_array.h"
#include "base/helpers.h"
#include "base/spi_helper.h"
#include "base/shift_reg.h"
#include "base/text_helper.h"
#include "base/fonts/NotoSans_Regular_Base_11pt.h"
#include "base/fonts/NotoSans_Medium_Base_8pt.h"
#include "base/fonts/gfx_used_fonts.h"

#include "base/crc32.h"

#include "quantum/quantum_keycodes.h"
#include "quantum/keymap_extras/keymap_german.h"
#include "quantum/via.h"

#include "raw_hid.h"
#include "oled_driver.h"
#include "version.h"
#include "print.h"
#include "debug.h"



#include <transactions.h>

#include "../default/lang/lang_lut.h"

#define FULL_BRIGHT 50
#define MIN_BRIGHT 1
#define DISP_OFF 0
#define BRIGHT_STEP 1

#define EMJ(x) UM(PRIVATE_EMOJI_##x)

//6 sec
#define FADE_TRANSITION_TIME 6000
//1 min
#define FADE_OUT_TIME 60000
//20 min
//#define TURN_OFF_TIME 1200000
#define TURN_OFF_TIME 120000

/*[[[cog
import cog
import os
from textwrap import wrap
from openpyxl import load_workbook
wb = load_workbook(filename = os.path.join(os.path.abspath(os.path.dirname(cog.inFile)), "lang", "lang_lut.xlsx"))
sheet = wb['key_lut']

languages = []
lang_index = 0
lang_key = sheet["B1"].value
while lang_key:
    languages.append(lang_key)
    if lang_index==0:
        cog.outl(f"static enum lang_layer g_lang_init = {lang_key};")
    lang_index = lang_index + 1
    lang_key = sheet.cell(row = 1, column = 2 + lang_index*3).value
]]]*/
static enum lang_layer g_lang_init = LANG_EN;
//[[[end]]]

enum kb_layers {
    _BL = 0x00,
    _L0 = _BL,
    _L1,
    _L2,
    _L3,
    _L4,
    _L5,
    _FL0,
    _FL1,
    _NL,
    _UL,
    _SL,
    _LL,
    _ADDLANG1,
    _EMJ0,
    _EMJ1 };

enum my_keycodes {
    KC_LANG = QK_KB_0,
    KC_DMIN,
    KC_DMAX,
    KC_DDIM,
    KC_DBRI,
    KC_DHLF,
    KC_D1Q,
    KC_D3Q,
    KC_BASE,
    KC_L0,
    KC_L1,
    KC_L2,
    KC_L3,
    KC_L4,
    KC_L5,
    KC_DEADKEY,
    KC_TOGMODS,
    KC_TOGTEXT,
    KC_SAVE,
    KC_SCRENCAP,
    KC_LOCK,
    KC_3FS,
    /*[[[cog
      for lang in languages:
          cog.out(f"KC_{lang}, ")
    ]]]*/
    KC_LANG_EN, KC_LANG_DE, KC_LANG_FR, KC_LANG_ES, KC_LANG_PT, KC_LANG_IT, KC_LANG_TR, KC_LANG_KO, KC_LANG_JA, KC_LANG_AR, KC_LANG_GR,
    //[[[end]]]
    /*[[[cog
      for idx in range(10):
          cog.out(f"KC_LAT{idx}, ")
    ]]]*/
    KC_LAT0, KC_LAT1, KC_LAT2, KC_LAT3, KC_LAT4, KC_LAT5, KC_LAT6, KC_LAT7, KC_LAT8, KC_LAT9,
    //[[[end]]]
    //Lables, no functionality:
    LBL_TEXT
};

struct display_info {
    uint8_t bitmask[NUM_SHIFT_REGISTERS];
};

#define BITMASK1(x) .bitmask = {~0, ~0, ~0, ~0, (uint8_t)(~(1<<x))}
#define BITMASK2(x) .bitmask = {~0, ~0, ~0, (uint8_t)(~(1<<x)), ~0}
#define BITMASK3(x) .bitmask = {~0, ~0, (uint8_t)(~(1<<x)), ~0, ~0}
#define BITMASK4(x) .bitmask = {~0, (uint8_t)(~(1<<x)), ~0, ~0, ~0}
#define BITMASK5(x) .bitmask = {(uint8_t)(~(1<<x)), ~0, ~0, ~0, ~0}

struct display_info key_display[] = {
        {BITMASK1(0)}, {BITMASK1(1)}, {BITMASK1(2)}, {BITMASK1(3)}, {BITMASK1(4)}, {BITMASK1(5)}, {BITMASK1(6)}, {BITMASK1(7)},
        {BITMASK2(0)}, {BITMASK2(1)}, {BITMASK2(2)}, {BITMASK2(3)}, {BITMASK2(4)}, {BITMASK2(5)}, {BITMASK2(6)}, {BITMASK2(7)},
        {BITMASK3(0)}, {BITMASK3(1)}, {BITMASK3(2)}, {BITMASK3(3)}, {BITMASK3(4)}, {BITMASK3(5)}, {BITMASK3(6)}, {BITMASK3(7)},
        {BITMASK4(0)}, {BITMASK4(1)}, {BITMASK4(2)}, {BITMASK4(3)}, {BITMASK4(4)}, {BITMASK4(5)}, {BITMASK4(6)}, {BITMASK4(7)},
        {BITMASK5(0)}, {BITMASK5(1)}, {BITMASK5(2)}, {BITMASK5(3)}, {BITMASK5(4)}, {BITMASK5(5)}, {BITMASK5(6)}, {BITMASK5(7)}
};

struct display_info disp_row_0 = { BITMASK1(0) };
struct display_info disp_row_3 = { BITMASK4(0) };

enum refresh_mode { START_FIRST_HALF, START_SECOND_HALF, DONE_ALL, ALL_AT_ONCE };
static enum refresh_mode g_refresh = DONE_ALL;

enum com_state { NOT_INITIALIZED, USB_HOST, BRIDGE };
static enum com_state com = NOT_INITIALIZED;

bool is_usb_host_side(void) {
    return com == USB_HOST;
}

enum side_state { UNDECIDED, LEFT_SIDE, RIGHT_SIDE };
static enum side_state side = UNDECIDED;

bool is_left_side(void) {
    return side == LEFT_SIDE;
}

bool is_right_side(void) {
    return side == RIGHT_SIDE;
}
typedef struct _poly_eeconf_t {
    uint8_t lang;
    uint8_t brightness;
    uint16_t unused;
    uint8_t latin_ex[26];
} poly_eeconf_t;
static uint8_t latin_ex[26];

_Static_assert(sizeof(poly_eeconf_t) == EECONFIG_USER_DATA_SIZE, "Mismatch in keyboard EECONFIG stored data");

enum flags {
    STATUS_DISP_ON      = 1 << 0,
    IDLE_TRANSITION     = 1 << 1,
    DISP_IDLE           = 1 << 2,
    DEAD_KEY_ON_WAKEUP  = 1 << 3,
    DBG_ON              = 1 << 4,
    MODS_AS_TEXT        = 1 << 5,
    MORE_TEXT           = 1 << 6
    };

typedef struct _poly_sync_t {
    uint8_t lang;
    uint8_t contrast;
    uint8_t flags;
    layer_state_t default_ls;
    uint16_t last_latin_kc;
    uint32_t crc32;
} poly_sync_t;

#define SYNC_ACK 0b11001010
typedef struct _poly_sync_reply_t {
    uint8_t ack;
} poly_sync_reply_t;

typedef struct _poly_state_t {
    led_t led_state;
    uint8_t mod_state;
    layer_state_t layer_state;
    poly_sync_t s;
} poly_state_t;

static poly_state_t g_state;
static poly_sync_t g_local;

static int32_t last_update = 0;

bool display_wakeup(keyrecord_t* record);
void update_displays(enum refresh_mode mode);
void set_displays(uint8_t contrast, bool idle);
void set_selected_displays(int8_t old_value, int8_t new_value);
void toggle_stagger(bool new_state);
void oled_update_buffer(void);
void poly_suspend(void);

uint16_t fletcher16(const uint8_t *data, int count )
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;
   int index;

   for ( index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
}

void save_user_eeconf(void) {
    poly_eeconf_t ee;
    ee.lang = g_local.lang;
    ee.brightness = ~g_local.contrast;
    ee.unused = 0;
    memcpy(ee.latin_ex, latin_ex, sizeof(latin_ex));
    eeconfig_update_user_datablock(&ee);
}

poly_eeconf_t load_user_eeconf(void) {
    poly_eeconf_t ee;
    eeconfig_read_user_datablock(&ee);
    ee.brightness = ~ee.brightness;
    if(ee.brightness>FULL_BRIGHT) {
        ee.brightness = FULL_BRIGHT;
    }
    return ee;
}

void inc_brightness(void) {
    if (g_local.contrast < FULL_BRIGHT) {
        g_local.contrast += BRIGHT_STEP;
    }
    if (g_local.contrast > FULL_BRIGHT) {
        g_local.contrast = FULL_BRIGHT;
    }

    save_user_eeconf();
}

void dec_brightness(void) {
    if (g_local.contrast > (MIN_BRIGHT+BRIGHT_STEP)) {
        g_local.contrast -= BRIGHT_STEP;
    } else {
        g_local.contrast = MIN_BRIGHT;
    }

    save_user_eeconf();
}

void select_all_displays(void) {
    // make sure we are talking to all shift registers
    sr_shift_out_0_latch(NUM_SHIFT_REGISTERS);
}

void clear_all_displays(void) {
    select_all_displays();

    kdisp_set_buffer(0x00);
    kdisp_send_buffer();
}

void early_hardware_init_post(void) {
    spi_hw_setup();
}

void update_performed(void) {
    last_update = timer_read32();
}

layer_state_t persistent_default_layer_get(void) {
    return (layer_state_t)eeconfig_read_default_layer();
}

void persistent_default_layer_set(uint16_t default_layer) {
  eeconfig_update_default_layer(default_layer);
  default_layer_set(default_layer);
}

void request_disp_refresh(void) {
    g_refresh = ALL_AT_ONCE;
    //use the following for partial update (during housekeeping)
    // g_refresh = START_FIRST_HALF;
}

void user_sync_poly_data_handler(uint8_t in_len, const void* in_data, uint8_t out_len, void* out_data) {
    if (in_len == sizeof(poly_sync_t) && in_data != NULL && out_len == sizeof(poly_sync_reply_t) && out_data!= NULL) {
        uint32_t crc32 = crc32_1byte(in_data, sizeof(g_local)-sizeof(g_local.crc32), 0);
        if(crc32 == ((const poly_sync_t *)in_data)->crc32) {
            memcpy(&g_local, in_data, sizeof(poly_sync_t));
            ((poly_sync_reply_t*)out_data)->ack = SYNC_ACK;
        }
    } else if(in_len == sizeof(latin_ex)) {
        memcpy(latin_ex, in_data, sizeof(latin_ex)); //no check sum for now
        save_user_eeconf();
        request_disp_refresh();
    }
}

void oled_on_off(bool on);

void sync_and_refresh_displays(void) {
    bool sync_success = true;

    if (is_usb_host_side()) {
        if(debug_enable && (g_local.flags & DBG_ON) == 0) {
            g_local.flags |= DBG_ON;

            printf("DEBUG: enable=%u, keyboard=%u, matrix=%u\n", debug_enable, debug_keyboard, debug_matrix);
            print(QMK_KEYBOARD "/" QMK_KEYMAP " @ " QMK_VERSION ", Built on: " QMK_BUILDDATE "\n");
        } else if(!debug_enable && (g_local.flags & DBG_ON) != 0) {
            g_local.flags &= ~((uint8_t)DBG_ON);

            printf("DEBUG: enable=%u, keyboard=%u, matrix=%u\n", debug_enable, debug_keyboard, debug_matrix);
            print(QMK_KEYBOARD "/" QMK_KEYMAP " @ " QMK_VERSION ", Built on: " QMK_BUILDDATE "\n");
        }
        const bool back_from_idle_transition = (g_local.flags & IDLE_TRANSITION) == 0 && (g_state.s.flags & IDLE_TRANSITION) != 0;
        if (back_from_idle_transition) {
            poly_eeconf_t ee   = load_user_eeconf();
            g_local.contrast = ee.brightness;
            //request_disp_refresh();
        }

        //master syncs data
        if ( memcmp(&g_local, &g_state.s, sizeof(poly_sync_t))!=0 ) {
            poly_sync_reply_t reply;
            g_local.crc32 = crc32_1byte((const void *)&g_local, sizeof(g_local)-sizeof(g_local.crc32), 0);
            uint8_t retry = 0;
            for(; retry<10; ++retry) {
                sync_success = transaction_rpc_exec(USER_SYNC_POLY_DATA, sizeof(poly_sync_t), &g_local, sizeof(poly_sync_reply_t), &reply);
                if(sync_success && reply.ack == SYNC_ACK) {
                    break;
                }
                sync_success = false;
                printf("Bridge sync retry %d (success: %d, ack: %d)\n", retry, sync_success, reply.ack == SYNC_ACK);
            }
            if(retry>0) {
                if(sync_success)
                    printf("Success on retry %d (success: %d, ack: %d)\n", retry, sync_success, reply.ack == SYNC_ACK);
                else
                    printf("Failed sync!\n");
            }
        }
    }
    if(sync_success) {
        const bool status_disp_turned_off    = (g_local.flags & STATUS_DISP_ON) == 0 && (g_state.s.flags & DISP_IDLE) != 0;
        const bool idle_changed              = (g_local.flags & DISP_IDLE) != (g_state.s.flags & DISP_IDLE);
        const bool in_idle_mode              = (g_local.flags & DISP_IDLE) != 0;
        const bool displays_turned_on        = (g_state.s.contrast <= DISP_OFF && g_local.contrast > DISP_OFF);
        const bool dbg_changed               = (g_local.flags & DBG_ON) != (g_state.s.flags & DBG_ON);
        if(idle_changed) {
            if(in_idle_mode) {
                oled_set_brightness(0);
            } else {
                oled_set_brightness(OLED_BRIGHTNESS);
            }
        }

        const led_t led_state = host_keyboard_led_state();
        const uint8_t mod_state = get_mods();

        if (led_state.raw != g_state.led_state.raw ||
            mod_state != g_state.mod_state ||
            layer_state != g_state.layer_state ||
            g_state.s.lang != g_local.lang ||
            g_state.s.default_ls != g_local.default_ls ||
            g_state.s.last_latin_kc != g_local.last_latin_kc) {
            //memcmp(&g_local, &g_state.s, sizeof(poly_sync_t))!=0 //no!

            g_state.led_state = led_state;
            g_state.mod_state = mod_state;
            g_state.layer_state = layer_state;

            request_disp_refresh();
        }

        //split the update cycle
        if (g_refresh == START_FIRST_HALF) {
            update_displays(START_FIRST_HALF);
            g_refresh = START_SECOND_HALF;
        }
        else if (g_refresh == START_SECOND_HALF || g_refresh == ALL_AT_ONCE) {
            update_displays(g_refresh);
            g_refresh = DONE_ALL;
        } else if (g_state.s.contrast != g_local.contrast || idle_changed) {
            set_displays(g_local.contrast, in_idle_mode);
        }

        // sync rgb matrix
        if (status_disp_turned_off) {
            rgb_matrix_disable_noeeprom();
        } else if(displays_turned_on) {
            rgb_matrix_reload_from_eeprom();
        }

        memcpy(&g_state.s, &g_local, sizeof(g_local));

        if(dbg_changed) {
            request_disp_refresh();
        }
    }
}

void housekeeping_task_user(void) {
    sync_and_refresh_displays();

    if(last_update>=0) {
        //turn off displays
        uint32_t elapsed_time_since_update = timer_elapsed32(last_update);

        if (is_usb_host_side()) {
            g_local.flags |= STATUS_DISP_ON;
            g_local.flags &= ~((uint8_t)IDLE_TRANSITION);

            if(elapsed_time_since_update > FADE_OUT_TIME && g_local.contrast >= MIN_BRIGHT && (g_local.flags & DISP_IDLE)==0) {
                poly_eeconf_t ee = load_user_eeconf();
                int32_t time_after = elapsed_time_since_update - FADE_OUT_TIME;
                int16_t brightness = ((FADE_TRANSITION_TIME - time_after) * ee.brightness) / FADE_TRANSITION_TIME;

                //transition to pulsing mode
                if(brightness<=MIN_BRIGHT) {
                    g_local.contrast = DISP_OFF;
                    g_local.flags |= DISP_IDLE;
                } else if(brightness>FULL_BRIGHT) {
                    g_local.contrast = FULL_BRIGHT;
                } else{
                    g_local.contrast = brightness;
                }
                g_local.flags |= IDLE_TRANSITION;
            } else if(elapsed_time_since_update > TURN_OFF_TIME) {
                poly_suspend();
            } else if((g_local.flags & DISP_IDLE)!=0) {
                int32_t time_after = PK_MAX(elapsed_time_since_update - FADE_OUT_TIME - FADE_TRANSITION_TIME, 0)/300;
                g_local.contrast = time_after%50;
            } else {
                g_local.flags &= ~((uint8_t)DISP_IDLE);
            }
        }
    }
}

uint16_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    //Base Layers
/*
                                                              ┌────────────────┐
                                                              │     QWERTY     │
                                                              └────────────────┘
   ┌────────┬───────┬───────┬───────┬───────┬───────┬───────┐                    ┌───────┬───────┬───────┬───────┬───────┬───────┬────────┐
   │  Esc   │   1   │   2   │   3   │   4   │   5   │  Nubs │ ╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮ │   6   │   7   │   8   │   9   │   0   │   -   │ BckSp  │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤ │╰╯╰╯╰╯╰╯╰╯╰╯╰╯╰╯│ ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  TAB   │   q   │   w   │   e   │   r   │   t   │   `   ├─╯                ╰─┤  Hypr │   y   │   u   │   i   │   o   │   p   │   \    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤                    ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  FN    │   a   │   s   │   d   │   f   │   g   │   '   │  (MB1)             │  Intl │   h   │   j   │   k   │   l   │   =   │  Ret   │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────╮  ╭────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │ Shift  │   z   │   x   │   c   │   v   │   b   │  Nuhs │  Num!  │  │   [    │   ]   │   n   │   m   │   ,   │   ;   │  Up   │ Shift  │
   └┬───────┼───────┼───────┼───────┼──────┬┴───────┼───────┼────────┤  ├────────┼───────┼───────┴┬──────┼───────┼───────┼───────┼───────┬┘
    │ Ctrl  │  Os   │  Alt  │  Intl │      │  Space │  Del  │   Ret  │  │  Lang  │   /   │ Space  │      │   .   │  Left │  Down │ Right │
    └───────┴───────┴───────┴───────┘      └────────┴───────┴────────╯  └────────┴───────┴────────┘      └───────┴───────┴───────┴───────┘
*/
    [_L0] = LAYOUT_left_right_stacked(
        KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_NUBS,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_GRAVE,
        MO(_FL0),   KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_QUOTE,   KC_MS_BTN1,
        KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       TO(_EMJ0),   MO(_NL),
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_APP,                 KC_SPACE,   KC_DEL,     KC_ENTER,

                    KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINUS,   KC_BSPC,
                    KC_HYPR,    KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_BSLS,
        KC_NO,      MO(_ADDLANG1),KC_H,     KC_J,       KC_K,       KC_L,       KC_EQUAL,   KC_ENTER,
        KC_LBRC,    KC_RBRC,    KC_N,       KC_M,       KC_COMMA,   KC_SCLN,    KC_UP,      KC_RSFT,
        KC_LANG,    KC_SLASH,    KC_SPC,                KC_DOT,     KC_LEFT,    KC_DOWN,    KC_RIGHT
        ),

/*
                                                              ┌────────────────┐
                                                              │     QWERTY!    │
                                                              └────────────────┘
   ┌────────┬───────┬───────┬───────┬───────┬───────┬───────┐                    ┌───────┬───────┬───────┬───────┬───────┬───────┬────────┐
   │  Esc   │   1   │   2   │   3   │   4   │   5   │   6   │ ╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮ │   7   │   8   │   9   │   0   │   -   │   =   │  Hypr  │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤ │╰╯╰╯╰╯╰╯╰╯╰╯╰╯╰╯│ ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  TAB   │   q   │   w   │   e   │   r   │   t   │   `   ├─╯                ╰─┤   y   │   u   │   i   │   o   │   p   │   [   │  Nubs  │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤                    ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  FN    │   a   │   s   │   d   │   f   │   g   │   '   │  (MB1)             │   h   │   j   │   k   │   l   │   ;   │   ]   │    \   │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────╮  ╭────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │ Shift  │ Nuhs  │   z   │   x   │   c   │   v   │   b   │  Num!  │  │  Lang  │  Ctx  │   n   │   m   │   ,   │   .   │   /   │ Shift  │
   └┬───────┼───────┼───────┼───────┼──────┬┴───────┼───────┼────────┤  ├────────┼───────┼───────┴┬──────┼───────┼───────┼───────┼───────┬┘
    │ Ctrl  │  Os   │  Alt  │  Intl │      │  Space │  Del  │   Ins  │  │  Ret   │ BckSp │ Space  │      │ Left  │   Up  │  Down │ Right │
    └───────┴───────┴───────┴───────┘      └────────┴───────┴────────╯  └────────┴───────┴────────┘      └───────┴───────┴───────┴───────┘
*/

    [_L1] = LAYOUT_left_right_stacked(
        KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,
        KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_GRAVE,
        MO(_FL1),   KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_QUOTE,   KC_MS_BTN1,
        KC_LSFT,    TO(_EMJ0),   KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       MO(_NL),
        KC_LCTL,    KC_LWIN,    KC_LALT,    MO(_ADDLANG1),          KC_SPACE,   KC_DEL,     KC_INS,

                    KC_7,       KC_8,       KC_9,       KC_0,       KC_MINUS,   KC_EQUAL,   KC_HYPR,
                    KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_NUBS,
        KC_NO,      KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_RBRC,    KC_BSLS,
        KC_LANG,    KC_APP,     KC_N,       KC_M,       KC_COMMA,   KC_DOT,     KC_SLASH,   KC_RSFT,
        KC_ENTER,   KC_BSPC,    KC_SPC,                 KC_LEFT,    KC_UP,      KC_DOWN,    KC_RIGHT
        ),
/*
                                                              ┌────────────────┐
                                                              │   Colemak DH   │
                                                              └────────────────┘
   ┌────────┬───────┬───────┬───────┬───────┬───────┬───────┐                    ┌───────┬───────┬───────┬───────┬───────┬───────┬────────┐
   │  Esc   │   1   │   2   │   3   │   4   │   5   │  Nub  │ ╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮ │   6   │   7   │   8   │   9   │   0   │   -   │   =    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤ │╰╯╰╯╰╯╰╯╰╯╰╯╰╯╰╯│ ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  TAB   │   q   │   w   │   f   │   p   │   b   │   `   ├─╯                ╰─┤   j   │   l   │   u   │   y   │   ;   │   [   │  Intl  │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤                    ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  FN    │   a   │   r   │   s   │   t   │   g   │   '   │  (MB1)             │   m   │   n   │   e   │   i   │   o   │   ]   │    \   │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────╮  ╭────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │ Shift  │   z   │   x   │   c   │   d   │   v   │  Nuhs |  Num!  │  │  Lang  │  Hypr │   k   │   h   │   ,   │   .   │   /   │ Shift  │
   └┬───────┼───────┼───────┼───────┼──────┬┴───────┼───────┼────────┤  ├────────┼───────┼───────┴┬──────┼───────┼───────┼───────┼───────┬┘
    │ Ctrl  │  Os   │  Alt  │  Ctx  │      │  Space │  Del  │   Ret  │  │  Ret   │ BckSp │ Space  │      │ Left  │   Up  │  Down │ Right │
    └───────┴───────┴───────┴───────┘      └────────┴───────┴────────╯  └────────┴───────┴────────┘      └───────┴───────┴───────┴───────┘
*/
    [_L2] = LAYOUT_left_right_stacked(
        KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_NUBS,
        KC_TAB,     KC_Q,       KC_W,       KC_F,       KC_P,       KC_B,       KC_GRAVE,
        MO(_FL1),   KC_A,       KC_R,       KC_S,       KC_T,       KC_G,       KC_QUOTE,   KC_MS_BTN1,
        KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_D,       KC_V,       TO(_EMJ0),    MO(_NL),
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_APP,                 KC_SPACE,   KC_DEL,     KC_ENTER,

                    KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINUS,   KC_EQUAL,
                    KC_J,       KC_L,       KC_U,       KC_Y,       KC_SCLN,    KC_LBRC,    MO(_ADDLANG1),
        KC_NO,      KC_M,       KC_N,       KC_E,       KC_I,       KC_O,       KC_RBRC,    KC_BSLS,
        KC_LANG,    KC_HYPR,    KC_K,       KC_H,       KC_COMMA,   KC_DOT,     KC_SLASH,   KC_RSFT,
        KC_ENTER,   KC_BSPC,    KC_SPC,                 KC_LEFT,    KC_UP,      KC_DOWN,    KC_RIGHT
        ),
        /*
                                                              ┌────────────────┐
                                                              │       Neo      │
                                                              └────────────────┘
   ┌────────┬───────┬───────┬───────┬───────┬───────┬───────┐                    ┌───────┬───────┬───────┬───────┬───────┬───────┬────────┐
   │  Esc   │   1   │   2   │   3   │   4   │   5   │   <   │ ╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮ │   6   │   7   │   8   │   9   │   0   │   -   │   `    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤ │╰╯╰╯╰╯╰╯╰╯╰╯╰╯╰╯│ ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  TAB   │   x   │   v   │   l   │   c   │   w   │   ^   ├─╯                ╰─┤   k   │   h   │   g   │   f   │   q   │   ß   │   ´    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤                    ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  FN    │   u   │   i   │   a   │   e   │   o   │   '   │  (MB1)             │   s   │   n   │   r   │   t   │   d   │   y   │   \    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────╮  ╭────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │ Shift  │   #   │   ü   │   ö   │   ä   │   p   │   z   │  Num!  │  │  Lang  │   +   │   b   │   m   │   ,   │   .   │   j   │ Shift  │
   └┬───────┼───────┼───────┼───────┼──────┬┴───────┼───────┼────────┤  ├────────┼───────┼───────┴┬──────┼───────┼───────┼───────┼───────┬┘
    │ Ctrl  │  Os   │  Alt  │  Ctx  │      │  Space │  Del  │   Ret  │  │  Ret   │ BckSp │ Space  │      │ Left  │   Up  │  Down │ Right │
    └───────┴───────┴───────┴───────┘      └────────┴───────┴────────╯  └────────┴───────┴────────┘      └───────┴───────┴───────┴───────┘
*/
    [_L3] = LAYOUT_left_right_stacked(
        KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       DE_LABK,
        KC_TAB,     KC_X,       KC_V,       KC_L,       KC_C,       KC_W,       DE_CIRC,
        MO(_FL0),   KC_U,       KC_I,       KC_A,       KC_E,       KC_O,       KC_QUOTE,   KC_MS_BTN1,
        KC_LSFT,    DE_HASH,    DE_UDIA,    DE_ODIA,    DE_ADIA,    KC_P,       DE_Z,       MO(_NL),
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_APP,                 KC_SPACE,   KC_DEL,     KC_ENTER,

                    KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       DE_MINS,    DE_GRV,
                    KC_K,       KC_H,       KC_G,       KC_F,       KC_Q,       DE_SS,      DE_ACUT,
        KC_NO,      KC_S,       KC_N,       KC_R,       KC_T,       KC_D,       DE_Y,       KC_BSLS,
        KC_LANG,    DE_PLUS,    KC_B,       KC_M,       KC_COMMA,   KC_DOT,     KC_J,       KC_RSFT,
        KC_ENTER,   KC_BSPC,    KC_SPC,                 KC_LEFT,    KC_UP,      KC_DOWN,    KC_RIGHT
        ),
        /*
                                                              ┌────────────────┐
                                                              │    Workman     │
                                                              └────────────────┘
   ┌────────┬───────┬───────┬───────┬───────┬───────┬───────┐                    ┌───────┬───────┬───────┬───────┬───────┬───────┬────────┐
   │  Esc   │   1   │   2   │   3   │   4   │   5   │   `   │ ╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮ │   6   │   7   │   8   │   9   │   0   │   -   │   =    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤ │╰╯╰╯╰╯╰╯╰╯╰╯╰╯╰╯│ ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  TAB   │   q   │   d   │   r   │   w   │   b   │  Hypr ├─╯                ╰─┤   j   │   f   │   u   │   p   │   ;   │   [   │   ]    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤                    ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  FN    │   a   │   s   │   h   │   t   │   g   │  Meh  │  (MB1)             │   y   │   n   │   e   │   o   │   i   │   '   │   \    │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────╮  ╭────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │ Shift  │   z   │   x   │   m   │   c   │   v   │  Intl │  Num!  │  │  Lang  │   k   │   b   │   l   │   ,   │   .   │   /   │ Shift  │
   └┬───────┼───────┼───────┼───────┼──────┬┴───────┼───────┼────────┤  ├────────┼───────┼───────┴┬──────┼───────┼───────┼───────┼───────┬┘
    │ Ctrl  │  Os   │  Alt  │  Ctx  │      │  Space │  Del  │   Ret  │  │  Ret   │ BckSp │ Space  │      │ Left  │   Up  │  Down │ Right │
    └───────┴───────┴───────┴───────┘      └────────┴───────┴────────╯  └────────┴───────┴────────┘      └───────┴───────┴───────┴───────┘
*/
    [_L4] = LAYOUT_left_right_stacked(
        KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_GRAVE,
        KC_TAB,     KC_Q,       KC_D,       KC_R,       KC_W,       KC_B,       KC_HYPR,
        MO(_FL1),   KC_A,       KC_S,       KC_H,       KC_T,       KC_G,       TO(_EMJ0),     KC_MS_BTN1,
        KC_LSFT,    KC_Z,       KC_X,       KC_M,       KC_C,       KC_V,       MO(_ADDLANG1), MO(_NL),
        KC_LCTL,    KC_LWIN,    KC_LALT,    KC_APP,                 KC_SPACE,   KC_DEL,     KC_ENTER,

                    KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINUS,   KC_EQUAL,
                    KC_J,       KC_F,       KC_U,       KC_P,       KC_SCLN,    KC_LBRC,    KC_RBRC,
        KC_NO,      KC_Y,       KC_N,       KC_E,       KC_O,       KC_I,       KC_QUOTE,   KC_BSLS,
        KC_LANG,    KC_K,       KC_B,       KC_L,       KC_COMMA,   KC_DOT,     KC_SLASH,   KC_RSFT,
        KC_ENTER,   KC_BSPC,    KC_SPC,                 KC_LEFT,    KC_UP,      KC_DOWN,    KC_RIGHT
        ),
        /*
                                                              ┌────────────────┐
                                                              │    Chad3814    │
                                                              └────────────────┘
   ┌────────┬───────┬───────┬───────┬───────┬───────┬───────┐                    ┌───────┬───────┬───────┬───────┬───────┬───────┬────────┐
   │  `~    │   1   │   2   │   3   │   4   │   5   │  Save │ ╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮╭╮ │  3FS  │   6   │   7   │   8   │   9   │   0   │   -_   │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤ │╰╯╰╯╰╯╰╯╰╯╰╯╰╯╰╯│ ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  Tab   │   q   │   w   │   e   │   r   │   t   │  Esc  ├─╯                ╰─┤  Lock │   y   │   u   │   i   │   o   │   p   │   \|   │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┤                    ├───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │  FN    │   a   │   s   │   d   │   f   │   g   │   [   │  (MB1)             │   ]   │   h   │   j   │   k   │   l   │   ;:  │   '"   │
   ├────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────╮  ╭────────┼───────┼───────┼───────┼───────┼───────┼───────┼────────┤
   │ Shift  │   z   │   x   │   c   │   v   │   b   │   F8  │ ScrCap │  │  PgUp  │ Intl  │   n   │   m   │   ,<  │   .>  │  /?   │   =+   │
   └┬───────┼───────┼───────┼───────┼──────┬┴───────┼───────┼────────┤  ├────────┼───────┼───────┴┬──────┼───────┼───────┼───────┼───────┬┘
    │ Ctrl  │  Os   │  Alt  │ Left  │      │  Right │ Space │  Num!  │  │  PgDn  │ BckSp │  Enter │      │   Up  │ Down  │ RCtrl │ BackSp│
    └───────┴───────┴───────┴───────┘      └────────┴───────┴────────╯  └────────┴───────┴────────┘      └───────┴───────┴───────┴───────┘
*/
    [_L5] = LAYOUT_left_right_stacked(
        KC_GRAVE,   KC_1,          KC_2,       KC_3,       KC_4,       KC_5,       KC_SAVE,
        KC_TAB,     KC_Q,          KC_W,       KC_E,       KC_R,       KC_T,       KC_ESC,
        MO(_FL0),   KC_A,          KC_S,       KC_D,       KC_F,       KC_G,       KC_LBRC,  KC_MS_BTN1,
        KC_LSFT,    KC_Z,          KC_X,       KC_C,       KC_V,       KC_B,       KC_F8,    KC_SCRENCAP,
        KC_LCTL,    KC_LWIN,       KC_LALT,    KC_LEFT,                KC_RIGHT,   KC_SPACE, MO(_NL),

                    KC_3FS,        KC_6,       KC_7,       KC_8,       KC_9,       KC_0,     KC_MINUS,
                    KC_LOCK,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,     KC_BSLS,
        KC_NO,      KC_RBRC,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,  KC_QUOTE,
        KC_PGUP,    MO(_ADDLANG1), KC_N,       KC_M,       KC_COMMA,   KC_DOT,     KC_SLASH, KC_EQUAL,
        KC_PGDN,    KC_BACKSPACE,  KC_ENTER,                KC_UP,     KC_DOWN,    KC_RCTL,  KC_BACKSPACE
        ),
    //Function Layer (Fn)
    [_FL0] = LAYOUT_left_right_stacked(
        OSL(_UL),   KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,     TO(_UL),
        _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        KC_CAPS,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    KC_BTN2,                _______,    _______,    _______,

                    KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,      KC_F11,    KC_F12,
                    KC_BTN3,    KC_BTN2,    _______,    _______,    _______,    _______,    TO(_SL),
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        TO(_NL),    KC_RALT,    _______,    _______,    _______,    _______,    _______,    _______,
        KC_BTN1,    KC_RWIN,    KC_RCTL,                KC_HOME,    KC_PGUP,    KC_PGDN,    KC_END
        ),
    [_FL1] = LAYOUT_left_right_stacked(
        OSL(_UL),   KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,             _______,    _______,    _______,

                    KC_F7,      KC_F8,      KC_F9,      KC_F10,      KC_F11,     KC_F12,    TO(_UL),
                    KC_BTN3,    KC_BTN2,    _______,    _______,    _______,    _______,    TO(_SL),
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_CAPS,
        TO(_NL),    KC_RALT,    _______,    _______,    _______,    _______,    _______,    _______,
        KC_BTN1,    KC_RWIN,    KC_RCTL,                KC_HOME,    KC_PGUP,    KC_PGDN,    KC_END
        ),
     //Num Layer
    [_NL] = LAYOUT_left_right_stacked(
        KC_NO,      KC_NUM,     KC_PSLS,    KC_PAST,    KC_PMNS,    KC_NO,      KC_NO,
        KC_BTN1,    KC_KP_7,    KC_KP_8,    KC_KP_9,    KC_PPLS,    KC_INS,     KC_NO,
        KC_NO,      KC_KP_4,    KC_KP_5,    KC_KP_6,    KC_PPLS,    KC_DEL,     KC_NO,     _______,
        KC_NO,      KC_KP_1,    KC_KP_2,    KC_KP_3,    KC_PENT,    KC_NO,      KC_NO,     _______,
        KC_BASE,    KC_KP_0,    KC_PDOT,    KC_PENT,                KC_MS_BTN2, KC_NO,     KC_NO,

                    KC_NO,      KC_NO,      KC_NUM,     KC_PSLS,    KC_PAST,    KC_PMNS,   KC_NO,
                    KC_NO,      KC_INS,     KC_KP_7,    KC_KP_8,    KC_KP_9,    KC_PPLS,   KC_NO,
        _______,    KC_NO,      KC_DEL,     KC_KP_4,    KC_KP_5,    KC_KP_6,    KC_PPLS,   KC_NO,
        _______,    _______,    KC_NO,      KC_KP_1,    KC_KP_2,    KC_KP_3,    KC_PENT,   KC_NO,
        _______,    KC_NO,      KC_NO,                  KC_KP_0,    KC_PDOT,    KC_PENT,   KC_BASE
        ),
    //Util Layer
    [_UL] = LAYOUT_left_right_stacked(
        KC_NO,      KC_F13,     KC_F14,     KC_F15,     KC_F16,     KC_F17,     KC_F18,
        KC_MYCM,    KC_CALC,    KC_PSCR,    KC_SCRL,    KC_BRK,     KC_NO,      KC_NO,
        KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      _______,
        KC_LSFT,    KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,
        KC_BASE,    KC_NO,      KC_NO,      KC_NO,                  KC_NO,      KC_NO,      KC_NO,

                    KC_F19,     KC_F20,     KC_F21,     KC_F22,     KC_F23,     KC_F24,     KC_NO,
                    KC_NO,      KC_MPRV,    KC_MPLY,    KC_MSTP,    KC_MNXT,    KC_NO,      TO(_SL),
        _______,    KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,
        KC_NO,      KC_NO,      KC_MUTE,    KC_VOLD,    KC_VOLU,    KC_NO,      KC_NO,      KC_RSFT,
        KC_NO,      KC_NO,      KC_NO,                  KC_NO,      KC_NO,      KC_NO,      KC_BASE
        ),
    //Settings Layer
    [_SL] = LAYOUT_left_right_stacked(
        KC_DDIM,    KC_DMIN,    KC_D1Q,     KC_DHLF,    KC_D3Q,     KC_DMAX,    KC_DBRI,
        KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,
        KC_NO,      KC_L0,      KC_L1,      KC_L2,      KC_L3,      KC_L4,      KC_L5,      _______,
        KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      QK_RBT,
        KC_BASE,    LBL_TEXT,   KC_TOGMODS, KC_TOGTEXT,             KC_NO,      QK_MAKE,    QK_BOOT,


                    RGB_RMOD,   RGB_M_SW,   RGB_M_R,    RGB_TOG,    RGB_M_P,    RGB_M_B,    RGB_MOD,
                    KC_NO,      RGB_SPD,    RGB_SPI,    KC_NO,      RGB_HUD,    RGB_HUI,    KC_NO,
        _______,    KC_NO,      RGB_VAD,    RGB_VAI,    KC_NO,      RGB_SAD,    RGB_SAI,    KC_NO,
        EE_CLR,     KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,
        DB_TOGG,    KC_DEADKEY, KC_NO,                  KC_NO,      KC_NO,      KC_NO,      KC_BASE
        ),
    //Language Selection Layer
    [_LL] = LAYOUT_left_right_stacked(
                        KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,
                        KC_NO,      KC_LANG_PT, KC_LANG_ES, KC_LANG_AR, KC_LANG_GR, KC_NO,      KC_NO,
        QK_UNICODE_MODE_WINCOMPOSE, KC_LANG_FR, KC_LANG_DE, KC_LANG_JA, KC_LANG_TR, KC_NO,      KC_NO,      KC_MS_BTN1,
        QK_UNICODE_MODE_EMACS,      KC_LANG_IT, KC_LANG_EN, KC_LANG_KO, KC_NO,      KC_NO,      KC_NO,      KC_NO,
        KC_BASE,                    KC_NO,      KC_NO,      KC_NO,                  KC_NO,      KC_NO,      KC_NO,

                    KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      KC_NO,      QK_UNICODE_MODE_MACOS,
                    KC_NO,      KC_LANG_GR, KC_LANG_AR, KC_LANG_ES, KC_LANG_PT, KC_NO,      QK_UNICODE_MODE_LINUX,
        _______,    KC_NO,      KC_LANG_TR, KC_LANG_JA, KC_LANG_DE, KC_LANG_FR, KC_NO,      QK_UNICODE_MODE_WINDOWS,
        KC_NO,      KC_NO,      KC_NO,      KC_LANG_KO, KC_LANG_EN, KC_LANG_IT, KC_NO,      QK_UNICODE_MODE_BSD,
        KC_NO,      KC_NO,      KC_NO,                  KC_NO,      KC_NO,      KC_NO,      KC_BASE
        ),
    [_ADDLANG1] = LAYOUT_left_right_stacked(
        KC_NO,      KC_NO,      KC_LAT0,    KC_LAT1,    KC_LAT2,    KC_LAT3,    KC_LAT4,
        KC_NO,      _______,    _______,    _______,    _______,    _______,    _______,
        KC_NO,      _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        KC_NO,      KC_NO,      _______,    _______,                _______,    _______,    _______,

                    KC_LAT5,    KC_LAT6,    KC_LAT7,    KC_LAT8,    KC_LAT9,    KC_NO,      KC_NO,
                    _______,    _______,    _______,    _______,    _______,    _______,    KC_NO,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    KC_NO,
        _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
        _______,    _______,    _______,                _______,    _______,    _______,    _______
        ),
    [_EMJ0] = LAYOUT_left_right_stacked(
        UM(0),    UM(1),    UM(2),    UM(3),    UM(4),    UM(5),    UM(6),
        UM(14),   UM(15),   UM(16),   UM(17),   UM(18),   UM(19),   UM(20),
        UM(28),   UM(29),   UM(30),   UM(31),   UM(32),   UM(33),   UM(34),   _______,
        UM(42),   UM(43),   UM(44),   UM(45),   UM(46),   UM(47),   UM(48),   UM(49),
        KC_BASE,  UM(58),   UM(59),   UM(60),             UM(61),   UM(62),   UM(63),

                  UM(7),    UM(8),    UM(9),    UM(10),   UM(11),   UM(12),   UM(13),
                  UM(21),   UM(22),   UM(23),   UM(24),   UM(25),   UM(26),   UM(27),
        _______,  UM(35),   UM(36),   UM(37),   UM(38),   UM(39),   UM(40),   UM(41),
        UM(50),   UM(51),   UM(52),   UM(53),   UM(54),   UM(55),   UM(56),   UM(57),
        UM(64),   UM(65),   UM(66),             UM(67),   UM(68),   UM(69),   TO(_EMJ1)
        ),
    [_EMJ1] = LAYOUT_left_right_stacked(
        UM(70+0),    UM(70+1),    UM(70+2),    UM(70+3),    UM(70+4),    UM(70+5),    UM(70+6),
        UM(70+14),   UM(70+15),   UM(70+16),   UM(70+17),   UM(70+18),   UM(70+19),   UM(70+20),
        UM(70+28),   UM(70+29),   UM(70+30),   UM(70+31),   UM(70+32),   UM(70+33),   UM(70+34),   _______,
        UM(70+42),   UM(70+43),   UM(70+44),   UM(70+45),   UM(70+46),   UM(70+47),   UM(70+48),   UM(70+49),
        KC_BASE,     UM(70+58),   UM(70+59),   UM(70+60),                UM(70+61),   UM(70+62),   UM(70+63),

                     UM(70+7),    UM(70+8),    UM(70+9),    UM(70+10),   UM(70+11),   UM(70+12),   UM(70+13),
                     UM(70+21),   UM(70+22),   UM(70+23),   UM(70+24),   UM(70+25),   UM(70+26),   UM(70+27),
        _______,     UM(70+35),   UM(70+36),   UM(70+37),   UM(70+38),   UM(70+39),   UM(70+40),   UM(70+41),
        UM(70+50),   UM(70+51),   UM(70+52),   UM(70+53),   UM(70+54),   UM(70+55),   UM(70+56),   UM(70+57),
        UM(70+64),   UM(70+65),   UM(70+66),                UM(70+67),   UM(70+68),   UM(70+69),   TO(_EMJ0)
        )
};


#define LX(x,y) ((x)/2),y
led_config_t g_led_config = { {// Key Matrix to LED Index
                              {6, 5, 4, 3, 2, 1, 0, NO_LED},
                              {13, 12, 11, 10, 9, 8, 7, NO_LED},
                              {20, 19, 18, 17, 16, 15, 14, NO_LED},
                              {27, 26, 25, 24, 23, 22, 21, NO_LED},
                              {35, 34, 33, 32, 31, 30, 29, 28},

                              {NO_LED, 42, 41, 40, 39, 38, 37, 36},
                              {NO_LED, 49, 48, 47, 46, 45, 44, 43},
                              {NO_LED, 56, 55, 54, 53, 52, 51, 50},
                              {NO_LED, 63, 62, 61, 60, 59, 58, 57},
                              {71, 70, 69, 68, 67, 66, 65, 64}
                             },
                             {
                                // LED Index to Physical Position
                                                {LX(144, 9)},   {LX(129, 9)},   {LX(104, 5)},   {LX(79, 1)},    {LX(55, 5)},    {LX(30, 9)},    {LX(0, 9)},
                                                {LX(144, 33)},  {LX(129, 33)},  {LX(104, 19)},  {LX(79, 25)},   {LX(55, 29)},   {LX(30, 33)},   {LX(0, 33)},
                                                {LX(144, 58)},  {LX(129, 58)},  {LX(104, 54)},  {LX(79, 50)},   {LX(55, 54)},   {LX(30, 58)},   {LX(0, 58)},
                                                {LX(144, 83)},  {LX(129, 83)},  {LX(104, 79)},  {LX(79, 75)},   {LX(55, 79)},   {LX(30, 83)},   {LX(0, 83)},
                {LX(170, 99)},  {LX(170, 127)}, {LX(144, 118)}, {LX(129, 113)},                 {LX(79, 99)},   {LX(55, 103)},  {LX(30, 107)},  {LX(6, 107)},

                                                {LX(446, 9)},   {LX(415, 9)},   {LX(390, 5)},   {LX(365, 1)},   {LX(341, 5)},   {LX(316, 9)},   {LX(286, 9)},
                                                {LX(446, 33)},  {LX(415, 33)},  {LX(390, 19)},  {LX(365, 25)},  {LX(341, 29)},  {LX(316, 33)},  {LX(286, 33)},
                                                {LX(446, 58)},  {LX(415, 58)},  {LX(390, 54)},  {LX(365, 50)},  {LX(341, 54)},  {LX(316, 58)},  {LX(286, 58)},
                                                {LX(446, 83)},  {LX(415, 83)},  {LX(390, 79)},  {LX(365, 75)},  {LX(341, 79)},  {LX(316, 83)},  {LX(286, 83)},
                                                {LX(440, 107)}, {LX(415, 107)}, {LX(390, 103)}, {LX(365, 99)},                  {LX(324, 113)}, {LX(290, 118)}, {LX(264, 127)},  {LX(264, 99)}
                             },
                             {
                                 // LED Index to Flag
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4, 4,

                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4,
                                 4, 4, 4, 4, 4, 4, 4, 4
                             } };

const uint16_t* keycode_to_disp_text(uint16_t keycode, led_t state) {
    switch (keycode) {
    /*[[[cog
    wb = load_workbook(filename = os.path.join(os.path.abspath(os.path.dirname(cog.inFile)), "lang/lang_lut.xlsx"), data_only=True)
    sheet = wb['named_glyphs']

    idx = 0
    glyph_index = 1
    glyph_key = sheet[f"A{glyph_index}"].value
    while glyph_key and not glyph_key.startswith("PRIVATE_EMOJI"):
        glyph_key = sheet[f"A{glyph_index}"].value
        glyph_index = glyph_index + 1
    while glyph_key and glyph_key.startswith("PRIVATE_EMOJI"):
        cog.outl(f'case UM({idx}): return u" " {glyph_key};')
        glyph_key = sheet[f"A{glyph_index}"].value
        idx = idx + 1
        glyph_index = glyph_index + 1

    ]]]*/
    case UM(0): return u" " PRIVATE_EMOJI_1F600;
    case UM(1): return u" " PRIVATE_EMOJI_1F601;
    case UM(2): return u" " PRIVATE_EMOJI_1F602;
    case UM(3): return u" " PRIVATE_EMOJI_1F603;
    case UM(4): return u" " PRIVATE_EMOJI_1F604;
    case UM(5): return u" " PRIVATE_EMOJI_1F605;
    case UM(6): return u" " PRIVATE_EMOJI_1F606;
    case UM(7): return u" " PRIVATE_EMOJI_1F607;
    case UM(8): return u" " PRIVATE_EMOJI_1F608;
    case UM(9): return u" " PRIVATE_EMOJI_1F609;
    case UM(10): return u" " PRIVATE_EMOJI_1F60A;
    case UM(11): return u" " PRIVATE_EMOJI_1F60B;
    case UM(12): return u" " PRIVATE_EMOJI_1F60C;
    case UM(13): return u" " PRIVATE_EMOJI_1F60D;
    case UM(14): return u" " PRIVATE_EMOJI_1F60E;
    case UM(15): return u" " PRIVATE_EMOJI_1F60F;
    case UM(16): return u" " PRIVATE_EMOJI_1F610;
    case UM(17): return u" " PRIVATE_EMOJI_1F611;
    case UM(18): return u" " PRIVATE_EMOJI_1F612;
    case UM(19): return u" " PRIVATE_EMOJI_1F613;
    case UM(20): return u" " PRIVATE_EMOJI_1F614;
    case UM(21): return u" " PRIVATE_EMOJI_1F615;
    case UM(22): return u" " PRIVATE_EMOJI_1F616;
    case UM(23): return u" " PRIVATE_EMOJI_1F617;
    case UM(24): return u" " PRIVATE_EMOJI_1F618;
    case UM(25): return u" " PRIVATE_EMOJI_1F619;
    case UM(26): return u" " PRIVATE_EMOJI_1F61A;
    case UM(27): return u" " PRIVATE_EMOJI_1F61B;
    case UM(28): return u" " PRIVATE_EMOJI_1F61C;
    case UM(29): return u" " PRIVATE_EMOJI_1F61D;
    case UM(30): return u" " PRIVATE_EMOJI_1F61E;
    case UM(31): return u" " PRIVATE_EMOJI_1F61F;
    case UM(32): return u" " PRIVATE_EMOJI_1F620;
    case UM(33): return u" " PRIVATE_EMOJI_1F621;
    case UM(34): return u" " PRIVATE_EMOJI_1F622;
    case UM(35): return u" " PRIVATE_EMOJI_1F623;
    case UM(36): return u" " PRIVATE_EMOJI_1F624;
    case UM(37): return u" " PRIVATE_EMOJI_1F625;
    case UM(38): return u" " PRIVATE_EMOJI_1F626;
    case UM(39): return u" " PRIVATE_EMOJI_1F627;
    case UM(40): return u" " PRIVATE_EMOJI_1F628;
    case UM(41): return u" " PRIVATE_EMOJI_1F629;
    case UM(42): return u" " PRIVATE_EMOJI_1F62A;
    case UM(43): return u" " PRIVATE_EMOJI_1F62B;
    case UM(44): return u" " PRIVATE_EMOJI_1F62C;
    case UM(45): return u" " PRIVATE_EMOJI_1F62D;
    case UM(46): return u" " PRIVATE_EMOJI_1F62E;
    case UM(47): return u" " PRIVATE_EMOJI_1F62F;
    case UM(48): return u" " PRIVATE_EMOJI_1F630;
    case UM(49): return u" " PRIVATE_EMOJI_1F631;
    case UM(50): return u" " PRIVATE_EMOJI_1F632;
    case UM(51): return u" " PRIVATE_EMOJI_1F633;
    case UM(52): return u" " PRIVATE_EMOJI_1F634;
    case UM(53): return u" " PRIVATE_EMOJI_1F635;
    case UM(54): return u" " PRIVATE_EMOJI_1F636;
    case UM(55): return u" " PRIVATE_EMOJI_1F637;
    case UM(56): return u" " PRIVATE_EMOJI_1F638;
    case UM(57): return u" " PRIVATE_EMOJI_1F639;
    case UM(58): return u" " PRIVATE_EMOJI_1F644;
    case UM(59): return u" " PRIVATE_EMOJI_1F645;
    case UM(60): return u" " PRIVATE_EMOJI_1F646;
    case UM(61): return u" " PRIVATE_EMOJI_1F647;
    case UM(62): return u" " PRIVATE_EMOJI_1F648;
    case UM(63): return u" " PRIVATE_EMOJI_1F649;
    case UM(64): return u" " PRIVATE_EMOJI_1F64A;
    case UM(65): return u" " PRIVATE_EMOJI_1F64B;
    case UM(66): return u" " PRIVATE_EMOJI_1F64C;
    case UM(67): return u" " PRIVATE_EMOJI_1F64D;
    case UM(68): return u" " PRIVATE_EMOJI_1F64E;
    case UM(69): return u" " PRIVATE_EMOJI_1F64F;
    case UM(70): return u" " PRIVATE_EMOJI_1F440;
    case UM(71): return u" " PRIVATE_EMOJI_1F441;
    case UM(72): return u" " PRIVATE_EMOJI_1F442;
    case UM(73): return u" " PRIVATE_EMOJI_1F443;
    case UM(74): return u" " PRIVATE_EMOJI_1F444;
    case UM(75): return u" " PRIVATE_EMOJI_1F445;
    case UM(76): return u" " PRIVATE_EMOJI_1F446;
    case UM(77): return u" " PRIVATE_EMOJI_1F447;
    case UM(78): return u" " PRIVATE_EMOJI_1F448;
    case UM(79): return u" " PRIVATE_EMOJI_1F449;
    case UM(80): return u" " PRIVATE_EMOJI_1F44A;
    case UM(81): return u" " PRIVATE_EMOJI_1F44B;
    case UM(82): return u" " PRIVATE_EMOJI_1F44C;
    case UM(83): return u" " PRIVATE_EMOJI_1F44D;
    case UM(84): return u" " PRIVATE_EMOJI_1F44E;
    case UM(85): return u" " PRIVATE_EMOJI_1F44F;
    case UM(86): return u" " PRIVATE_EMOJI_1F450;
    case UM(87): return u" " PRIVATE_EMOJI_1F451;
    case UM(88): return u" " PRIVATE_EMOJI_1F452;
    case UM(89): return u" " PRIVATE_EMOJI_1F453;
    case UM(90): return u" " PRIVATE_EMOJI_1F47B;
    case UM(91): return u" " PRIVATE_EMOJI_1F47C;
    case UM(92): return u" " PRIVATE_EMOJI_1F47D;
    case UM(93): return u" " PRIVATE_EMOJI_1F47E;
    case UM(94): return u" " PRIVATE_EMOJI_1F47F;
    case UM(95): return u" " PRIVATE_EMOJI_1F480;
    case UM(96): return u" " PRIVATE_EMOJI_1F481;
    case UM(97): return u" " PRIVATE_EMOJI_1F482;
    case UM(98): return u" " PRIVATE_EMOJI_1F483;
    case UM(99): return u" " PRIVATE_EMOJI_1F484;
    case UM(100): return u" " PRIVATE_EMOJI_1F485;
    case UM(101): return u" " PRIVATE_EMOJI_1F489;
    case UM(102): return u" " PRIVATE_EMOJI_1F48A;
    case UM(103): return u" " PRIVATE_EMOJI_1F48B;
    case UM(104): return u" " PRIVATE_EMOJI_1F48C;
    case UM(105): return u" " PRIVATE_EMOJI_1F48D;
    case UM(106): return u" " PRIVATE_EMOJI_1F48E;
    case UM(107): return u" " PRIVATE_EMOJI_1F48F;
    case UM(108): return u" " PRIVATE_EMOJI_1F490;
    case UM(109): return u" " PRIVATE_EMOJI_1F491;
    case UM(110): return u" " PRIVATE_EMOJI_1F492;
    case UM(111): return u" " PRIVATE_EMOJI_1F493;
    case UM(112): return u" " PRIVATE_EMOJI_1F494;
    case UM(113): return u" " PRIVATE_EMOJI_1F495;
    case UM(114): return u" " PRIVATE_EMOJI_1F496;
    case UM(115): return u" " PRIVATE_EMOJI_1F4A1;
    case UM(116): return u" " PRIVATE_EMOJI_1F4A2;
    case UM(117): return u" " PRIVATE_EMOJI_1F4A3;
    case UM(118): return u" " PRIVATE_EMOJI_1F4A4;
    case UM(119): return u" " PRIVATE_EMOJI_1F4A5;
    case UM(120): return u" " PRIVATE_EMOJI_1F4A6;
    case UM(121): return u" " PRIVATE_EMOJI_1F4A7;
    case UM(122): return u" " PRIVATE_EMOJI_1F4A8;
    case UM(123): return u" " PRIVATE_EMOJI_1F4A9;
    case UM(124): return u" " PRIVATE_EMOJI_1F4AA;
    case UM(125): return u" " PRIVATE_EMOJI_1F4AB;
    case UM(126): return u" " PRIVATE_EMOJI_1F4AC;
    case UM(127): return u" " PRIVATE_EMOJI_1F4AD;
    case UM(128): return u" " PRIVATE_EMOJI_1F4AE;
    case UM(129): return u" " PRIVATE_EMOJI_1F4AF;
    case UM(130): return u" " PRIVATE_EMOJI_1F4B0;
    case UM(131): return u" " PRIVATE_EMOJI_1F4B1;
    case UM(132): return u" " PRIVATE_EMOJI_1F912;
    case UM(133): return u" " PRIVATE_EMOJI_1F913;
    case UM(134): return u" " PRIVATE_EMOJI_1F914;
    case UM(135): return u" " PRIVATE_EMOJI_1F915;
    case UM(136): return u" " PRIVATE_EMOJI_1F916;
    case UM(137): return u" " PRIVATE_EMOJI_1F917;
    case UM(138): return u" " PRIVATE_EMOJI_1F918;
    case UM(139): return u" " PRIVATE_EMOJI_1F919;
    //[[[end]]]
    case TO(_EMJ0): return u" " PRIVATE_EMOJI_1F600 u"\v" ICON_LAYER;
    case TO(_EMJ1): return u" " PRIVATE_EMOJI_1F440 u"\v" ICON_LAYER;
    case KC_DEADKEY: return (g_local.flags&DEAD_KEY_ON_WAKEUP)==0 ? u"WakeX\r\v" ICON_SWITCH_OFF : u"WakeX\r\v" ICON_SWITCH_ON;
    case LBL_TEXT: return u"Text:";
    case KC_TOGMODS: return (g_local.flags&MODS_AS_TEXT)==0 ? u"Mods\r\v" ICON_SWITCH_OFF : u"Mods\r\v" ICON_SWITCH_ON;
    case KC_TOGTEXT: return (g_local.flags&MORE_TEXT)==0 ? u"Cmds\r\v" ICON_SWITCH_OFF : u"Cmds\r\v" ICON_SWITCH_ON;
    case QK_UNICODE_MODE_MACOS: return u"Mac";
    case QK_UNICODE_MODE_LINUX: return u"Lnx";
    case QK_UNICODE_MODE_WINDOWS: return u"Win";
    case QK_UNICODE_MODE_BSD: return u"BSD";
    case QK_UNICODE_MODE_WINCOMPOSE: return u"WinC";
    case QK_UNICODE_MODE_EMACS: return u"Emcs";
    case QK_LEAD:
        return u"Lead";
    case KC_HYPR:
        return (g_local.flags&MORE_TEXT)!=0 ? u"Hypr" : u" " PRIVATE_HYPER;
    case KC_MEH:
        return (g_local.flags&MORE_TEXT)!=0 ? u"Meh" : u" " PRIVATE_MEH;
    case KC_EXEC:
        return u"Exec";
    case KC_NUM_LOCK:
        return !state.num_lock ? u"Nm" ICON_NUMLOCK_OFF : u"Nm" ICON_NUMLOCK_ON;
    case KC_KP_SLASH:
        return u"/";
    case KC_KP_ASTERISK:
    case KC_KP_MINUS:
        return u"-";
    case KC_KP_7:
        return !state.num_lock ? ARROWS_LEFTSTOP : u"7";
    case KC_KP_8:
        return !state.num_lock ? u"   " ICON_UP : u"8";
    case KC_KP_9:
        return !state.num_lock ? ARROWS_UPSTOP : u"9";
    case KC_KP_PLUS:
        return u"+";
    case KC_KP_4:
        return !state.num_lock ? u"  " ICON_LEFT : u"4";
    case KC_KP_5:
        return !state.num_lock ? u"" : u"5";
    case KC_KP_6:
        return !state.num_lock ? u"  " ICON_RIGHT : u"6";
    case KC_KP_EQUAL:
        return u"=";
    case KC_KP_1:
        return !state.num_lock ? ARROWS_RIGHTSTOP : u"1";
    case KC_KP_2:
        return !state.num_lock ? u"   " ICON_DOWN : u"2";
    case KC_KP_3:
        return !state.num_lock ? ARROWS_DOWNSTOP : u"3";
    case KC_CALCULATOR:
        return u"   " PRIVATE_CALC;
    case KC_KP_0:
        return !state.num_lock ? u"Ins" : u"0";
    case KC_KP_DOT:
        return !state.num_lock ? u"Del" : u".";
    case KC_KP_ENTER:
        return u"Enter";
    case QK_BOOTLOADER:
        return u"Boot";
    case QK_DEBUG_TOGGLE:
        return ((g_local.flags & DBG_ON) == 0) ? u"Dbg\r\v" ICON_SWITCH_OFF : u"Dbg\r\v" ICON_SWITCH_ON;
    case RGB_RMOD:
        return u" " ICON_LEFT PRIVATE_LIGHT;
    case RGB_TOG:
        return u"  I/O";
    case RGB_MOD:
        return PRIVATE_LIGHT ICON_RIGHT;
    case RGB_HUI:
        return u"Hue+";
    case RGB_HUD:
        return u"Hue-";
    case RGB_SAI:
        return u"Sat+";
    case RGB_SAD:
        return u"Sat-";
    case RGB_VAI:
        return u"Bri+";
    case RGB_VAD:
        return u"Bri-";
    case RGB_SPI:
        return u"Spd+";
    case RGB_SPD:
        return u"Spd-";
    case RGB_MODE_PLAIN:
        return u"Plan";
    case RGB_MODE_BREATHE:
        return u"Brth";
    case RGB_MODE_SWIRL:
        return u"Swrl";
    case RGB_MODE_RAINBOW:
        return u"Rnbw";
    case KC_MEDIA_NEXT_TRACK:
        return ICON_RIGHT ICON_RIGHT;
    case KC_MEDIA_PLAY_PAUSE:
        return u"  " ICON_RIGHT;
    case KC_MEDIA_STOP:
        return u"Stop";
    case KC_MEDIA_PREV_TRACK:
        return ICON_LEFT ICON_LEFT;
    case KC_MS_ACCEL0:
        return u">>";
    case KC_MS_ACCEL1:
        return u">>>";
    case KC_MS_ACCEL2:
        return u">>>>";
    case KC_BTN1:
        return u"  " ICON_LMB;
    case KC_BTN2:
        return u"  " ICON_RMB;
    case KC_BTN3:
        return u"  " ICON_MMB;
    case KC_MS_UP:
        return u"  " ICON_UP;
    case KC_MS_DOWN:
        return u"  " ICON_DOWN;
    case KC_MS_LEFT:
        return u"  " ICON_LEFT;
    case KC_MS_RIGHT:
        return u"  " ICON_RIGHT;
    case KC_AUDIO_MUTE:
        return u"  " PRIVATE_MUTE;
    case KC_AUDIO_VOL_DOWN:
        return u"  " PRIVATE_VOL_DOWN;
    case KC_AUDIO_VOL_UP:
        return u"  " PRIVATE_VOL_UP;
    case KC_PRINT_SCREEN:
        return u"  " PRIVATE_IMAGE;
    case KC_SCROLL_LOCK:
        return u"ScLk";
    case KC_PAUSE:
        return u"Paus";
    case KC_INSERT:
        return u"Ins";
    case KC_HOME:
        return ARROWS_LEFTSTOP;
    case KC_END:
        return u"   " ARROWS_RIGHTSTOP;
    case KC_PAGE_UP:
        return u"  " ARROWS_UPSTOP;
    case KC_PAGE_DOWN:
        return u"  " ARROWS_DOWNSTOP;
    case KC_DELETE:
        return (g_local.flags&MORE_TEXT)!=0 ? u"Del" : TECHNICAL_ERASERIGHT;
    case KC_MYCM:
        return u"  " PRIVATE_PC;
    case TO(_SL):
        return PRIVATE_SETTINGS u"\v" ICON_LAYER;
    case MO(_FL0):
    case MO(_FL1):
        return u"Fn\r\v\t" ICON_LAYER;
    case TO(_NL):
        return u"Nm\r\v\t" ICON_LAYER;
    case MO(_NL):
        return u"Nm!\r\v\t" ICON_LAYER;
    case KC_BASE:
        return u"Base\r\v\t" ICON_LAYER;
    case KC_L0:
        return g_local.default_ls==_L0 ? u"Qwty\r\v" ICON_SWITCH_ON : u"Qwty\r\v" ICON_SWITCH_OFF;
    case KC_L1:
        return g_local.default_ls==_L1 ? u"Qwty!\r\v" ICON_SWITCH_ON : u"Qwty!\r\v" ICON_SWITCH_OFF;
    case KC_L2:
        return g_local.default_ls==_L2 ? u"Clmk\r\v" ICON_SWITCH_ON : u"Clmk\r\v" ICON_SWITCH_OFF;
    case KC_L3:
        return g_local.default_ls==_L3 ? u"Neo\r\v" ICON_SWITCH_ON : u"Neo\r\v" ICON_SWITCH_OFF;
    case KC_L4:
        return g_local.default_ls==_L4 ? u"Wkm\r\v" ICON_SWITCH_ON : u"Wkm\r\v" ICON_SWITCH_OFF;
    case KC_L5:
        return g_local.default_ls==_L5 ? u"Chad\r\v" ICON_SWITCH_ON : u"Chad\r\v" ICON_SWITCH_OFF;
    case OSL(_UL):
        return u"Util*\r\v\t" ICON_LAYER;
    case TO(_UL):
        return u"Util\r\v\t" ICON_LAYER;
    case MO(_ADDLANG1):
        return u"Intl";
    case KC_F1: return u" F1";
    case KC_F2: return u" F2";
    case KC_F3: return u" F3";
    case KC_F4: return u" F4";
    case KC_F5: return u" F5";
    case KC_F6: return u" F6";
    case KC_F7: return u" F7";
    case KC_F8: return u" F8";
    case KC_F9: return u" F9";
    case KC_F10:return u" F10";
    case KC_F11:return u" F11";
    case KC_F12:return u" F12";
    case KC_F13:return u" F13";
    case KC_F14:return u" F14";
    case KC_F15:return u" F15";
    case KC_F16:return u" F16";
    case KC_F17:return u" F17";
    case KC_F18:return u" F18";
    case KC_F19:return u" F19";
    case KC_F20:return u" F20";
    case KC_F21:return u" F21";
    case KC_F22:return u" F22";
    case KC_F23:return u" F23";
    case KC_F24:return u" F24";
    case KC_LEFT_CTRL:
    case KC_RIGHT_CTRL:
        return (g_local.flags&MODS_AS_TEXT)!=0 ? u"Ctrl" : TECHNICAL_CONTROL;
    case KC_LEFT_ALT:
        return (g_local.flags&MODS_AS_TEXT)!=0 ? u"Alt" : TECHNICAL_OPTION;
    case KC_RIGHT_ALT:
        return (g_local.flags&MODS_AS_TEXT)!=0 ? ( g_local.lang == KC_LANG_EN ? u"Alt" : u"Alt\nGr") : TECHNICAL_OPTION;
    case KC_LGUI:
    case KC_RGUI:
        return (g_local.flags&MODS_AS_TEXT)!=0 ? u"GUI" : DINGBAT_BLACK_DIA_X;
    case KC_LEFT:
        return u"  " ICON_LEFT;
    case KC_RIGHT:
        return u"  " ICON_RIGHT;
    case KC_UP:
        return u"  " ICON_UP;
    case KC_DOWN:
        return u"  " ICON_DOWN;
    case KC_CAPS_LOCK:
        return state.caps_lock ? u"Cap" ICON_CAPSLOCK_ON : u"Cap" ICON_CAPSLOCK_OFF;
    case KC_LSFT:
    case KC_RSFT:
        return (g_local.flags&MODS_AS_TEXT)!=0 ? u"Shft" : u" " ICON_SHIFT;
    case KC_NO:
        return u"";
    case KC_DDIM:
        return u"  " ICON_LEFT;
    case KC_DBRI:
        return u"  " ICON_RIGHT;
    case KC_D1Q:
        return u"  " PRIVATE_DISP_DARKER;
    case KC_D3Q:
        return u"  " PRIVATE_DISP_BRIGHTER;
    case KC_DHLF:
        return u"  " PRIVATE_DISP_HALF;
    case KC_DMIN:
        return u"  " PRIVATE_DISP_DARK;
    case KC_DMAX:
        return u"  " PRIVATE_DISP_BRIGHT;
    case KC_LANG:
        return (g_local.flags&MORE_TEXT)!=0 ? u"Lang" : PRIVATE_WORLD;
    case SH_TOGG:
        return u"SwpH";
    case QK_MAKE:
        return u"Make";
    case EE_CLR:
        return u"ClrEE";
    case QK_REBOOT:
        return u" " ARROWS_CIRCLE;
    case KC_LNG1:
        return u"Han/Y";
    case KC_APP:
        return u" Ctx";
    case DE_GRV: //for Neo Layout
        return u"`";
    case KC_SAVE: // for chad3814 layout
        return u"Save";
    case KC_SCRENCAP: // for chad3814
        return u"ScrnCap";
    case KC_3FS: // for chad3814
        return u"3FS";
    case KC_LOCK: // for chad3814
        return u"    " PRIVATE_LOCK;
    /*[[[cog
        for lang in languages:
            short = lang.split("_")[1]
            cog.outl(f'case KC_{lang}: return g_local.lang == {lang} ? u"[{short}]" : u" {short}";')
    ]]]*/
    case KC_LANG_EN: return g_local.lang == LANG_EN ? u"[EN]" : u" EN";
    case KC_LANG_DE: return g_local.lang == LANG_DE ? u"[DE]" : u" DE";
    case KC_LANG_FR: return g_local.lang == LANG_FR ? u"[FR]" : u" FR";
    case KC_LANG_ES: return g_local.lang == LANG_ES ? u"[ES]" : u" ES";
    case KC_LANG_PT: return g_local.lang == LANG_PT ? u"[PT]" : u" PT";
    case KC_LANG_IT: return g_local.lang == LANG_IT ? u"[IT]" : u" IT";
    case KC_LANG_TR: return g_local.lang == LANG_TR ? u"[TR]" : u" TR";
    case KC_LANG_KO: return g_local.lang == LANG_KO ? u"[KO]" : u" KO";
    case KC_LANG_JA: return g_local.lang == LANG_JA ? u"[JA]" : u" JA";
    case KC_LANG_AR: return g_local.lang == LANG_AR ? u"[AR]" : u" AR";
    case KC_LANG_GR: return g_local.lang == LANG_GR ? u"[GR]" : u" GR";
    //[[[end]]]
    //The following entries will over-rule language specific enties in the follow langauge lookup table,
    //however with this we can control them by flags and so far those wehere not lanuage specific anyway.
    case KC_ENTER:      return (g_local.flags&MORE_TEXT)!=0 ? u"Enter"    : ARROWS_RETURN;
    case KC_ESCAPE:	    return (g_local.flags&MORE_TEXT)!=0 ? u"Esc"      : TECHNICAL_ESCAPE;
    case KC_BACKSPACE:  return (g_local.flags&MORE_TEXT)!=0 ? u"Bksp"     : TECHNICAL_ERASELEFT;
    case KC_TAB:        return (g_local.flags&MORE_TEXT)!=0 ? u"Tab"      : ARROWS_TAB;
    case KC_SPACE:      return (g_local.flags&MORE_TEXT)!=0 ? u"Space"    : ICON_SPACE;
    default:
    {
        bool shift = ((get_mods() & MOD_MASK_SHIFT) != 0);
        bool add_lang = get_highest_layer(layer_state)==_ADDLANG1;
        bool alt = ((get_mods() & MOD_MASK_ALT) != 0);
        if(keycode>=KC_A && keycode<=KC_Z && add_lang) {
            const uint8_t offset = (shift || state.caps_lock) ? 0 : 26;
            uint8_t variation = (shift || state.caps_lock) ? latin_ex[keycode-KC_A]>>4 : latin_ex[keycode-KC_A]&0xf;

            const uint16_t* def_variation = latin_ex_map[offset+keycode-KC_A][0];
            return (def_variation!=NULL) ? latin_ex_map[offset+keycode-KC_A][variation] : NULL;
        }

        if(keycode>=KC_LAT0 && keycode<=KC_LAT9 && add_lang && alt && g_local.last_latin_kc!=0) {
            const uint8_t offset = (shift || state.caps_lock) ? 0 : 26;
            return latin_ex_map[offset+g_local.last_latin_kc-KC_A][keycode-KC_LAT0];
        }

        const uint16_t* text = translate_keycode(g_local.lang, keycode, shift, state.caps_lock);
        if (text != NULL) {
            return text;
        }
    }
    break;
    }
    return NULL;
}

const uint16_t* keycode_to_disp_overlay(uint16_t keycode, led_t state) {
    switch (keycode)
    {
        case KC_F2: return u"      " PRIVATE_NOTE;
        case KC_F5: return u"     " ARROWS_CIRCLE;
        default: break;
    }

    if( (get_mods() & MOD_MASK_CTRL) != 0) {
        switch(keycode) {
            case KC_A: return u"      " BOX_WITH_CHECK_MARK;
            case KC_C: return u"     " CLIPBOARD_COPY;
            case KC_D: return u"\t " PRIVATE_DELETE;
            case KC_F: return u"    " PRIVATE_FIND;
            case KC_X: return u"\t\b\b\b\b" CLIPBOARD_CUT;
            case KC_V: return u"     " CLIPBOARD_PASTE;
            case KC_S: return u"\t" PRIVATE_FLOPPY;
            case KC_O: return u"\t" FILE_OPEN;
            case KC_P: return u"\t\b\b" PRIVATE_PRINTER;
            case KC_Z: return u"      " ARROWS_UNDO;
            case KC_Y: return u"      " ARROWS_REDO;
            default: break;
        }
    } else if((get_mods() & MOD_MASK_GUI) != 0) {
        switch(keycode) {
            case KC_D:      return u"    " PRIVATE_PC;
            case KC_L:      return u"    " PRIVATE_LOCK;
            case KC_P:      return u"    " PRIVATE_SCREEN;
            case KC_UP:     return u"     " PRIVATE_MAXIMIZE;
            case KC_DOWN:   return u"     " PRIVATE_WINDOW;
            default: break;
        }
    }
    return NULL;
}


void update_displays(enum refresh_mode mode) {
    if(g_local.contrast<=DISP_OFF || (g_local.flags&DISP_IDLE)!=0) {
        return;
    }

    //uint8_t layer = get_highest_layer(layer_state);

    led_t state = host_keyboard_led_state();
    bool capital_case = ((get_mods() & MOD_MASK_SHIFT) != 0) || state.caps_lock;
    //the left side has an offset of 0, the right side an offset of MATRIX_ROWS_PER_SIDE
    uint8_t offset = is_left_side() ? 0 : MATRIX_ROWS_PER_SIDE;
    uint8_t start_row = 0;

    //select first display (and later on shift that 0 till the end)
    if (mode == START_SECOND_HALF) {
        sr_shift_out_buffer_latch(disp_row_3.bitmask, sizeof(struct display_info));
        start_row = 3;
    }
    else {
        sr_shift_out_buffer_latch(disp_row_0.bitmask, sizeof(struct display_info));
    }

    uint8_t max_rows = mode == START_FIRST_HALF ? 3 : MATRIX_ROWS_PER_SIDE;

    uint8_t skip = 0;
    for (uint8_t r = start_row; r < max_rows; ++r) {
        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            uint8_t  disp_idx = LAYOUT_TO_INDEX(r, c);

            //since MATRIX_COLS==8 we don't need to shift multiple times at the end of the row
            //except there was a leading and missing physical key (KC_NO on base layer)
            uint16_t keycode = keymaps[_BL][r + offset][c];
            if (keycode == KC_NO) {
                skip++;
            }
            else {
                if (disp_idx != 255) {
                    uint8_t layer = get_highest_layer(layer_state);
                    uint16_t highest_kc = keymaps[layer][r + offset][c];
                    //if we encounter a transparent key go down one layer (but only one!)
                    keycode = (highest_kc == KC_TRNS) ? keymaps[get_highest_layer(layer_state&~(1<<layer))][r + offset][c] : highest_kc;
                    kdisp_enable(true);//(g_local.flags&STATUS_DISP_ON) != 0);
                    kdisp_set_contrast(g_local.contrast-1);
                    if(keycode!=KC_TRNS) {
                        const uint16_t* text = keycode_to_disp_text(keycode, state);
                        kdisp_set_buffer(0x00);
                        if(text==NULL){
                            if((keycode&QK_UNICODEMAP_PAIR)!=0){
                                uint16_t chr = capital_case ? QK_UNICODEMAP_PAIR_GET_SHIFTED_INDEX(keycode) : QK_UNICODEMAP_PAIR_GET_UNSHIFTED_INDEX(keycode);
                                kdisp_write_gfx_char(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 28, 23, unicode_map[chr]);
                            }
                        } else {
                            kdisp_write_gfx_text(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 28, 23, text);
                        }
                        text = keycode_to_disp_overlay(keycode, state);
                        if(text!=NULL) {
                            kdisp_write_gfx_text(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 28, 23, text);
                        }
                        kdisp_send_buffer();
                    }
                }
                sr_shift_once_latch();
            }

        }
        for (;skip > 0;skip--) {
            sr_shift_once_latch();
        }
    }
}

uint8_t to_brightness(uint8_t b) {
    switch(b) {
        case 23: case 24: case 25: case 26: case 27: return 7;
        case 22: case 28: return 6;
        case 21: case 29: return 5;
        case 20: case 30: return 4;
        case 19: case 31: return 3;
        case 18: case 32: return 2;
        case 1: case 7: return 1;
        case 2: case 6: return 3;
        case 3: case 4: case 5: return 5;
        default: return 0;
    }
}

void kdisp_idle(uint8_t contrast) {
    uint8_t offset = is_left_side() ? 0 : MATRIX_ROWS_PER_SIDE;
    uint8_t skip = 0;
    sr_shift_out_buffer_latch(disp_row_0.bitmask, sizeof(struct display_info));

    //uint8_t idx = 0;
    for (uint8_t r = 0; r < MATRIX_ROWS_PER_SIDE; ++r) {
        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            uint8_t  disp_idx = LAYOUT_TO_INDEX(r, c);

            //since MATRIX_COLS==8 we don't need to shift multiple times at the end of the row
            //except there was a leading and missing physical key (KC_NO on base layer)
            uint16_t keycode = keymaps[_BL][r + offset][c];
            if (keycode == KC_NO) {
                skip++;
            } else {
                if (disp_idx != 255) {
                    uint8_t idle_brightness = to_brightness((contrast+(c%3+r)*keycode+offset+r)%50);
                    if(idle_brightness==0) {
                        kdisp_enable(false);
                    } else {
                        kdisp_enable(true);
                        kdisp_set_contrast(idle_brightness-1);
                    }
                }
                sr_shift_once_latch();
            }

        }
        for (;skip > 0;skip--) {
            sr_shift_once_latch();
        }
    }
}

void display_message(uint8_t row, uint8_t col, const uint16_t* message, const GFXfont* font) {

    const GFXfont* displayFont[] = { font };
    uint8_t index = 0;
    for (uint8_t c = 0; c < MATRIX_COLS; ++c) {

        uint8_t disp_idx = LAYOUT_TO_INDEX(row, c);

        if (disp_idx != 255) {

            sr_shift_out_buffer_latch(key_display[disp_idx].bitmask, sizeof(key_display->bitmask));
            kdisp_set_buffer(0x00);

            if (c >= col && message[index] != 0) {
                const uint16_t text[2] = { message[index], 0 };
                kdisp_write_gfx_text(displayFont, 1, 49, 38, text);
                index++;
            }
            kdisp_send_buffer();
        }
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t* record) {
    const bool addlang = get_highest_layer(layer_state)==_ADDLANG1;
    if (record->event.pressed) {
        switch (keycode) {
            case QK_BOOTLOADER:
                uprintf("Bootloader entered. Please copy new Firmware.\n");
                clear_all_displays();
                display_message(1, 1, u"BOOT-", &FreeSansBold24pt7b);
                display_message(3, 0, u"LOADER!", &FreeSansBold24pt7b);
                return true;
            case KC_A ... KC_Z:
                g_local.last_latin_kc = keycode;
                if((get_mods() & MOD_MASK_ALT) == 0 && addlang) {
                    const bool lshift = get_mods() == MOD_BIT(KC_LEFT_SHIFT);
                    const bool rshift = get_mods() == MOD_BIT(KC_RIGHT_SHIFT);
                    const bool upper_case = lshift || rshift || g_state.led_state.caps_lock;
                    const uint8_t offset = upper_case ? 0 : 26;
                    if(latin_ex_map[offset+keycode-KC_A][0]) {
                        uint8_t variation = upper_case ? latin_ex[keycode-KC_A]>>4 : latin_ex[keycode-KC_A]&0xf;

                        //this is a work-around (at least for I-Bus on Linux we need to remove the shift, otherwise the Unicode sequence will not be recognized!)
                        if(lshift) unregister_code16(KC_LEFT_SHIFT);
                        if(rshift) unregister_code16(KC_RIGHT_SHIFT);
                        register_unicode(latin_ex_map[offset+keycode-KC_A][variation][0]);
                        if(lshift) register_code16(KC_LEFT_SHIFT);
                        if(rshift) register_code16(KC_RIGHT_SHIFT);
                        return false;
                    }
                }
                break;
            default:
                break;
        }

        if((get_mods() & MOD_MASK_ALT) != 0 && addlang) {
            switch(keycode) {
                case KC_LAT0 ... KC_LAT9:
                    if( g_local.last_latin_kc!=0) {
                        uint8_t current = latin_ex[g_local.last_latin_kc-KC_A];
                        if((get_mods() & MOD_MASK_SHIFT) || g_state.led_state.caps_lock) {
                            latin_ex[g_local.last_latin_kc-KC_A] = ((keycode-KC_LAT0)<<4) | (current&0xf);
                        } else {
                            latin_ex[g_local.last_latin_kc-KC_A] = (keycode-KC_LAT0) | (current&0xf0);
                        }
                        bool sync_success = false;
                        for(uint8_t retry = 0; retry<3 && !sync_success; ++retry) {
                            sync_success = transaction_rpc_send(USER_SYNC_POLY_DATA, sizeof(latin_ex), latin_ex);
                        }
                        save_user_eeconf();
                        request_disp_refresh();
                    }
                    break;
                case KC_A ... KC_Z:
                    request_disp_refresh();
                    break;
                default:
                    break;
            }

            return false;
        } else {
            g_local.last_latin_kc = 0;
        }
    }
    return display_wakeup(record);
}

void post_process_record_user(uint16_t keycode, keyrecord_t* record) {
    if (keycode == KC_CAPS_LOCK) {
        request_disp_refresh();
    }

    if (!record->event.pressed) {
        switch (keycode) {
        case KC_DEADKEY:
            if((g_local.flags&DEAD_KEY_ON_WAKEUP)==0) {
                g_local.flags |= DEAD_KEY_ON_WAKEUP;
            } else {
                g_local.flags &= ~((uint8_t)DEAD_KEY_ON_WAKEUP);
            }
            request_disp_refresh();
            break;
        case KC_TOGMODS:
            if((g_local.flags&MODS_AS_TEXT)==0) {
                g_local.flags |= MODS_AS_TEXT;
            } else {
                g_local.flags &= ~((uint8_t)MODS_AS_TEXT);
            }
            request_disp_refresh();
            break;
        case KC_TOGTEXT:
            if((g_local.flags&MORE_TEXT)==0) {
                g_local.flags |= MORE_TEXT;
            } else {
                g_local.flags &= ~((uint8_t)MORE_TEXT);
            }
            request_disp_refresh();
            break;
        case KC_L0:
            g_local.default_ls = _L0;
            persistent_default_layer_set(g_local.default_ls);
            request_disp_refresh();
            break;
        case KC_L1:
            g_local.default_ls = _L1;
            persistent_default_layer_set(g_local.default_ls);
            request_disp_refresh();
            break;
        case KC_L2:
            g_local.default_ls = _L2;
            persistent_default_layer_set(g_local.default_ls);
            request_disp_refresh();
            break;
        case KC_L3:
            g_local.default_ls = _L3;
            persistent_default_layer_set(g_local.default_ls);
            request_disp_refresh();
            break;
        case KC_L4:
            g_local.default_ls = _L4;
            persistent_default_layer_set(g_local.default_ls);
            request_disp_refresh();
            break;
        // chad3814 codes
        case KC_L5:
            g_local.default_ls = _L5;
            persistent_default_layer_set(g_local.default_ls);
            request_disp_refresh();
            break;
        case KC_SAVE:
            SEND_STRING(SS_DOWN(X_LEFT_CTRL) SS_DELAY(100) SS_TAP(X_X) SS_DELAY(100) SS_TAP(X_S) SS_DELAY(100) SS_UP(X_LEFT_CTRL));
            break;
        case KC_3FS: // 3-finger sulute
            SEND_STRING(SS_DOWN(X_LEFT_CTRL) SS_DELAY(100) SS_DOWN(X_LALT) SS_DELAY(100) SS_TAP(X_DELETE) SS_DELAY(100) SS_UP(X_LEFT_CTRL) SS_DELAY(100) SS_UP(X_LALT));
            break;
        case KC_SCRENCAP:
            SEND_STRING(SS_TAP(X_SYRQ));
            break;
        case KC_BASE:
            layer_clear();
            layer_on(g_local.default_ls);
            break;
        case KC_RIGHT_SHIFT:
        case KC_LEFT_SHIFT:
            request_disp_refresh();
            break;
        case KC_D1Q:
            g_local.contrast = FULL_BRIGHT/4;
            save_user_eeconf();
            break;
        case KC_D3Q:
            g_local.contrast = (FULL_BRIGHT/4)*3;
            save_user_eeconf();
            break;
        case KC_DHLF:
            g_local.contrast = FULL_BRIGHT/2;
            save_user_eeconf();
            break;
        case KC_DMAX:
            g_local.contrast = FULL_BRIGHT;
            save_user_eeconf();
            break;
        case KC_DMIN:
            g_local.contrast = 2;
            save_user_eeconf();
            break;
        case KC_DDIM:
            dec_brightness();
            break;
        case KC_DBRI:
            inc_brightness();
            break;
        /*[[[cog
            for lang in languages:
                cog.outl(f'case KC_{lang}: g_local.lang = {lang}; save_user_eeconf(); layer_off(_LL); break;')
            ]]]*/
        case KC_LANG_EN: g_local.lang = LANG_EN; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_DE: g_local.lang = LANG_DE; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_FR: g_local.lang = LANG_FR; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_ES: g_local.lang = LANG_ES; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_PT: g_local.lang = LANG_PT; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_IT: g_local.lang = LANG_IT; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_TR: g_local.lang = LANG_TR; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_KO: g_local.lang = LANG_KO; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_JA: g_local.lang = LANG_JA; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_AR: g_local.lang = LANG_AR; save_user_eeconf(); layer_off(_LL); break;
        case KC_LANG_GR: g_local.lang = LANG_GR; save_user_eeconf(); layer_off(_LL); break;
        //[[[end]]]
        case KC_F1:case KC_F2:case KC_F3:case KC_F4:case KC_F5:case KC_F6:
        case KC_F7:case KC_F8:case KC_F9:case KC_F10:case KC_F11:case KC_F12:
            layer_off(_LL);
            break;
        default:
            break;
        }
    }
    else {
        switch (keycode)
        {
        case KC_RIGHT_SHIFT:
        case KC_LEFT_SHIFT:
            request_disp_refresh();
            break;
        case KC_LANG:
            if (IS_LAYER_ON(_LL)) {
                g_local.lang = (g_local.lang + 1) % NUM_LANG;
                save_user_eeconf();
                layer_off(_LL);
            }
            else {
                layer_on(_LL);
            }
            break;
        case RGB_MOD:
        case RGB_RMOD:
            request_disp_refresh();
            break;
        default:
            break;
        }
    }

    //uprintf("Key 0x%04X, col/row: %u/%u, %s, time: %u, int: %d, cnt: %u disp#: %d 0x%02X%02X%02X%02X%02X\n",
     //   keycode, record->event.key.col, record->event.key.row, record->event.pressed ? "DN" : "UP",
     //   record->event.time, record->tap.interrupted ? 1 : 0, record->tap.count, disp_idx, bitmask[4], bitmask[3], bitmask[2], bitmask[1], bitmask[0]);

    update_performed();
};

void show_splash_screen(void) {
    clear_all_displays();
    display_message(1, 1, u"POLY", &FreeSansBold24pt7b);
    display_message(2, 1, u"KYBD", &FreeSansBold24pt7b);
}

void oled_on_off(bool on) {
    if (!on) {
        oled_off();
    } else {
        oled_on();
        uprintf("oled_on %s\n", is_usb_host_side() ? "Host" : "Bridge");
    }
}

void set_displays(uint8_t contrast, bool idle) {
    if(idle) {
        kdisp_idle(contrast);
    } else {
        select_all_displays();
        if(contrast==DISP_OFF) {
            kdisp_enable(false);
        } else {
            kdisp_enable(true);
            kdisp_set_contrast(contrast - 1);
        }
    }
}



//disable first keypress if the displays are turned off
bool display_wakeup(keyrecord_t* record) {
    bool accept_keypress = true;
    if ((g_local.contrast==DISP_OFF || (g_local.flags & DISP_IDLE)!=0) && record->event.pressed) {
        if(g_local.contrast==DISP_OFF && (g_local.flags&DEAD_KEY_ON_WAKEUP)!=0) {
            accept_keypress = timer_elapsed32(last_update) <= TURN_OFF_TIME;
        }
        poly_eeconf_t ee = load_user_eeconf();
        g_local.contrast = ee.brightness;
        g_local.flags &= ~((uint8_t)DISP_IDLE);
        g_local.flags |= STATUS_DISP_ON;
        update_performed();
        request_disp_refresh();
    }

    return accept_keypress;
}

void keyboard_post_init_user(void) {
    // Customise these values to desired behaviour
    debug_enable = true;
    debug_matrix = false;
    debug_keyboard = false;
    debug_mouse = false;

    //pointing_device_set_cpi(20000);
    pointing_device_set_cpi(650);
    //pimoroni_trackball_set_rgbw(0,0,255,100);
    g_local.default_ls = persistent_default_layer_get();
    layer_clear();
    layer_on(g_local.default_ls);

    //set these values, they will never change
    com = is_keyboard_master() ? USB_HOST : BRIDGE;
    side = is_keyboard_left() ? LEFT_SIDE : RIGHT_SIDE;

    //encoder pins
    setPinInputHigh(GP25);
    setPinInputHigh(GP29);

    //srand(halGetCounterValue());

    memset(&g_state, 0, sizeof(g_state));

    transaction_register_rpc(USER_SYNC_POLY_DATA, user_sync_poly_data_handler);
}

void keyboard_pre_init_user(void) {
    kdisp_hw_setup();
    kdisp_init(NUM_SHIFT_REGISTERS);
    peripherals_reset();
    kdisp_setup(true);

    select_all_displays();
    kdisp_scroll_vlines(47);
    kdisp_scroll_modehv(true, 3, 1);
    kdisp_scroll(false);

    poly_eeconf_t ee = load_user_eeconf();

    g_local.lang = ee.lang;
    g_local.default_ls = 0;
    g_local.contrast = ee.brightness;
    g_local.flags = STATUS_DISP_ON;
    memcpy(latin_ex, ee.latin_ex, sizeof(latin_ex));

    set_displays(g_local.contrast, false);
    g_local.last_latin_kc = 0;
    show_splash_screen();

    setPinInputHigh(I2C1_SDA_PIN);
}

void eeconfig_init_user(void) {
    uprint("Init EE config\n");
    poly_eeconf_t ee;
    ee.lang = g_lang_init;
    ee.brightness = ~FULL_BRIGHT;
    ee.unused = 0;
    memset(ee.latin_ex, 0, sizeof(ee.latin_ex));
    eeconfig_read_user_datablock(&ee);
}


void num_to_u16_string(char* buffer, uint8_t buffer_len, uint8_t value) {
    if(value<10) {
        snprintf((char*) buffer, buffer_len, "%d", value);
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = 0;
    } else if(value>99) {
        snprintf((char*) buffer, buffer_len, "%d %d %d", value/100, (value/10)%10, value%10);
        buffer[1] = 0;
        buffer[3] = 0;
        buffer[5] = 0;
        buffer[6] = 0;
        buffer[7] = 0;
    } else {
        snprintf((char*) buffer, buffer_len, "%d %d", value/10, value%10);
        buffer[1] = 0;
        buffer[3] = 0;
        buffer[4] = 0;
        buffer[5] = 0;
    }
}

void hex_to_u16_string(char* buffer, uint8_t buffer_len, uint8_t value) {
    if(value<16) {
        snprintf((char*) buffer, buffer_len, "%X", value);
        buffer[1] = 0;
        buffer[2] = 0;
        buffer[3] = 0;
    } else {
        snprintf((char*) buffer, buffer_len, "%X %X", value/16, value%16);
        buffer[1] = 0;
        buffer[3] = 0;
        buffer[4] = 0;
        buffer[5] = 0;
    }
}

void oled_update_buffer(void) {
    uint16_t buffer[32];

    kdisp_set_buffer(0);

    const GFXfont* displayFont[] = { &NotoSans_Regular11pt7b };
    const GFXfont* smallFont[] = { &NotoSans_Medium8pt7b };
    kdisp_write_gfx_text(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 0, 14, ICON_LAYER);
    hex_to_u16_string((char*) buffer, sizeof(buffer), get_highest_layer(layer_state));
    kdisp_write_gfx_text(displayFont, 1, 20, 14, buffer);
    if(side==UNDECIDED) {
        kdisp_write_gfx_text(displayFont, 1, 50, 14, u"Uknw");
    } else {
        kdisp_write_gfx_text(displayFont, 1, 38, 14, is_left_side() ? u"LEFT" : u"RIGHT");
    }

    kdisp_write_gfx_text(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 108, 16, g_state.led_state.num_lock ? ICON_NUMLOCK_ON : ICON_NUMLOCK_OFF);
    kdisp_write_gfx_text(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 108, 38, g_state.led_state.caps_lock ? ICON_CAPSLOCK_ON : ICON_CAPSLOCK_OFF);
    if(g_state.led_state.scroll_lock) {
        kdisp_write_gfx_text(ALL_FONTS, sizeof(ALL_FONTS) / sizeof(GFXfont*), 112, 54, ARROWS_DOWNSTOP);
    } else {
        kdisp_write_gfx_text(smallFont, 1, 112, 56, is_usb_host_side() ? u"H" : u"B");
    }



    if(is_right_side()) {
        kdisp_write_gfx_text(smallFont, 1, 0, 30, u"RGB");

        if(!rgb_matrix_is_enabled()) {
            kdisp_write_gfx_text(smallFont, 1, 34, 30, u"Off");
        } else {
            num_to_u16_string((char*) buffer, sizeof(buffer), rgb_matrix_get_mode());
            kdisp_write_gfx_text(smallFont, 1, 34, 30, buffer);
            kdisp_write_gfx_text(smallFont, 1, 58, 30, get_led_matrix_text(rgb_matrix_get_mode()));
            kdisp_write_gfx_text(smallFont, 1, 0, 44, u"HSV");
            hex_to_u16_string((char*) buffer, sizeof(buffer), rgb_matrix_get_hue());
            kdisp_write_gfx_text(smallFont, 1, 38, 44, buffer);
            hex_to_u16_string((char*) buffer, sizeof(buffer), rgb_matrix_get_sat());
            kdisp_write_gfx_text(smallFont, 1, 60, 44, buffer);
            hex_to_u16_string((char*) buffer, sizeof(buffer), rgb_matrix_get_val());
            kdisp_write_gfx_text(smallFont, 1, 82, 44, buffer);
            kdisp_write_gfx_text(smallFont, 1, 0, 58, u"Speed");
            num_to_u16_string((char*) buffer, sizeof(buffer), rgb_matrix_get_speed());
            kdisp_write_gfx_text(smallFont, 1, 58, 58, buffer);
        }
    } else {
        switch(g_local.default_ls) {
            case 0: kdisp_write_gfx_text(smallFont, 1, 0, 30, u"Qwerty"); break;
            case 1: kdisp_write_gfx_text(smallFont, 1, 0, 30, u"Qwerty Stag!"); break;
            case 2: kdisp_write_gfx_text(smallFont, 1, 0, 30, u"Colemak DH"); break;
            case 3: kdisp_write_gfx_text(smallFont, 1, 0, 30, u"Neo"); break;
            case 4: kdisp_write_gfx_text(smallFont, 1, 0, 30, u"Workman"); break;
            default: kdisp_write_gfx_text(smallFont, 1, 0, 30, u"Unknown"); break;
        }
        kdisp_write_gfx_text(smallFont, 1, 0, 44, u"Dsp*");
        num_to_u16_string((char*) buffer, sizeof(buffer), g_local.contrast);
        kdisp_write_gfx_text(smallFont, 1, 42, 44, buffer);
        uint8_t i=0;
        for(;i<(g_local.contrast/5);++i) {
            buffer[i] = 'l';
        }
        buffer[i] = 0;
        buffer[i+1] = 0;
        kdisp_write_gfx_text(smallFont, 1, 64, 44, buffer);

        kdisp_write_gfx_text(smallFont, 1, 0, 58, u"WPM");
        num_to_u16_string((char*) buffer, sizeof(buffer), get_current_wpm());
        kdisp_write_gfx_text(smallFont, 1, 44, 58, buffer);

        kdisp_write_gfx_text(smallFont, 1, 68, 58, u"L");
        num_to_u16_string((char*) buffer, sizeof(buffer), g_local.lang);
        kdisp_write_gfx_text(smallFont, 1, 84, 58, buffer);
    }
}

void oled_status_screen(void) {
     if( (g_local.flags&STATUS_DISP_ON) == 0) {
        oled_off();
        return;
    } else if( (g_local.flags&STATUS_DISP_ON) != 0) {
        oled_on();
    }

    oled_update_buffer();
    oled_clear();
    oled_write_raw((char *)get_scratch_buffer(), get_scratch_buffer_size());
}

void oled_render_logos(void) {
    if(is_left_side()) {
        oled_draw_poly();
        oled_scroll_right();
    } else {
        oled_draw_kybd();
        oled_scroll_left();
    }
}
bool oled_task_user(void) {
    if((g_local.flags&DISP_IDLE) != 0) {
        oled_render_logos();
    } else {
        oled_scroll_off();
        oled_status_screen();
    }
    return false;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) { /* First encoder */
        if (clockwise) {
            tap_code(KC_PGDN);
        } else {
            tap_code(KC_PGUP);
        }
    } else if (index == 1) { /* Second encoder */
        if (clockwise) {
            tap_code(KC_VOLD);
        } else {
            tap_code(KC_VOLU);
        }
    }
    return false;
}

void invert_display(uint8_t r, uint8_t c, bool state) {
    uint16_t keycode = keymaps[_BL][r][0];
    if (keycode == KC_NO) {
        c--;
    }

    r = r % MATRIX_ROWS_PER_SIDE;
    uint8_t disp_idx = LAYOUT_TO_INDEX(r, c);
    const uint8_t* bitmask = key_display[disp_idx].bitmask;
    sr_shift_out_buffer_latch(bitmask, sizeof(key_display->bitmask));

    if (disp_idx != 255) {
        kdisp_invert(state);
    }
}

// invert displays directly when pressed (no need to do split sync)
extern matrix_row_t matrix[MATRIX_ROWS];
static matrix_row_t last_matrix[MATRIX_ROWS_PER_SIDE];
void matrix_scan_user(void) {
    uint8_t first   = is_left_side() ? 0 : MATRIX_ROWS_PER_SIDE;
    bool    changed = false;
    for (uint8_t r = first; r < first + MATRIX_ROWS_PER_SIDE; r++) {
        if (last_matrix[r - first] != matrix[r]) {
            changed = true;
            for (uint8_t c = 0; c < MATRIX_COLS; c++) {
                bool old     = ((last_matrix[r - first] >> c) & 1) == 1;
                bool current = ((matrix[r] >> c) & 1) == 1;
                if (!old && current) {
                    invert_display(r,c,true);
                } else if (old && !current) {
                    invert_display(r,c,false);
                }
            }
        }
    }
    if (changed) {
        memcpy(last_matrix, &matrix[first], sizeof(last_matrix));
    }
}

void matrix_slave_scan_user(void) {
    matrix_scan_user();
}

oled_rotation_t oled_init_user(oled_rotation_t rotation){
    oled_off();
    oled_clear();
    oled_render();
    oled_scroll_set_speed(0);
    oled_render_logos();
    oled_on();
    return rotation;
}

void poly_suspend(void) {
    g_local.flags &= ~((uint8_t)STATUS_DISP_ON);
    g_local.flags &= ~((uint8_t)DISP_IDLE);
    g_local.contrast = DISP_OFF;
    last_update = -1;
}

void suspend_power_down_kb(void) {
    poly_suspend();
    rgb_matrix_disable_noeeprom();
    housekeeping_task_user();
    suspend_power_down_user();
}


void suspend_wakeup_init_kb(void) {
    g_local.flags |= STATUS_DISP_ON;
    g_local.flags &= ~((uint8_t)DISP_IDLE);
    poly_eeconf_t ee = load_user_eeconf();
    g_local.contrast = ee.brightness;
    last_update = 0;

    rgb_matrix_reload_from_eeprom();

    update_performed();
    housekeeping_task_user();
    suspend_wakeup_init_user();
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    const char * name = "P0.PolyKybd Split72";

    if(debug_enable && length < 33)
    {
        char cmdstring[33];
        memcpy(cmdstring, data, length);
        cmdstring[length] = 0; // Make sure string ends with 0
        dprintf("DEBUG: custom hid or via command, length=%d cmd=%s\n", length, cmdstring);
    }

    if(length>1 && (data[0] == /*via_command_id::*/id_custom_save || data[0] == 'P')) {
        switch(data[1]) {
            case /*via_channel_id::*/id_qmk_backlight_channel ... /*via_channel_id::*/id_qmk_led_matrix_channel:
                //used by VIA, so that will never occuer if used together with VIA
                //without VIA, lets handle it like '0'
            case '0': //id
                memset(data, 0, length);
                memcpy(data, name, strlen(name));
                break;
            case '1': //lang
                memset(data, 0, length);
                switch(g_local.lang) {
                    /*[[[cog
                    for lang in languages:
                        cog.outl(f'case {lang}: memcpy(data, "P1.{lang[5:]}", 5); break;')
                    ]]]*/
                    case LANG_EN: memcpy(data, "P1.EN", 5); break;
                    case LANG_DE: memcpy(data, "P1.DE", 5); break;
                    case LANG_FR: memcpy(data, "P1.FR", 5); break;
                    case LANG_ES: memcpy(data, "P1.ES", 5); break;
                    case LANG_PT: memcpy(data, "P1.PT", 5); break;
                    case LANG_IT: memcpy(data, "P1.IT", 5); break;
                    case LANG_TR: memcpy(data, "P1.TR", 5); break;
                    case LANG_KO: memcpy(data, "P1.KO", 5); break;
                    case LANG_JA: memcpy(data, "P1.JA", 5); break;
                    case LANG_AR: memcpy(data, "P1.AR", 5); break;
                    case LANG_GR: memcpy(data, "P1.GR", 5); break;
                    //[[[end]]]
                    default:
                        memcpy(data, "P1!", 3);
                        break;
                }
                break;
            case '2': //lang list
                memset(data, 0, length);
                /*[[[cog
                lang_list = "P2."
                for lang in languages:
                    lang_list += lang[5:]
                    if len(lang_list)>=(32-3-1):
                        cog.outl(f'memcpy(data, "{lang_list}", {len(lang_list)});')
                        cog.outl(f'raw_hid_send(data, length);')
                        cog.outl(f'memset(data, 0, length);')
                        lang_list = "P2."
                    elif lang != languages[-1]:
                        lang_list += ","
                cog.outl(f'memcpy(data, "{lang_list}", {len(lang_list)});')
                ]]]*/
                memcpy(data, "P2.EN,DE,FR,ES,PT,IT,TR,KO,JA", 29);
                raw_hid_send(data, length);
                memset(data, 0, length);
                memcpy(data, "P2.AR,GR", 8);
                //[[[end]]]
                break;
            case '3': //change language
                if(data[3]< NUM_LANG) {
                    g_local.lang = data[3];
                    data[2] = '.';
                    update_performed();
                } else {
                    memset(data, 0, length);
                    memcpy(data, "P3!", 3);
                }
                                break;
            default:
                //memcpy(response, "P??", 3);
                //response[1] = data[1];
                //raw_hid_send(response, length);
                break;
        }
        #ifndef VIA_ENABLE
            raw_hid_send(data, length);
        #endif
    }
}
#ifndef VIA_ENABLE
void raw_hid_receive(uint8_t *data, uint8_t length) {
    via_custom_value_command_kb(data, length);
}
#endif
