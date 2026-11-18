// Sample screens for README/PR screenshots: drawn with the real core into the fake controller
// (tests/fake_hd44780.h), printed as "name cols rows hex..." lines for tools/preview.
#include <cstdio>
#include "../../tests/fake_hd44780.h"
#include "hd44780.h"
#include "hd44780_screen.h"

static void dump(const char *name, FakeLcd &f, int cols, int rows) {
    std::printf("%s %d %d", name, cols, rows);
    for (int r = 0; r < rows; r++)
        for (unsigned char c : f.row(r, cols, rows)) std::printf(" %02x", c);
    for (int i = 0; i < 64; i++) std::printf(" %02x", f.chip[0].cg[i]);  // CGRAM after the cells
    std::printf("\n");
}

int main() {
    {   // 20x4: screen buffer with fields and a bar
        FakeLcd f; hd44780 l; hd44780_bus b = f.bus();
        hd44780_init(&l, &b, 20, 4);
        static uint8_t mem[HD44780_SCREEN_MEM(20, 4)];
        hd44780_screen s; hd44780_screen_init(&s, 20, 4, HD44780_ROM_A00, mem);
        for (uint8_t i = 0; i < 4; i++) hd44780_create_char(&l, (uint8_t)(4 + i), hd44780_screen_bar_chars(i));
        hd44780_screen_text(&s, 0, 0, 20, "Darkroom", HD44780_CENTER);
        hd44780_screen_text(&s, 0, 1, 6, "Bath", HD44780_LEFT);
        hd44780_screen_text(&s, 6, 1, 14, "20.4 \xC2\xB0" "C", HD44780_RIGHT);
        hd44780_screen_text(&s, 0, 2, 6, "Time", HD44780_LEFT);
        hd44780_screen_text(&s, 6, 2, 14, "07:42", HD44780_RIGHT);
        hd44780_screen_bar(&s, 0, 3, 20, 37, 60, 4);
        hd44780_screen_flush(&s, &l);
        dump("20x4-dashboard", f, 20, 4);
    }
    {   // 20x4: print() with UTF-8
        FakeLcd f; hd44780 l; hd44780_bus b = f.bus();
        hd44780_init(&l, &b, 20, 4);
        hd44780_print(&l, "Hello, world!\nGr\xC3\xBC\xC3\x9F" "e & Tsch\xC3\xBC" "ss\n23.5 \xC2\xB0" "C  12 \xC2\xB5" "s\n\xCE\xA9 \xCF\x80 \xE2\x86\x92 \xE2\x86\x90 \xE2\x88\x9A \xC3\xB7");
        dump("20x4-utf8", f, 20, 4);
    }
    {   // 16x2 with a custom character
        FakeLcd f; hd44780 l; hd44780_bus b = f.bus();
        hd44780_init(&l, &b, 16, 2);
        uint8_t heart[8] = {0, 10, 31, 31, 14, 4, 0, 0};
        hd44780_create_char(&l, 0, heart);
        hd44780_print(&l, "CharLcd 1.0.0");
        hd44780_set_cursor(&l, 0, 1);
        hd44780_print_n(&l, "I \0 HD44780", 11);
        dump("16x2-custom", f, 16, 2);
    }
    return 0;
}
