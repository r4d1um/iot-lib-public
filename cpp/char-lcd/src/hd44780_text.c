/* hd44780_text - see hd44780_text.h. Tables after the HD44780U datasheet, table 4. */
#include "hd44780_text.h"

const char *hd44780_utf8_next(const char *s, const char *end, uint32_t *cp)
{
    const unsigned char *p = (const unsigned char *)s;
    uint32_t c = p[0];
    int n, i;
    if (c < 0x80u) { *cp = c; return s + 1; }
    if ((c & 0xE0u) == 0xC0u) { n = 1; c &= 0x1Fu; }
    else if ((c & 0xF0u) == 0xE0u) { n = 2; c &= 0x0Fu; }
    else if ((c & 0xF8u) == 0xF0u) { n = 3; c &= 0x07u; }
    else { *cp = 0xFFFDu; return s + 1; }
    if (end && end - s <= n) { *cp = 0xFFFDu; return s + 1; }
    for (i = 1; i <= n; i++) {
        if ((p[i] & 0xC0u) != 0x80u) { *cp = 0xFFFDu; return s + 1; }
        c = (c << 6) | (p[i] & 0x3Fu);
    }
    *cp = c;
    return s + n + 1;
}

typedef struct { uint16_t cp; uint8_t code; } pair;

static const pair a00[] = {
    {0x00E4, 0xE1}, {0x00F6, 0xEF}, {0x00FC, 0xF5}, {0x00DF, 0xE2}, /* ä ö ü ß */
    {0x00C4, 'A'},  {0x00D6, 'O'},  {0x00DC, 'U'},                  /* Ä Ö Ü fall back */
    {0x00B0, 0xDF}, {0x00B5, 0xE4}, {0x03BC, 0xE4}, {0x2126, 0xF4}, /* ° µ μ Ω */
    {0x03A9, 0xF4}, {0x03C0, 0xF7}, {0x03A3, 0xF6}, {0x00F7, 0xFD}, /* Ω π Σ ÷ */
    {0x221A, 0xE8}, {0x221E, 0xF3}, {0x00A2, 0xEC}, {0x00B7, 0xA5}, /* √ ∞ ¢ · */
    {0x2192, 0x7E}, {0x2190, 0x7F}, {0x00A5, 0x5C}, {0x2588, 0xFF}, /* → ← ¥ █ */
    {0x03B1, 0xE0}, {0x03B2, 0xE2}, {0x03B5, 0xE3}, {0x03C3, 0xE5}, /* α β ε σ */
    {0x03C1, 0xE6}, {0x03B8, 0xF2},                                 /* ρ θ */
    {'\\', '/'},    {'~', '-'},                                     /* not in A00 */
};

static const pair a02[] = {
    {0x2588, 0xFF}, /* █; the rest of the upper half follows Latin-1 below */
};

uint8_t hd44780_map(uint32_t cp, hd44780_rom rom)
{
    const pair *t = rom == HD44780_ROM_A02 ? a02 : a00;
    size_t n = rom == HD44780_ROM_A02 ? sizeof a02 / sizeof a02[0] : sizeof a00 / sizeof a00[0];
    size_t i;
    for (i = 0; i < n; i++)
        if (t[i].cp == cp) return t[i].code;
    if (cp < 8u) return (uint8_t)cp; /* custom characters 0..7 */
    if (cp >= 0x20u && cp < 0x7Fu) return (uint8_t)cp;
    if (rom == HD44780_ROM_A02 && cp >= 0xA0u && cp <= 0xFFu) return (uint8_t)cp; /* Latin-1 */
    return '?';
}
