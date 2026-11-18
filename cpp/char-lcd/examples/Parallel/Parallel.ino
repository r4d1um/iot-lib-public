// 16x2 LCD on 4-bit parallel pins: RS 12, E 11, D4..D7 5,4,3,2; RW to GND, V0 to a pot.
#include <CharLcd.h>

CharLcd lcd(12, 11, 5, 4, 3, 2, 16, 2);

void setup() {
  lcd.begin();
  lcd.print("4-bit parallel");
  lcd.blink();
}

void loop() {}
