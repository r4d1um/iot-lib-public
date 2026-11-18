// Shift595 - Arduino adapter for 74HC595 chains. The state, bar graph and update logic are the
// precompiled shift595 core; this adapter only clocks bits (shiftOut or hardware SPI).
//
// Wiring (per chip): SER (14) data, SRCLK (11) clock, RCLK (12) latch, OE (13) to GND or a PWM
// pin for brightness, SRCLR (10) to VCC, QH' (9) to SER of the next chip, 100 nF at VCC.
//
//   Shift595 leds(D5, D6, D7);              // data, clock, latch; 1 register
//   Shift595 chain(13, 14, 15, 3, 9);       // 3 registers, OE on pin 9 for brightness
//   Shift595 fast(Shift595::Spi{10}, 2);    // hardware SPI: MOSI -> SER, SCK -> SRCLK, latch 10
//
//   leds.begin();  leds.set(3, true);  leds.bar(0, 8, rssi + 100, 70);  leds.update();
#ifndef SHIFT595_ARDUINO_H
#define SHIFT595_ARDUINO_H

#include <Arduino.h>
#include "shift595.h"

class Shift595 {
public:
    static const uint8_t NO_PIN = 255;
    struct Spi {  // hardware SPI: latch pin, clock rate
        uint8_t latch;
        uint32_t hz;
        Spi(uint8_t latchPin, uint32_t clockHz = 8000000) : latch(latchPin), hz(clockHz) {}
    };

    Shift595(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin, uint8_t registers = 1, uint8_t oePin = NO_PIN);
    Shift595(Spi spi, uint8_t registers = 1, uint8_t oePin = NO_PIN);

    void begin();  // pins as outputs, all outputs off (written at once)

    void set(uint16_t pin, bool on) { shift595_set(&s_, pin, on); }
    bool get(uint16_t pin) const { return shift595_get(&s_, pin) != 0; }
    void toggle(uint16_t pin) { shift595_toggle(&s_, pin); }
    void setByte(uint8_t reg, uint8_t value) { shift595_set_byte(&s_, reg, value); }
    uint8_t getByte(uint8_t reg) const { return shift595_get_byte(&s_, reg); }
    void clear() { shift595_clear(&s_); }
    void bar(uint16_t first, uint16_t count, uint32_t value, uint32_t max, shift595_bar_mode mode = SHIFT595_BAR) {
        shift595_bar(&s_, first, count, value, max, mode);
    }
    bool update() { return shift595_update(&s_) != 0; }  // writes only if something changed
    void write() { shift595_write(&s_); }

    // 0 = dark ... 255 = full, via PWM on OE (active low). Needs oePin; ignored without.
    void brightness(uint8_t level);

    uint8_t registers() const { return s_.regs; }
    shift595 &core() { return s_; }

private:
    static void shiftBits(void *ctx, uint8_t b);
    static void shiftSpi(void *ctx, uint8_t b);
    static void latchPulse(void *ctx);

    shift595 s_;
    uint8_t mem_[SHIFT595_MAX_REGS];
    uint8_t data_, clock_, latch_, oe_;
    bool spi_;
    uint32_t hz_;
};

#endif
