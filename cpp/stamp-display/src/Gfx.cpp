#include "Gfx.h"

#include <string.h>

#include "Font5x7.h"

namespace stampdisplay {

bool Gfx::clip(int16_t& x, int16_t& y, int16_t& w, int16_t& h) const {
  if (w <= 0 || h <= 0) return false;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > width()) w = width() - x;
  if (y + h > height()) h = height() - y;
  return w > 0 && h > 0;
}

void Gfx::fillScreen(uint16_t color) {
  panel_.fillRect(0, 0, width(), height(), color);
}

void Gfx::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (clip(x, y, w, h)) panel_.fillRect(x, y, w, h, color);
}

void Gfx::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void Gfx::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}

void Gfx::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawHLine(x, y, w, color);
  drawHLine(x, y + h - 1, w, color);
  drawVLine(x, y, h, color);
  drawVLine(x + w - 1, y, h, color);
}

void Gfx::blitClipped(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) {
  int16_t cx = x, cy = y, cw = w, ch = h;
  if (!clip(cx, cy, cw, ch)) return;
  if (cw == w && ch == h) {
    panel_.blit(x, y, w, h, pixels);
    return;
  }
  // Partially visible: copy the visible part row by row.
  for (int16_t row = 0; row < ch; ++row) {
    const uint16_t* src = pixels + (cy - y + row) * w + (cx - x);
    panel_.blit(cx, cy + row, cw, 1, src);
  }
}

void Gfx::drawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg) {
  if (c < FONT_FIRST || c > FONT_LAST) c = '?';
  const uint8_t* glyph = &FONT_5X7[(c - FONT_FIRST) * FONT_GLYPH_W];

  uint16_t cell[CHAR_W * CHAR_H];
  for (int16_t row = 0; row < CHAR_H; ++row) {
    for (int16_t col = 0; col < CHAR_W; ++col) {
      bool on = col < FONT_GLYPH_W && ((glyph[col] >> row) & 1);
      cell[row * CHAR_W + col] = on ? fg : bg;
    }
  }
  blitClipped(x, y, CHAR_W, CHAR_H, cell);
}

int16_t Gfx::drawText(int16_t x, int16_t y, const char* text, uint16_t fg, uint16_t bg) {
  for (const char* p = text; *p; ++p) {
    drawChar(x, y, *p, fg, bg);
    x += CHAR_W;
  }
  return x;
}

void Gfx::drawField(int16_t x, int16_t y, const char* text, uint8_t chars, uint16_t fg, uint16_t bg) {
  size_t len = strlen(text);
  for (uint8_t i = 0; i < chars; ++i) {
    drawChar(x + i * CHAR_W, y, i < len ? text[i] : ' ', fg, bg);
  }
}

void Gfx::drawFieldRight(int16_t x, int16_t y, const char* text, uint8_t chars, uint16_t fg, uint16_t bg) {
  size_t len = strlen(text);
  size_t pad = len < chars ? chars - len : 0;
  size_t skip = len > chars ? len - chars : 0;  // keep the end of overlong text
  for (uint8_t i = 0; i < chars; ++i) {
    char c = i < pad ? ' ' : text[skip + i - pad];
    drawChar(x + i * CHAR_W, y, c, fg, bg);
  }
}

int16_t Gfx::drawTextScaled(int16_t x, int16_t y, const char* text, uint8_t scale,
                            uint16_t fg, uint16_t bg) {
  if (scale <= 1) return drawText(x, y, text, fg, bg);
  if (scale > 4) scale = 4;
  const int16_t w = CHAR_W * scale, h = CHAR_H * scale;
  uint16_t cell[CHAR_W * 4 * CHAR_H * 4];

  for (const char* p = text; *p; ++p) {
    char c = (*p < FONT_FIRST || *p > FONT_LAST) ? '?' : *p;
    const uint8_t* glyph = &FONT_5X7[(c - FONT_FIRST) * FONT_GLYPH_W];
    for (int16_t py = 0; py < h; ++py) {
      int16_t row = py / scale;
      for (int16_t px = 0; px < w; ++px) {
        int16_t col = px / scale;
        bool on = col < FONT_GLYPH_W && ((glyph[col] >> row) & 1);
        cell[py * w + px] = on ? fg : bg;
      }
    }
    blitClipped(x, y, w, h, cell);
    x += w;
  }
  return x;
}

void Gfx::drawPixels(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) {
  blitClipped(x, y, w, h, pixels);
}

int16_t Gfx::textWidth(const char* text, uint8_t scale) {
  return static_cast<int16_t>(strlen(text) * CHAR_W * (scale ? scale : 1));
}

}  // namespace stampdisplay
