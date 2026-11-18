/*
 * hd44780 - driver core for HD44780-compatible character LCDs (8x1 ... 40x4), C99.
 *
 * The core knows the controller's protocol (4-bit init, commands, timing, DDRAM row offsets,
 * CGRAM, 40x4 with two controllers) but no hardware: the caller passes a bus with two callbacks.
 * Ready-made buses: the PCF8574 I2C backpack (hd44780_pcf8574.h, encodes the expander bytes) and,
 * in the Arduino adapter CharLcd.h, 4-bit parallel GPIO. No malloc, no float.
 *
 * Text: hd44780_print() takes UTF-8 and maps it to the character ROM (hd44780_text.h), so
 * "23.5 °C" or "Grüße" come out right on the common A00 ROM. hd44780_write() sends raw codes.
 */
#ifndef HD44780_H
#define HD44780_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HD44780_VERSION "1.0.0"

/* flags for hd44780_bus.send */
#define HD44780_RS      0x01u /* data register (0 = command) */
#define HD44780_NIBBLE  0x02u /* send only the high nibble of value (init sequence) */
#define HD44780_CHIP2   0x04u /* second controller (rows 2..3 of a 40x4 display) */

/* Character ROMs: A00 = Japanese (katakana, most cheap modules), A02 = European. */
typedef enum { HD44780_ROM_A00 = 0, HD44780_ROM_A02 = 1 } hd44780_rom;

typedef struct {
    /* Write value to the controller in 4-bit mode: high nibble, then low nibble (only the high
     * nibble with HD44780_NIBBLE), each latched with an E pulse. flags: HD44780_RS/_NIBBLE/_CHIP2.
     * Returns 0 on success, non-zero on a bus error (e.g. no I2C ACK). */
    int (*send)(void *ctx, uint8_t value, uint8_t flags);
    /* Wait at least us microseconds. */
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} hd44780_bus;

typedef struct {
    hd44780_bus bus;
    uint8_t     cols, rows;
    uint8_t     row_offset[4]; /* DDRAM address of each row's first cell */
    uint8_t     two_chips;     /* 40x4: rows 2..3 on a second controller (own E line) */
    uint8_t     control;       /* display/cursor/blink bits of command 0x08 */
    uint8_t     entry;         /* entry mode bits of command 0x04 */
    uint8_t     col, row;      /* where the next character goes (for line wrap) */
    hd44780_rom rom;
    int         error;         /* first bus error since init, 0 = none */
} hd44780;

/* Initialise the controller(s) in 4-bit mode for a cols x rows display (1..40 x 1..4):
 * function set, display on (no cursor), clear, left-to-right. Waits the 50 ms power-up time
 * itself. Returns 0, or the bus error. */
int hd44780_init(hd44780 *lcd, const hd44780_bus *bus, uint8_t cols, uint8_t rows);

void hd44780_clear(hd44780 *lcd);                     /* blank, cursor home (1.6 ms) */
void hd44780_home(hd44780 *lcd);                      /* cursor home, undo shift (1.6 ms) */
void hd44780_set_cursor(hd44780 *lcd, uint8_t col, uint8_t row); /* clamped to the display */
void hd44780_write(hd44780 *lcd, uint8_t code);       /* one raw ROM/CGRAM code */
void hd44780_write_n(hd44780 *lcd, const uint8_t *codes, size_t n);
/* UTF-8 text at the cursor, mapped to the ROM; '\n' goes to the next row's start.
 * Characters past the row end are dropped (no wrap into another row's DDRAM). Returns cells written. */
size_t hd44780_print(hd44780 *lcd, const char *utf8);
size_t hd44780_print_n(hd44780 *lcd, const char *utf8, size_t len); /* same, len bytes; \0 = custom char 0 */

/* Custom character: slot 0..7, 8 rows of 5 bits (bit 4 = left). Cursor goes back to (col,row). */
void hd44780_create_char(hd44780 *lcd, uint8_t slot, const uint8_t pattern[8]);

void hd44780_display(hd44780 *lcd, int on);           /* contents visible (kept in DDRAM) */
void hd44780_cursor(hd44780 *lcd, int on);            /* underline cursor */
void hd44780_blink(hd44780 *lcd, int on);             /* blinking block cursor */
void hd44780_scroll(hd44780 *lcd, int8_t cells);      /* shift the whole display, + = right */
void hd44780_command(hd44780 *lcd, uint8_t cmd);      /* raw command, to all controllers */

/* DDRAM address of (col,row) - exposed for tests and custom buses. */
uint8_t hd44780_address(const hd44780 *lcd, uint8_t col, uint8_t row);

#ifdef __cplusplus
}
#endif

#endif
