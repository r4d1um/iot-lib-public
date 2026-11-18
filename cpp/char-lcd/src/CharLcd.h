// CharLcd - Arduino driver for HD44780 character LCDs (16x2, 20x4, ... 40x4), over a PCF8574
// I2C backpack or 4-bit parallel pins. The protocol, UTF-8 mapping and screen buffer are the
// precompiled hd44780 core; this adapter only moves bits (Wire / digitalWrite).
//
//   CharLcd lcd(0x27, 20, 4);          // I2C backpack; address 0 = find it (0x20..0x27, 0x38..0x3F)
//   CharLcd lcd(12, 11, 5, 4, 3, 2, 16, 2);  // parallel: RS, E, D4..D7, cols, rows (RW to GND)
//   CharLcd lcd(CharLcd::Shift595Pins{5, 6, 7}, 20, 4);  // one 74HC595: data, clock, latch
//
//   lcd.begin();  lcd.print("23.5 °C");      // print() takes UTF-8 (ä ö ü ß ° µ Ω ...)
//   lcd.write(0xFF);                         // write(byte) sends a raw ROM / CGRAM code
//
// Screen buffer (no flicker, only changed cells go over the bus):
//   lcd.text(0, 0, 10, "Temp");  lcd.text(10, 0, 10, "23.5 °C", HD44780_RIGHT);  lcd.refresh();
//
// Also fits templates that expect a LiquidCrystal-style type (createChar, clear, setCursor, write).
#ifndef CHAR_LCD_H
#define CHAR_LCD_H

#include <Arduino.h>
#include <Wire.h>
#include "hd44780.h"
#include "hd44780_text.h"
#include "hd44780_pcf8574.h"
#include "hd44780_595.h"
#include "hd44780_screen.h"

class CharLcd : public Print {
public:
    static const uint8_t NO_PIN = 255;
    // One 74HC595 between the board and the LCD: QA RS, QC E, QD backlight, QE..QH D4..D7.
    struct Shift595Pins {
        uint8_t data, clock, latch;
        Shift595Pins(uint8_t d, uint8_t c, uint8_t l) : data(d), clock(c), latch(l) {}
    };

    CharLcd(uint8_t i2cAddr, uint8_t cols, uint8_t rows, TwoWire &wire = Wire);
    CharLcd(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
            uint8_t cols, uint8_t rows, uint8_t en2 = NO_PIN);
    CharLcd(Shift595Pins pins, uint8_t cols, uint8_t rows);

    // Starts Wire (I2C) or sets the pins, initialises the display. false: no backpack answers.
    bool begin();
    bool ok() const { return lcd_.error == 0; }
    uint8_t address() const { return pcf_.addr; }   // I2C address in use (0 for parallel)
    uint8_t cols() const { return lcd_.cols; }
    uint8_t rows() const { return lcd_.rows; }

    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void createChar(uint8_t slot, const uint8_t pattern[8]);
    void createChar(uint8_t slot, uint8_t pattern[8]) { createChar(slot, (const uint8_t *)pattern); }
    void display() { hd44780_display(&lcd_, 1); }
    void noDisplay() { hd44780_display(&lcd_, 0); }
    void cursor() { hd44780_cursor(&lcd_, 1); }
    void noCursor() { hd44780_cursor(&lcd_, 0); }
    void blink() { hd44780_blink(&lcd_, 1); }
    void noBlink() { hd44780_blink(&lcd_, 0); }
    void scrollDisplayLeft() { hd44780_scroll(&lcd_, -1); }
    void scrollDisplayRight() { hd44780_scroll(&lcd_, 1); }
    void backlight(bool on = true);
    void noBacklight() { backlight(false); }
    // Other backpack / 595 wiring than RS 0, RW 1, E 2, BL 3, D4..D7 4..7; call before begin().
    void setBackpackPins(const hd44780_expander_pins &pins) { pcf_.pins = pins; sr_.pins = pins; }
    void setRom(hd44780_rom rom) { lcd_.rom = rom; screen_.rom = rom; }  // HD44780_ROM_A00 (default) / _A02

    size_t write(uint8_t code) override;                       // raw code
    size_t write(const uint8_t *buf, size_t n) override;       // UTF-8 text
    using Print::write;

    // Screen buffer: draw, then refresh() (or Print's flush()) sends only what changed.
    uint8_t text(uint8_t col, uint8_t row, uint8_t width, const char *utf8, hd44780_align align = HD44780_LEFT) {
        return hd44780_screen_text(&screen_, col, row, width, utf8, align);
    }
    void put(uint8_t col, uint8_t row, uint8_t code) { hd44780_screen_put(&screen_, col, row, code); }
    void bar(uint8_t col, uint8_t row, uint8_t width, uint32_t value, uint32_t max, uint8_t cgramBase = 4);
    void clearScreen() { hd44780_screen_clear(&screen_); }
    uint16_t refresh() { return hd44780_screen_flush(&screen_, &lcd_); }  // returns cells written
    void flush() override { refresh(); }
    // Call after drawing directly (print/setCursor/another library) before the next refresh().
    void invalidate() { hd44780_screen_invalidate(&screen_); }

    hd44780 &core() { return lcd_; }
    hd44780_screen &screen() { return screen_; }

private:
    static int parSend(void *ctx, uint8_t value, uint8_t flags);
    static void delayUs(void *ctx, uint32_t us);
    static int i2cWrite(void *ctx, uint8_t addr, const uint8_t *bytes, uint8_t n);
    static void srWrite(void *ctx, uint8_t byte);
    void parNibble(uint8_t nib, uint8_t en);

    hd44780 lcd_;
    hd44780_screen screen_;
    hd44780_pcf8574 pcf_;
    hd44780_595 sr_;
    bool shift_;
    TwoWire *wire_;
    uint8_t cols_, rows_;
    uint8_t rs_, en_, en2_, d_[4];
    uint8_t *mem_;  // screen buffer, sized for this display (20x4: 210 bytes)
    bool barChars_;
};

#endif
