#!/bin/sh
# SPDX-License-Identifier: MIT
# Fetches the pinned ESP cross compilers (the ones Arduino ESP8266 core 3.1.2 and ESP32 core 2.0.17
# use) from their upstream releases, verifies SHA-256 and unpacks only what is missing.
#   tools/toolchains.sh                     all
#   tools/toolchains.sh xtensa-esp32-elf    only the named ones
# Prints the bin folders; inside GitHub Actions they are also appended to $GITHUB_PATH.
# AVR comes from the distro (apt install gcc-avr avr-libc), arduino-cli from its official installer.
set -eu
DIR=${TOOLCHAINS_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/iot-lib-public/toolchains}
WANT=" ${*:-xtensa-lx106-elf xtensa-esp32-elf} "

pin() { # name sha256 url
  case "$WANT" in *" $1 "*) WANT=$(echo "$WANT" | sed "s/ $1 / /") ;; *) return 0 ;; esac
  if [ ! -f "$DIR/$1/.sha256-$2" ]; then
    echo "toolchains: fetching $1" >&2
    mkdir -p "$DIR" && rm -rf "${DIR:?}/${1:?}" "$DIR/$1.tmp" && mkdir "$DIR/$1.tmp"
    curl -fsSL --retry 3 -o "$DIR/$1.tar.gz" "$3"
    echo "$2  $DIR/$1.tar.gz" | sha256sum -c --quiet -
    tar -xzf "$DIR/$1.tar.gz" -C "$DIR/$1.tmp" --strip-components=1
    touch "$DIR/$1.tmp/.sha256-$2" && rm "$DIR/$1.tar.gz" && mv "$DIR/$1.tmp" "$DIR/$1"
  fi
  echo "$DIR/$1/bin"
  [ -z "${GITHUB_PATH:-}" ] || echo "$DIR/$1/bin" >> "$GITHUB_PATH"
}

pin xtensa-lx106-elf 89d21f38a516e34e0bb44df7e30ef6b1bc2f7f125aae3b7986e5cae545a3ed83 \
  https://github.com/earlephilhower/esp-quick-toolchain/releases/download/3.1.0-gcc10.3/x86_64-linux-gnu.xtensa-lx106-elf-e5f9fec.220621.tar.gz
pin xtensa-esp32-elf 8ef14e0409c2011b41e504a30f70d3e35287313a795d1f2462ad2cd0e2052d37 \
  https://github.com/espressif/crosstool-NG/releases/download/esp-2021r2-patch5/xtensa-esp32-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz

[ "$WANT" = " " ] || { echo "toolchains: unknown:$WANT" >&2; exit 2; }
