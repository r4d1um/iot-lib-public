#include <Arduino.h>
#line 1 "/home/claude/iot-lib-public/cpp/shift595/examples/LedBar/LedBar.ino"
// 8 LEDs on one 74HC595 as a bar graph (e.g. WiFi signal): SER D5, SRCLK D6, RCLK D7.
// Each LED through a 330 ohm resistor from Q to GND; OE to GND, SRCLR to VCC.
#include <Shift595.h>

Shift595 leds(5, 6, 7);

#line 7 "/home/claude/iot-lib-public/cpp/shift595/examples/LedBar/LedBar.ino"
void setup();
#line 11 "/home/claude/iot-lib-public/cpp/shift595/examples/LedBar/LedBar.ino"
void loop();
#line 7 "/home/claude/iot-lib-public/cpp/shift595/examples/LedBar/LedBar.ino"
void setup() {
  leds.begin();
}

void loop() {
  int value = analogRead(A0);              // 0..1023, replace with RSSI + 100 etc.
  leds.bar(0, 8, value, 1023);
  leds.update();                           // shifts only when the bar changed
  delay(20);
}

