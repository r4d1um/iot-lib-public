/* irrx decoders - see irrx.h. Timings after the protocol specs (NEC, Samsung32, Sony SIRC,
 * Philips RC5, RC6 mode 0). Demodulating receivers stretch marks and shorten spaces by up to
 * ~150 µs, so header checks allow 25 % + 150 µs and bit decisions use the midpoint between the
 * two allowed lengths, never an exact value. */
#include "irrx.h"

static int near(uint16_t d, uint16_t e)
{
    uint32_t tol = (uint32_t)e / 4u + 150u;
    return (uint32_t)d + tol >= e && (uint32_t)d <= (uint32_t)e + tol;
}

/* ---- pulse distance: NEC, Samsung32 ---- */

static int pulse_distance32(const uint16_t *d, uint16_t n, uint8_t b[4])
{
    uint8_t i;
    if (n != 67u) return 0;
    for (i = 0; i < 4u; i++) b[i] = 0;
    for (i = 0; i < 32u; i++) {
        uint16_t mark = d[2u + 2u * i], space = d[3u + 2u * i];
        if (mark > 1100u || mark < 250u || space > 2600u || space < 250u) return 0;
        if (space > 1125u) b[i / 8u] |= (uint8_t)(1u << (i % 8u));  /* 560 vs 1690 µs */
    }
    return d[66] <= 1100u;   /* stop mark */
}

static int decode_nec(const uint16_t *d, uint16_t n, irrx_result *r)
{
    uint8_t b[4];
    if (!near(d[0], 9000) || !near(d[1], 4500) || !pulse_distance32(d, n, b)) return 0;
    if ((uint8_t)(b[2] ^ b[3]) != 0xFFu) return 0;
    r->protocol = IRRX_NEC;
    r->address = (uint8_t)(b[0] ^ b[1]) == 0xFFu ? b[0] : (uint16_t)(b[0] | (b[1] << 8));
    r->command = b[2];
    r->bits = 32;
    return 1;
}

static int decode_samsung(const uint16_t *d, uint16_t n, irrx_result *r)
{
    uint8_t b[4];
    if (!near(d[0], 4500) || !near(d[1], 4500) || !pulse_distance32(d, n, b)) return 0;
    if ((uint8_t)(b[2] ^ b[3]) != 0xFFu) return 0;
    r->protocol = IRRX_SAMSUNG;
    r->address = (uint16_t)(b[0] | (b[1] << 8));
    r->command = b[2];
    r->bits = 32;
    return 1;
}

/* ---- pulse width: Sony SIRC ---- */

static int decode_sony(const uint16_t *d, uint16_t n, irrx_result *r)
{
    uint8_t bits, i;
    uint32_t v = 0;
    /* header space 600 µs; < 780 keeps RC6's 889 µs leader space out */
    if (!near(d[0], 2400) || d[1] < 300u || d[1] > 780u || n < 25u || !(n & 1u)) return 0;
    bits = (uint8_t)((n - 1u) / 2u);
    if (bits != 12u && bits != 15u && bits != 20u) return 0;
    for (i = 0; i < bits; i++) {
        uint16_t mark = d[2u + 2u * i];
        if (mark < 300u || mark > 1700u) return 0;
        if (i + 1u < bits && (d[3u + 2u * i] < 250u || d[3u + 2u * i] > 1100u)) return 0;
        if (mark > 900u) v |= (uint32_t)1u << i;                    /* 600 vs 1200 µs, LSB first */
    }
    r->protocol = IRRX_SONY;
    r->command = (uint16_t)(v & 0x7Fu);
    r->address = (uint16_t)(v >> 7);                                /* 5, 8 or 13 bits */
    r->bits = bits;
    return 1;
}

/* ---- Manchester: RC5, RC6 ---- */

/* Durations to half-bit levels of length t (1 = mark), each duration 1..maxu units.
 * bias: how much longer the receiver makes marks (and shorter spaces); removed first. */
static int expand(const uint16_t *d, uint16_t n, uint16_t first, uint16_t t, uint8_t maxu,
                  int16_t bias, uint8_t *lv, uint8_t pos, uint8_t cap)
{
    uint16_t i;
    for (i = first; i < n; i++) {
        int32_t c = (int32_t)d[i] + (((i & 1u) == 0u) ? -bias : bias);
        uint16_t dv = (uint16_t)(c < 1 ? 1 : (c > 0xFFFF ? 0xFFFF : c));
        uint16_t u = (uint16_t)((dv + t / 2u) / t), k;
        uint16_t lo = (uint16_t)(u * t), err = dv > lo ? (uint16_t)(dv - lo) : (uint16_t)(lo - dv);
        if (u < 1u || u > maxu || err > t * 2u / 5u + 100u) return -1;
        for (k = 0; k < u; k++) {
            if (pos >= cap) return -1;
            lv[pos++] = (uint8_t)((i & 1u) == 0u);   /* even index = mark */
        }
    }
    return pos;
}

