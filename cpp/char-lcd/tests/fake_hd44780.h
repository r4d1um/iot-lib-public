// Fake HD44780 for host tests and screenshots: interprets the 4-bit protocol exactly like the
// controller (init sequence, commands, DDRAM/CGRAM, address counter, display shift), so tests
// check what a real display would show, not which bytes were sent. Two chips for 40x4.
// Also a fake PCF8574 that turns expander bytes back into nibbles on E's falling edge.
#ifndef FAKE_HD44780_H
#define FAKE_HD44780_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "hd44780.h"
#include "hd44780_pcf8574.h"

struct FakeChip {
    bool four_bit = false, have_hi = false, cgram = false, inc = true;
    bool display = false, cursor = false, blink = false, two_line = false;
    uint8_t hi = 0, ac = 0, shift = 0;
    uint8_t ddram[128];
    uint8_t cg[64];
    int commands = 0, data = 0, nibbles = 0;
    FakeChip() { std::memset(ddram, ' ', sizeof ddram); std::memset(cg, 0, sizeof cg); }

    void nibble(uint8_t n, bool rs) {
        nibbles++;
        if (!four_bit) {             // 8-bit mode: each nibble is a whole command (low bits 0)
            uint8_t v = (uint8_t)(n << 4);
            if (!rs && (v & 0xE0) == 0x20) four_bit = !(v & 0x10);
            return;
        }
        if (!have_hi) { hi = n; have_hi = true; return; }
        have_hi = false;
        byte((uint8_t)(hi << 4 | n), rs);
    }
    void byte(uint8_t v, bool rs) {
        if (rs) {
            data++;
            if (cgram) { cg[ac & 63] = v & 0x1F; ac = (uint8_t)((ac + 1) & 63); }
            else { ddram[ac & 127] = v; step(); }
            return;
        }
        commands++;
        if (v & 0x80) { cgram = false; ac = v & 0x7F; }
        else if (v & 0x40) { cgram = true; ac = v & 0x3F; }
        else if (v & 0x20) { two_line = v & 0x08; if (v & 0x10) four_bit = false; }
        else if (v & 0x10) { if (v & 0x08) shift = (uint8_t)(shift + ((v & 0x04) ? 1 : -1)); else step(); }
        else if (v & 0x08) { display = v & 4; cursor = v & 2; blink = v & 1; }
        else if (v & 0x04) { inc = v & 2; }
        else if (v & 0x02) { ac = 0; shift = 0; cgram = false; }
        else if (v & 0x01) { std::memset(ddram, ' ', sizeof ddram); ac = 0; shift = 0; cgram = false; inc = true; }
    }
    void step() { ac = (uint8_t)(inc ? ac + 1 : ac - 1); if (two_line) { if (ac == 0x28) ac = 0x40; if (ac == 0x68) ac = 0; } }
};

struct FakeLcd {
    FakeChip chip[2];
    uint32_t delayed_us = 0;
    int send_calls = 0;

    static int send(void *ctx, uint8_t v, uint8_t flags) {
        FakeLcd *f = static_cast<FakeLcd *>(ctx);
        FakeChip &c = f->chip[(flags & HD44780_CHIP2) ? 1 : 0];
        f->send_calls++;
        c.nibble(v >> 4, flags & HD44780_RS);
        if (!(flags & HD44780_NIBBLE)) c.nibble(v & 15, flags & HD44780_RS);
        return 0;
    }
    static void delay(void *ctx, uint32_t us) { static_cast<FakeLcd *>(ctx)->delayed_us += us; }
    hd44780_bus bus() { hd44780_bus b; b.send = send; b.delay_us = delay; b.ctx = this; return b; }

    // What row r of a cols x rows display shows (raw codes), honouring display shift.
    std::string row(int r, int cols, int rows) const {
        bool two = rows == 4 && cols > 20;
        const FakeChip &c = chip[two && r >= 2 ? 1 : 0];
        int line = r % 2, start = (!two && r >= 2) ? cols : 0;
        std::string s;
        for (int x = 0; x < cols; x++) {
            int p = ((start + x - (int8_t)c.shift) % 40 + 40) % 40;
            s += (char)c.ddram[line * 0x40 + p];
        }
        return s;
    }
};

// PCF8574 on the I2C side: bytes in, nibbles to the fake controller on E high->low.
struct FakePcf {
    FakeLcd *lcd;
    hd44780_pcf8574_pins pins = HD44780_PCF8574_DEFAULT;
    uint8_t last = 0, addr = 0x27;
    int transfers = 0, bytes = 0;
    std::vector<uint8_t> log;
    static int write(void *ctx, uint8_t addr, const uint8_t *b, uint8_t n) {
        FakePcf *p = static_cast<FakePcf *>(ctx);
        if (addr != p->addr) return 2;  // NACK
        p->transfers++;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t v = b[i];
            p->log.push_back(v);
            p->bytes++;
            bool e_was = p->last & (1 << p->pins.en), e_now = v & (1 << p->pins.en);
            if (e_was && !e_now) p->lcd->chip[0].nibble((uint8_t)((p->last >> p->pins.d4) & 15), p->last & (1 << p->pins.rs));
            p->last = v;
        }
        return 0;
    }
    static void delay(void *ctx, uint32_t us) { static_cast<FakePcf *>(ctx)->lcd->delayed_us += us; }
};

#endif
