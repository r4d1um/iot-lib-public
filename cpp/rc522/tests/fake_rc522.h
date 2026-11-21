// Fake MFRC522 + ISO 14443A cards for host tests, at SPI level.
//
// Chip: registers, 64-byte FIFO, Transceive with BitFramingReg (TxLastBits, RxAlign, StartSend),
// ComIrqReg, ErrorReg (CollErr), CollReg (CollPos counted from the first received byte incl.
// RxAlign, bits after a collision read 0), ControlReg (RxLastBits), MFAuthent + Status2Reg
// Crypto1On, SoftReset, VersionReg. Address byte = reg << 1, bit 7 = read; in a read burst every
// byte sent is the next address, as on the real chip.
// Cards: IDLE/READY/ACTIVE/HALT state machine, REQA/WUPA, cascade levels for 4/7/10-byte UIDs,
// bit-exact anticollision with several cards, SELECT/SAK, HLTA, CRC_A checked on every frame,
// MIFARE Classic 1K (keys per sector, READ/WRITE after auth) and Ultralight/NTAG pages.
// Encryption itself is the chip's business, so after MFAuthent the fake talks plain.
#ifndef FAKE_RC522_H
#define FAKE_RC522_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "rc522.h"

struct FakeCard {
    enum Kind { CLASSIC_1K, ULTRALIGHT } kind;
    enum State { IDLE, READY, ACTIVE, HALT } state = IDLE;
    std::vector<uint8_t> uid;
    uint8_t atqa[2];
    uint8_t sak;
    std::vector<uint8_t> mem;      // Classic: 64 x 16 bytes; UL: pages x 4 bytes
    int level = 0;                 // cascade levels already selected
    int authed = -1;               // sector authenticated with Crypto1
    int pending_write = -1;        // Classic WRITE: block waiting for its data
    bool in_field = true;

    static FakeCard classic(std::vector<uint8_t> uid) {
        FakeCard c;
        c.kind = CLASSIC_1K;
        c.uid = uid;
        c.atqa[0] = uid.size() == 4 ? 0x04 : 0x44;
        c.atqa[1] = 0x00;
        c.sak = 0x08;
        c.mem.assign(64 * 16, 0);
        for (size_t i = 0; i < 4 && i < uid.size(); i++) c.mem[i] = uid[i];
        c.mem[4] = (uint8_t)(uid[0] ^ uid[1] ^ uid[2] ^ uid[3]);
        for (int s = 0; s < 16; s++) {  // transport keys FF..FF, access bits FF 07 80 69
            uint8_t *t = &c.mem[(s * 4 + 3) * 16];
            std::memset(t, 0xFF, 6);
            t[6] = 0xFF; t[7] = 0x07; t[8] = 0x80; t[9] = 0x69;
            std::memset(t + 10, 0xFF, 6);
        }
        return c;
    }
    static FakeCard ultralight(std::vector<uint8_t> uid, int pages = 45) {  // NTAG213 size
        FakeCard c;
        c.kind = ULTRALIGHT;
        c.uid = uid;
        c.atqa[0] = 0x44;
        c.atqa[1] = 0x00;
        c.sak = 0x00;
        c.mem.assign((size_t)pages * 4, 0);
        for (int i = 0; i < 7; i++) c.mem[i < 3 ? i : i + 1] = uid[i];
        return c;
    }
    const uint8_t *key(int sector, bool b) const { return &mem[(sector * 4 + 3) * 16 + (b ? 10 : 0)]; }
    int levels() const { return uid.size() == 4 ? 1 : uid.size() == 7 ? 2 : 3; }
    // UID CLn incl. cascade tag and BCC, 5 bytes
    void cl(int lvl, uint8_t out[5]) const {
        int n = levels();
        int off = lvl * 3;
        if (lvl < n - 1) { out[0] = 0x88; for (int i = 0; i < 3; i++) out[1 + i] = uid[off + i]; }
        else for (int i = 0; i < 4; i++) out[i] = uid[off + i];
        out[4] = (uint8_t)(out[0] ^ out[1] ^ out[2] ^ out[3]);
    }
    static int bit(const uint8_t *b, int i) { return (b[i / 8] >> (i % 8)) & 1; }
};

