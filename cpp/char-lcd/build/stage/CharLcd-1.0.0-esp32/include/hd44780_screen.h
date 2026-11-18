/* hd44780_screen - screen buffer for HD44780 displays: draw into RAM, then flush only the cells
 * that changed (few bus transfers, no flicker, no clear()). Logic only; flushing uses hd44780. */
#ifndef HD44780_SCREEN_H
#define HD44780_SCREEN_H

#include <stddef.h>
#include <stdint.h>
#include "hd44780.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum { HD44780_LEFT = 0, HD44780_RIGHT = 1, HD44780_CENTER = 2 } hd44780_align;

typedef struct {
    uint8_t     cols, rows;
    hd44780_rom rom;
    uint8_t    *cell;   /* what should be shown, cols*rows */
    uint8_t    *shown;  /* what the display shows, cols*rows */
    uint8_t    *known;  /* bit set: shown[] is valid, (cols*rows+7)/8 */
} hd44780_screen;

/* Bytes of memory a cols x rows buffer needs (20x4: 210). */
#define HD44780_SCREEN_MEM(cols, rows) (2u * (cols) * (rows) + ((cols) * (rows) + 7u) / 8u)

/* mem: HD44780_SCREEN_MEM(cols, rows) bytes, owned by the caller (static array, stack, ...).
 * All blank; every cell unknown, so the first flush writes the whole screen.
 * cols/rows are clamped to 1..40 / 1..4 - size mem for the clamped values. */
void hd44780_screen_init(hd44780_screen *s, uint8_t cols, uint8_t rows, hd44780_rom rom, uint8_t *mem);
void hd44780_screen_clear(hd44780_screen *s);              /* buffer to blanks */
void hd44780_screen_invalidate(hd44780_screen *s);         /* display content unknown (after clear(), splash, ...) */
void hd44780_screen_assume_blank(hd44780_screen *s);       /* display was just cleared */

void    hd44780_screen_put(hd44780_screen *s, uint8_t col, uint8_t row, uint8_t code); /* off-screen ignored */
uint8_t hd44780_screen_get(const hd44780_screen *s, uint8_t col, uint8_t row);

/* Text field: UTF-8 text in width cells from (col,row), aligned, padded with blanks so the
 * field's old content is gone. Text longer than width is cut. width 0 = to the row end.
 * Returns the cells the text itself used. */
uint8_t hd44780_screen_text(hd44780_screen *s, uint8_t col, uint8_t row, uint8_t width,
                            const char *utf8, hd44780_align align);

/* Bar graph of value/max over width cells with 5 steps per cell. Needs 4 CGRAM characters with
 * 1..4 left columns lit in slots base..base+3 (hd44780_screen_bar_chars); full cells use 0xFF. */
void hd44780_screen_bar(hd44780_screen *s, uint8_t col, uint8_t row, uint8_t width,
                        uint32_t value, uint32_t max, uint8_t base);
/* Pattern i (0..3) for the bar characters: i+1 left columns, rows 0..6. */
const uint8_t *hd44780_screen_bar_chars(uint8_t i);

/* Write changed cells to the display, moving the cursor only where a run of changes starts.
 * Returns the number of cells written. */
uint16_t hd44780_screen_flush(hd44780_screen *s, hd44780 *lcd);

#ifdef __cplusplus
}
#endif

#endif
