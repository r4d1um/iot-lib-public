// Drawing primitives and 6x8 text on top of any Panel.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Panel.h"

namespace stampdisplay {

constexpr int16_t CHAR_W = 6;  // 5 px glyph + 1 px spacing
constexpr int16_t CHAR_H = 8;

class Gfx {
 public:
  explicit Gfx(Panel& panel) : panel_(panel) {}

  int16_t width() const { return panel_.width(); }
  int16_t height() const { return panel_.height(); }

  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

  // Draws one character cell (6x8 px, background included). Characters outside
  // printable ASCII are drawn as '?'.
  void drawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg);

  // Draws text with an opaque background. Returns the x position after the text.
  int16_t drawText(int16_t x, int16_t y, const char* text, uint16_t fg, uint16_t bg);

  // Draws text in a field exactly `chars` cells wide: longer text is cut off,
  // shorter text is padded with background. Overwrites whatever was in the
  // field before, so values that get shorter never leave old pixels behind.
  void drawField(int16_t x, int16_t y, const char* text, uint8_t chars, uint16_t fg, uint16_t bg);

  // Like drawField, but the text is right-aligned inside the field.
  void drawFieldRight(int16_t x, int16_t y, const char* text, uint8_t chars, uint16_t fg, uint16_t bg);

  // Text with every font pixel drawn as a scale x scale block (scale 1..4).
  // Returns the x position after the text.
  int16_t drawTextScaled(int16_t x, int16_t y, const char* text, uint8_t scale,
                         uint16_t fg, uint16_t bg);

  static int16_t textWidth(const char* text, uint8_t scale = 1);

  // Copies a block of RGB565 pixels (row by row, w * h values), clipped to
  // the panel. With an off-screen panel (see BandPanel) a whole area can be
  // drawn first and sent in one go: no flicker while it is redrawn.
  void drawPixels(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels);

 private:
  bool clip(int16_t& x, int16_t& y, int16_t& w, int16_t& h) const;
  void blitClipped(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels);

  Panel& panel_;
};

}  // namespace stampdisplay
