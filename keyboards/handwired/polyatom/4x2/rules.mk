# Build Options
#   change yes to no to disable
#
BOOTMAGIC_ENABLE = no       # Virtual DIP switch configuration(+1000)
MOUSEKEY_ENABLE = no       # Mouse keys(+4700)
EXTRAKEY_ENABLE = no       # Audio control and System control(+450)
CONSOLE_ENABLE = yes        # Console for debug(+400)
COMMAND_ENABLE = no         # Commands for debug and configuration
DEBUG_ENABLE = yes
# Do not enable SLEEP_LED_ENABLE. it uses the same timer as BACKLIGHT_ENABLE
SLEEP_LED_ENABLE = no       # Breathing sleep LED during USB suspend
# if this doesn't work, see here: https://github.com/tmk/tmk_keyboard/wiki/FAQ#nkro-doesnt-work
NKRO_ENABLE = yes           # USB Nkey Rollover
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality on B7 by default
RGBLIGHT_ENABLE = no        # Enable keyboard RGB underglow
MIDI_ENABLE = no            # MIDI support (+2400 to 4200, depending on config)
UNICODE_ENABLE = no         # Unicode
BLUETOOTH_ENABLE = no       # Enable Bluetooth with the Adafruit EZ-Key HID
AUDIO_ENABLE = no           # Audio output on port C6
ENCODER_ENABLE = yes
DEFERRED_EXEC_ENABLE = yes

OLED_ENABLE = yes
OLED_DRIVER = SSD1306

RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = WS2812

QUANTUM_LIB_SRC += spi_master.c
SRC += base/disp_array.c base/shift_reg.c base/spi_helper.c

DEFAULT_FOLDER = handwired/polyatom/4x5/stm32f407
