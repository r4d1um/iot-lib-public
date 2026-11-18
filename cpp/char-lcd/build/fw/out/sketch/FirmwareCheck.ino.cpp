#include <Arduino.h>
#line 1 "/home/claude/iot-lib-public/cpp/char-lcd/tests/firmware/FirmwareCheck/FirmwareCheck.ino"
// Link check (make firmware-check): both transports, print, screen buffer and bar against the
// packaged library on an Arduino Uno.
#include <CharLcd.h>

CharLcd i2c(0, 20, 4);
CharLcd par(12, 11, 5, 4, 3, 2, 16, 2);

#line 8 "/home/claude/iot-lib-public/cpp/char-lcd/tests/firmware/FirmwareCheck/FirmwareCheck.ino"
void setup();
#line 20 "/home/claude/iot-lib-public/cpp/char-lcd/tests/firmware/FirmwareCheck/FirmwareCheck.ino"
void loop();
#line 8 "/home/claude/iot-lib-public/cpp/char-lcd/tests/firmware/FirmwareCheck/FirmwareCheck.ino"
void setup() {
  i2c.begin();
  par.begin();
  i2c.print("23.5 °C");
  par.text(0, 0, 16, "Grüße", HD44780_CENTER);
  par.bar(0, 1, 16, 7, 10);
  par.refresh();
  uint8_t heart[8] = {0, 10, 31, 31, 14, 4, 0, 0};
  i2c.createChar(0, heart);
  i2c.write((uint8_t)0);
}

void loop() {}

