#!/bin/sh
# Installiert die Toolchains fuer dieses Repo aus r4d1um/setup (nur was fehlt).
#   tools/session-setup.sh                 alle Toolchains unten
#   tools/session-setup.sh avr-gcc         nur die genannten
# Nur oeffentliche Toolchains: das CI-Token dieses public Repos kann setup-private nicht lesen.
set -eu
R4SETUP_REF=v1.2.0
[ "$#" -gt 0 ] || set -- avr-gcc arm-none-eabi aarch64-linux-gnu arm-linux-gnueabihf i686-linux-gnu
curl -fsSL "https://raw.githubusercontent.com/r4d1um/setup/$R4SETUP_REF/bootstrap.sh" \
  | R4SETUP_REF=$R4SETUP_REF sh -s -- "$@"
