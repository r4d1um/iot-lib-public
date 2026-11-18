# Changelog

## 1.0.0

- First version: MFRC522 init, REQA/WUPA, anticollision over all cascade levels (4/7/10-byte
  UIDs, several cards in the field), SELECT, HLTA, card type from SAK, MIFARE Classic
  auth/read/write (UID3..UID6 for 7-byte UIDs), Ultralight/NTAG read/write, CRC_A in software.
- Arduino adapter `Rc522`: hardware SPI, reset pin, `find()`/`findAny()`/`halt()`, hex UIDs.
