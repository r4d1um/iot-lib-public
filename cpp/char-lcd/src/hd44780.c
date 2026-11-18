/* hd44780 core - see hd44780.h. */
#include "hd44780.h"
#include "hd44780_text.h"

#define CMD_CLEAR    0x01u
#define CMD_HOME     0x02u
#define CMD_ENTRY    0x04u
#define CMD_CONTROL  0x08u
#define CMD_SHIFT    0x10u
#define CMD_FUNCTION 0x20u
#define CMD_CGRAM    0x40u
#define CMD_DDRAM    0x80u

#define ENTRY_INC    0x02u
#define CTL_DISPLAY  0x04u
#define CTL_CURSOR   0x02u
#define CTL_BLINK    0x01u
#define SHIFT_SCREEN 0x08u
#define SHIFT_RIGHT  0x04u
#define FN_2LINE     0x08u

#define T_CMD_US      40u  /* most commands: 37 us */
#define T_SLOW_US   1600u  /* clear, home: 1.52 ms */

static void send(hd44780 *l, uint8_t v, uint8_t flags, uint32_t wait_us)
{
    int e = l->bus.send(l->bus.ctx, v, flags);
    if (e && !l->error) l->error = e;
    if (wait_us) l->bus.delay_us(l->bus.ctx, wait_us);
}

/* chip of a row: second controller for rows 2..3 of a 40x4 display */
static uint8_t chip_of(const hd44780 *l, uint8_t row)
{
    return (l->two_chips && row >= 2u) ? HD44780_CHIP2 : 0u;
}

static void command_all(hd44780 *l, uint8_t cmd, uint32_t wait_us)
{
    send(l, cmd, 0u, wait_us);
    if (l->two_chips) send(l, cmd, HD44780_CHIP2, wait_us);
}

uint8_t hd44780_address(const hd44780 *l, uint8_t col, uint8_t row)
{
    if (row >= l->rows) row = (uint8_t)(l->rows - 1u);
    if (col >= l->cols) col = (uint8_t)(l->cols - 1u);
    return (uint8_t)(l->row_offset[row] + col);
}

static void init_chip(hd44780 *l, uint8_t chip, uint8_t lines2)
{
    /* datasheet figure 24: three times "8-bit", then switch to 4-bit */
    send(l, 0x30u, HD44780_NIBBLE | chip, 4500u);
    send(l, 0x30u, HD44780_NIBBLE | chip, 150u);
    send(l, 0x30u, HD44780_NIBBLE | chip, 150u);
    send(l, 0x20u, HD44780_NIBBLE | chip, 150u);
    send(l, (uint8_t)(CMD_FUNCTION | (lines2 ? FN_2LINE : 0u)), chip, T_CMD_US);
    send(l, (uint8_t)(CMD_CONTROL | l->control), chip, T_CMD_US);
    send(l, CMD_CLEAR, chip, T_SLOW_US);
    send(l, (uint8_t)(CMD_ENTRY | l->entry), chip, T_CMD_US);
}

int hd44780_init(hd44780 *l, const hd44780_bus *bus, uint8_t cols, uint8_t rows)
{
    if (cols < 1u) cols = 1u;
    if (cols > 40u) cols = 40u;
    if (rows < 1u) rows = 1u;
    if (rows > 4u) rows = 4u;
    l->bus = *bus;
    l->cols = cols;
    l->rows = rows;
    l->two_chips = (uint8_t)(rows == 4u && cols > 20u);
    l->row_offset[0] = 0x00u;
    l->row_offset[1] = 0x40u;
    if (l->two_chips) {            /* 40x4: each controller drives a 40x2 half */
        l->row_offset[2] = 0x00u;
        l->row_offset[3] = 0x40u;
    } else {                       /* 16x4: 0x10/0x50, 20x4: 0x14/0x54 */
        l->row_offset[2] = cols;
        l->row_offset[3] = (uint8_t)(0x40u + cols);
    }
    l->control = CTL_DISPLAY;
    l->entry = ENTRY_INC;
    l->col = l->row = 0;
    l->rom = HD44780_ROM_A00;
    l->error = 0;

    l->bus.delay_us(l->bus.ctx, 50000u); /* power-up: Vcc must be above 4.5 V for 40 ms */
    init_chip(l, 0u, rows > 1u);
    if (l->two_chips) init_chip(l, HD44780_CHIP2, 1u);
    return l->error;
}

