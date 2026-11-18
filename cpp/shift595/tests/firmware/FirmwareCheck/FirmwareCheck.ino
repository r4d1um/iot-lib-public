// Link check (make firmware-check): bit-bang and SPI adapters against the packaged library.
#include <Shift595.h>

Shift595 bits(5, 6, 7, 2, 3);
Shift595 spi(Shift595::Spi{10}, 3);

void setup() {
  bits.begin();
  spi.begin();
  bits.bar(0, 16, 7, 10, SHIFT595_DOT);
  bits.brightness(128);
  spi.setByte(1, 0xA5);
  bits.update();
  spi.update();
}

void loop() {}
