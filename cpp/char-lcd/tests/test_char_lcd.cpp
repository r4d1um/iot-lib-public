// Host tests for the hd44780 core against the fake controller (make test).
#include <cstdio>
#include <string>
#include "fake_hd44780.h"
#include "hd44780.h"
#include "hd44780_pcf8574.h"
#include "hd44780_screen.h"
#include "hd44780_text.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECK_EQ(a, b) do { std::string x_ = (a), y_ = (b); if (x_ != y_) { std::printf("FAIL %s:%d\n  got  [%s]\n  want [%s]\n", __FILE__, __LINE__, x_.c_str(), y_.c_str()); fails++; } } while (0)

static void test_init_and_print_20x4() {
    FakeLcd f;
    hd44780 l;
    hd44780_bus b = f.bus();
    CHECK(hd44780_init(&l, &b, 20, 4) == 0);
    CHECK(f.chip[0].four_bit && f.chip[0].two_line && f.chip[0].display && !f.chip[0].cursor);
    CHECK(f.delayed_us >= 50000 + 4100 + 1520);  // power-up, init, clear
    for (int r = 0; r < 4; r++) {
        hd44780_set_cursor(&l, 0, (uint8_t)r);
        char t[16];
        std::snprintf(t, sizeof t, "row %d", r);
        hd44780_print(&l, t);
    }
    CHECK_EQ(f.row(0, 20, 4), "row 0               ");
    CHECK_EQ(f.row(1, 20, 4), "row 1               ");
    CHECK_EQ(f.row(2, 20, 4), "row 2               ");
    CHECK_EQ(f.row(3, 20, 4), "row 3               ");
    // past the row end nothing spills into row 2 (DDRAM 0x14 follows row 0 directly)
    hd44780_set_cursor(&l, 15, 0);
    CHECK(hd44780_print(&l, "ABCDEFGH") == 5);
    CHECK_EQ(f.row(0, 20, 4), "row 0          ABCDE");
    CHECK_EQ(f.row(2, 20, 4), "row 2               ");
    // newline goes to the next row
    hd44780_clear(&l);
    hd44780_print(&l, "one\ntwo");
    CHECK_EQ(f.row(1, 20, 4), "two                 ");
}

static void test_16x4_and_16x2_offsets() {
    FakeLcd f;
    hd44780 l;
    hd44780_bus b = f.bus();
    hd44780_init(&l, &b, 16, 4);
    CHECK(hd44780_address(&l, 0, 2) == 0x10 && hd44780_address(&l, 3, 3) == 0x53);
    hd44780_set_cursor(&l, 2, 3);
    hd44780_print(&l, "x");
    CHECK_EQ(f.row(3, 16, 4), "  x             ");
    FakeLcd g;
    hd44780_bus gb = g.bus();
    hd44780_init(&l, &gb, 16, 2);
    hd44780_set_cursor(&l, 99, 9);  // clamped to (15,1)
    hd44780_write(&l, '#');
    CHECK_EQ(g.row(1, 16, 2), "               #");
}

