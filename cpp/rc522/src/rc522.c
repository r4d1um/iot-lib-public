/* rc522 core - see rc522.h. Register and command names follow the MFRC522 datasheet. */
#include "rc522.h"

#define REG_COMMAND     0x01u
#define REG_COM_IRQ     0x04u
#define REG_DIV_IRQ     0x05u
#define REG_ERROR       0x06u
#define REG_STATUS2     0x08u
#define REG_FIFO_DATA   0x09u
#define REG_FIFO_LEVEL  0x0Au
#define REG_CONTROL     0x0Cu
#define REG_BIT_FRAMING 0x0Du
#define REG_COLL        0x0Eu
#define REG_MODE        0x11u
#define REG_TX_MODE     0x12u
#define REG_RX_MODE     0x13u
#define REG_TX_CONTROL  0x14u
#define REG_TX_ASK      0x15u
#define REG_MOD_WIDTH   0x24u
#define REG_RF_CFG      0x26u
#define REG_T_MODE      0x2Au
#define REG_T_PRESCALER 0x2Bu
#define REG_T_RELOAD_H  0x2Cu
#define REG_T_RELOAD_L  0x2Du
#define REG_VERSION     0x37u

#define CMD_IDLE        0x00u
#define CMD_TRANSCEIVE  0x0Cu
#define CMD_MF_AUTHENT  0x0Eu
#define CMD_SOFT_RESET  0x0Fu

#define IRQ_TIMER       0x01u
#define IRQ_IDLE        0x10u
#define IRQ_RX          0x20u
#define ERR_COLL        0x08u
#define ERR_FATAL       0x13u /* BufferOvfl, ParityErr, ProtocolErr */
#define STATUS2_CRYPTO1 0x08u

#define PICC_REQA       0x26u
#define PICC_WUPA       0x52u
#define PICC_CT         0x88u
#define PICC_SEL_CL1    0x93u
#define PICC_HLTA       0x50u
#define PICC_READ       0x30u
#define PICC_MF_WRITE   0xA0u
#define PICC_UL_WRITE   0xA2u
#define PICC_ACK        0x0Au

#define POLL_US       1000u
#define POLL_MAX        40u  /* > the 25 ms chip timer: a dead chip still ends */

/* ---- SPI framing: address byte = (reg << 1), bit 7 set for read ---- */

static void xfer(rc522 *d, uint8_t *buf, uint16_t n)
{
    if (d->bus.transfer(d->bus.ctx, buf, n) != 0) d->bus_failed = 1;
}

static void wr(rc522 *d, uint8_t reg, uint8_t v)
{
    uint8_t b[2];
    b[0] = (uint8_t)((reg << 1) & 0x7Eu);
    b[1] = v;
    xfer(d, b, 2);
}

static uint8_t rd(rc522 *d, uint8_t reg)
{
    uint8_t b[2];
    b[0] = (uint8_t)(((reg << 1) & 0x7Eu) | 0x80u);
    b[1] = 0;
    xfer(d, b, 2);
    return b[1];
}

static void set_bits(rc522 *d, uint8_t reg, uint8_t m) { wr(d, reg, (uint8_t)(rd(d, reg) | m)); }
static void clr_bits(rc522 *d, uint8_t reg, uint8_t m) { wr(d, reg, (uint8_t)(rd(d, reg) & (uint8_t)~m)); }

static void fifo_write(rc522 *d, const uint8_t *data, uint8_t n)
{
    uint8_t b[65];
    uint8_t i;
    b[0] = (uint8_t)((REG_FIFO_DATA << 1) & 0x7Eu);
    for (i = 0; i < n; i++) b[i + 1u] = data[i];
    xfer(d, b, (uint16_t)(n + 1u));
}

static void fifo_read(rc522 *d, uint8_t *out, uint8_t n)
{
    uint8_t b[65];
    uint8_t i;
    for (i = 0; i < n; i++) b[i] = (uint8_t)(((REG_FIFO_DATA << 1) & 0x7Eu) | 0x80u);
    b[n] = 0;
    xfer(d, b, (uint16_t)(n + 1u));
    for (i = 0; i < n; i++) out[i] = b[i + 1u];
}

/* ---- CRC_A: ISO 14443-3 annex B, preset 0x6363 ---- */