struct FakeRc522 {
    uint8_t regs[64];
    std::vector<uint8_t> fifo;
    std::vector<FakeCard> cards;
    uint8_t version = 0x92;
    bool chip_present = true;
    int fail_transfers = 0;        // make the next n transfers fail
    int transfers = 0, frames = 0;
    uint8_t rx_last_bits = 0, collpos = 0;
    bool trace = false;            // print every frame and answer (debugging)

    FakeRc522() { reset(); }
    void reset() { std::memset(regs, 0, sizeof regs); fifo.clear(); regs[0x01] = 0x20; regs[0x11] = 0x3F; }

    static int transfer(void *ctx, uint8_t *buf, uint16_t n) { return static_cast<FakeRc522 *>(ctx)->spi(buf, n); }
    static void delay(void *, uint32_t) {}
    rc522_bus bus() { rc522_bus b = {transfer, delay, this}; return b; }

    int spi(uint8_t *buf, uint16_t n) {
        transfers++;
        if (fail_transfers > 0) { fail_transfers--; return -1; }
        if (!chip_present) { std::memset(buf, 0, n); return 0; }
        uint8_t reg = (uint8_t)((buf[0] >> 1) & 0x3F);
        if (buf[0] & 0x80) {           // read burst: each byte is the next address
            std::vector<uint8_t> out(n, 0);
            for (uint16_t i = 1; i < n; i++) {
                out[i] = read(reg);
                reg = (uint8_t)((buf[i] >> 1) & 0x3F);
            }
            std::memcpy(buf, out.data(), n);
        } else {
            for (uint16_t i = 1; i < n; i++) write(reg, buf[i]);
            std::memset(buf, 0, n);
        }
        return 0;
    }

    uint8_t read(uint8_t reg) {
        switch (reg) {
        case 0x09: { if (fifo.empty()) return 0; uint8_t v = fifo.front(); fifo.erase(fifo.begin()); return v; }
        case 0x0A: return (uint8_t)fifo.size();
        case 0x0C: return (uint8_t)(0x10 | rx_last_bits);
        case 0x0E: return (uint8_t)((regs[0x0E] & 0x80) | collpos);
        case 0x37: return version;
        default: return regs[reg];
        }
    }

    void write(uint8_t reg, uint8_t v) {
        switch (reg) {
        case 0x01:
            regs[0x01] = (uint8_t)(v & 0x3F);
            if ((v & 0x0F) == 0x0F) reset();
            else if ((v & 0x0F) == 0x0E) authent();
            return;
        case 0x04:  // Set1: bit 7 sets the marked bits, else clears them
            if (v & 0x80) regs[0x04] |= (uint8_t)(v & 0x7F); else regs[0x04] &= (uint8_t)~v;
            return;
        case 0x09: if (fifo.size() < 64) fifo.push_back(v); else regs[0x06] |= 0x10; return;
        case 0x0A: if (v & 0x80) fifo.clear(); return;
        case 0x0D:
            regs[0x0D] = v;
            if ((v & 0x80) && (regs[0x01] & 0x0F) == 0x0C) transceive((uint8_t)(v & 7), (uint8_t)((v >> 4) & 7));
            return;
        case 0x08:
            regs[0x08] = v;
            if (!(v & 0x08)) for (auto &c : cards) c.authed = -1;
            return;
        default: regs[reg] = v;
        }
    }

    FakeCard *active() {
        for (auto &c : cards) if (c.in_field && c.state == FakeCard::ACTIVE) return &c;
        return nullptr;
    }

