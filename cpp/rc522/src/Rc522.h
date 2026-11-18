// Rc522 - Arduino adapter for MFRC522 readers ("RFID-RC522" modules). The protocol is the
// precompiled rc522 core; this adapter only does SPI and the reset pin.
//
// The module runs on 3.3 V only (3.3V pin, not 5 V). Its inputs are not 5 V tolerant: on 5 V
// boards (Uno, Nano, Mega) put a level shifter in SCK, MOSI, SS and RST.
//
//   Rc522 rfid(D8, D3);                  // SS (SDA pin of the module), RST; hardware SPI
//   rfid.begin();
//   rc522_uid card;
//   if (rfid.find(card)) { Serial.println(Rc522::hex(card)); rfid.halt(); }
#ifndef RC522_ARDUINO_H
#define RC522_ARDUINO_H

#include <Arduino.h>
#include <SPI.h>
#include "rc522.h"

class Rc522 {
public:
    static const uint8_t NO_PIN = 255;

    Rc522(uint8_t ssPin, uint8_t rstPin = NO_PIN, SPIClass &spi = SPI, uint32_t hz = 4000000);

    // Starts SPI, hard-resets the chip (if RST is wired), initialises it. false: no reader found.
    bool begin();
    uint8_t version() const { return dev_.version; }   // 0x91/0x92 genuine, 0x88/0x12/0xB2 clones

    // A new card (not halted) in the field? Fills uid. find() returns true for one card at a time;
    // halt() it afterwards, else it is reported only every second call while it stays.
    bool find(rc522_uid &uid) { return (last_ = rc522_find(&dev_, &uid)) == RC522_OK; }
    // Any card, halted ones too.
    bool findAny(rc522_uid &uid) { return (last_ = rc522_find_any(&dev_, &uid)) == RC522_OK; }
    rc522_status halt() { return last_ = rc522_halt(&dev_); }

    // MIFARE Classic (key A by default) / Ultralight / NTAG
    rc522_status auth(uint8_t block, const uint8_t key[6], const rc522_uid &uid, bool keyB = false) {
        return last_ = rc522_auth(&dev_, keyB, block, key, &uid);
    }
    rc522_status read(uint8_t block, uint8_t out[16]) { return last_ = rc522_read(&dev_, block, out); }
    rc522_status write(uint8_t block, const uint8_t data[16]) { return last_ = rc522_write(&dev_, block, data); }
    rc522_status writePage(uint8_t page, const uint8_t data[4]) { return last_ = rc522_ul_write(&dev_, page, data); }
    void stopCrypto() { rc522_stop_crypto(&dev_); }

    void antenna(bool on) { rc522_antenna(&dev_, on); }
    void gain(uint8_t level) { rc522_gain(&dev_, level); }   // 0..7, 7 = 48 dB for more range

    rc522_status lastStatus() const { return last_; }
    const char *lastStatusName() const { return rc522_status_name(last_); }

    // "04:A3:1F:..." (static buffer, valid until the next call)
    static const char *hex(const rc522_uid &uid);
    static const char *typeName(const rc522_uid &uid) { return rc522_type_name(rc522_card_type(uid.sak)); }

    rc522 &core() { return dev_; }

private:
    static int transfer(void *ctx, uint8_t *buf, uint16_t n);
    static void delayUs(void *ctx, uint32_t us);

    rc522 dev_;
    SPIClass *spi_;
    uint32_t hz_;
    uint8_t ss_, rst_;
    rc522_status last_;
};

#endif
