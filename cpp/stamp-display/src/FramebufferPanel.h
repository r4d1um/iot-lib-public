// In-memory panel for rendering screens on a PC (README screenshots, tests).
// Not used on the device.
#pragma once
#ifndef ARDUINO

#include <cstdio>
#include <vector>

#include "Panel.h"

namespace stampdisplay {

class FramebufferPanel : public Panel {
 public:
  FramebufferPanel(int16_t w, int16_t h) : w_(w), h_(h), pixels_(size_t(w) * h, 0) {}

  int16_t width() const override { return w_; }
  int16_t height() const override { return h_; }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    for (int16_t row = y; row < y + h; ++row)
      for (int16_t col = x; col < x + w; ++col) pixels_[size_t(row) * w_ + col] = color;
  }

  void blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* src) override {
    for (int16_t row = 0; row < h; ++row)
      for (int16_t col = 0; col < w; ++col)
        pixels_[size_t(y + row) * w_ + x + col] = src[row * w + col];
  }

  uint16_t pixel(int16_t x, int16_t y) const { return pixels_[size_t(y) * w_ + x]; }

  // Writes a binary PPM (P6) with RGB565 expanded to 8 bits per channel.
  bool writePpm(const char* path) const {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w_, h_);
    for (uint16_t p : pixels_) {
      unsigned char rgb[3] = {
        static_cast<unsigned char>(((p >> 11) & 0x1F) * 255 / 31),
        static_cast<unsigned char>(((p >> 5) & 0x3F) * 255 / 63),
        static_cast<unsigned char>((p & 0x1F) * 255 / 31),
      };
      std::fwrite(rgb, 1, 3, f);
    }
    return std::fclose(f) == 0;
  }

 private:
  int16_t w_, h_;
  std::vector<uint16_t> pixels_;
};

}  // namespace stampdisplay

#endif  // !ARDUINO
