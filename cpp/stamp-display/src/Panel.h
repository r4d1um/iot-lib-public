// Low-level panel interface. A panel only knows how to fill a rectangle with
// one colour and how to copy a block of pixels; everything else (lines, text,
// clipping) lives in Gfx so it works the same on the real display and on the
// in-memory framebuffer used for screenshots.
#pragma once
#include <stdint.h>

namespace stampdisplay {

// RGB565 colour from 8-bit components.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace color {
constexpr uint16_t BLACK  = rgb565(0, 0, 0);
constexpr uint16_t WHITE  = rgb565(255, 255, 255);
constexpr uint16_t RED    = rgb565(255, 0, 0);
constexpr uint16_t GREEN  = rgb565(0, 255, 0);
constexpr uint16_t BLUE   = rgb565(0, 0, 255);
constexpr uint16_t CYAN   = rgb565(0, 255, 255);
constexpr uint16_t YELLOW = rgb565(255, 255, 0);
constexpr uint16_t ORANGE = rgb565(255, 165, 0);
constexpr uint16_t GREY   = rgb565(128, 128, 128);
}  // namespace color

class Panel {
 public:
  virtual ~Panel() = default;

  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;

  // Both calls receive rectangles that Gfx has already clipped to the panel.
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) = 0;
};

}  // namespace stampdisplay
