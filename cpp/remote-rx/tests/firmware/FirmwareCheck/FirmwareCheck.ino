// Link check (make firmware-check): adapter, ISR path and decoders against the packaged library.
#include <RemoteRx.h>

RemoteRx ir(2);

void setup() {
  Serial.begin(115200);
  ir.begin();
}

void loop() {
  irrx_result c;
  if (ir.read(c)) {
    RemoteRx::print(Serial, c);
    uint16_t n;
    if (ir.raw(n)[0] > 0 && c.protocol == IRRX_UNKNOWN) Serial.println(n);
  }
}
