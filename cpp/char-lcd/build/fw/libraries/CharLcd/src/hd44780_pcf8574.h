/* hd44780_pcf8574 - bus for the PCF8574 I2C backpack ("LCD1602/2004 I2C" modules).
 * The core part only encodes expander bytes; the caller sends them over I2C. */
#ifndef HD44780_PCF8574_H
#define HD44780_PCF8574_H

#include <stdint.h>
#include "hd44780.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Which expander bit drives which LCD pin. */
typedef struct {
    uint8_t rs, rw, en, bl;  /* bit numbers 0..7 */
    uint8_t d4;              /* bit of D4; D5..D7 follow on d4+1..d4+3 */
    uint8_t bl_active_low;   /* 1 if the backlight is on with the bit low */
} hd44780_pcf8574_pins;

/* The usual backpack: P0 RS, P1 RW, P2 E, P3 backlight, P4..P7 D4..D7. */
extern const hd44780_pcf8574_pins HD44780_PCF8574_DEFAULT;

/* Encode value (see hd44780_bus.send flags) into expander bytes: for each nibble one byte with
 * E high and one with E low. Returns the number of bytes in out (2 or 4). */
uint8_t hd44780_pcf8574_encode(const hd44780_pcf8574_pins *pins, uint8_t value, uint8_t flags,
                               int backlight, uint8_t out[4]);

/* Expander byte that only sets the backlight (all other pins low). */
uint8_t hd44780_pcf8574_idle(const hd44780_pcf8574_pins *pins, int backlight);

/* Ready bus: write(ctx, addr, bytes, n) sends n bytes to the expander in one I2C transfer and
 * returns 0 on ACK. Fill this, then pass hd44780_pcf8574_bus() to hd44780_init. */
typedef struct {
    int (*write)(void *ctx, uint8_t addr, const uint8_t *bytes, uint8_t n);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
    uint8_t addr;                    /* 7-bit address, usually 0x27 (PCF8574) or 0x3F (PCF8574A) */
    hd44780_pcf8574_pins pins;
    int backlight;
} hd44780_pcf8574;

hd44780_bus hd44780_pcf8574_bus(hd44780_pcf8574 *pcf);

/* Switch the backlight; takes effect at once. Returns the I2C result. */
int hd44780_pcf8574_backlight(hd44780_pcf8574 *pcf, int on);

#ifdef __cplusplus
}
#endif

#endif