    void respond(const std::vector<uint8_t> &r, uint8_t last_bits = 0) {
        if (trace) { std::printf("   <-"); for (auto b : r) std::printf(" %02X", b); std::printf(" (bits %u, err %02X, collpos %u)\n", last_bits, regs[0x06], collpos); }
        fifo = r;
        rx_last_bits = last_bits;
        regs[0x04] |= 0x30;  // RxIRq | IdleIRq
    }
    void silence() { if (trace) std::printf("   <- (silence)\n"); fifo.clear(); regs[0x04] |= 0x01; }  // timer ran out
    void ack(bool ok) { respond({(uint8_t)(ok ? 0x0A : 0x04)}, 4); }
    static bool crc_ok(const std::vector<uint8_t> &f) {
        if (f.size() < 3) return false;
        uint8_t c[2];
        rc522_crc_a(f.data(), f.size() - 2, c);
        return c[0] == f[f.size() - 2] && c[1] == f[f.size() - 1];
    }
    static std::vector<uint8_t> with_crc(std::vector<uint8_t> d) {
        uint8_t c[2];
        rc522_crc_a(d.data(), d.size(), c);
        d.push_back(c[0]);
        d.push_back(c[1]);
        return d;
    }

    void transceive(uint8_t tx_bits, uint8_t rx_align) {
        std::vector<uint8_t> f = fifo;
        fifo.clear();
        frames++;
        regs[0x06] = 0;
        collpos = 0;
        rx_last_bits = 0;
        if (trace) { std::printf("-> "); for (auto b : f) std::printf(" %02X", b); std::printf(" (tx bits %u, align %u)\n", tx_bits, rx_align); }
        if (f.empty()) { silence(); return; }

        // REQA / WUPA: short frame
        if (f.size() == 1 && tx_bits == 7) {
            bool wupa = f[0] == 0x52;
            std::vector<FakeCard *> ready;
            for (auto &c : cards) {
                if (!c.in_field) continue;
                if (c.state == FakeCard::ACTIVE) { c.state = FakeCard::IDLE; c.authed = -1; continue; }  // unexpected frame
                if (c.state == FakeCard::IDLE || (wupa && c.state == FakeCard::HALT) || c.state == FakeCard::READY) {
                    c.state = FakeCard::READY;
                    c.level = 0;
                    ready.push_back(&c);
                }
            }
            if (ready.empty()) { silence(); return; }
            uint8_t a0 = ready[0]->atqa[0], a1 = ready[0]->atqa[1];
            for (auto *c : ready) if (c->atqa[0] != a0 || c->atqa[1] != a1) regs[0x06] |= 0x08;
            respond({a0, a1});
            return;
        }

        uint8_t cmd = f[0];
        if ((cmd == 0x93 || cmd == 0x95 || cmd == 0x97) && f.size() >= 2) {
            int lvl = (cmd - 0x93) / 2;
            if (f[1] == 0x70) { select(lvl, f); return; }
            anticoll(lvl, f, tx_bits, rx_align);
            return;
        }

        FakeCard *c = active();
        if (!c) { silence(); return; }
        if (!crc_ok(f)) { silence(); return; }  // cards ignore damaged frames

        if (c->pending_write >= 0) {             // Classic WRITE, part 2: 16 data bytes
            int b = c->pending_write;
            c->pending_write = -1;
            if (f.size() != 18) { ack(false); return; }
            std::memcpy(&c->mem[b * 16], f.data(), 16);
            ack(true);
            return;
        }
        switch (cmd) {
        case 0x50:  // HLTA
            c->state = FakeCard::HALT;
            c->authed = -1;
            silence();
            return;
        case 0x30: {  // READ
            int b = f[1];
            std::vector<uint8_t> d;
            if (c->kind == FakeCard::CLASSIC_1K) {
                if (b >= 64 || c->authed != b / 4) { ack(false); c->state = FakeCard::IDLE; return; }
                d.assign(&c->mem[b * 16], &c->mem[b * 16] + 16);
            } else {
                int pages = (int)c->mem.size() / 4;
                if (b >= pages) { ack(false); c->state = FakeCard::IDLE; return; }
                for (int i = 0; i < 16; i++) d.push_back(c->mem[((b * 4 + i) % (pages * 4))]);
            }
            respond(with_crc(d));
            return;
        }
        case 0xA0: {  // Classic WRITE, part 1
            int b = f[1];
            if (c->kind != FakeCard::CLASSIC_1K || b >= 64 || c->authed != b / 4) { ack(false); return; }
            c->pending_write = b;
            ack(true);
            return;
        }
        case 0xA2: {  // Ultralight WRITE
            int p = f[1];
            if (c->kind != FakeCard::ULTRALIGHT || f.size() != 8 || p < 2 || p >= (int)c->mem.size() / 4) { ack(false); return; }
            std::memcpy(&c->mem[p * 4], &f[2], 4);
            ack(true);
            return;
        }
        default:
            ack(false);
        }
    }

