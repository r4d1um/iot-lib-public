# Shared build for Arduino libraries in this repo: an Arduino-free core, precompiled per target,
# plus a thin Arduino adapter shipped as source. Included by <sprache>/<lib>/Makefile, which sets
#
#   LIB         Arduino library name (folder and lib<LIB>.a), e.g. StampDisplay
#   CORE_SRC    .cpp/.c files of the core (no Arduino headers), e.g. src/Gfx.cpp
#   TEST_SRC    host test program(s), each built against the host .a and run by `make test`
#   EXTRA_PKG   further files for the packages (LICENSE-font.txt, ...)
#
# Targets:  make test | make dist | make package | make version-check | make print-version
#
# Results (build/pkg/, attached to the release <sprache>/<lib>/v<VERSION> by the workflow):
#   <LIB>-<v>-arduino.zip          Arduino library: headers + adapter sources + src/<mcu>/lib<LIB>.a,
#                                  library.properties with precompiled=true (arduino-cli, IDE, builder)
#   <LIB>-<v>-<target>.tar.gz      include/ + lib/lib<LIB>.a for non-Arduino builds (PC tools, tests)
#   SHA256SUMS
#
# Incremental: one source = one object per target; only changed sources are compiled again.
# Objects are named <file>.cpp.o / <file>.c.o like Arduino's own: the ESP8266 linker script puts
# only code from *.cpp.o / *.c.o into flash, anything else would land in the scarce IRAM.

VERSION  := $(shell sed -n 's/^version=//p' library.properties)
BUILD     = build
PKG       = $(BUILD)/pkg
TAR_DET   = tar --sort=name --mtime='2000-01-01 00:00Z' --owner=0 --group=0 --numeric-owner

# Targets: name, compiler, flags. ESP8266: the compiler of core 3.1.2 (xtensa-lx106-elf from
# r4d1um/setup); ESP32: GCC 8.4 of core 2.0.17, links with core 3.x too. The Arduino folder name of
# each .a is the core's build.mcu.
TARGETS        = linux-x86_64 esp8266 esp32
CXX_linux-x86_64 = g++
CC_linux-x86_64  = gcc
AR_linux-x86_64  = gcc-ar
FL_linux-x86_64  = -O2 -fPIC
CXX_esp8266    = xtensa-lx106-elf-g++
CC_esp8266     = xtensa-lx106-elf-gcc
AR_esp8266     = xtensa-lx106-elf-gcc-ar
FL_esp8266     = -Os -mlongcalls -mtext-section-literals -falign-functions=4
CXX_esp32      = xtensa-esp32-elf-g++
CC_esp32       = xtensa-esp32-elf-gcc
AR_esp32       = xtensa-esp32-elf-gcc-ar
FL_esp32       = -Os -mlongcalls -fstrict-volatile-bitfields
MCU_TARGETS    = esp8266 esp32

WARN      = -Wall -Wextra -Werror
COMMON    = -ffunction-sections -fdata-sections -Isrc $(WARN) $(EXTRA_CFLAGS)
# with RTTI: sketches built with -frtti (ESP32 cores) need the typeinfo; -fno-rtti users ignore it
CXXSTD    = -std=gnu++17 -fno-exceptions
CSTD      = -std=gnu99
CCACHE   ?= $(shell command -v ccache 2>/dev/null)

HEADERS   = $(wildcard src/*.h)
ADAPTER   = $(filter-out $(CORE_SRC),$(wildcard src/*.cpp src/*.c))

.PHONY: all test dist package version-check print-version clean
all: test

define TARGET_RULES
$(BUILD)/$(1)/%.cpp.o: src/%.cpp $(HEADERS) Makefile
	@mkdir -p $$(@D)
	$(CCACHE) $(CXX_$(1)) $(CXXSTD) $(FL_$(1)) $(COMMON) -c $$< -o $$@
$(BUILD)/$(1)/%.c.o: src/%.c $(HEADERS) Makefile
	@mkdir -p $$(@D)
	$(CCACHE) $(CC_$(1)) $(CSTD) $(FL_$(1)) $(COMMON) -c $$< -o $$@
$(BUILD)/$(1)/lib$(LIB).a: $(patsubst src/%,$(BUILD)/$(1)/%.o,$(CORE_SRC))
	rm -f $$@ && $(AR_$(1)) rcsD $$@ $$^
endef
$(foreach t,$(TARGETS),$(eval $(call TARGET_RULES,$(t))))

dist: $(foreach t,$(TARGETS),$(BUILD)/$(t)/lib$(LIB).a)

# Host tests link the host archive like any user of the package would.
test: $(BUILD)/linux-x86_64/lib$(LIB).a
	@set -e; for t in $(TEST_SRC); do \
	  echo "== $$t"; \
	  g++ -std=gnu++17 -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc $(EXTRA_CFLAGS) \
	    $$t $(BUILD)/linux-x86_64/lib$(LIB).a $(TEST_LIBS) -o $(BUILD)/$$(basename $$t .cpp) && ./$(BUILD)/$$(basename $$t .cpp); \
	done

version-check:
	@test -n "$(VERSION)" || { echo "library.properties: no version"; exit 1; }
	@grep -q '^## $(VERSION)' CHANGELOG.md || { echo "CHANGELOG.md: no entry for $(VERSION)"; exit 1; }
	@echo "$(LIB) $(VERSION)"

print-version:
	@echo $(VERSION)

package: test dist version-check
	rm -rf $(PKG) $(BUILD)/stage && mkdir -p $(PKG)
	@# Arduino library: precompiled core per build.mcu, adapter and headers as source
	d=$(BUILD)/stage/arduino/$(LIB); mkdir -p $$d/src; \
	  cp $(HEADERS) $(ADAPTER) $$d/src/; \
	  for t in $(MCU_TARGETS); do mkdir -p $$d/src/$$t; cp $(BUILD)/$$t/lib$(LIB).a $$d/src/$$t/; done; \
	  grep -v '^\(precompiled\|ldflags\)=' library.properties > $$d/library.properties; \
	  printf 'precompiled=true\nldflags=-l$(LIB)\n' >> $$d/library.properties; \
	  cp README.md CHANGELOG.md $(EXTRA_PKG) $$d/; \
	  [ -d examples ] && cp -r examples $$d/ || true; \
	  cd $(BUILD)/stage/arduino && find $(LIB) -exec touch -d '2000-01-01 00:00Z' {} + && \
	  find $(LIB) | LC_ALL=C sort | zip -qX -@ ../../pkg/$(LIB)-$(VERSION)-arduino.zip
	@# plain archives per target
	set -e; for t in $(TARGETS); do \
	  d=$(BUILD)/stage/$(LIB)-$(VERSION)-$$t; mkdir -p $$d/include $$d/lib; \
	  cp $(HEADERS) $$d/include/; cp $(BUILD)/$$t/lib$(LIB).a $$d/lib/; cp README.md CHANGELOG.md $(EXTRA_PKG) $$d/; \
	  $(TAR_DET) -C $(BUILD)/stage -cf - $(LIB)-$(VERSION)-$$t | gzip -n > $(PKG)/$(LIB)-$(VERSION)-$$t.tar.gz; \
	done
	cd $(PKG) && sha256sum * > SHA256SUMS && cat SHA256SUMS

clean:
	rm -rf $(BUILD)
