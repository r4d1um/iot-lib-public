/*
 * irrx - infrared remote control receiver core, C99.
 *
 * Works with any demodulating IR receiver (VS1838B / KY-022, TSOP38238, TSOP4838 ...): its
 * output is LOW while a 38 kHz burst ("mark") arrives and HIGH in the pauses ("space").
 *
 *   capture:  irrx_edge() from the pin-change interrupt stores mark/space durations in µs.
 *             It is static inline, so it is compiled into the caller's ISR (ESP8266/ESP32 can
 *             place it in IRAM); the rest of the core never runs in interrupt context.
 *   frame:    irrx_check() (with interrupts off, O(1)) sees a frame is complete once the line
 *             has been quiet for gap_us; irrx_read() decodes it and frees the buffer.
 *   decode:   irrx_decode() is a pure function on a duration list, usable without capture
 *             (e.g. with the ESP32 RMT peripheral or recorded timings).
 *
 * Protocols: NEC (incl. extended address and repeat codes), Samsung32, Sony SIRC 12/15/20,
 * Philips RC5, RC6 mode 0. Anything else gets a 32-bit hash of its timing pattern, so unknown
 * remotes can still be learned (same button -> same hash). No malloc, no float.
 */
#ifndef IRRX_H
#define IRRX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IRRX_VERSION "1.0.0"

#define IRRX_MAX_DURATIONS 136u   /* NEC needs 67, RC6 up to ~44, Sony 41; room for long codes */
#define IRRX_GAP_US        5000u  /* a space this long ends a frame: > NEC/Samsung 4.5 ms spaces, < the ~6 ms
                                     between Sony 20-bit repeats. Marks never end a frame (NEC: 9 ms). */
#define IRRX_REPEAT_MS      200u  /* same code again within this time = button held */
#define IRRX_MIN_DURATIONS    8u  /* shorter frames are noise */

typedef enum {
    IRRX_UNKNOWN = 0,
    IRRX_NEC,
    IRRX_SAMSUNG,
    IRRX_SONY,
    IRRX_RC5,
    IRRX_RC6
} irrx_protocol;

typedef struct {
    irrx_protocol protocol;
    uint16_t address;   /* NEC: 8 bit, or 16 bit if the inverse byte doesn't match (extended) */
    uint16_t command;
    uint8_t  bits;      /* data bits of the frame (0 for UNKNOWN) */
    uint8_t  repeat;    /* 1: button held (NEC repeat code, same code/toggle within IRRX_REPEAT_MS) */
    uint8_t  toggle;    /* RC5/RC6 toggle bit: flips on every new press */
    uint32_t hash;      /* timing-pattern hash; set for every frame, the only id for UNKNOWN */
} irrx_result;

typedef struct {
    irrx_result last;
    uint32_t    last_end_us;
    uint8_t     have_last;
} irrx_decoder;

/* Decode one frame: d[0] is the first mark, then space, mark, ... (n durations in µs).
 * end_us: time of the frame's last edge, for repeat detection. Returns 1 and fills out,
 * or 0 for noise (fewer than IRRX_MIN_DURATIONS durations). */
int irrx_decode(irrx_decoder *dec, const uint16_t *d, uint16_t n, uint32_t end_us, irrx_result *out);

const char *irrx_protocol_name(irrx_protocol p);

/* ---- capture ---- */

#define IRRX_IDLE  0u
#define IRRX_RX    1u
#define IRRX_READY 2u

typedef struct {
    volatile uint16_t d[IRRX_MAX_DURATIONS];
    volatile uint16_t n;
    volatile uint8_t  state;
    volatile uint8_t  overflow;
    volatile uint8_t  line;     /* pin level after the last edge (1 = space) */
    volatile uint32_t last_us;
    uint32_t          gap_us;
    irrx_decoder      dec;
} irrx;

void irrx_init(irrx *rx);

#if defined(__GNUC__)
#define IRRX_INLINE static inline __attribute__((always_inline))
#else
#define IRRX_INLINE static inline
#endif

/* Pin-change interrupt: now_us = micros(), level = pin level after the edge (receiver output:
 * 0 = mark). Keeps nothing but durations; a missed edge drops the frame. Always inlined, so it
 * ends up wherever the caller's ISR lives (IRAM on ESP8266/ESP32), never in the flash-resident
 * precompiled core. */
IRRX_INLINE void irrx_edge(irrx *rx, uint32_t now_us, int level)
{
    uint32_t dur;
    if (rx->state == IRRX_READY) return;              /* previous frame not read yet */
    if (rx->state == IRRX_IDLE) {
        if (level == 0) {                             /* first mark starts a frame */
            rx->n = 0;
            rx->overflow = 0;
            rx->line = 0;
            rx->last_us = now_us;
            rx->state = IRRX_RX;
        }
        return;
    }
    dur = now_us - rx->last_us;
    if (level == 0 && dur > rx->gap_us) {             /* long space: frame over, irrx_check came */
        rx->state = IRRX_READY;                       /* too late; this edge's frame is lost */
        return;
    }
    rx->last_us = now_us;
    rx->line = (uint8_t)(level != 0);
    if (rx->n >= IRRX_MAX_DURATIONS) {               /* too long: swallow the rest, drop it later */
        rx->overflow = 1;
        return;
    }
    rx->d[rx->n++] = (uint16_t)(dur > 0xFFFFu ? 0xFFFFu : dur);
    /* after a mark (odd count) the line must be high again, after a space low */
    if ((rx->n & 1u) != (uint16_t)(level != 0)) rx->state = IRRX_IDLE;
}

/* Call with interrupts off: 1 if a complete frame is waiting (then the capture is paused).
 * Complete = the line has been in a space for gap_us; a mark stuck for > 65 ms ends it too. */
int irrx_check(irrx *rx, uint32_t now_us);
/* Decode the waiting frame and restart capture. Returns 1 with a result, 0 for noise. */
int irrx_read(irrx *rx, irrx_result *out);
/* Durations of the frame irrx_read just decoded (valid until capture refills the buffer). */
const volatile uint16_t *irrx_raw(const irrx *rx, uint16_t *n);

#ifdef __cplusplus
}
#endif

#endif
