// Host tests for the rc522 core against the fake MFRC522 and cards (make test).
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include "fake_rc522.h"
#include "rc522.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
#define CHECK_ST(a, b) do { rc522_status x_ = (a), y_ = (b); if (x_ != y_) { std::printf("FAIL %s:%d got %s want %s\n", __FILE__, __LINE__, rc522_status_name(x_), rc522_status_name(y_)); fails++; } } while (0)

static std::string hex(const rc522_uid &u) { char b[32]; return rc522_uid_hex(&u, b, sizeof b); }

static void setup(FakeRc522 &f, rc522 &d) {
    rc522_bus b = f.bus();
    CHECK_ST(rc522_init(&d, &b), RC522_OK);
}

static void test_crc_and_init() {
    uint8_t hlta[2] = {0x50, 0x00}, c[2];
    rc522_crc_a(hlta, 2, c);
    CHECK(c[0] == 0x57 && c[1] == 0xCD);          // known HLTA CRC
    uint8_t read0[2] = {0x30, 0x00};
    rc522_crc_a(read0, 2, c);
    CHECK(c[0] == 0x02 && c[1] == 0xA8);          // known READ 0 CRC

    FakeRc522 f;
    rc522 d;
    setup(f, d);
    CHECK(d.version == 0x92);
    CHECK(f.regs[0x2A] == 0x80 && f.regs[0x2B] == 0xA9 && f.regs[0x2C] == 0x03 && f.regs[0x2D] == 0xE8);
    CHECK(f.regs[0x15] == 0x40 && f.regs[0x11] == 0x3D && (f.regs[0x14] & 3) == 3);
    rc522_gain(&d, 7);
    CHECK(f.regs[0x26] == 0x70);
    rc522_antenna(&d, 0);
    CHECK((f.regs[0x14] & 3) == 0);

    FakeRc522 none;
    none.chip_present = false;
    rc522_bus b = none.bus();
    CHECK_ST(rc522_init(&d, &b), RC522_BUS);
    FakeRc522 broken;
    broken.fail_transfers = 1;
    b = broken.bus();
    CHECK_ST(rc522_init(&d, &b), RC522_BUS);
}

