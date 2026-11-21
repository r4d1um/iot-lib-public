// Prints UID and type of every card held to the reader.
// D1 mini: SDA(SS) D8, SCK D5, MOSI D7, MISO D6, RST D3, 3.3V, GND.
// ESP32:   SDA(SS) 5,  SCK 18, MOSI 23, MISO 19, RST 22.
#include <Rc522.h>

#if defined(ESP8266)
Rc522 rfid(D8, D3);
#elif defined(ESP32)
Rc522 rfid(5, 22);
#else
Rc522 rfid(10, 9);   // Uno/Nano: SS 10, RST 9 - through a level shifter, the module is 3.3 V only
#endif

void setup() {
  Serial.begin(115200);
  if (!rfid.begin()) {
    Serial.println("no RC522 found - check wiring and 3.3 V");
    return;
  }
  Serial.print("RC522 version 0x");
  Serial.println(rfid.version(), HEX);
}

void loop() {
  rc522_uid card;
  if (rfid.find(card)) {
    Serial.print(Rc522::hex(card));
    Serial.print("  ");
    Serial.println(Rc522::typeName(card));
    rfid.halt();   // report it once; it shows up again after leaving the field
  }
  delay(50);
}
