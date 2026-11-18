#include <Arduino.h>
#line 1 "/home/claude/iot-lib-public/cpp/char-lcd/examples/HelloI2C/HelloI2C.ino"
// 20x4 LCD with a PCF8574 I2C backpack: SDA/SCL to the board's I2C pins, VCC 5 V.
// Address 0 finds the backpack (0x27 or 0x3F usually). Turn the blue pot for contrast.
#include <CharLcd.h>

CharLcd lcd(0, 20, 4);

#line 7 "/home/claude/iot-lib-public/cpp/char-lcd/examples/HelloI2C/HelloI2C.ino"
void setup();
#line 20 "/home/claude/iot-lib-public/cpp/char-lcd/examples/HelloI2C/HelloI2C.ino"
void loop();
#line 7 "/home/claude/iot-lib-public/cpp/char-lcd/examples/HelloI2C/HelloI2C.ino"
void setup() {
  Serial.begin(115200);
  if (!lcd.begin()) {
    Serial.println("no LCD backpack found on I2C");
    return;
  }
  Serial.print("LCD at 0x");
  Serial.println(lcd.address(), HEX);
  lcd.print("Hello, world!");
  lcd.setCursor(0, 1);
  lcd.print("23.5 °C  Grüße");     // UTF-8, mapped to the display's ROM
}

void loop() {
  lcd.setCursor(0, 3);
  lcd.print(millis() / 1000);
  delay(1000);
}

