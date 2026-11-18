// Fake 74HC595 chain for host tests: bits enter register 0's shift stage on each clock, ripple
// out of QH' into the next register, and appear on the outputs only on latch - like the chip.
#ifndef FAKE_595_H
#define FAKE_595_H

#include <cstdint>
#include <vector>
#include "shift595.h"

struct Fake595 {
    std::vector<uint8_t> shiftreg, out;  // per register; bit i = Q(i)
    int latches = 0, bytes = 0;
    explicit Fake595(int regs) : shiftreg(regs, 0), out(regs, 0) {}

    void clock_bit(int bit) {  // one SRCLK edge: QA <- SER, QH -> next register's QA
        for (size_t r = 0; r < shiftreg.size(); r++) {
            int carry = (shiftreg[r] >> 7) & 1;
            shiftreg[r] = (uint8_t)((shiftreg[r] << 1) | bit);
            bit = carry;
        }
    }
    static void shift(void *ctx, uint8_t b) {  // MSB first, like shiftOut(MSBFIRST) / SPI mode 0
        Fake595 *f = static_cast<Fake595 *>(ctx);
        for (int i = 7; i >= 0; i--) f->clock_bit((b >> i) & 1);
        f->bytes++;
    }
    static void latch(void *ctx) {
        Fake595 *f = static_cast<Fake595 *>(ctx);
        f->out = f->shiftreg;
        f->latches++;
    }
    shift595_bus bus() { shift595_bus b = {shift, latch, this}; return b; }
    bool q(int pin) const { return (out[pin / 8] >> (pin % 8)) & 1; }
};

#endif
