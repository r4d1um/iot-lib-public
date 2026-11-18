# CharLcd: hd44780 core (C99: protocol, UTF-8, PCF8574 encoding, screen buffer) precompiled;
# the Arduino adapter CharLcd.cpp (Wire / digitalWrite) is source.
LIB        = CharLcd
CORE_SRC   = src/hd44780.c src/hd44780_text.c src/hd44780_pcf8574.c src/hd44780_595.c src/hd44780_screen.c
TEST_SRC   = tests/test_char_lcd.cpp
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

# Real firmware (Arduino Uno, arduino-cli + AVR core from r4d1um/setup) against the packaged zip.
.PHONY: firmware-check screenshots
firmware-check: package
	rm -rf $(BUILD)/fw && mkdir -p $(BUILD)/fw/libraries
	cd $(BUILD)/fw/libraries && unzip -q ../../pkg/$(LIB)-$(VERSION)-arduino.zip
	arduino-cli compile -b arduino:avr:uno --libraries $(BUILD)/fw/libraries \
	  --build-path $(BUILD)/fw/out tests/firmware/FirmwareCheck

# PNG screenshots of sample screens, drawn through the core and the fake controller.
screenshots: $(BUILD)/linux-x86_64/lib$(LIB).a
	g++ -std=gnu++17 -O1 -Wall -Wextra -Werror -Isrc tools/screens/screens.cpp $< -o $(BUILD)/screens-dump
	$(BUILD)/screens-dump | (cd tools/preview && go run . -o ../../$(BUILD)/screens)
