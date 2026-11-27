// Host tests for the irrx core: simulated remotes -> receiver model -> irrx_edge -> decode.
#include <cstdio>
#include <string>
#include "fake_remote.h"
#include "irrx.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint32_t rng = 99;
static uint32_t rnd() { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static bool expect(const irrx_result &r, irrx_protocol p, uint16_t a, uint16_t c) {
    return r.protocol == p && r.address == a && r.command == c;
}

// every protocol, random codes, ideal and through a sloppy receiver
static void test_protocols() {
    const int stretches[] = {0, 100, 180, -60};
    for (int s : stretches) {
        Receiver rx;
        rx.stretch = s;
        rx.jitter = 60;
        irrx_result r;
        for (int i = 0; i < 300; i++) {
            uint8_t a = (uint8_t)rnd(), c = (uint8_t)rnd();
            int bad = 0;
            // pauses > IRRX_REPEAT_MS so nothing counts as held
            if (!rx.play(Remote::nec(a, c), &r, 250) || !expect(r, IRRX_NEC, a, c) || r.repeat) bad = 1;
            uint16_t ea = (uint16_t)(rnd() & 0xFFFF);
            if ((uint8_t)(ea ^ (ea >> 8)) == 0xFF) ea ^= 0x0100;
            if (!rx.play(Remote::nec(ea, c, true), &r, 250) || !expect(r, IRRX_NEC, ea, c)) bad |= 2;
            if (!rx.play(Remote::samsung(a, c), &r, 250) || !expect(r, IRRX_SAMSUNG, (uint16_t)(a | a << 8), c)) bad |= 4;
            const int bits[] = {12, 15, 20};
            int sb = bits[rnd() % 3];
            uint16_t sa = (uint16_t)(rnd() & ((1u << (sb - 7)) - 1));
            if (!rx.play(Remote::sony(c & 0x7F, sa, sb), &r, 250) || !expect(r, IRRX_SONY, sa, c & 0x7F) || r.bits != sb) bad |= 8;
            int t = rnd() & 1;
            if (!rx.play(Remote::rc5(a & 0x1F, c & 0x7F, t), &r, 250) || !expect(r, IRRX_RC5, a & 0x1F, c & 0x7F) || r.toggle != t) bad |= 16;
            if (!rx.play(Remote::rc6(a, c, t), &r, 250) || !expect(r, IRRX_RC6, a, c) || r.toggle != t) bad |= 32;
            if (bad) {
                std::printf("FAIL stretch %d code %d: mask %d (got %s %04X %04X)\n", s, i, bad,
                            irrx_protocol_name(r.protocol), r.address, r.command);
                fails++;
                break;
            }
        }
    }
}

static void test_nec_hold() {
    Receiver rx;
    irrx_result r;
    CHECK(!rx.play(Remote::nec_repeat(), &r, 40));               // repeat of nothing: ignored
    CHECK(rx.play(Remote::nec(0x04, 0x08), &r, 40) && !r.repeat);
    for (int i = 0; i < 3; i++) {                                // held: repeat codes every 108 ms
        CHECK(rx.play(Remote::nec_repeat(), &r, 96) && r.repeat && expect(r, IRRX_NEC, 0x04, 0x08));
    }
    rx.idle(500, &r);
    CHECK(!rx.play(Remote::nec_repeat(), &r, 40));               // too late: not ours
}

static void test_sony_and_rc5_repeat() {
    Receiver rx;
    irrx_result r;
    // Sony sends every frame 3 times, 45 ms apart
    CHECK(rx.play(Remote::sony(0x15, 0x01, 12), &r, 25) && !r.repeat);
    CHECK(rx.play(Remote::sony(0x15, 0x01, 12), &r, 25) && r.repeat);
    CHECK(rx.play(Remote::sony(0x15, 0x01, 12), &r, 300) && r.repeat);
    CHECK(rx.play(Remote::sony(0x15, 0x01, 12), &r, 25) && !r.repeat);   // new press
    CHECK(rx.play(Remote::sony(0x16, 0x01, 12), &r, 25) && !r.repeat);   // other button
    // RC5: same toggle = held, flipped toggle = new press even if fast
    rx.idle(300, &r);
    CHECK(rx.play(Remote::rc5(0, 12, 0), &r, 90) && !r.repeat);
    CHECK(rx.play(Remote::rc5(0, 12, 0), &r, 90) && r.repeat);
    CHECK(rx.play(Remote::rc5(0, 12, 1), &r, 90) && !r.repeat && r.toggle == 1);
    CHECK(rx.play(Remote::rc6(0, 12, 1), &r, 90) && !r.repeat);          // other protocol
}

static void test_unknown_learn() {
    Receiver rx;
    irrx_result r1, r2, r3;
    Frame weird = {3300, 1700};                                  // made-up 3.3/1.7 ms header protocol
    for (int i = 0; i < 20; i++) { weird.push_back(420); weird.push_back((i % 3) ? 1260 : 420); }
    weird.push_back(420);
    Frame weird2 = weird;
    weird2[9] = 1260;                                             // another button
    rx.jitter = 40;
    CHECK(rx.play(weird, &r1, 300) && r1.protocol == IRRX_UNKNOWN && r1.bits == 0);
    CHECK(rx.play(weird, &r2, 300) && r2.hash == r1.hash && !r2.repeat);
    CHECK(rx.play(weird, &r2, 50) && r2.hash == r1.hash && !r2.repeat);
    CHECK(rx.play(weird, &r2, 50) && r2.hash == r1.hash && r2.repeat);   // held
    CHECK(rx.play(weird2, &r3, 300) && r3.hash != r1.hash);
    uint16_t n;
    const volatile uint16_t *raw = irrx_raw(&rx.rx, &n);
    CHECK(n == weird2.size() && raw[0] > 3000 && raw[0] < 3600);
}

static void test_noise_and_errors() {
    Receiver rx;
    irrx_result r;
    CHECK(!rx.play({300, 200, 300}, &r));                         // glitch
    CHECK(rx.play(Remote::nec(1, 2), &r, 250) && expect(r, IRRX_NEC, 1, 2));
    Frame huge;
    for (int i = 0; i < 201; i++) huge.push_back(500);
    CHECK(!rx.play(huge, &r));                                   // overflow: dropped
    CHECK(rx.play(Remote::nec(1, 3), &r, 250) && expect(r, IRRX_NEC, 1, 3));  // recovers
    // missed edge: two falling edges in a row -> frame dropped, next one fine
    irrx_edge(&rx.rx, rx.now, 0);
    rx.now += 9000; irrx_edge(&rx.rx, rx.now, 1);
    rx.now += 4500; irrx_edge(&rx.rx, rx.now, 1);
    CHECK(rx.rx.state == IRRX_IDLE);
    CHECK(rx.play(Remote::nec(1, 4), &r, 250) && expect(r, IRRX_NEC, 1, 4));
    // loop() too slow: the next frame's first edge ends the frame, nothing is mixed up
    rx.feed(Remote::nec(7, 7));
    rx.now += 30000;
    irrx_edge(&rx.rx, rx.now, 0);                                 // next frame starts, frame 1 ready
    CHECK(rx.rx.state == IRRX_READY);
    CHECK(irrx_check(&rx.rx, rx.now) && irrx_read(&rx.rx, &r) && expect(r, IRRX_NEC, 7, 7));
    CHECK(!irrx_read(&rx.rx, &r));                                // nothing waiting
    // garbled NEC (command inverse wrong) is not NEC
    uint8_t b[4] = {1, 0xFE, 5, 5};
    CHECK(rx.play(Remote::pulse_distance(9000, 4500, b), &r, 250) && r.protocol == IRRX_UNKNOWN);
}

static void test_pure_decode() {
    irrx_decoder dec = {};
    irrx_result r;
    Frame f = Remote::rc6(0x04, 0x0C, 0);
    CHECK(irrx_decode(&dec, f.data(), (uint16_t)f.size(), 0, &r) && expect(r, IRRX_RC6, 0x04, 0x0C));
    CHECK(!irrx_decode(&dec, f.data(), 5, 0, &r));
    CHECK(std::string(irrx_protocol_name(IRRX_SONY)) == "Sony");
}

int main() {
    test_protocols();
    test_nec_hold();
    test_sony_and_rc5_repeat();
    test_unknown_learn();
    test_noise_and_errors();
    test_pure_decode();
    if (fails) { std::printf("%d failure(s)\n", fails); return 1; }
    std::printf("irrx %s: all tests passed\n", IRRX_VERSION);
    return 0;
}
