#include <Arduino.h>
#line 1 "/home/claude/iot-lib-public/cpp/char-lcd/examples/Dashboard/Dashboard.ino"
// Screen buffer: fixed fields that update without flicker; only changed cells go over I2C.
#include <CharLcd.h>

CharLcd lcd(0x27, 20, 4);

#line 6 "/home/claude/iot-lib-public/cpp/char-lcd/examples/Dashboard/Dashboard.ino"
void setup();
#line 14 "/home/claude/iot-lib-public/cpp/char-lcd/examples/Dashboard/Dashboard.ino"
void loop();
#line 6 "/home/claude/iot-lib-public/cpp/char-lcd/examples/Dashboard/Dashboard.ino"
void setup() {
  lcd.begin();
  lcd.text(0, 0, 20, "Darkroom", HD44780_CENTER);
  lcd.text(0, 1, 6, "Bath");
  lcd.text(0, 2, 6, "Time");
  lcd.refresh();
}

void loop() {
  static uint32_t start = millis();
  uint32_t s = (millis() - start) / 1000;
  char buf[16];
  snprintf(buf, sizeof buf, "%u.%u °C", 20 + (unsigned)(s % 3), (unsigned)(s % 10));
  lcd.text(6, 1, 14, buf, HD44780_RIGHT);
  snprintf(buf, sizeof buf, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
  lcd.text(6, 2, 14, buf, HD44780_RIGHT);
  lcd.bar(0, 3, 20, s % 60, 59);   // 100 steps over 20 cells (CGRAM 4..7)
  lcd.refresh();
  delay(200);
}

