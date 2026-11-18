/* irrx capture (frame handling outside the ISR) and protocol-independent parts - see irrx.h. */
#include "irrx.h"

void irrx_init(irrx *rx)
{
    rx->n = 0;
    rx->state = IRRX_IDLE;
    rx->overflow = 0;
    rx->line = 1;
    rx->last_us = 0;
    rx->gap_us = IRRX_GAP_US;
    rx->dec.have_last = 0;
    rx->dec.last_end_us = 0;
}

int irrx_check(irrx *rx, uint32_t now_us)
{
    if (rx->state == IRRX_RX) {
        uint32_t quiet = (uint32_t)(now_us - rx->last_us);
        if ((rx->line && quiet > rx->gap_us) || quiet > 65535u) rx->state = IRRX_READY;
    }
    return rx->state == IRRX_READY;
}

int irrx_read(irrx *rx, irrx_result *out)
{
    uint16_t buf[IRRX_MAX_DURATIONS], n, i;
    int ok;
    if (rx->state != IRRX_READY) return 0;
    n = rx->n;
    for (i = 0; i < n; i++) buf[i] = rx->d[i];
    ok = rx->overflow ? 0 : irrx_decode(&rx->dec, buf, n, rx->last_us, out);
    rx->state = IRRX_IDLE;   /* capture starts again with the next mark */
    return ok;
}

const volatile uint16_t *irrx_raw(const irrx *rx, uint16_t *n)
{
    *n = rx->n;
    return rx->d;
}

const char *irrx_protocol_name(irrx_protocol p)
{
    switch (p) {
    case IRRX_NEC: return "NEC";
    case IRRX_SAMSUNG: return "Samsung";
    case IRRX_SONY: return "Sony";
    case IRRX_RC5: return "RC5";
    case IRRX_RC6: return "RC6";
    default: return "unknown";
    }
}