static void test_find_sizes() {
    FakeRc522 f;
    rc522 d;
    setup(f, d);
    rc522_uid u;
    CHECK_ST(rc522_find(&d, &u), RC522_NO_CARD);
    CHECK(u.size == 0);

    f.cards.push_back(FakeCard::classic({0xDE, 0xAD, 0xBE, 0xEF}));
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK(hex(u) == "DE:AD:BE:EF" && u.sak == 0x08 && u.atqa[0] == 0x04);
    CHECK(rc522_card_type(u.sak) == RC522_TYPE_MIFARE_1K);

    FakeRc522 g;
    setup(g, d);
    g.cards.push_back(FakeCard::ultralight({0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66}));
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK(u.size == 7 && hex(u) == "04:11:22:33:44:55:66" && u.sak == 0x00);
    CHECK(rc522_card_type(u.sak) == RC522_TYPE_ULTRALIGHT);

    FakeRc522 h;
    setup(h, d);
    FakeCard big = FakeCard::classic({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    big.sak = 0x20;
    h.cards.push_back(big);
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK(u.size == 10 && u.bytes[9] == 10 && rc522_card_type(u.sak) == RC522_TYPE_ISO_14443_4);
}

static void test_states() {
    FakeRc522 f;
    rc522 d;
    setup(f, d);
    f.cards.push_back(FakeCard::classic({0x11, 0x22, 0x33, 0x44}));
    rc522_uid u;
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK_ST(rc522_halt(&d), RC522_OK);
    CHECK(f.cards[0].state == FakeCard::HALT);
    CHECK_ST(rc522_find(&d, &u), RC522_NO_CARD);     // halted: REQA ignored
    CHECK_ST(rc522_find_any(&d, &u), RC522_OK);      // WUPA wakes it
    // without halt, an active card drops to IDLE on the next REQA: every other call finds it
    CHECK_ST(rc522_find(&d, &u), RC522_NO_CARD);
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    // card leaves mid-exchange
    f.cards[0].in_field = false;
    uint8_t blk[16];
    CHECK_ST(rc522_read(&d, 4, blk), RC522_TIMEOUT);
}

static uint32_t rng = 12345;
static uint8_t rnd() { rng = rng * 1103515245u + 12345u; return (uint8_t)(rng >> 16); }

// several cards at once: find + halt must return every card exactly once
static void test_anticollision() {
    FakeRc522 f;
    rc522 d;
    setup(f, d);
    f.cards.push_back(FakeCard::classic({0x10, 0x00, 0x00, 0x00}));
    f.cards.push_back(FakeCard::classic({0x11, 0x00, 0x00, 0x00}));  // differ in bit 0
    rc522_uid u;
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK(hex(u) == "11:00:00:00");                  // the 1-branch wins
    rc522_halt(&d);
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK(hex(u) == "10:00:00:00");
    rc522_halt(&d);
    CHECK_ST(rc522_find(&d, &u), RC522_NO_CARD);

    for (int round = 0; round < 500; round++) {
        FakeRc522 g;
        setup(g, d);
        int n = 2 + rnd() % 5;
        std::set<std::string> want, got;
        for (int i = 0; i < n; i++) {
            int len = (rnd() % 3 == 0) ? 7 : 4;
            std::vector<uint8_t> id(len);
            for (auto &b : id) b = rnd();
            if (len == 4 && id[0] == 0x88) id[0] = 0x89;  // CT is not a valid first UID byte
            if (round % 4 == 0 && i > 0) {               // near-identical UIDs: late, mid-byte collisions
                id = g.cards[0].uid;
                id[id.size() - 1] ^= (uint8_t)(1u << (rnd() % 8));
            }
            FakeCard c = id.size() == 7 ? FakeCard::ultralight(id) : FakeCard::classic(id);
            rc522_uid tmp;
            tmp.size = (uint8_t)id.size();
            std::copy(id.begin(), id.end(), tmp.bytes);
            if (want.insert(hex(tmp)).second) g.cards.push_back(c);
        }
        for (int k = 0; k < 20; k++) {
            rc522_status st = rc522_find(&d, &u);
            if (st == RC522_NO_CARD) break;
            CHECK_ST(st, RC522_OK);
            if (st != RC522_OK) break;
            CHECK(got.insert(hex(u)).second);
            CHECK_ST(rc522_halt(&d), RC522_OK);
        }
        CHECK(got == want);
        if (got != want) { std::printf("  round %d: %zu of %zu cards\n", round, got.size(), want.size()); break; }
    }
}

static void test_classic() {
    FakeRc522 f;
    rc522 d;
    setup(f, d);
    f.cards.push_back(FakeCard::classic({0xCA, 0xFE, 0xBA, 0xBE}));
    rc522_uid u;
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    const uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t blk[16], data[16];
    for (int i = 0; i < 16; i++) data[i] = (uint8_t)(i * 7);
    CHECK_ST(rc522_auth(&d, 0, 4, key, &u), RC522_OK);
    CHECK((f.regs[0x08] & 0x08) != 0);
    CHECK_ST(rc522_write(&d, 4, data), RC522_OK);
    CHECK_ST(rc522_read(&d, 4, blk), RC522_OK);
    CHECK(std::memcmp(blk, data, 16) == 0);
    CHECK(std::memcmp(&f.cards[0].mem[64], data, 16) == 0);
    CHECK_ST(rc522_write(&d, 0, data), RC522_ARG);    // manufacturer block
    CHECK_ST(rc522_write(&d, 7, data), RC522_ARG);    // sector trailer
    CHECK_ST(rc522_read(&d, 8, blk), RC522_NAK);      // sector 2 not authenticated
    // wrong key
    CHECK_ST(rc522_find_any(&d, &u), RC522_OK);
    const uint8_t bad[6] = {1, 2, 3, 4, 5, 6};
    CHECK_ST(rc522_auth(&d, 1, 4, bad, &u), RC522_AUTH);
    CHECK_ST(rc522_read(&d, 4, blk), RC522_TIMEOUT);  // card dropped out
    // key B works too, and stop_crypto ends the session
    CHECK_ST(rc522_find_any(&d, &u), RC522_OK);
    CHECK_ST(rc522_auth(&d, 1, 5, key, &u), RC522_OK);
    CHECK_ST(rc522_read(&d, 5, blk), RC522_OK);
    rc522_stop_crypto(&d);
    CHECK((f.regs[0x08] & 0x08) == 0);
    CHECK_ST(rc522_read(&d, 5, blk), RC522_NAK);

    // 7-byte Classic: auth uses the last 4 UID bytes
    FakeRc522 g;
    setup(g, d);
    g.cards.push_back(FakeCard::classic({0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6}));
    g.cards[0].sak = 0x08;
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    CHECK(u.size == 7);
    CHECK_ST(rc522_auth(&d, 0, 1, key, &u), RC522_OK);
    CHECK_ST(rc522_read(&d, 1, blk), RC522_OK);
    rc522_uid shortuid;
    shortuid.size = 0;
    CHECK_ST(rc522_auth(&d, 0, 1, key, &shortuid), RC522_ARG);
}

static void test_ultralight() {
    FakeRc522 f;
    rc522 d;
    setup(f, d);
    f.cards.push_back(FakeCard::ultralight({0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66}));
    rc522_uid u;
    CHECK_ST(rc522_find(&d, &u), RC522_OK);
    uint8_t blk[16];
    CHECK_ST(rc522_read(&d, 0, blk), RC522_OK);
    CHECK(blk[0] == 0x04 && blk[4] == 0x33 && blk[7] == 0x66);
    const uint8_t page[4] = {'H', 'i', '!', 0};
    CHECK_ST(rc522_ul_write(&d, 4, page), RC522_OK);
    CHECK_ST(rc522_read(&d, 4, blk), RC522_OK);
    CHECK(std::memcmp(blk, page, 4) == 0);
    CHECK_ST(rc522_ul_write(&d, 2, page), RC522_ARG);
    CHECK_ST(rc522_ul_write(&d, 200, page), RC522_NAK);
    CHECK_ST(rc522_read(&d, 200, blk), RC522_NAK);
}

static void test_bus_and_names() {
    FakeRc522 f;
    rc522 d;
    setup(f, d);
    f.cards.push_back(FakeCard::classic({1, 2, 3, 4}));
    rc522_uid u;
    f.fail_transfers = 3;
    CHECK_ST(rc522_find(&d, &u), RC522_BUS);
    CHECK_ST(rc522_find_any(&d, &u), RC522_OK);       // recovers
    CHECK(rc522_card_type(0x18) == RC522_TYPE_MIFARE_4K && rc522_card_type(0x09) == RC522_TYPE_MIFARE_MINI);
    CHECK(rc522_card_type(0x04) == RC522_TYPE_NOT_COMPLETE && rc522_card_type(0x10) == RC522_TYPE_MIFARE_PLUS);
    CHECK(rc522_card_type(0x28) == RC522_TYPE_ISO_14443_4 && rc522_card_type(0x01) == RC522_TYPE_UNKNOWN);
    CHECK(std::string(rc522_type_name(RC522_TYPE_MIFARE_1K)) == "MIFARE Classic 1K");
    char small[6];
    rc522_uid w;
    w.size = 4;
    w.bytes[0] = 0xAB; w.bytes[1] = 0xCD; w.bytes[2] = 0xEF; w.bytes[3] = 0x01;
    CHECK(std::string(rc522_uid_hex(&w, small, sizeof small)) == "AB:CD");
    char one[1];
    CHECK(std::string(rc522_uid_hex(&w, one, 1)) == "");
    rc522_uid v = w;
    CHECK(rc522_uid_equal(&w, &v));
    v.bytes[3] = 2;
    CHECK(!rc522_uid_equal(&w, &v));
}

int main() {
    test_crc_and_init();
    test_find_sizes();
    test_states();
    test_anticollision();
    test_classic();
    test_ultralight();
    test_bus_and_names();
    if (fails) { std::printf("%d failure(s)\n", fails); return 1; }
    std::printf("rc522 %s: all tests passed\n", RC522_VERSION);
    return 0;
}