void rc522_crc_a(const uint8_t *data, size_t n, uint8_t out[2])
{
    uint16_t crc = 0x6363u;
    size_t i;
    for (i = 0; i < n; i++) {
        uint8_t b = (uint8_t)(data[i] ^ (uint8_t)(crc & 0xFFu));
        b = (uint8_t)(b ^ (uint8_t)(b << 4));
        crc = (uint16_t)((crc >> 8) ^ ((uint16_t)b << 8) ^ ((uint16_t)b << 3) ^ ((uint16_t)b >> 4));
    }
    out[0] = (uint8_t)(crc & 0xFFu);
    out[1] = (uint8_t)(crc >> 8);
}

static int crc_ok(const uint8_t *data, uint8_t n)
{
    uint8_t c[2];
    if (n < 3u) return 0;
    rc522_crc_a(data, (size_t)(n - 2u), c);
    return c[0] == data[n - 2u] && c[1] == data[n - 1u];
}

/* ---- one exchange with the card through the FIFO ---- */

typedef struct {
    uint8_t  n;       /* bytes received */
    uint8_t  bits;    /* valid bits in the last byte, 0 = all 8 */
} rx_info;

static rc522_status exchange(rc522 *d, uint8_t cmd, const uint8_t *tx, uint8_t txn, uint8_t tx_bits,
                             uint8_t rx_align, uint8_t *rx, uint8_t rxcap, rx_info *info)
{
    uint8_t wait = cmd == CMD_TRANSCEIVE ? (uint8_t)(IRQ_RX | IRQ_IDLE) : IRQ_IDLE;
    uint8_t irq = 0, err, i;
    info->n = 0;
    info->bits = 0;
    wr(d, REG_COMMAND, CMD_IDLE);
    wr(d, REG_COM_IRQ, 0x7Fu);        /* clear all IRQ bits */
    wr(d, REG_FIFO_LEVEL, 0x80u);     /* flush FIFO */
    fifo_write(d, tx, txn);
    wr(d, REG_BIT_FRAMING, (uint8_t)((rx_align << 4) | (tx_bits & 7u)));
    wr(d, REG_COMMAND, cmd);
    if (cmd == CMD_TRANSCEIVE) wr(d, REG_BIT_FRAMING, (uint8_t)(0x80u | (rx_align << 4) | (tx_bits & 7u)));
    for (i = 0; i < POLL_MAX; i++) {
        irq = rd(d, REG_COM_IRQ);
        if (d->bus_failed) return RC522_BUS;
        if (irq & wait) break;
        if (irq & IRQ_TIMER) return RC522_TIMEOUT;
        d->bus.delay_us(d->bus.ctx, POLL_US);
    }
    clr_bits(d, REG_BIT_FRAMING, 0x80u);
    if (!(irq & wait)) return RC522_TIMEOUT;
    err = rd(d, REG_ERROR);
    if (err & ERR_FATAL) return RC522_ERROR;
    if (rx && rxcap) {
        uint8_t n = rd(d, REG_FIFO_LEVEL);
        if (n > rxcap) return RC522_ERROR;
        fifo_read(d, rx, n);
        info->n = n;
        info->bits = (uint8_t)(rd(d, REG_CONTROL) & 7u);
    }
    if (d->bus_failed) return RC522_BUS;
    return (err & ERR_COLL) ? RC522_COLLISION : RC522_OK;
}

/* plain frame with CRC appended; response checked for CRC if crc_rx */
static rc522_status transceive_crc(rc522 *d, const uint8_t *tx, uint8_t txn, uint8_t *rx, uint8_t rxcap,
                                   rx_info *info, int crc_rx)
{
    uint8_t buf[20];
    rc522_status st;
    uint8_t i;
    for (i = 0; i < txn; i++) buf[i] = tx[i];
    rc522_crc_a(buf, txn, &buf[txn]);
    st = exchange(d, CMD_TRANSCEIVE, buf, (uint8_t)(txn + 2u), 0, 0, rx, rxcap, info);
    if (st != RC522_OK) return st;
    /* a 4-bit answer is ACK/NAK */
    if (info->n == 1u && info->bits == 4u) return (rx[0] & 0x0Fu) == PICC_ACK ? RC522_OK : RC522_NAK;
    if (crc_rx && !crc_ok(rx, info->n)) return RC522_CRC;
    return RC522_OK;
}

/* ---- setup ---- */

