// Link check (make firmware-check): adapter and the whole core API against the packaged library.
#include <Rc522.h>

Rc522 rfid(10, 9);
const uint8_t KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  Serial.begin(115200);
  rfid.begin();
  rfid.gain(7);
}

void loop() {
  rc522_uid c;
  if (!rfid.findAny(c)) return;
  Serial.println(Rc522::hex(c));
  Serial.println(Rc522::typeName(c));
  uint8_t b[16];
  if (rfid.auth(4, KEY, c) == RC522_OK && rfid.read(4, b) == RC522_OK) rfid.write(4, b);
  const uint8_t p[4] = {1, 2, 3, 4};
  rfid.writePage(4, p);
  rfid.stopCrypto();
  rfid.halt();
  Serial.println(rfid.lastStatusName());
}
