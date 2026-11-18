// Rc522 - Arduino adapter, see Rc522.h. Only SPI and pin I/O live here.
#include "Rc522.h"

Rc522::Rc522(uint8_t ssPin, uint8_t rstPin, SPIClass &spi, uint32_t hz)
    : spi_(&spi), hz_(hz), ss_(ssPin), rst_(rstPin), last_(RC522_BUS) {
    dev_.version = 0;
    dev_.bus_failed = 0;
}

int Rc522::transfer(void *ctx, uint8_t *buf, uint16_t n) {
    Rc522 *self = static_cast<Rc522 *>(ctx);
    self->spi_->beginTransaction(SPISettings(self->hz_, MSBFIRST, SPI_MODE0));
    digitalWrite(self->ss_, LOW);
    for (uint16_t i = 0; i < n; i++) buf[i] = self->spi_->transfer(buf[i]);
    digitalWrite(self->ss_, HIGH);
    self->spi_->endTransaction();
    return 0;
}

void Rc522::delayUs(void *, uint32_t us) {
    if (us >= 1000) delay(us / 1000);
    delayMicroseconds((unsigned int)(us % 1000));
}

bool Rc522::begin() {
    pinMode(ss_, OUTPUT);
    digitalWrite(ss_, HIGH);
    spi_->begin();
    if (rst_ != NO_PIN) {  // hard reset: NRSTPD low, then high; oscillator needs ~40 ms
        pinMode(rst_, OUTPUT);
        digitalWrite(rst_, LOW);
        delayMicroseconds(10);
        digitalWrite(rst_, HIGH);
        delay(50);
    }
    rc522_bus bus = {transfer, delayUs, this};
    last_ = rc522_init(&dev_, &bus);
    return last_ == RC522_OK;
}

const char *Rc522::hex(const rc522_uid &uid) {
    static char buf[32];
    return rc522_uid_hex(&uid, buf, sizeof buf);
}
