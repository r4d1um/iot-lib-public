# Shift595: the shift595 core (C99) precompiled; the Arduino adapter Shift595.cpp is source.
LIB        = Shift595
CORE_SRC   = src/shift595.c
TEST_SRC   = tests/test_shift595.cpp
EXTRA_PKG  = LICENSE

# Extra targets on top of the shared make file: AVR boards (Uno/Nano/Pro Mini, Mega), with the
# avr-gcc 7.3 of Arduino AVR core 1.8.6. The folder name of each .a is the core's build.mcu.
override TARGETS     = linux-x86_64 esp8266 esp32 atmega328p atmega2560
override MCU_TARGETS = esp8266 esp32 atmega328p atmega2560
CC_atmega328p  = avr-gcc
AR_atmega328p  = avr-gcc-ar
FL_atmega328p  = -Os -mmcu=atmega328p
CC_atmega2560  = avr-gcc
AR_atmega2560  = avr-gcc-ar
FL_atmega2560  = -Os -mmcu=atmega2560
include ../../tools/mk/arduino-lib.mk

# Real firmware (Arduino Uno, arduino-cli + Arduino AVR core 1.8.6) against the packaged zip.
.PHONY: firmware-check
firmware-check: package
	rm -rf $(BUILD)/fw && mkdir -p $(BUILD)/fw/libraries
	cd $(BUILD)/fw/libraries && unzip -q ../../pkg/$(LIB)-$(VERSION)-arduino.zip
	arduino-cli compile -b arduino:avr:uno --libraries $(BUILD)/fw/libraries \
	  --build-path $(BUILD)/fw/out tests/firmware/FirmwareCheck

