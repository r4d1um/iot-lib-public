// Point any remote at the receiver and press buttons: prints protocol, address and command.
// Unknown remotes print a hash (same button = same hash) and the raw timings.
// KY-022: G -> GND, R -> 3.3 V (ESP) or 5 V (Uno), Y -> the pin below.
#include <RemoteRx.h>

#if defined(ESP8266)
RemoteRx ir(D5);
#elif defined(ESP32)
RemoteRx ir(4);
#else
RemoteRx ir(2);   // Uno/Nano: interrupt pin 2 or 3
#endif

void setup() {
  Serial.begin(115200);
  if (!ir.begin()) Serial.println("pin has no interrupt");
}

void loop() {
  irrx_result code;
  if (!ir.read(code)) return;
  RemoteRx::print(Serial, code);
  if (code.protocol == IRRX_UNKNOWN) {
    uint16_t n;
    const volatile uint16_t *d = ir.raw(n);
    Serial.print("  raw:");
    for (uint16_t i = 0; i < n; i++) { Serial.print(' '); Serial.print(d[i]); }
    Serial.println();
  }
}
