// Arduino adapter for the 1.77" 160x128 ST7735S TFT module (pins: GND VCC SCK SDA RES RS CS LEDA,
// plus unused GT_* pins for the optional font ROM). Thin layer over St7735sPanel: SPI, CS, DC,
// RESET and the backlight with the Arduino API. Shipped as source, so it compiles against the
// board core in use; the protocol itself is in the precompiled libStampDisplay.a.
#pragma once
#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>

#include "St7735sPanel.h"

namespace stampdisplay {

// St7735sBus on Arduino hardware SPI.
class ArduinoSpiBus : public St7735sBus {
 public:
  ArduinoSpiBus(const St7735sConfig& config, SPIClass& spi);
  void beginPins();                    // pinMode + SPI.begin; called by St7735s::begin()
  void beginWrite() override;
  void endWrite() override;
  void dc(bool data) override;
  void write(const uint8_t* data, size_t len) override;
  bool hasReset() const override { return pinRst_ >= 0; }
  void reset(bool high) override;
  void delayMs(uint16_t ms) override { delay(ms); }

 private:
  SPIClass& spi_;
  SPISettings settings_;
  int8_t pinCs_, pinDc_, pinRst_, pinSck_, pinMosi_;
};

namespace detail {
// Holds the bus so it exists before the St7735sPanel base that refers to it.
struct BusHolder {
  BusHolder(const St7735sConfig& c, SPIClass& s) : bus(c, s) {}
  ArduinoSpiBus bus;
};
}  // namespace detail

class St7735s : private detail::BusHolder, public St7735sPanel {
 public:
  explicit St7735s(const St7735sConfig& config, SPIClass& spi = SPI);

  // Pins, SPI, backlight on, then reset + init sequence (about 1 s).
  void begin();
  // Backlight pin high/low; does nothing if pinBacklight is -1.
  void setBacklight(bool on);
};

}  // namespace stampdisplay

#endif  // ARDUINO
