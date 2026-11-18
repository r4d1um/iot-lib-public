// 20x4 LCD through one 74HC595: 3 GPIOs, no I2C backpack needed.
// 595: SER (14) <- pin 5, SRCLK (11) <- pin 6, RCLK (12) <- pin 7, OE (13) GND, SRCLR (10) VCC.
// LCD: RS <- QA (15), E <- QC (2), D4..D7 <- QE..QH (4..7), RW GND, V0 on a 10k pot,
// backlight K through an NPN (e.g. BC547, 1k base resistor) from QD (3).
#include <CharLcd.h>

CharLcd lcd(CharLcd::Shift595Pins(5, 6, 7), 20, 4);

void setup() {
  lcd.begin();
  lcd.text(0, 0, 20, "74HC595 -> HD44780", HD44780_CENTER);
  lcd.text(0, 2, 20, "3 Pins, 20.4 °C", HD44780_CENTER);
  lcd.refresh();
}

void loop() {
  lcd.bar(0, 3, 20, millis() % 5000, 5000);
  lcd.refresh();
  delay(50);
}
