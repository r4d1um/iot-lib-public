// CharLcd - Arduino adapter, see CharLcd.h. Only I/O lives here; the logic is the hd44780 core.
#include "CharLcd.h"
#include <stdlib.h>

// The screen buffer is allocated once, sized for the display (no 40x4 worst case on small AVRs).
static uint8_t *screenMem(uint8_t &cols, uint8_t &rows) {
    if (cols < 1) cols = 1;
    if (cols > 40) cols = 40;
    if (rows < 1) rows = 1;
    if (rows > 4) rows = 4;
    return (uint8_t *)malloc(HD44780_SCREEN_MEM(cols, rows));
}

CharLcd::CharLcd(uint8_t i2cAddr, uint8_t cols, uint8_t rows, TwoWire &wire)
    : wire_(&wire), cols_(cols), rows_(rows), rs_(NO_PIN), en_(NO_PIN), en2_(NO_PIN), barChars_(false) {
    pcf_.write = i2cWrite;
    pcf_.delay_us = delayUs;
    pcf_.ctx = this;
    pcf_.addr = i2cAddr;
    pcf_.pins = HD44780_PCF8574_DEFAULT;
    pcf_.backlight = 1;
    lcd_.error = 0;
    mem_ = screenMem(cols_, rows_);
    hd44780_screen_init(&screen_, cols_, rows_, HD44780_ROM_A00, mem_);
}

CharLcd::CharLcd(uint8_t rs, uint8_t en, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7,
                 uint8_t cols, uint8_t rows, uint8_t en2)
    : wire_(nullptr), cols_(cols), rows_(rows), rs_(rs), en_(en), en2_(en2), barChars_(false) {
    d_[0] = d4; d_[1] = d5; d_[2] = d6; d_[3] = d7;
    pcf_.addr = 0;
    lcd_.error = 0;
    mem_ = screenMem(cols_, rows_);
    hd44780_screen_init(&screen_, cols_, rows_, HD44780_ROM_A00, mem_);
}

int CharLcd::i2cWrite(void *ctx, uint8_t addr, const uint8_t *bytes, uint8_t n) {
    CharLcd *self = static_cast<CharLcd *>(ctx);
    self->wire_->beginTransmission(addr);
    self->wire_->write(bytes, n);
    return self->wire_->endTransmission();
}

void CharLcd::delayUs(void *, uint32_t us) {
    if (us >= 1000) delay(us / 1000);
    delayMicroseconds((unsigned int)(us % 1000));
}

void CharLcd::parNibble(uint8_t nib, uint8_t en) {
    for (uint8_t i = 0; i < 4; i++) digitalWrite(d_[i], (nib >> i) & 1);
    digitalWrite(en, HIGH);
    delayMicroseconds(1);  // E pulse >= 450 ns
    digitalWrite(en, LOW);
}

int CharLcd::parSend(void *ctx, uint8_t value, uint8_t flags) {
    CharLcd *self = static_cast<CharLcd *>(ctx);
    uint8_t en = (flags & HD44780_CHIP2) ? self->en2_ : self->en_;
    if (en == NO_PIN) return 0;
    digitalWrite(self->rs_, (flags & HD44780_RS) ? HIGH : LOW);
    self->parNibble(value >> 4, en);
    if (!(flags & HD44780_NIBBLE)) self->parNibble(value & 0x0F, en);
    return 0;
}

bool CharLcd::begin() {
    hd44780_bus bus;
    if (wire_) {
        wire_->begin();
        if (pcf_.addr == 0) {  // find the backpack: PCF8574 0x20..0x27, PCF8574A 0x38..0x3F
            static const uint8_t base[2] = {0x27, 0x3F};
            for (uint8_t b = 0; b < 2 && pcf_.addr == 0; b++)
                for (uint8_t i = 0; i < 8; i++) {
                    wire_->beginTransmission((uint8_t)(base[b] - i));
                    if (wire_->endTransmission() == 0) { pcf_.addr = (uint8_t)(base[b] - i); break; }
                }
            if (pcf_.addr == 0) { lcd_.error = -1; return false; }
        }
        bus = hd44780_pcf8574_bus(&pcf_);
    } else {
        pinMode(rs_, OUTPUT);
        pinMode(en_, OUTPUT);
        digitalWrite(en_, LOW);
        if (en2_ != NO_PIN) { pinMode(en2_, OUTPUT); digitalWrite(en2_, LOW); }
        for (uint8_t i = 0; i < 4; i++) pinMode(d_[i], OUTPUT);
        bus.send = parSend;
        bus.delay_us = delayUs;
        bus.ctx = this;
    }
    hd44780_init(&lcd_, &bus, cols_, rows_);
    lcd_.rom = screen_.rom;
    hd44780_screen_assume_blank(&screen_);
    barChars_ = false;
    return lcd_.error == 0;
}

void CharLcd::clear() {
    hd44780_clear(&lcd_);
    hd44780_screen_assume_blank(&screen_);
}

void CharLcd::home() { hd44780_home(&lcd_); }

void CharLcd::setCursor(uint8_t col, uint8_t row) { hd44780_set_cursor(&lcd_, col, row); }

void CharLcd::createChar(uint8_t slot, const uint8_t pattern[8]) {
    hd44780_create_char(&lcd_, slot, pattern);
    barChars_ = false;
}

void CharLcd::backlight(bool on) {
    if (wire_) hd44780_pcf8574_backlight(&pcf_, on);
}

size_t CharLcd::write(uint8_t code) {
    hd44780_write(&lcd_, code);
    return 1;
}

size_t CharLcd::write(const uint8_t *buf, size_t n) {
    hd44780_print_n(&lcd_, (const char *)buf, n);
    return n;
}

void CharLcd::bar(uint8_t col, uint8_t row, uint8_t width, uint32_t value, uint32_t max, uint8_t cgramBase) {
    if (!barChars_) {  // load the 4 bar characters once
        for (uint8_t i = 0; i < 4; i++) hd44780_create_char(&lcd_, (uint8_t)(cgramBase + i), hd44780_screen_bar_chars(i));
        barChars_ = true;
    }
    hd44780_screen_bar(&screen_, col, row, width, value, max, cgramBase);
}
