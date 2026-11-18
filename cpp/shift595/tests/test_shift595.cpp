// Host tests for the shift595 core against a fake 74HC595 chain (make test).
#include <cstdio>
#include "fake_595.h"
#include "shift595.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void test_single() {
    Fake595 f(1);
    shift595 s;
    uint8_t mem[1];
    shift595_bus b = f.bus();
    shift595_init(&s, &b, 1, mem);
    CHECK(shift595_update(&s) == 1 && f.latches == 1);  // first update writes (all off)
    CHECK(shift595_update(&s) == 0 && f.latches == 1);  // nothing changed
    shift595_set(&s, 0, 1);
    shift595_set(&s, 7, 1);
    CHECK(f.out[0] == 0);                               // not latched yet
    CHECK(shift595_update(&s) == 1);
    CHECK(f.q(0) && f.q(7) && !f.q(3) && f.out[0] == 0x81);
    shift595_set(&s, 0, 1);                             // same value: stays clean
    CHECK(shift595_update(&s) == 0);
    shift595_toggle(&s, 0);
    shift595_update(&s);
    CHECK(!f.q(0) && shift595_get(&s, 7));
    shift595_set(&s, 8, 1);                             // past the chain: ignored
    CHECK(shift595_get(&s, 8) == 0 && shift595_update(&s) == 0);
}

static void test_chain_order() {
    Fake595 f(3);
    shift595 s;
    uint8_t mem[3];
    shift595_bus b = f.bus();
    shift595_init(&s, &b, 3, mem);
    shift595_set_byte(&s, 0, 0x01);
    shift595_set_byte(&s, 1, 0xA5);
    shift595_set(&s, 23, 1);                            // QH of register 2
    shift595_update(&s);
    CHECK(f.out[0] == 0x01 && f.out[1] == 0xA5 && f.out[2] == 0x80);
    CHECK(f.bytes == 3 && f.latches == 1);
    CHECK(shift595_get_byte(&s, 1) == 0xA5 && shift595_get_byte(&s, 9) == 0);
    shift595_clear(&s);
    shift595_update(&s);
    CHECK(f.out[0] == 0 && f.out[1] == 0 && f.out[2] == 0);
}

static void test_bar() {
    Fake595 f(2);
    shift595 s;
    uint8_t mem[2];
    shift595_bus b = f.bus();
    shift595_init(&s, &b, 2, mem);
    shift595_bar(&s, 0, 8, 50, 100, SHIFT595_BAR);
    shift595_update(&s);
    CHECK(f.out[0] == 0x0F);
    shift595_bar(&s, 0, 8, 100, 100, SHIFT595_BAR);
    shift595_update(&s);
    CHECK(f.out[0] == 0xFF);
    shift595_bar(&s, 0, 8, 0, 100, SHIFT595_BAR);
    shift595_update(&s);
    CHECK(f.out[0] == 0x00);
    shift595_bar(&s, 4, 10, 6, 10, SHIFT595_DOT);       // across both registers: 6th of 10 = pin 9
    shift595_update(&s);
    CHECK(f.out[0] == 0x00 && f.out[1] == 0x02);
    shift595_bar(&s, 0, 8, 7, 0, SHIFT595_BAR);         // max 0, value > max: full
    shift595_update(&s);
    CHECK(f.out[0] == 0xFF);
    shift595_bar(&s, 0, 16, 300, 300, SHIFT595_BAR);
    CHECK(shift595_get_byte(&s, 1) == 0xFF);
}

static void test_clamp() {
    Fake595 f(1);
    shift595 s;
    uint8_t mem[SHIFT595_MAX_REGS];
    shift595_bus b = f.bus();
    shift595_init(&s, &b, 0, mem);
    CHECK(s.regs == 1);
    shift595_init(&s, &b, 200, mem);
    CHECK(s.regs == SHIFT595_MAX_REGS);
}

int main() {
    test_single();
    test_chain_order();
    test_bar();
    test_clamp();
    if (fails) { std::printf("%d failure(s)\n", fails); return 1; }
    std::printf("shift595 %s: all tests passed\n", SHIFT595_VERSION);
    return 0;
}
