// RemoteRx - Arduino adapter, see RemoteRx.h. Only the interrupt and the pin live here.
#include "RemoteRx.h"

RemoteRx *RemoteRx::active_ = nullptr;

void IRAM_ATTR RemoteRx::isr() {
    RemoteRx *self = active_;
    if (self) irrx_edge(&self->rx_, micros(), digitalRead(self->pin_));
}

bool RemoteRx::begin() {
    int irq = digitalPinToInterrupt(pin_);
    if (irq < 0 || (active_ && active_ != this)) return false;
    pinMode(pin_, INPUT);   // the receiver has its own pull-up on the output
    irrx_init(&rx_);
    active_ = this;
    attachInterrupt(irq, isr, CHANGE);
    return true;
}

void RemoteRx::end() {
    if (active_ != this) return;
    detachInterrupt(digitalPinToInterrupt(pin_));
    active_ = nullptr;
}

bool RemoteRx::read(irrx_result &code) {
    noInterrupts();
    int ready = irrx_check(&rx_, micros());   // O(1): just looks at state and the last edge time
    interrupts();
    return ready && irrx_read(&rx_, &code);   // decoding runs with interrupts on
}

void RemoteRx::print(Print &out, const irrx_result &code) {
    out.print(irrx_protocol_name(code.protocol));
    if (code.protocol == IRRX_UNKNOWN) {
        out.print(" #");
        out.print(code.hash, HEX);
    } else {
        out.print(" 0x");
        out.print(code.address, HEX);
        out.print(" 0x");
        out.print(code.command, HEX);
    }
    if (code.repeat) out.print(" repeat");
    out.println();
}