rc522_status rc522_init(rc522 *d, const rc522_bus *bus)
{
    uint8_t i;
    d->bus = *bus;
    d->bus_failed = 0;
    d->version = 0;
    wr(d, REG_COMMAND, CMD_SOFT_RESET);
    d->bus.delay_us(d->bus.ctx, 50000u);              /* oscillator start-up */
    for (i = 0; i < 10u && (rd(d, REG_COMMAND) & 0x10u); i++) d->bus.delay_us(d->bus.ctx, 5000u);
    d->version = rd(d, REG_VERSION);
    if (d->bus_failed || d->version == 0x00u || d->version == 0xFFu) return RC522_BUS;
    wr(d, REG_TX_MODE, 0x00u);
    wr(d, REG_RX_MODE, 0x00u);
    wr(d, REG_MOD_WIDTH, 0x26u);
    /* timer: TAuto, prescaler 0x0A9 -> 40 kHz, reload 1000 -> 25 ms timeout */
    wr(d, REG_T_MODE, 0x80u);
    wr(d, REG_T_PRESCALER, 0xA9u);
    wr(d, REG_T_RELOAD_H, 0x03u);
    wr(d, REG_T_RELOAD_L, 0xE8u);
    wr(d, REG_TX_ASK, 0x40u);   /* 100 % ASK */
    wr(d, REG_MODE, 0x3Du);     /* CRC preset 0x6363 */
    rc522_antenna(d, 1);
    return d->bus_failed ? RC522_BUS : RC522_OK;
}

void rc522_antenna(rc522 *d, int on)
{
    if (on) set_bits(d, REG_TX_CONTROL, 0x03u);
    else clr_bits(d, REG_TX_CONTROL, 0x03u);
}

void rc522_gain(rc522 *d, uint8_t level)
{
    wr(d, REG_RF_CFG, (uint8_t)((level & 7u) << 4));
}

void rc522_stop_crypto(rc522 *d)
{
    clr_bits(d, REG_STATUS2, STATUS2_CRYPTO1);
}

/* ---- activation ---- */

static rc522_status request(rc522 *d, uint8_t cmd, uint8_t atqa[2])
{
    uint8_t rx[2];
    rx_info info;
    rc522_status st;
    clr_bits(d, REG_COLL, 0x80u);   /* ValuesAfterColl = 0: bits after a collision read as 0 */
    st = exchange(d, CMD_TRANSCEIVE, &cmd, 1, 7, 0, rx, 2, &info);
    if (st == RC522_TIMEOUT) return RC522_NO_CARD;
    if (st != RC522_OK && st != RC522_COLLISION) return st; /* ATQA collision: several cards */
    if (info.n != 2u || info.bits != 0u) return RC522_ERROR;
    atqa[0] = rx[0];
    atqa[1] = rx[1];
    return RC522_OK;
}

static rc522_status select_card(rc522 *d, rc522_uid *uid)
{
    uint8_t level, filled = 0;
    for (level = 0; level < 3u; level++) {
        uint8_t buf[9], rx[5], tries;
        uint8_t kb = 0; /* known bits of UID CLn (32 bits + BCC) */
        uint8_t i;
        rx_info info;
        rc522_status st;
        buf[0] = (uint8_t)(PICC_SEL_CL1 + 2u * level);
        for (i = 2; i < 9u; i++) buf[i] = 0;
        for (tries = 0; tries < 33u && kb < 32u; tries++) {
            uint8_t nbytes = (uint8_t)(kb / 8u), nbits = (uint8_t)(kb % 8u);
            uint8_t txn = (uint8_t)(2u + nbytes + (nbits ? 1u : 0u));
            buf[1] = (uint8_t)(((2u + nbytes) << 4) | nbits);   /* NVB */
            st = exchange(d, CMD_TRANSCEIVE, buf, txn, nbits, nbits, rx, sizeof rx, &info);
            if (st != RC522_OK && st != RC522_COLLISION) return st;
            if (info.n == 0u) return RC522_ERROR;
            for (i = 0; i < info.n && 2u + nbytes + i < 7u; i++) {
                uint8_t idx = (uint8_t)(2u + nbytes + i);
                if (i == 0u && nbits) {
                    uint8_t keep = (uint8_t)((1u << nbits) - 1u);
                    buf[idx] = (uint8_t)((buf[idx] & keep) | (rx[0] & (uint8_t)~keep));
                } else {
                    buf[idx] = rx[i];
                }
            }
            if (st == RC522_COLLISION) {
                uint8_t coll = rd(d, REG_COLL), pos, abs;
                if (coll & 0x20u) return RC522_COLLISION;          /* CollPosNotValid */
                pos = (uint8_t)(coll & 0x1Fu);
                if (pos == 0u) pos = 32u;
                abs = (uint8_t)(nbytes * 8u + pos);               /* 1-based bit in UID CLn */
                if (abs <= kb || abs > 32u) return RC522_COLLISION;
                kb = abs;
                buf[2u + (abs - 1u) / 8u] |= (uint8_t)(1u << ((abs - 1u) % 8u)); /* take the 1-branch */
            } else {
                if ((uint8_t)(buf[2] ^ buf[3] ^ buf[4] ^ buf[5]) != buf[6]) return RC522_ERROR; /* BCC */
                kb = 32;
            }
        }
        if (kb < 32u) return RC522_COLLISION;
        /* SELECT; BCC from the UID bytes: after a collision in the last UID bit the received
         * BCC is cleared (ValuesAfterColl), so it cannot be taken from the answer */
        buf[1] = 0x70u;
        buf[6] = (uint8_t)(buf[2] ^ buf[3] ^ buf[4] ^ buf[5]);
        st = transceive_crc(d, buf, 7, rx, sizeof rx, &info, 1);
        if (st != RC522_OK) return st == RC522_NAK ? RC522_ERROR : st;
        if (info.n != 3u) return RC522_ERROR;
        uid->sak = rx[0];
        if (rx[0] & 0x04u) {                 /* cascade bit: UID not complete */
            if (buf[2] != PICC_CT || level == 2u) return RC522_ERROR;
            for (i = 0; i < 3u; i++) uid->bytes[filled++] = buf[3u + i];
        } else {
            for (i = 0; i < 4u; i++) uid->bytes[filled++] = buf[2u + i];
            uid->size = filled;
            return RC522_OK;
        }
    }
    return RC522_ERROR;
}

