/* hd44780_text - UTF-8 to character ROM codes (A00 Japanese, A02 European). */
#ifndef HD44780_TEXT_H
#define HD44780_TEXT_H

#include <stdint.h>
#include "hd44780.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Decode one UTF-8 code point from s into *cp; returns the position after it. end is the end
 * of the text (NULL: NUL-terminated); call only while s < end. Invalid or cut-off sequences
 * decode as U+FFFD, one byte at a time. */
const char *hd44780_utf8_next(const char *s, const char *end, uint32_t *cp);

/* ROM code for a code point. ASCII maps to itself except where the ROM differs
 * (A00: '\\' -> '/', '~' -> '-'). A00: ä ö ü ß ° µ Ω π Σ ÷ √ ∞ ¢ → ← and · ; capital umlauts
 * fall back to A O U. A02: code points U+00A0..U+00FF pass through as Latin-1 (Ä Ö Ü ä ö ü ß é °);
 * checked against the datasheet table only, not on a real A02 module yet.
 * U+0000..U+0007 are the custom characters (CGRAM 0..7, "\x01" in a string).
 * Anything without a glyph becomes '?'. */
uint8_t hd44780_map(uint32_t cp, hd44780_rom rom);

#ifdef __cplusplus
}
#endif

#endif
