// StampDisplay minimal example: text and a frame on the 1.77" ST7735S TFT (ESP8266 pins).
#include <StampDisplay.h>

using namespace stampdisplay;

St7735sConfig displayConfig() {
  St7735sConfig c{};
  c.pinCs = 15;     // D8
  c.pinDc = 4;      // D2 (RS)
  c.pinRst = 5;     // D1 (RES)
  c.rotation = 1;   // landscape 160 x 128
  return c;
}

St7735s panel(displayConfig());
Gfx gfx(panel);

void setup() {
  panel.begin();
  gfx.fillScreen(color::BLACK);
  gfx.drawRect(0, 0, gfx.width(), gfx.height(), color::CYAN);
  gfx.drawTextScaled(8, 8, "Hello", 2, color::WHITE, color::BLACK);
}

void loop() {
  char buf[12];
  snprintf(buf, sizeof buf, "%lus", millis() / 1000);
  gfx.drawField(8, 40, buf, 10, color::YELLOW, color::BLACK);  // fixed width: no leftovers
  delay(1000);
}