static rc522_status find(rc522 *d, uint8_t cmd, rc522_uid *uid)
{
    rc522_status st;
    d->bus_failed = 0;
    rc522_stop_crypto(d);
    uid->size = 0;
    st = request(d, cmd, uid->atqa);
    if (st != RC522_OK) return st;
    st = select_card(d, uid);
    if (st != RC522_OK) uid->size = 0;
    return d->bus_failed ? RC522_BUS : st;
}

rc522_status rc522_find(rc522 *d, rc522_uid *uid) { return find(d, PICC_REQA, uid); }
rc522_status rc522_find_any(rc522 *d, rc522_uid *uid) { return find(d, PICC_WUPA, uid); }

rc522_status rc522_halt(rc522 *d)
{
    uint8_t tx[2] = { PICC_HLTA, 0x00 }, rx[4];
    rx_info info;
    rc522_status st = transceive_crc(d, tx, 2, rx, sizeof rx, &info, 0);
    rc522_stop_crypto(d);
    /* ISO 14443-3: a card that answers HLTA at all means "not acknowledged" */
    if (st == RC522_TIMEOUT) return RC522_OK;
    return st == RC522_OK ? RC522_NAK : st;
}

/* ---- card types ---- */

rc522_type rc522_card_type(uint8_t sak)
{
    sak &= 0x7Fu;
    if (sak & 0x04u) return RC522_TYPE_NOT_COMPLETE;
    switch (sak) {
    case 0x09: return RC522_TYPE_MIFARE_MINI;
    case 0x08: case 0x88: return RC522_TYPE_MIFARE_1K;
    case 0x18: return RC522_TYPE_MIFARE_4K;
    case 0x00: return RC522_TYPE_ULTRALIGHT;
    case 0x10: case 0x11: return RC522_TYPE_MIFARE_PLUS;
    case 0x20: return RC522_TYPE_ISO_14443_4;
    default: return (sak & 0x20u) ? RC522_TYPE_ISO_14443_4 : RC522_TYPE_UNKNOWN;
    }
}

const char *rc522_type_name(rc522_type t)
{
    switch (t) {
    case RC522_TYPE_MIFARE_MINI: return "MIFARE Mini";
    case RC522_TYPE_MIFARE_1K: return "MIFARE Classic 1K";
    case RC522_TYPE_MIFARE_4K: return "MIFARE Classic 4K";
    case RC522_TYPE_ULTRALIGHT: return "MIFARE Ultralight / NTAG";
    case RC522_TYPE_MIFARE_PLUS: return "MIFARE Plus";
    case RC522_TYPE_ISO_14443_4: return "ISO 14443-4 (DESFire, bank card, phone)";
    case RC522_TYPE_NOT_COMPLETE: return "UID not complete";
    default: return "unknown";
    }
}

