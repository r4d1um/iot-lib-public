/* shift595 - see shift595.h. */
#include "shift595.h"

void shift595_init(shift595 *s, const shift595_bus *bus, uint8_t regs, uint8_t *mem)
{
    if (regs < 1u) regs = 1u;
    if (regs > SHIFT595_MAX_REGS) regs = SHIFT595_MAX_REGS;
    s->bus = *bus;
    s->regs = regs;
    s->state = mem;
    shift595_clear(s);
}

void shift595_set(shift595 *s, uint16_t pin, int on)
{
    uint8_t reg = (uint8_t)(pin >> 3), bit = (uint8_t)(1u << (pin & 7u)), old;
    if (pin >= (uint16_t)s->regs * 8u) return;
    old = s->state[reg];
    s->state[reg] = on ? (uint8_t)(old | bit) : (uint8_t)(old & (uint8_t)~bit);
    if (s->state[reg] != old) s->dirty = 1;
}

int shift595_get(const shift595 *s, uint16_t pin)
{
    if (pin >= (uint16_t)s->regs * 8u) return 0;
    return (s->state[pin >> 3] >> (pin & 7u)) & 1;
}

void shift595_toggle(shift595 *s, uint16_t pin)
{
    shift595_set(s, pin, !shift595_get(s, pin));
}

void shift595_set_byte(shift595 *s, uint8_t reg, uint8_t value)
{
    if (reg >= s->regs || s->state[reg] == value) return;
    s->state[reg] = value;
    s->dirty = 1;
}

uint8_t shift595_get_byte(const shift595 *s, uint8_t reg)
{
    return reg < s->regs ? s->state[reg] : 0u;
}

void shift595_clear(shift595 *s)
{
    uint8_t i;
    for (i = 0; i < s->regs; i++) s->state[i] = 0;
    s->dirty = 1;
}

void shift595_bar(shift595 *s, uint16_t first, uint16_t count, uint32_t value, uint32_t max,
                  shift595_bar_mode mode)
{
    uint32_t lit;
    uint16_t i;
    if (max == 0u) max = 1u;
    if (value > max) value = max;
    lit = (uint32_t)(((uint64_t)value * count + max / 2u) / max);
    for (i = 0; i < count; i++) {
        int on = mode == SHIFT595_DOT ? (lit > 0u && i == lit - 1u) : (i < lit);
        shift595_set(s, (uint16_t)(first + i), on);
    }
}

void shift595_write(shift595 *s)
{
    uint8_t i = s->regs;
    while (i-- > 0u) s->bus.shift(s->bus.ctx, s->state[i]); /* last register first */
    s->bus.latch(s->bus.ctx);
    s->dirty = 0;
}

int shift595_update(shift595 *s)
{
    if (!s->dirty) return 0;
    shift595_write(s);
    return 1;
}