void hd44780_command(hd44780 *l, uint8_t cmd)
{
    command_all(l, cmd, (cmd == CMD_CLEAR || (cmd & 0xFEu) == CMD_HOME) ? T_SLOW_US : T_CMD_US);
}

void hd44780_clear(hd44780 *l)
{
    command_all(l, CMD_CLEAR, T_SLOW_US);
    l->col = l->row = 0;
}

void hd44780_home(hd44780 *l)
{
    command_all(l, CMD_HOME, T_SLOW_US);
    l->col = l->row = 0;
}

void hd44780_set_cursor(hd44780 *l, uint8_t col, uint8_t row)
{
    if (row >= l->rows) row = (uint8_t)(l->rows - 1u);
    if (col >= l->cols) col = (uint8_t)(l->cols - 1u);
    l->col = col;
    l->row = row;
    send(l, (uint8_t)(CMD_DDRAM | hd44780_address(l, col, row)), chip_of(l, row), T_CMD_US);
}

void hd44780_write(hd44780 *l, uint8_t code)
{
    send(l, code, (uint8_t)(HD44780_RS | chip_of(l, l->row)), T_CMD_US);
    if (l->col < 0xFFu) l->col++;
}

void hd44780_write_n(hd44780 *l, const uint8_t *codes, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) hd44780_write(l, codes[i]);
}

size_t hd44780_print(hd44780 *l, const char *s)
{
    size_t len = 0;
    if (s) while (s[len]) len++;
    return hd44780_print_n(l, s, len);
}

size_t hd44780_print_n(hd44780 *l, const char *s, size_t len)
{
    size_t n = 0;
    const char *end = s + len;
    while (s && s < end) {
        uint32_t cp;
        s = hd44780_utf8_next(s, end, &cp);
        if (cp == '\n') {
            if (l->row + 1u < l->rows) hd44780_set_cursor(l, 0, (uint8_t)(l->row + 1u));
            else l->col = l->cols; /* no row left: drop the rest of the line */
            continue;
        }
        if (cp == '\r') continue;
        if (l->col >= l->cols) continue;
        hd44780_write(l, hd44780_map(cp, l->rom));
        n++;
    }
    return n;
}

void hd44780_create_char(hd44780 *l, uint8_t slot, const uint8_t pattern[8])
{
    uint8_t i;
    send(l, (uint8_t)(CMD_CGRAM | ((slot & 7u) << 3)), 0u, T_CMD_US);
    for (i = 0; i < 8u; i++) send(l, (uint8_t)(pattern[i] & 0x1Fu), HD44780_RS, T_CMD_US);
    if (l->two_chips) {
        send(l, (uint8_t)(CMD_CGRAM | ((slot & 7u) << 3)), HD44780_CHIP2, T_CMD_US);
        for (i = 0; i < 8u; i++) send(l, (uint8_t)(pattern[i] & 0x1Fu), HD44780_RS | HD44780_CHIP2, T_CMD_US);
    }
    /* back to DDRAM, else the next write lands in CGRAM */
    hd44780_set_cursor(l, l->col < l->cols ? l->col : (uint8_t)(l->cols - 1u), l->row);
}

static void control(hd44780 *l, uint8_t bit, int on)
{
    if (on) l->control |= bit;
    else l->control &= (uint8_t)~bit;
    command_all(l, (uint8_t)(CMD_CONTROL | l->control), T_CMD_US);
}

void hd44780_display(hd44780 *l, int on) { control(l, CTL_DISPLAY, on); }
void hd44780_cursor(hd44780 *l, int on)  { control(l, CTL_CURSOR, on); }
void hd44780_blink(hd44780 *l, int on)   { control(l, CTL_BLINK, on); }

void hd44780_scroll(hd44780 *l, int8_t cells)
{
    uint8_t dir = cells > 0 ? SHIFT_RIGHT : 0u;
    int n = cells > 0 ? cells : -cells;
    while (n-- > 0) command_all(l, (uint8_t)(CMD_SHIFT | SHIFT_SCREEN | dir), T_CMD_US);
}
