/* hd44780_595 - bus for an HD44780 LCD behind one 74HC595 shift register: 3 GPIOs (data, clock,
 * latch) instead of 6, no I2C. Same pin map type as the PCF8574 backpack; default wiring:
 *   QA RS, QB free (LCD RW to GND), QC E, QD backlight (through a transistor), QE..QH D4..D7.
 * The core encodes the bytes; the caller shifts each byte out and latches it. */
#ifndef HD44780_595_H
#define HD44780_595_H

#include <stdint.h>
#include "hd44780.h"
#include "hd44780_pcf8574.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const hd44780_expander_pins HD44780_595_DEFAULT;

typedef struct {
    /* Shift byte out MSB first (bit 7 -> QH) and pulse the latch, so it appears on QA..QH. */
    void (*write)(void *ctx, uint8_t byte);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
    hd44780_expander_pins pins;
    int backlight;
    uint8_t last;   /* byte on the outputs now (set by the bus) */
} hd44780_595;

hd44780_bus hd44780_595_bus(hd44780_595 *sr);

/* Switch the backlight output at once. */
void hd44780_595_backlight(hd44780_595 *sr, int on);

#ifdef __cplusplus
}
#endif

#endif
