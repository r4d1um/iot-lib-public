/* hd44780_595 - see hd44780_595.h. */
#include "hd44780_595.h"

const hd44780_expander_pins HD44780_595_DEFAULT = { 0, 1, 2, 3, 4, 0 };

/* Every byte is a full shift + latch (8 clocks), far longer than E's 450 ns minimum pulse even on
 * a 240 MHz ESP32, so E high and E low can follow each other directly. 40x4 needs a second E line,
 * which one 595 bus does not have: HD44780_CHIP2 writes are ignored. */
static int sr_send(void *ctx, uint8_t value, uint8_t flags)
{
    hd44780_595 *sr = (hd44780_595 *)ctx;
    uint8_t b[5], n, i;
    if (flags & HD44780_CHIP2) return 0;
    n = hd44780_pcf8574_encode(&sr->pins, value, flags, sr->backlight, sr->last, b);
    for (i = 0; i < n; i++) sr->write(sr->ctx, b[i]);
    sr->last = b[n - 1u];
    return 0;
}

static void sr_delay(void *ctx, uint32_t us)
{
    hd44780_595 *sr = (hd44780_595 *)ctx;
    sr->delay_us(sr->ctx, us);
}

hd44780_bus hd44780_595_bus(hd44780_595 *sr)
{
    hd44780_bus bus;
    bus.send = sr_send;
    bus.delay_us = sr_delay;
    bus.ctx = sr;
    return bus;
}

void hd44780_595_backlight(hd44780_595 *sr, int on)
{
    sr->backlight = on;
    sr->last = hd44780_pcf8574_idle(&sr->pins, on);
    sr->write(sr->ctx, sr->last);
}
