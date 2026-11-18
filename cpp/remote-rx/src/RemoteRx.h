// RemoteRx - Arduino adapter for IR remote receivers (VS1838B / KY-022, TSOP38238, ...).
// Decoding is the precompiled irrx core; this adapter only attaches the pin-change interrupt.
//
//   RemoteRx ir(D5);                 // receiver output pin (KY-022: Y / S)
//   void setup() { ir.begin(); }
//   void loop() {
//     irrx_result code;
//     if (ir.read(code)) RemoteRx::print(Serial, code);   // "NEC 0x04 0x08" (+ " repeat")
//   }
//
// One receiver per sketch (the interrupt handler is static). The pin needs interrupt support
// (Uno/Nano: 2 or 3; ESP8266: any except D0/GPIO16; ESP32: any input).
#ifndef REMOTE_RX_ARDUINO_H
#define REMOTE_RX_ARDUINO_H

#include <Arduino.h>
#include "irrx.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

class RemoteRx {
public:
    explicit RemoteRx(uint8_t pin) : pin_(pin) { irrx_init(&rx_); }

    // Attaches the interrupt. false: the pin can't interrupt, or another RemoteRx runs already.
    bool begin();
    void end();

    // A decoded code is waiting? Fills code and frees the buffer for the next one.
    bool read(irrx_result &code);

    // Durations (µs, mark first) of the frame the last read() decoded - for learning or
    // replaying unknown remotes.
    const volatile uint16_t *raw(uint16_t &n) const { return irrx_raw(&rx_, &n); }

    // "NEC 0x04 0x08", "Sony 0x01 0x15 repeat", "unknown #1A2B3C4D"
    static void print(Print &out, const irrx_result &code);
    static const char *protocolName(const irrx_result &code) { return irrx_protocol_name(code.protocol); }

    irrx &core() { return rx_; }

private:
    static void IRAM_ATTR isr();
    static RemoteRx *active_;
    irrx rx_;
    uint8_t pin_;
};

#endif
