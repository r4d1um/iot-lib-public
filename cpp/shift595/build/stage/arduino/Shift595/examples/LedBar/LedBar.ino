// 8 LEDs on one 74HC595 as a bar graph (e.g. WiFi signal): SER D5, SRCLK D6, RCLK D7.
// Each LED through a 330 ohm resistor from Q to GND; OE to GND, SRCLR to VCC.
#include <Shift595.h>

Shift595 leds(5, 6, 7);

void setup() {
  leds.begin();
}

void loop() {
  int value = analogRead(A0);              // 0..1023, replace with RSSI + 100 etc.
  leds.bar(0, 8, value, 1023);
  leds.update();                           // shifts only when the bar changed
  delay(20);
}
