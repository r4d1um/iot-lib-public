// MIFARE Classic 1K: authenticate with the transport key, write block 4, read it back.
#include <Rc522.h>

#if defined(ESP8266)
Rc522 rfid(D8, D3);
#elif defined(ESP32)
Rc522 rfid(5, 22);
#else
Rc522 rfid(10, 9);
#endif

const uint8_t KEY[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // factory default key A

void setup() {
  Serial.begin(115200);
  rfid.begin();
}

void loop() {
  rc522_uid card;
  if (!rfid.find(card)) { delay(50); return; }
  if (rc522_card_type(card.sak) != RC522_TYPE_MIFARE_1K) { rfid.halt(); return; }

  uint8_t block[16];
  if (rfid.auth(4, KEY, card) == RC522_OK) {
    uint8_t text[16] = "hello, RC522!";
    rfid.write(4, text);
    if (rfid.read(4, block) == RC522_OK) {
      Serial.print(Rc522::hex(card));
      Serial.print(" block 4: ");
      Serial.println((const char *)block);
    }
  } else {
    Serial.print("auth: ");
    Serial.println(rfid.lastStatusName());
  }
  rfid.halt();
}
