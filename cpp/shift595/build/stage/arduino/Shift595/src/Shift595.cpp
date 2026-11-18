// Shift595 - Arduino adapter, see Shift595.h. Only pin I/O lives here.
#include "Shift595.h"
#include <SPI.h>

Shift595::Shift595(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin, uint8_t registers, uint8_t oePin)
    : data_(dataPin), clock_(clockPin), latch_(latchPin), oe_(oePin), spi_(false), hz_(0) {
    shift595_bus bus = {shiftBits, latchPulse, this};
    shift595_init(&s_, &bus, registers, mem_);
}

Shift595::Shift595(Spi spi, uint8_t registers, uint8_t oePin)
    : data_(NO_PIN), clock_(NO_PIN), latch_(spi.latch), oe_(oePin), spi_(true), hz_(spi.hz) {
    shift595_bus bus = {shiftSpi, latchPulse, this};
    shift595_init(&s_, &bus, registers, mem_);
}

void Shift595::begin() {
    pinMode(latch_, OUTPUT);
    digitalWrite(latch_, LOW);
    if (spi_) {
        SPI.begin();
    } else {
        pinMode(data_, OUTPUT);
        pinMode(clock_, OUTPUT);
        digitalWrite(clock_, LOW);
    }
    shift595_clear(&s_);
    shift595_write(&s_);
    if (oe_ != NO_PIN) {  // enable outputs only after they hold a known state
        pinMode(oe_, OUTPUT);
        digitalWrite(oe_, LOW);
    }
}

void Shift595::shiftBits(void *ctx, uint8_t b) {
    Shift595 *self = static_cast<Shift595 *>(ctx);
    shiftOut(self->data_, self->clock_, MSBFIRST, b);
}

void Shift595::shiftSpi(void *ctx, uint8_t b) {
    Shift595 *self = static_cast<Shift595 *>(ctx);
    SPI.beginTransaction(SPISettings(self->hz_, MSBFIRST, SPI_MODE0));
    SPI.transfer(b);
    SPI.endTransaction();
}

void Shift595::latchPulse(void *ctx) {
    Shift595 *self = static_cast<Shift595 *>(ctx);
    digitalWrite(self->latch_, HIGH);
    digitalWrite(self->latch_, LOW);
}

void Shift595::brightness(uint8_t level) {
    if (oe_ == NO_PIN) return;
    analogWrite(oe_, 255 - level);  // OE is active low
}
