// Off-screen panel for one horizontal band of the screen (W x up to MAX_H
// pixels), in RAM: draw a header, a table row etc. into it with a Gfx, then
// send it to the display with Gfx::drawPixels in one go. Redrawing an area
// this way never shows it half-cleared. Works on the device and on a PC.
#pragma once
#include <stdint.h>

#include "Panel.h"

namespace stampdisplay {

template <int16_t W, int16_t MAX_H>
class BandPanel : public Panel {
 public:
  // Starts a new band of height h (clamped to MAX_H) filled with `color`.
  void begin(int16_t h, uint16_t color) {
    h_ = h < 1 ? 1 : (h > MAX_H ? MAX_H : h);
    fillRect(0, 0, W, h_, color);
  }

  int16_t width() const override { return W; }
  int16_t height() const override { return h_; }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    for (int16_t row = y; row < y + h; ++row)
      for (int16_t col = x; col < x + w; ++col) pixels_[row * W + col] = color;
  }

  void blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* src) override {
    for (int16_t row = 0; row < h; ++row)
      for (int16_t col = 0; col < w; ++col) pixels_[(y + row) * W + x + col] = src[row * w + col];
  }

  const uint16_t* pixels() const { return pixels_; }

 private:
  int16_t h_ = MAX_H;
  uint16_t pixels_[W * MAX_H];
};

}  // namespace stampdisplay
