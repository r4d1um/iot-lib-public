/*
 * rc522 - driver core for the NXP MFRC522 13.56 MHz reader ("RFID-RC522" modules), C99.
 *
 * Finds ISO 14443A cards (4-, 7- and 10-byte UIDs, several cards in the field via anticollision),
 * tells the card type from SAK, authenticates and reads/writes MIFARE Classic blocks and writes
 * MIFARE Ultralight / NTAG pages. CRC_A is computed here, not by the chip (fewer SPI transfers).
 *
 * No hardware access: the caller passes a bus whose transfer() does one full-duplex SPI
 * transaction with SS low for its whole length (mode 0, MSB first, up to 10 MHz).
 * No malloc, no float. Arduino adapter: Rc522.h.
 */
#ifndef RC522_H
#define RC522_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC522_VERSION "1.0.0"

typedef enum {
    RC522_OK = 0,
    RC522_NO_CARD,     /* nothing answered */
    RC522_TIMEOUT,     /* the card stopped answering mid-exchange */
    RC522_COLLISION,   /* unresolvable collision */
    RC522_ERROR,       /* protocol, parity or buffer error reported by the chip */
    RC522_CRC,         /* response CRC wrong */
    RC522_NAK,         /* card refused (wrong block, not authenticated, read-only) */
    RC522_AUTH,        /* MIFARE authentication failed (wrong key) */
    RC522_BUS,         /* no chip found / SPI error */
    RC522_ARG          /* bad argument */
} rc522_status;

typedef struct {
    /* One SPI transaction: shift buf out while reading into buf, SS low throughout.
     * Returns 0 on success. */
    int (*transfer)(void *ctx, uint8_t *buf, uint16_t n);
    void (*delay_us)(void *ctx, uint32_t us);
    void *ctx;
} rc522_bus;

typedef struct {
    rc522_bus bus;
    uint8_t   version;   /* VersionReg: 0x91 = v1.0, 0x92 = v2.0, 0x88/0x12/0xB2 = clones */
    uint8_t   bus_failed; /* a transfer() failed since the last call; reported as RC522_BUS */
} rc522;

typedef struct {
    uint8_t size;        /* 4, 7 or 10 */
    uint8_t bytes[10];
    uint8_t sak;         /* select acknowledge of the last cascade level */
    uint8_t atqa[2];
} rc522_uid;

typedef enum {
    RC522_TYPE_UNKNOWN = 0,
    RC522_TYPE_MIFARE_MINI,
    RC522_TYPE_MIFARE_1K,
    RC522_TYPE_MIFARE_4K,
    RC522_TYPE_ULTRALIGHT,   /* also NTAG 21x */
    RC522_TYPE_MIFARE_PLUS,
    RC522_TYPE_ISO_14443_4,  /* DESFire, bank cards, phones */
    RC522_TYPE_NOT_COMPLETE
} rc522_type;

/* Soft-resets the chip, sets the timer (25 ms card timeout), 100 % ASK, CRC preset 0x6363 and
 * switches the antenna on. RC522_BUS if no chip answers (VersionReg 0x00 or 0xFF). */
rc522_status rc522_init(rc522 *dev, const rc522_bus *bus);

void rc522_antenna(rc522 *dev, int on);
/* Receiver gain 0..7 = 18, 23, 18, 23, 33, 38, 43, 48 dB (default after init: 4 = 33 dB). */
void rc522_gain(rc522 *dev, uint8_t level);

/* A card that has not been halted is in the field: REQA, anticollision, select.
 * RC522_NO_CARD if none. With several cards, one of them is selected (lowest UID bit wins). */
rc522_status rc522_find(rc522 *dev, rc522_uid *uid);
/* Same with WUPA: also wakes halted cards. */
rc522_status rc522_find_any(rc522 *dev, rc522_uid *uid);
/* Selected card to HALT: it ignores rc522_find until it leaves the field. */
rc522_status rc522_halt(rc522 *dev);

rc522_type  rc522_card_type(uint8_t sak);
const char *rc522_type_name(rc522_type t);

/* MIFARE Classic: authenticate to the sector of block with key A (use_key_b 0) or key B.
 * Needs the selected card's uid; uses its last 4 bytes (7-byte UIDs: UID3..UID6, as NXP AN10927
 * says). RC522_AUTH on a wrong key (the card then needs a new find). */
rc522_status rc522_auth(rc522 *dev, int use_key_b, uint8_t block, const uint8_t key[6], const rc522_uid *uid);
void         rc522_stop_crypto(rc522 *dev);

/* READ: 16 bytes (MIFARE Classic: one block; Ultralight/NTAG: 4 pages from page block). */
rc522_status rc522_read(rc522 *dev, uint8_t block, uint8_t out[16]);
/* MIFARE Classic WRITE of one 16-byte block (after rc522_auth). Block 0 and sector trailers
 * are refused here (RC522_ARG): writing them wrongly bricks a sector. */
rc522_status rc522_write(rc522 *dev, uint8_t block, const uint8_t data[16]);
/* Ultralight/NTAG WRITE of one 4-byte page. Pages 0..3 (UID, lock, OTP) refused (RC522_ARG). */
rc522_status rc522_ul_write(rc522 *dev, uint8_t page, const uint8_t data[4]);

/* "04:A3:1F:..." into out (3*size bytes incl. NUL); returns out. */
char *rc522_uid_hex(const rc522_uid *uid, char *out, size_t size);
/* 1 if a and b are the same card. */
int rc522_uid_equal(const rc522_uid *a, const rc522_uid *b);

/* CRC_A (ISO 14443-3), as sent after frames: out[0] low byte, out[1] high byte. */
void rc522_crc_a(const uint8_t *data, size_t n, uint8_t out[2]);

const char *rc522_status_name(rc522_status s);

#ifdef __cplusplus
}
#endif

#endif
