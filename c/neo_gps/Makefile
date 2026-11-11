CC      ?= cc
CFLAGS  ?= -std=c99 -D_DEFAULT_SOURCE -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wcast-align
SRC      = src/neo_gps.c src/neo_gps_filter.c src/neo_gps_time.c src/neo_gps_util.c

.PHONY: all test bench linux sim clean

all: test linux

build:
	mkdir -p build

build/test_neo_gps: $(SRC) src/neo_gps.h tests/test_neo_gps.c | build
	$(CC) $(CFLAGS) -Isrc $(SRC) tests/test_neo_gps.c -lm -o $@

test: build/test_neo_gps
	./build/test_neo_gps

bench: build/test_neo_gps
	NEO_BENCH=1 ./build/test_neo_gps

linux: $(SRC) examples/linux/main.c | build
	$(CC) $(CFLAGS) -D_DEFAULT_SOURCE -Isrc $(SRC) examples/linux/main.c -lm -o build/neo_gps_linux

sim: $(SRC) examples/simulation/terrain_sim.c | build
	$(CC) $(CFLAGS) -Isrc $(SRC) examples/simulation/terrain_sim.c -lm -o build/terrain_sim
	./build/terrain_sim > build/sim.json
	cp examples/simulation/index.html build/index.html
	@echo "build/sim.json + build/index.html geschrieben - ansehen: cd build && python3 -m http.server"

clean:
	rm -rf build

# ---------------------------------------------------------------------------
# Fertige Binaries: dist/<ziel>/libneo_gps.a + dist/include/*.h
# Wird in GitHub Actions einmal gebaut und nach dist/ committet; Projekte
# kopieren nur die .a und die Header. Inkrementell: jede .c ist eine eigene .o,
# nur geaenderte Quellen werden neu uebersetzt (ccache wird genutzt, wenn da).
# Fehlt ein Cross-Compiler, wird das Ziel uebersprungen (DIST_STRICT=1: Fehler).
# ---------------------------------------------------------------------------
CCACHE    ?= $(shell command -v ccache 2>/dev/null)
DIST       = dist
DIST_HDR   = src/neo_gps.h src/neo_gps_util.h src/neo_gps_time.h
DIST_W     = -std=c99 -Wall -Wextra -Wpedantic -Wconversion -Werror -Isrc
DIST_HOST  = -O2 -fPIC -D_DEFAULT_SOURCE
DIST_MCU   = -Os -ffunction-sections -fdata-sections

DIST_TARGETS = linux-x86_64 linux-i686 linux-armv6hf linux-aarch64 \
               avr-atmega328p cortex-m0 cortex-m0plus cortex-m3 cortex-m4f cortex-m33

CC_linux-x86_64    = gcc
FL_linux-x86_64    = $(DIST_HOST)
CC_linux-i686      = i686-linux-gnu-gcc
FL_linux-i686      = $(DIST_HOST)
CC_linux-armv6hf   = arm-linux-gnueabihf-gcc
FL_linux-armv6hf   = $(DIST_HOST) -march=armv6+fp -mfpu=vfp -mfloat-abi=hard -marm
CC_linux-aarch64   = aarch64-linux-gnu-gcc
FL_linux-aarch64   = $(DIST_HOST)
CC_avr-atmega328p  = avr-gcc
FL_avr-atmega328p  = $(DIST_MCU) -mmcu=atmega328p
CC_cortex-m0       = arm-none-eabi-gcc
FL_cortex-m0       = $(DIST_MCU) -mcpu=cortex-m0 -mthumb
CC_cortex-m0plus   = arm-none-eabi-gcc
FL_cortex-m0plus   = $(DIST_MCU) -mcpu=cortex-m0plus -mthumb
CC_cortex-m3       = arm-none-eabi-gcc
FL_cortex-m3       = $(DIST_MCU) -mcpu=cortex-m3 -mthumb
CC_cortex-m4f      = arm-none-eabi-gcc
FL_cortex-m4f      = $(DIST_MCU) -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CC_cortex-m33      = arm-none-eabi-gcc
FL_cortex-m33      = $(DIST_MCU) -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16

AR_FOR = $(patsubst %gcc,%gcc-ar,$(CC_$(1)))

define DIST_RULES
build/dist/$(1)/%.o: src/%.c $(DIST_HDR) Makefile
	@mkdir -p $$(@D)
	$(CCACHE) $(CC_$(1)) $(DIST_W) $(FL_$(1)) -c $$< -o $$@

$(DIST)/$(1)/libneo_gps.a: $(patsubst src/%.c,build/dist/$(1)/%.o,$(SRC))
	@mkdir -p $$(@D)
	rm -f $$@ && $(call AR_FOR,$(1)) rcsD $$@ $$^

