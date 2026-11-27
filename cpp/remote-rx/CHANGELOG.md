# Changelog

## 1.0.0

- First version: interrupt capture (inlined into the ISR, IRAM-safe on ESP8266/ESP32), frames end
  on a 5 ms space, decoders for NEC (extended address, repeat codes), Samsung32, Sony SIRC
  12/15/20, RC5, RC6 mode 0 (receiver stretch corrected from the leader), timing hash for
  unknown remotes, repeat detection (pause or RC5/RC6 toggle bit).
- Arduino adapter `RemoteRx`: `read()`, `raw()`, `print()`.
