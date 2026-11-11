#ifdef ARDUINO
#include "St7735s.h"

#include <string.h>

namespace stampdisplay {

ArduinoSpiBus::ArduinoSpiBus(const St7735sConfig& c, SPIClass& spi)
    : spi_(spi), settings_(c.spiHz, MSBFIRST, SPI_MODE0),
      pinCs_(c.pinCs), pinDc_(c.pinDc), pinRst_(c.pinRst), pinSck_(c.pinSck), pinMosi_(c.pinMosi) {}

void ArduinoSpiBus::beginPins() {
  pinMode(pinCs_, OUTPUT);
  digitalWrite(pinCs_, HIGH);
  pinMode(pinDc_, OUTPUT);
  digitalWrite(pinDc_, HIGH);
  if (pinRst_ >= 0) {
    pinMode(pinRst_, OUTPUT);
    digitalWrite(pinRst_, HIGH);
  }
#if defined(ARDUINO_ARCH_ESP32)
  spi_.begin(pinSck_, -1, pinMosi_, -1);
#else
  spi_.begin();  // ESP8266: fixed hardware SPI pins, SCK GPIO 14 (D5), MOSI GPIO 13 (D7)
#endif
}

void ArduinoSpiBus::beginWrite() {
  spi_.beginTransaction(settings_);
  digitalWrite(pinCs_, LOW);
}

void ArduinoSpiBus::endWrite() {
  digitalWrite(pinCs_, HIGH);
  spi_.endTransaction();
}

void ArduinoSpiBus::dc(bool data) { digitalWrite(pinDc_, data ? HIGH : LOW); }

void ArduinoSpiBus::reset(bool high) {
  if (pinRst_ >= 0) digitalWrite(pinRst_, high ? HIGH : LOW);
}

void ArduinoSpiBus::write(const uint8_t* data, size_t len) {
#if defined(ARDUINO_ARCH_ESP32)
  spi_.writeBytes(data, len);  // FIFO bursts: much faster than byte by byte
#elif defined(ARDUINO_ARCH_ESP8266)
  // The ESP8266's SPI.writeBytes() reads the data as 32-bit words: an address that isn't
  // 4-byte aligned is a fatal exception (9). The panel's buffers are aligned; anything else
  // (init sequence arguments from the table) goes through an aligned copy.
  if ((reinterpret_cast<uintptr_t>(data) & 3) == 0) {
    spi_.writeBytes(data, len);
    return;
  }
  alignas(4) uint8_t tmp[64];
  while (len > 0) {
    size_t n = len > sizeof(tmp) ? sizeof(tmp) : len;
    memcpy(tmp, data, n);
    spi_.writeBytes(tmp, n);
    data += n;
    len -= n;
  }
#else
  for (size_t i = 0; i < len; ++i) spi_.transfer(data[i]);
#endif
}

St7735s::St7735s(const St7735sConfig& config, SPIClass& spi)
    : detail::BusHolder(config, spi), St7735sPanel(bus, config) {}

void St7735s::begin() {
  bus.beginPins();
  if (config().pinBacklight >= 0) {
    pinMode(config().pinBacklight, OUTPUT);
    setBacklight(true);
  }
  initPanel();
}

void St7735s::setBacklight(bool on) {
  if (config().pinBacklight >= 0) digitalWrite(config().pinBacklight, on ? HIGH : LOW);
}

}  // namespace stampdisplay

#endif  // ARDUINO