static int decode_rc5(const uint16_t *d, uint16_t n, irrx_result *r)
{
    uint8_t lv[32], i;
    int len;
    uint16_t v = 0;
    if (n < 11u || n > 27u) return 0;
    lv[0] = 0;                                   /* S1 = 1 starts with an unseen space half */
    len = expand(d, n, 0, 889, 2, 0, lv, 1, sizeof lv);
    if (len < 27) return 0;
    while (len < 28) lv[len++] = 0;              /* final space half merges into the gap */
    if (len != 28) return 0;
    for (i = 0; i < 14u; i++) {
        uint8_t a = lv[2u * i], b = lv[2u * i + 1u];
        if (a == b) return 0;
        v = (uint16_t)((v << 1) | (a == 0u && b == 1u ? 1u : 0u));   /* RC5: space-mark = 1 */
    }
    if (!(v & 0x2000u)) return 0;                /* S1 */
    r->protocol = IRRX_RC5;
    r->toggle = (uint8_t)((v >> 11) & 1u);
    r->address = (uint16_t)((v >> 6) & 0x1Fu);
    r->command = (uint16_t)((v & 0x3Fu) | ((v & 0x1000u) ? 0u : 0x40u));  /* S2 = inverted C6 */
    r->bits = 14;
    return 1;
}

static int decode_rc6(const uint16_t *d, uint16_t n, irrx_result *r)
{
    uint8_t lv[48], i, mode = 0;
    int len;
    uint16_t v = 0;
    int16_t bias;
    if (n < 12u || !near(d[0], 2666) || !near(d[1], 889)) return 0;
    /* T is only 444 µs: take the receiver's mark stretch from the 6T leader mark */
    bias = (int16_t)((int32_t)d[0] - 2666);
    if (bias > 250) bias = 250;
    if (bias < -200) bias = -200;
    len = expand(d, n, 2, 444, 3, bias, lv, 0, sizeof lv);
    if (len < 43) return 0;
    while (len < 44) lv[len++] = 0;
    if (len != 44) return 0;
    if (lv[0] != 1u || lv[1] != 0u) return 0;    /* start bit 1: mark-space */
    for (i = 0; i < 3u; i++) {
        if (lv[2u + 2u * i] == lv[3u + 2u * i]) return 0;
        mode = (uint8_t)((mode << 1) | lv[2u + 2u * i]);
    }
    if (mode != 0u) return 0;
    if (lv[8] != lv[9] || lv[10] != lv[11] || lv[8] == lv[10]) return 0;   /* trailer, 2T halves */
    for (i = 0; i < 16u; i++) {
        uint8_t a = lv[12u + 2u * i], b = lv[13u + 2u * i];
        if (a == b) return 0;
        v = (uint16_t)((v << 1) | a);            /* RC6: mark-space = 1 */
    }
    r->protocol = IRRX_RC6;
    r->toggle = lv[8];
    r->address = (uint16_t)(v >> 8);
    r->command = (uint16_t)(v & 0xFFu);
    r->bits = 16;
    return 1;
}

/* ---- unknown remotes: hash of the timing pattern (each duration vs. the one two later) ---- */

static uint32_t pattern_hash(const uint16_t *d, uint16_t n)
{
    uint32_t h = 2166136261u;
    uint16_t i;
    for (i = 0; i + 2u < n; i++) {
        uint32_t a = d[i], b = d[i + 2u];
        uint32_t v = b * 10u < a * 8u ? 0u : (b * 10u > a * 12u ? 2u : 1u);
        h = (h * 16777619u) ^ v;
    }
    return h;
}

static int same_code(const irrx_result *a, const irrx_result *b)
{
    if (a->protocol != b->protocol) return 0;
    if (a->protocol == IRRX_UNKNOWN) return a->hash == b->hash;
    return a->address == b->address && a->command == b->command && a->bits == b->bits;
}

int irrx_decode(irrx_decoder *dec, const uint16_t *d, uint16_t n, uint32_t end_us, irrx_result *out)
{
    irrx_result r;
    int within = dec->have_last && (uint32_t)(end_us - dec->last_end_us) <= IRRX_REPEAT_MS * 1000u;

    /* NEC repeat code: 9 ms mark, 2.25 ms space, stop mark */
    if (n == 3u && near(d[0], 9000) && near(d[1], 2250) && d[2] < 1100u) {
        if (!within || dec->last.protocol != IRRX_NEC) return 0;   /* repeat of nothing we saw */
        *out = dec->last;
        out->repeat = 1;
        dec->last_end_us = end_us;
        return 1;
    }
    if (n < IRRX_MIN_DURATIONS) return 0;

    r.protocol = IRRX_UNKNOWN;
    r.address = 0;
    r.command = 0;
    r.bits = 0;
    r.repeat = 0;
    r.toggle = 0;
    r.hash = pattern_hash(d, n);
    /* RC6 before Sony: its 2666/889 µs leader is within Sony's 2400/600 µs header tolerance */
    if (!decode_nec(d, n, &r) && !decode_samsung(d, n, &r) && !decode_rc6(d, n, &r) &&
        !decode_sony(d, n, &r) && !decode_rc5(d, n, &r)) {
        r.protocol = IRRX_UNKNOWN;
        r.address = 0;
        r.command = 0;
        r.bits = 0;
        r.toggle = 0;
    }
    if (within && same_code(&r, &dec->last)) {
        /* RC5/RC6 say "new press" with the toggle bit; the others by a pause */
        r.repeat = (r.protocol == IRRX_RC5 || r.protocol == IRRX_RC6) ? (uint8_t)(r.toggle == dec->last.toggle) : 1u;
    }
    dec->last = r;
    dec->last.repeat = 0;
    dec->last_end_us = end_us;
    dec->have_last = 1;
    *out = r;
    return 1;
}