dist-$(1):
	+@if command -v $(CC_$(1)) >/dev/null 2>&1; then $(MAKE) --no-print-directory $(DIST)/$(1)/libneo_gps.a; \
	 elif [ -n "$(DIST_STRICT)" ]; then echo "fehlt: $(CC_$(1))"; exit 1; \
	 else echo "uebersprungen: $(1) ($(CC_$(1)) fehlt)"; fi
endef
$(foreach t,$(DIST_TARGETS),$(eval $(call DIST_RULES,$(t))))

$(DIST)/include/%.h: src/%.h
	@mkdir -p $(@D)
	cp $< $@

.PHONY: dist dist-check $(addprefix dist-,$(DIST_TARGETS))
dist: $(patsubst src/%,$(DIST)/include/%,$(DIST_HDR)) $(addprefix dist-,$(DIST_TARGETS))
	@sed -n 's/^#define NEO_GPS_VERSION "\(.*\)"/\1/p' src/neo_gps.h > $(DIST)/VERSION

# Linktest: Host-Tests gegen die fertige .a, Cortex-M0+ als Firmware gelinkt
dist-check: dist | build
	$(CC) $(CFLAGS) -I$(DIST)/include tests/test_neo_gps.c $(DIST)/linux-x86_64/libneo_gps.a -lm -o build/test_dist && ./build/test_dist
	@if command -v arm-none-eabi-gcc >/dev/null 2>&1; then \
	  arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os --specs=nosys.specs -Wl,--gc-sections -I$(DIST)/include \
	    tests/link_mcu.c $(DIST)/cortex-m0plus/libneo_gps.a -lm -o build/link_m0plus.elf && \
	  arm-none-eabi-size build/link_m0plus.elf; fi
	@if command -v avr-gcc >/dev/null 2>&1; then \
	  avr-gcc -mmcu=atmega328p -Os -Wl,--gc-sections -I$(DIST)/include \
	    tests/link_mcu.c $(DIST)/avr-atmega328p/libneo_gps.a -lm -o build/link_avr.elf && \
	  avr-size build/link_avr.elf; fi

# ---------------------------------------------------------------------------
# Release-Pakete nach build/pkg/ (GitHub Actions haengt sie an das Release
# c/neo_gps/v<VERSION>):
#   neo_gps-<v>-<ziel>.tar.gz  fertige .a + include/ + README + LICENSE
#   neo_gps-<v>-src.tar.gz     Quellpaket fuer PlatformIO, Arduino, ESP-IDF, CMake
#   SHA256SUMS
# Archive deterministisch (feste Zeit, Besitzer, Sortierung), damit gleiche
# Quellen gleiche Pakete ergeben.
# ---------------------------------------------------------------------------
VERSION  := $(shell sed -n 's/^#define NEO_GPS_VERSION "\(.*\)"/\1/p' src/neo_gps.h)
PKG       = build/pkg
TAR_DET   = tar --sort=name --mtime='2000-01-01 00:00Z' --owner=0 --group=0 --numeric-owner
SRC_FILES = src library.json library.properties CMakeLists.txt Makefile LICENSE README.md CHANGELOG.md examples

.PHONY: package version-check
package: dist-check version-check
	rm -rf $(PKG) && mkdir -p $(PKG)
	set -e; for t in $(DIST_TARGETS); do \
	  d=build/stage/neo_gps-$(VERSION)-$$t; rm -rf $$d; mkdir -p $$d/lib; \
	  cp -r $(DIST)/include $$d/; cp $(DIST)/$$t/libneo_gps.a $$d/lib/; cp README.md LICENSE CHANGELOG.md $$d/; \
	  $(TAR_DET) -C build/stage -cf - neo_gps-$(VERSION)-$$t | gzip -n > $(PKG)/neo_gps-$(VERSION)-$$t.tar.gz; \
	done
	d=build/stage/neo_gps-$(VERSION)-src; rm -rf $$d; mkdir -p $$d; cp -r $(SRC_FILES) $$d/; \
	  $(TAR_DET) -C build/stage -cf - neo_gps-$(VERSION)-src | gzip -n > $(PKG)/neo_gps-$(VERSION)-src.tar.gz
	cd $(PKG) && sha256sum *.tar.gz > SHA256SUMS && cat SHA256SUMS

# Version muss in Header, library.json, library.properties und CHANGELOG gleich sein
version-check:
	@grep -q '"version": "$(VERSION)"' library.json        || { echo "library.json: Version != $(VERSION)"; exit 1; }
	@grep -q '^version=$(VERSION)$$' library.properties    || { echo "library.properties: Version != $(VERSION)"; exit 1; }
	@grep -q '^## $(VERSION)' CHANGELOG.md                 || { echo "CHANGELOG.md: kein Eintrag fuer $(VERSION)"; exit 1; }
	@echo "Version $(VERSION) ueberall gleich"

print-version:
	@echo $(VERSION)
