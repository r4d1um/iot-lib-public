/* hd44780_pcf8574 - see hd44780_pcf8574.h. */
#include "hd44780_pcf8574.h"

const hd44780_pcf8574_pins HD44780_PCF8574_DEFAULT = { 0, 1, 2, 3, 4, 0 };

uint8_t hd44780_pcf8574_idle(const hd44780_pcf8574_pins *p, int backlight)
{
    int bl = backlight ? !p->bl_active_low : p->bl_active_low;
    return (uint8_t)(bl ? (1u << p->bl) : 0u);
}

static uint8_t nibble_byte(const hd44780_pcf8574_pins *p, uint8_t nib, int rs, int backlight)
{
    uint8_t b = hd44780_pcf8574_idle(p, backlight);
    uint8_t i;
    if (rs) b |= (uint8_t)(1u << p->rs);
    for (i = 0; i < 4u; i++)
        if (nib & (1u << i)) b |= (uint8_t)(1u << (p->d4 + i));
    return b;
}

uint8_t hd44780_pcf8574_encode(const hd44780_pcf8574_pins *p, uint8_t value, uint8_t flags,
                               int backlight, uint8_t out[4])
{
    uint8_t en = (uint8_t)(1u << p->en);
    int rs = (flags & HD44780_RS) != 0;
    uint8_t hi = nibble_byte(p, (uint8_t)(value >> 4), rs, backlight);
    out[0] = (uint8_t)(hi | en);
    out[1] = hi;
    if (flags & HD44780_NIBBLE) return 2;
    {
        uint8_t lo = nibble_byte(p, (uint8_t)(value & 0x0Fu), rs, backlight);
        out[2] = (uint8_t)(lo | en);
        out[3] = lo;
    }
    return 4;
}

/* At 100 kHz each expander byte takes ~90 us, longer than E's 450 ns minimum pulse, so the
 * E-high / E-low bytes can go in one transfer. The 40x4 second controller needs its own E line,
 * which the usual backpack does not have: HD44780_CHIP2 writes are ignored here. */
static int pcf_send(void *ctx, uint8_t value, uint8_t flags)
{
    hd44780_pcf8574 *pcf = (hd44780_pcf8574 *)ctx;
    uint8_t b[4], n;
    if (flags & HD44780_CHIP2) return 0;
    n = hd44780_pcf8574_encode(&pcf->pins, value, flags, pcf->backlight, b);
    return pcf->write(pcf->ctx, pcf->addr, b, n);
}

static void pcf_delay(void *ctx, uint32_t us)
{
    hd44780_pcf8574 *pcf = (hd44780_pcf8574 *)ctx;
    pcf->delay_us(pcf->ctx, us);
}

hd44780_bus hd44780_pcf8574_bus(hd44780_pcf8574 *pcf)
{
    hd44780_bus bus;
    bus.send = pcf_send;
    bus.delay_us = pcf_delay;
    bus.ctx = pcf;
    return bus;
}

int hd44780_pcf8574_backlight(hd44780_pcf8574 *pcf, int on)
{
    uint8_t b;
    pcf->backlight = on;
    b = hd44780_pcf8574_idle(&pcf->pins, on);
    return pcf->write(pcf->ctx, pcf->addr, &b, 1);
}