const char *rc522_status_name(rc522_status s)
{
    switch (s) {
    case RC522_OK: return "ok";
    case RC522_NO_CARD: return "no card";
    case RC522_TIMEOUT: return "timeout";
    case RC522_COLLISION: return "collision";
    case RC522_ERROR: return "protocol error";
    case RC522_CRC: return "CRC error";
    case RC522_NAK: return "refused by card";
    case RC522_AUTH: return "authentication failed";
    case RC522_BUS: return "no reader / SPI error";
    case RC522_ARG: return "bad argument";
    default: return "?";
    }
}

/* ---- MIFARE ---- */

rc522_status rc522_auth(rc522 *d, int use_key_b, uint8_t block, const uint8_t key[6], const rc522_uid *uid)
{
    uint8_t tx[12], i;
    rx_info info;
    rc522_status st;
    if (!uid || uid->size < 4u) return RC522_ARG;
    d->bus_failed = 0;
    tx[0] = use_key_b ? 0x61u : 0x60u;
    tx[1] = block;
    for (i = 0; i < 6u; i++) tx[2u + i] = key[i];
    for (i = 0; i < 4u; i++) tx[8u + i] = uid->bytes[uid->size - 4u + i];
    st = exchange(d, CMD_MF_AUTHENT, tx, sizeof tx, 0, 0, 0, 0, &info);
    if (st == RC522_BUS) return st;
    if (st == RC522_TIMEOUT || !(rd(d, REG_STATUS2) & STATUS2_CRYPTO1)) return RC522_AUTH;
    return st;
}

rc522_status rc522_read(rc522 *d, uint8_t block, uint8_t out[16])
{
    uint8_t tx[2], rx[18], i;
    rx_info info;
    rc522_status st;
    d->bus_failed = 0;
    tx[0] = PICC_READ;
    tx[1] = block;
    st = transceive_crc(d, tx, 2, rx, sizeof rx, &info, 1);
    if (st != RC522_OK) return st;
    if (info.n != 18u) return RC522_ERROR;
    for (i = 0; i < 16u; i++) out[i] = rx[i];
    return RC522_OK;
}

static int is_trailer(uint8_t block)
{
    return block < 128u ? (block % 4u) == 3u : (block % 16u) == 15u;
}

rc522_status rc522_write(rc522 *d, uint8_t block, const uint8_t data[16])
{
    uint8_t tx[2], rx[2];
    rx_info info;
    rc522_status st;
    if (block == 0u || is_trailer(block)) return RC522_ARG;
    d->bus_failed = 0;
    tx[0] = PICC_MF_WRITE;
    tx[1] = block;
    st = transceive_crc(d, tx, 2, rx, sizeof rx, &info, 0);
    if (st != RC522_OK) return st;
    if (!(info.n == 1u && info.bits == 4u)) return RC522_ERROR; /* must be ACK */
    st = transceive_crc(d, data, 16, rx, sizeof rx, &info, 0);
    if (st != RC522_OK) return st;
    return (info.n == 1u && info.bits == 4u) ? RC522_OK : RC522_ERROR;
}

rc522_status rc522_ul_write(rc522 *d, uint8_t page, const uint8_t data[4])
{
    uint8_t tx[6], rx[2], i;
    rx_info info;
    rc522_status st;
    if (page < 4u) return RC522_ARG;
    d->bus_failed = 0;
    tx[0] = PICC_UL_WRITE;
    tx[1] = page;
    for (i = 0; i < 4u; i++) tx[2u + i] = data[i];
    st = transceive_crc(d, tx, 6, rx, sizeof rx, &info, 0);
    if (st != RC522_OK) return st;
    return (info.n == 1u && info.bits == 4u) ? RC522_OK : RC522_ERROR;
}

/* ---- UIDs ---- */

char *rc522_uid_hex(const rc522_uid *uid, char *out, size_t size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    uint8_t i;
    if (!out || size == 0u) return out;
    for (i = 0; uid && i < uid->size; i++) {
        size_t need = i ? 3u : 2u;           /* ":XX" or "XX" */
        if (o + need + 1u > size) break;     /* keep room for the NUL */
        if (i) out[o++] = ':';
        out[o++] = hex[uid->bytes[i] >> 4];
        out[o++] = hex[uid->bytes[i] & 15u];
    }
    out[o] = '\0';
    return out;
}

int rc522_uid_equal(const rc522_uid *a, const rc522_uid *b)
{
    uint8_t i;
    if (a->size != b->size) return 0;
    for (i = 0; i < a->size; i++)
        if (a->bytes[i] != b->bytes[i]) return 0;
    return 1;
}
