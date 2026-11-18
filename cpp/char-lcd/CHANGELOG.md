# Changelog

## 1.0.0

- First version: HD44780 driver core (4-bit init, commands, DDRAM row offsets for 16x2 ... 40x4
  incl. the second controller of 40x4, CGRAM), UTF-8 to ROM mapping (A00, A02), PCF8574 backpack
  encoding, screen buffer with text fields, bar graph and changed-cells-only flush.
- 74HC595 bus: the LCD on 3 GPIOs through one shift register, same bit layout as the backpack.
- RS settles before E rises: one extra expander byte whenever RS changes (backpack and 595).
- Arduino adapter `CharLcd`: I2C backpack (address auto-detect), 74HC595 or 4-bit parallel pins,
  LiquidCrystal-style API, `print()` with UTF-8.
