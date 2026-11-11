// ST7735S protocol for the 1.77" 160x128 TFT module, without Arduino: init sequence, rotation,
// RAM window and pixel streaming. The wires (SPI, CS, DC, RESET, delays) come from a St7735sBus,
// which the Arduino adapter St7735s (St7735s.h) implements with SPI and digitalWrite. This part is
// part of the precompiled core (libStampDisplay.a); the adapter is compiled with the sketch, so the
// archive works with every Arduino core version.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "Panel.h"

namespace stampdisplay {

// Wiring and panel options. The pin fields are only used by the Arduino adapter.
struct St7735sConfig {
  int8_t pinCs;
  int8_t pinDc;               // labelled RS on the module
  int8_t pinRst;              // labelled RES; -1 if tied to the board reset
  int8_t pinSck = -1;         // -1: board default SPI pins (ESP8266: always GPIO 14)
  int8_t pinMosi = -1;        // labelled SDA on the module (ESP8266: always GPIO 13)
  int8_t pinBacklight = -1;   // LEDA; -1 if wired straight to 3.3 V

  uint8_t rotation = 0;       // 0/2 portrait 128x160, 1/3 landscape 160x128
  bool bgr = false;           // set if red and blue come out swapped
  bool invert = false;        // set if the picture looks like a negative
  uint8_t xOffset = 0;        // some panels need 1-2 px to hit the glass edge
  uint8_t yOffset = 0;
  uint32_t spiHz = 20000000;
};

// What the protocol needs from the hardware. All calls come from one thread; every byte
// sequence is sent between beginWrite() and endWrite().
class St7735sBus {
 public:
  virtual ~St7735sBus() = default;
  virtual void beginWrite() = 0;                         // start SPI transaction, CS low
  virtual void endWrite() = 0;                           // CS high, end transaction
  virtual void dc(bool data) = 0;                        // false: next bytes are a command
  virtual void write(const uint8_t* data, size_t len) = 0;  // len bytes, MSB first
  virtual bool hasReset() const = 0;                     // RESET pin wired?
  virtual void reset(bool high) = 0;                     // drive RESET (only if hasReset())
  virtual void delayMs(uint16_t ms) = 0;
};

// A Panel on a ST7735S. Coordinates passed to fillRect/blit are already clipped by Gfx.
class St7735sPanel : public Panel {
 public:
  St7735sPanel(St7735sBus& bus, const St7735sConfig& config) : bus_(bus), config_(config) {}

  // Hardware reset (if wired), init sequence, inversion and rotation from the config.
  // Takes about 1 s because of the controller's required delays.
  void initPanel();

  // 0/2 portrait (128 x 160), 1/3 landscape (160 x 128); values > 3 wrap.
  void setRotation(uint8_t rotation);

  // Controller sleep mode on/off (waits the required 120 ms).
  void sleep(bool enable);

  int16_t width() const override { return width_; }
  int16_t height() const override { return height_; }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) override;

  const St7735sConfig& config() const { return config_; }

  static constexpr int16_t NATIVE_WIDTH = 128;
  static constexpr int16_t NATIVE_HEIGHT = 160;

 private:
  void command(uint8_t cmd);
  void commandWithData(uint8_t cmd, const uint8_t* data, uint8_t len);
  void runInitSequence(const uint8_t* seq);
  void setWindow(int16_t x, int16_t y, int16_t w, int16_t h);

  St7735sBus& bus_;
  St7735sConfig config_;
  int16_t width_ = NATIVE_WIDTH;
  int16_t height_ = NATIVE_HEIGHT;
  uint8_t colStart_ = 0;
  uint8_t rowStart_ = 0;
};

}  // namespace stampdisplay
