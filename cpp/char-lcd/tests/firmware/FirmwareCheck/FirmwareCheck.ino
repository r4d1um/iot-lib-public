// Link check (make firmware-check): all three transports, print, screen buffer and bar against the
// packaged library on an Arduino Uno.
#include <CharLcd.h>

CharLcd i2c(0, 20, 4);
CharLcd par(12, 11, 5, 4, 3, 2, 16, 2);
CharLcd sr(CharLcd::Shift595Pins(6, 7, 8), 20, 4);

void setup() {
  i2c.begin();
  par.begin();
  sr.begin();
  sr.print("via 595");
  sr.noBacklight();
  i2c.print("23.5 °C");
  par.text(0, 0, 16, "Grüße", HD44780_CENTER);
  par.bar(0, 1, 16, 7, 10);
  par.refresh();
  uint8_t heart[8] = {0, 10, 31, 31, 14, 4, 0, 0};
  i2c.createChar(0, heart);
  i2c.write((uint8_t)0);
}

void loop() {}