static void test_40x4_two_chips() {
    FakeLcd f;
    hd44780 l;
    hd44780_bus b = f.bus();
    hd44780_init(&l, &b, 40, 4);
    CHECK(l.two_chips && f.chip[1].four_bit && f.chip[1].display);
    hd44780_set_cursor(&l, 39, 3);
    hd44780_write(&l, 'Z');
    hd44780_set_cursor(&l, 0, 0);
    hd44780_write(&l, 'A');
    CHECK(f.row(3, 40, 4)[39] == 'Z' && f.row(0, 40, 4)[0] == 'A');
    CHECK(f.row(1, 40, 4)[39] == ' ');
    uint8_t pat[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    hd44780_create_char(&l, 2, pat);
    CHECK(f.chip[0].cg[16 + 7] == 8 && f.chip[1].cg[16 + 7] == 8);
}

static void test_utf8_and_rom() {
    FakeLcd f;
    hd44780 l;
    hd44780_bus b = f.bus();
    hd44780_init(&l, &b, 20, 2);
    hd44780_print(&l, "23.5 \xC2\xB0" "C Gr\xC3\xBC\xC3\x9F" "e");  // 23.5 °C Grüße
    std::string r = f.row(0, 20, 2);
    CHECK(r.substr(0, 5) == "23.5 ");
    CHECK((uint8_t)r[5] == 0xDF && r[6] == 'C');
    CHECK((uint8_t)r[10] == 0xF5 && (uint8_t)r[11] == 0xE2 && r[12] == 'e');
    CHECK(hd44780_map(0x00C4, HD44780_ROM_A00) == 'A');      // Ä falls back
    CHECK(hd44780_map(0x00C4, HD44780_ROM_A02) == 0xC4);     // Ä on A02
    CHECK(hd44780_map('\\', HD44780_ROM_A00) == '/');
    CHECK(hd44780_map('\\', HD44780_ROM_A02) == '\\');
    CHECK(hd44780_map(0x4E2D, HD44780_ROM_A00) == '?');      // no glyph
    CHECK(hd44780_map(3, HD44780_ROM_A00) == 3);             // custom char
    uint32_t cp;
    const char bad[] = "\xC3";                               // cut-off sequence
    CHECK(hd44780_utf8_next(bad, bad + 1, &cp) == bad + 1 && cp == 0xFFFD);
    const char emoji[] = "\xF0\x9F\x98\x80";
    CHECK(hd44780_utf8_next(emoji, 0, &cp) == emoji + 4 && cp == 0x1F600);
    CHECK(hd44780_map(cp, HD44780_ROM_A00) == '?');
}

static void test_controls_and_cgram() {
    FakeLcd f;
    hd44780 l;
    hd44780_bus b = f.bus();
    hd44780_init(&l, &b, 16, 2);
    hd44780_set_cursor(&l, 4, 1);
    uint8_t heart[8] = {0, 10, 31, 31, 14, 4, 0, 0};
    hd44780_create_char(&l, 0, heart);
    hd44780_write(&l, 0);   // still at (4,1): create_char restored DDRAM
    CHECK(f.row(1, 16, 2)[4] == 0);
    for (int i = 0; i < 8; i++) CHECK(f.chip[0].cg[i] == heart[i]);
    hd44780_cursor(&l, 1);
    hd44780_blink(&l, 1);
    CHECK(f.chip[0].cursor && f.chip[0].blink && f.chip[0].display);
    hd44780_display(&l, 0);
    CHECK(!f.chip[0].display && f.chip[0].cursor);
    hd44780_clear(&l);
    hd44780_print(&l, "AB");
    hd44780_scroll(&l, 2);
    CHECK_EQ(f.row(0, 16, 2), "  AB            ");
    hd44780_scroll(&l, -2);
    hd44780_home(&l);
    CHECK_EQ(f.row(0, 16, 2), "AB              ");
}

static void test_pcf8574() {
    const hd44780_pcf8574_pins &p = HD44780_PCF8574_DEFAULT;
    uint8_t out[4];
    CHECK(hd44780_pcf8574_encode(&p, 0x41, HD44780_RS, 1, out) == 4);
    CHECK(out[0] == (0x40 | 0x08 | 0x04 | 0x01) && out[1] == (0x40 | 0x08 | 0x01));
    CHECK(out[2] == (0x10 | 0x08 | 0x04 | 0x01) && out[3] == (0x10 | 0x08 | 0x01));
    CHECK(hd44780_pcf8574_encode(&p, 0x30, HD44780_NIBBLE, 0, out) == 2 && out[0] == 0x34 && out[1] == 0x30);
    hd44780_pcf8574_pins low = p;
    low.bl_active_low = 1;
    CHECK(hd44780_pcf8574_idle(&low, 1) == 0 && hd44780_pcf8574_idle(&low, 0) == 0x08);

    // whole path: core -> PCF bus -> fake expander -> fake controller
    FakeLcd f;
    FakePcf x;
    x.lcd = &f;
    hd44780_pcf8574 pcf;
    pcf.write = FakePcf::write;
    pcf.delay_us = FakePcf::delay;
    pcf.ctx = &x;
    pcf.addr = 0x27;
    pcf.pins = HD44780_PCF8574_DEFAULT;
    pcf.backlight = 1;
    hd44780 l;
    hd44780_bus b = hd44780_pcf8574_bus(&pcf);
    CHECK(hd44780_init(&l, &b, 20, 4) == 0);
    hd44780_set_cursor(&l, 3, 2);
    hd44780_print(&l, "I2C ok");
    CHECK_EQ(f.row(2, 20, 4), "   I2C ok           ");
    CHECK(x.log.back() & 0x08);  // backlight stays on
    hd44780_pcf8574_backlight(&pcf, 0);
    CHECK(x.log.back() == 0x00);
    // wrong address: error reported once and kept
    pcf.addr = 0x3F;
    FakeLcd g;
    x.lcd = &g;
    CHECK(hd44780_init(&l, &b, 20, 4) == 2 && l.error == 2);
}

static void test_screen_buffer() {
    FakeLcd f;
    hd44780 l;
    hd44780_bus b = f.bus();
    hd44780_init(&l, &b, 20, 4);
    hd44780_screen s;
    static uint8_t mem[HD44780_SCREEN_MEM(20, 4)];
    hd44780_screen_init(&s, 20, 4, HD44780_ROM_A00, mem);
    hd44780_screen_assume_blank(&s);
    hd44780_screen_text(&s, 0, 0, 10, "Temp", HD44780_LEFT);
    hd44780_screen_text(&s, 10, 0, 10, "23.5 \xC2\xB0" "C", HD44780_RIGHT);
    hd44780_screen_text(&s, 0, 1, 20, "CENTER!", HD44780_CENTER);
    uint16_t n = hd44780_screen_flush(&s, &l);
    CHECK(n == 4 + 6 + 7);  // only non-blanks: "23.5 °C" has a blank, already on screen
    CHECK_EQ(f.row(0, 20, 4), "Temp         23.5 \xDF" "C");
    CHECK_EQ(f.row(1, 20, 4), "      CENTER!       ");
    // nothing changed: nothing sent
    int before = f.send_calls;
    CHECK(hd44780_screen_flush(&s, &l) == 0 && f.send_calls == before);
    // shorter value overwrites the old one; only changed cells go out
    hd44780_screen_text(&s, 10, 0, 10, "9.1 \xC2\xB0" "C", HD44780_RIGHT);
    CHECK(hd44780_screen_flush(&s, &l) == 3);  // "23.5" -> " 9.1": cells 13, 14, 16
    CHECK_EQ(f.row(0, 20, 4), "Temp          9.1 \xDF" "C");
    // one set_cursor per run of changes: runs 13..14 and 16 -> 2 commands + 3 data
    CHECK(f.send_calls - before == 5);
    // cut at the field
    CHECK(hd44780_screen_text(&s, 16, 3, 0, "LONGTEXT", HD44780_LEFT) == 4);
    hd44780_screen_flush(&s, &l);
    CHECK_EQ(f.row(3, 20, 4), "                LONG");
    // invalidate: next flush rewrites everything
    hd44780_screen_invalidate(&s);
    CHECK(hd44780_screen_flush(&s, &l) == 80);
}

static void test_bar() {
    hd44780_screen s;
    static uint8_t mem[HD44780_SCREEN_MEM(20, 4)];
    hd44780_screen_init(&s, 20, 4, HD44780_ROM_A00, mem);
    hd44780_screen_bar(&s, 0, 3, 20, 50, 100, 4);
    for (int x = 0; x < 10; x++) CHECK(hd44780_screen_get(&s, (uint8_t)x, 3) == 0xFF);
    CHECK(hd44780_screen_get(&s, 10, 3) == ' ');
    hd44780_screen_bar(&s, 0, 3, 4, 7, 20, 4);  // 7/20 of 20 dots = 7 -> 1 full + 2 dots
    CHECK(hd44780_screen_get(&s, 0, 3) == 0xFF && hd44780_screen_get(&s, 1, 3) == 5 && hd44780_screen_get(&s, 2, 3) == ' ');
    hd44780_screen_bar(&s, 0, 2, 5, 999, 0, 4);  // max 0 / value > max: full
    CHECK(hd44780_screen_get(&s, 4, 2) == 0xFF);
    const uint8_t *c = hd44780_screen_bar_chars(2);
    CHECK(c && c[0] == 0x1C && c[7] == 0 && !hd44780_screen_bar_chars(4));
}

int main() {
    test_init_and_print_20x4();
    test_16x4_and_16x2_offsets();
    test_40x4_two_chips();
    test_utf8_and_rom();
    test_controls_and_cgram();
    test_pcf8574();
    test_screen_buffer();
    test_bar();
    if (fails) { std::printf("%d failure(s)\n", fails); return 1; }
    std::printf("hd44780 %s: all tests passed\n", HD44780_VERSION);
    return 0;
}
