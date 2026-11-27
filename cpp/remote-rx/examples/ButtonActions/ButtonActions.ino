// Map remote buttons to actions. Run PrintCodes first and put your remote's codes below.
#include <RemoteRx.h>

#if defined(ESP8266)
RemoteRx ir(D5);
#elif defined(ESP32)
RemoteRx ir(4);
#else
RemoteRx ir(2);
#endif

const uint16_t REMOTE_ADDRESS = 0x00;   // e.g. the small "Car MP3" remotes send NEC address 0x00
const uint16_t BTN_POWER = 0x45, BTN_PLUS = 0x15, BTN_MINUS = 0x07;

int level = 5;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  ir.begin();
}

void loop() {
  irrx_result code;
  if (!ir.read(code) || code.protocol != IRRX_NEC || code.address != REMOTE_ADDRESS) return;
  switch (code.command) {
  case BTN_POWER:
    if (!code.repeat) digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));   // once per press
    break;
  case BTN_PLUS:  if (level < 10) level++; break;                             // auto-repeats while held
  case BTN_MINUS: if (level > 0) level--; break;
  }
  Serial.print("level ");
  Serial.println(level);
}
