// NTAG213/215/216 or Ultralight: write 4 bytes to page 4 and read pages 4..7.
#include <Rc522.h>

#if defined(ESP8266)
Rc522 rfid(D8, D3);
#elif defined(ESP32)
Rc522 rfid(5, 22);
#else
Rc522 rfid(10, 9);
#endif

void setup() {
  Serial.begin(115200);
  rfid.begin();
}

void loop() {
  rc522_uid tag;
  if (!rfid.find(tag)) { delay(50); return; }
  if (rc522_card_type(tag.sak) == RC522_TYPE_ULTRALIGHT) {
    const uint8_t page[4] = {'r', 'c', '5', '2'};
    uint8_t pages[16];
    if (rfid.writePage(4, page) == RC522_OK && rfid.read(4, pages) == RC522_OK) {
      Serial.print(Rc522::hex(tag));
      Serial.print(" pages 4-7: ");
      for (uint8_t i = 0; i < 16; i++) { Serial.print(pages[i], HEX); Serial.print(' '); }
      Serial.println();
    }
  }
  rfid.halt();
}
