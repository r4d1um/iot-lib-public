/* hd44780_screen - see hd44780_screen.h. */
#include "hd44780_screen.h"
#include "hd44780_text.h"

#define BAR_ROW(k) ((uint8_t)((0x1Fu << (5u - (k))) & 0x1Fu))
#define BAR_CHAR(k) { BAR_ROW(k), BAR_ROW(k), BAR_ROW(k), BAR_ROW(k), BAR_ROW(k), BAR_ROW(k), BAR_ROW(k), 0 }
static const uint8_t bar_chars[4][8] = { BAR_CHAR(1), BAR_CHAR(2), BAR_CHAR(3), BAR_CHAR(4) };

const uint8_t *hd44780_screen_bar_chars(uint8_t i) { return i < 4u ? bar_chars[i] : 0; }

static uint16_t idx(const hd44780_screen *s, uint8_t col, uint8_t row)
{
    return (uint16_t)((uint16_t)row * s->cols + col);
}

static uint16_t cells(const hd44780_screen *s) { return (uint16_t)((uint16_t)s->cols * s->rows); }

void hd44780_screen_init(hd44780_screen *s, uint8_t cols, uint8_t rows, hd44780_rom rom, uint8_t *mem)
{
    if (cols < 1u) cols = 1u;
    if (cols > 40u) cols = 40u;
    if (rows < 1u) rows = 1u;
    if (rows > 4u) rows = 4u;
    s->cols = cols;
    s->rows = rows;
    s->rom = rom;
    s->cell = mem;
    s->shown = mem + cells(s);
    s->known = mem + 2u * cells(s);
    hd44780_screen_clear(s);
    hd44780_screen_invalidate(s);
}

void hd44780_screen_clear(hd44780_screen *s)
{
    uint16_t i;
    for (i = 0; i < cells(s); i++) s->cell[i] = ' ';
}

void hd44780_screen_invalidate(hd44780_screen *s)
{
    uint16_t i;
    for (i = 0; i < (cells(s) + 7u) / 8u; i++) s->known[i] = 0;
}

void hd44780_screen_assume_blank(hd44780_screen *s)
{
    uint16_t i;
    for (i = 0; i < cells(s); i++) s->shown[i] = ' ';
    for (i = 0; i < (cells(s) + 7u) / 8u; i++) s->known[i] = 0xFFu;
}

void hd44780_screen_put(hd44780_screen *s, uint8_t col, uint8_t row, uint8_t code)
{
    if (col < s->cols && row < s->rows) s->cell[idx(s, col, row)] = code;
}

uint8_t hd44780_screen_get(const hd44780_screen *s, uint8_t col, uint8_t row)
{
    return (col < s->cols && row < s->rows) ? s->cell[idx(s, col, row)] : (uint8_t)' ';
}

uint8_t hd44780_screen_text(hd44780_screen *s, uint8_t col, uint8_t row, uint8_t width,
                            const char *utf8, hd44780_align align)
{
    uint8_t buf[40], n = 0, pad, i;
    if (col >= s->cols || row >= s->rows) return 0;
    if (width == 0u || width > s->cols - col) width = (uint8_t)(s->cols - col);
    while (utf8 && *utf8 && n < width) {
        uint32_t cp;
        utf8 = hd44780_utf8_next(utf8, 0, &cp);
        if (cp == '\n' || cp == '\r') break;
        buf[n++] = hd44780_map(cp, s->rom);
    }
    pad = align == HD44780_RIGHT ? (uint8_t)(width - n) : align == HD44780_CENTER ? (uint8_t)((width - n) / 2u) : 0u;
    for (i = 0; i < width; i++) {
        uint8_t c = (i >= pad && i < pad + n) ? buf[i - pad] : (uint8_t)' ';
        s->cell[idx(s, (uint8_t)(col + i), row)] = c;
    }
    return n;
}

void hd44780_screen_bar(hd44780_screen *s, uint8_t col, uint8_t row, uint8_t width,
                        uint32_t value, uint32_t max, uint8_t base)
{
    uint32_t dots, full, part;
    uint8_t i;
    if (col >= s->cols || row >= s->rows) return;
    if (width == 0u || width > s->cols - col) width = (uint8_t)(s->cols - col);
    if (max == 0u) max = 1u;
    if (value > max) value = max;
    /* value * width * 5 / max without 32-bit overflow for value up to ~2^32/200 */
    dots = (uint32_t)(((uint64_t)value * width * 5u + max / 2u) / max);
    full = dots / 5u;
    part = dots % 5u;
    for (i = 0; i < width; i++) {
        uint8_t c = ' ';
        if (i < full) c = 0xFFu;
        else if (i == full && part) c = (uint8_t)(base + part - 1u);
        s->cell[idx(s, (uint8_t)(col + i), row)] = c;
    }
}

uint16_t hd44780_screen_flush(hd44780_screen *s, hd44780 *lcd)
{
    uint16_t written = 0;
    uint8_t row, col;
    for (row = 0; row < s->rows && row < lcd->rows; row++) {
        int at = 0; /* lcd cursor is right here (after the previous written cell) */
        for (col = 0; col < s->cols && col < lcd->cols; col++) {
            uint16_t i = idx(s, col, row);
            int known = (s->known[i >> 3] >> (i & 7u)) & 1;
            if (known && s->shown[i] == s->cell[i]) { at = 0; continue; }
            if (!at) hd44780_set_cursor(lcd, col, row);
            hd44780_write(lcd, s->cell[i]);
            s->shown[i] = s->cell[i];
            s->known[i >> 3] = (uint8_t)(s->known[i >> 3] | (1u << (i & 7u)));
            at = 1;
            written++;
        }
    }
    return written;
}