    void anticoll(int lvl, const std::vector<uint8_t> &f, uint8_t tx_bits, uint8_t rx_align) {
        int nvb_bytes = f[1] >> 4, nvb_bits = f[1] & 7;
        int kb = (nvb_bytes - 2) * 8 + nvb_bits;
        if (kb < 0 || kb > 32 || nvb_bits != tx_bits || rx_align != nvb_bits) { silence(); return; }
        std::vector<std::vector<uint8_t>> cls;
        for (auto &c : cards) {
            if (!c.in_field || c.state != FakeCard::READY || c.level != lvl) continue;
            uint8_t cl[5];
            c.cl(lvl, cl);
            bool match = true;
            for (int i = 0; i < kb && match; i++) match = FakeCard::bit(cl, i) == FakeCard::bit(&f[2], i);
            if (match) cls.emplace_back(cl, cl + 5);
        }
        if (cls.empty()) { silence(); return; }
        int coll = -1;  // first bit where the answering cards differ
        for (int i = kb; i < 40 && coll < 0; i++)
            for (auto &cl : cls)
                if (FakeCard::bit(cl.data(), i) != FakeCard::bit(cls[0].data(), i)) { coll = i; break; }
        int first_byte = kb / 8;
        std::vector<uint8_t> r(5 - first_byte, 0);
        for (int i = kb; i < 40; i++) {
            if (coll >= 0 && i >= coll) break;  // ValuesAfterColl = 0: cleared from the collision on
            if (FakeCard::bit(cls[0].data(), i)) r[i / 8 - first_byte] |= (uint8_t)(1 << (i % 8));
        }
        if (coll >= 0) {
            regs[0x06] |= 0x08;
            collpos = (uint8_t)((coll - first_byte * 8 + 1) & 0x1F);
        }
        respond(r);
    }

    void select(int lvl, const std::vector<uint8_t> &f) {
        if (f.size() != 9 || !crc_ok(f)) { silence(); return; }
        for (auto &c : cards) {
            if (!c.in_field || c.state != FakeCard::READY || c.level != lvl) continue;
            uint8_t cl[5];
            c.cl(lvl, cl);
            if (std::memcmp(cl, &f[2], 5) != 0) continue;
            bool last = lvl == c.levels() - 1;
            if (last) {
                for (auto &o : cards) if (&o != &c && o.state == FakeCard::READY) o.state = FakeCard::IDLE;
                c.state = FakeCard::ACTIVE;
                c.authed = -1;
                respond(with_crc({c.sak}));
            } else {
                c.level++;
                respond(with_crc({0x04}));
            }
            return;
        }
        silence();
    }

    void authent() {
        std::vector<uint8_t> f = fifo;
        fifo.clear();
        regs[0x04] &= (uint8_t)~0x7F;
        FakeCard *c = active();
        if (!c || f.size() != 12 || c->kind != FakeCard::CLASSIC_1K || (f[0] != 0x60 && f[0] != 0x61) || f[1] >= 64) {
            regs[0x04] |= 0x01;
            return;
        }
        int sector = f[1] / 4;
        bool key_ok = std::memcmp(c->key(sector, f[0] == 0x61), &f[2], 6) == 0;
        bool uid_ok = std::memcmp(&c->uid[c->uid.size() - 4], &f[8], 4) == 0;
        if (key_ok && uid_ok) {
            c->authed = sector;
            regs[0x08] |= 0x08;
            regs[0x04] |= 0x10;
        } else {
            c->state = FakeCard::IDLE;
            regs[0x04] |= 0x01;
        }
    }
};

#endif
