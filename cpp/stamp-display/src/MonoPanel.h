// 1-bit framebuffer panel for monochrome displays (SSD1306 OLED and alike), so Gfx (text, lines,
// rectangles) draws on them unchanged. A pixel is lit when its RGB565 colour is brighter than
// about 25 %. The buffer uses the SSD1306 page layout: byte (x + (y / 8) * width), bit y % 8.
// Plain C++: the same class renders the PC-side screenshots.
#pragma once
#include <stdint.h>
#include <string.h>

#include "Panel.h"

namespace stampdisplay {

template <int16_t W, int16_t H>
class MonoPanel : public Panel {
 public:
  static_assert(H % 8 == 0, "height must be a multiple of 8");

  int16_t width() const override { return W; }
  int16_t height() const override { return H; }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    const bool on = lit(color);
    for (int16_t yy = y; yy < y + h; ++yy)
      for (int16_t xx = x; xx < x + w; ++xx) set(xx, yy, on);
    dirty_ = true;
  }
  void blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px) override {
    for (int16_t yy = 0; yy < h; ++yy)
      for (int16_t xx = 0; xx < w; ++xx) set(x + xx, y + yy, lit(px[yy * w + xx]));
    dirty_ = true;
  }

  bool pixel(int16_t x, int16_t y) const { return buf_[x + (y / 8) * W] & (1u << (y & 7)); }
  const uint8_t* buffer() const { return buf_; }
  static constexpr size_t BYTES = W * H / 8;
  bool dirty() const { return dirty_; }
  void clean() { dirty_ = false; }

  static bool lit(uint16_t c) {
    // luminance from RGB565, 0..63 scale
    unsigned r = (c >> 11) << 1, g = (c >> 5) & 0x3F, b = (c & 0x1F) << 1;
    return (r * 3 + g * 6 + b) / 10 >= 16;
  }

 protected:
  void set(int16_t x, int16_t y, bool on) {
    uint8_t& byte = buf_[x + (y / 8) * W];
    const uint8_t bit = static_cast<uint8_t>(1u << (y & 7));
    byte = on ? (byte | bit) : (byte & ~bit);
  }
  uint8_t buf_[W * H / 8] = {};
  bool dirty_ = true;
};

}  // namespace stampdisplay
