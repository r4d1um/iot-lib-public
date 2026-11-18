/*
 * shift595 - 74HC595 shift register chains (1..32 registers), C99.
 *
 * Keeps the wanted state of every output in RAM; shift595_update() shifts it out and latches
 * only when something changed. Pin p is output Q(p%8) of register p/8, register 0 being the one
 * whose SER input is wired to the microcontroller (QA = bit 0, QH = bit 7).
 * No I/O here: the caller passes a bus that clocks out one byte and pulses the latch
 * (Arduino adapter Shift595.h: shiftOut or hardware SPI). No malloc, no float.
 */
#ifndef SHIFT595_H
#define SHIFT595_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHIFT595_VERSION "1.0.0"
#define SHIFT595_MAX_REGS 32u

typedef struct {
    /* Clock out 8 bits MSB first (bit 7 first, so it ends up on QH). */
    void (*shift)(void *ctx, uint8_t byte);
    /* Pulse RCLK (ST_CP): copies the shift register to the outputs. */
    void (*latch)(void *ctx);
    void *ctx;
} shift595_bus;

typedef enum { SHIFT595_BAR = 0, SHIFT595_DOT = 1 } shift595_bar_mode;

typedef struct {
    shift595_bus bus;
    uint8_t      regs;    /* registers in the chain */
    uint8_t     *state;   /* regs bytes, state[0] = register 0 */
    uint8_t      dirty;   /* state differs from the outputs */
} shift595;

/* mem: regs bytes owned by the caller. All outputs off; the first update writes them. */
void shift595_init(shift595 *s, const shift595_bus *bus, uint8_t regs, uint8_t *mem);

void    shift595_set(shift595 *s, uint16_t pin, int on);   /* pins past the chain ignored */
int     shift595_get(const shift595 *s, uint16_t pin);
void    shift595_toggle(shift595 *s, uint16_t pin);
void    shift595_set_byte(shift595 *s, uint8_t reg, uint8_t value); /* bit i = Q(i) */
uint8_t shift595_get_byte(const shift595 *s, uint8_t reg);
void    shift595_clear(shift595 *s);                       /* all off */

/* LED bar over count pins from first: BAR lights round(value/max*count) pins, DOT only the
 * last of them (none for 0). max 0 counts as 1; value is clamped to max. */
void shift595_bar(shift595 *s, uint16_t first, uint16_t count, uint32_t value, uint32_t max,
                  shift595_bar_mode mode);

/* Shift out and latch if anything changed; returns 1 if it wrote. */
int  shift595_update(shift595 *s);
/* Shift out and latch now (e.g. after power-up glitches). */
void shift595_write(shift595 *s);

#ifdef __cplusplus
}
#endif

#endif
