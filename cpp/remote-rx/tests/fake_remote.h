// Simulated IR remotes and receiver for host tests. Encoders are written from the protocol
// specs, independent of the decoders. Receiver model: like a VS1838B the output stretches marks
// and shortens spaces (stretch µs) with random jitter; feed() turns the durations into pin edges
// with timestamps and hands them to irrx_edge() exactly as the interrupt would.
#ifndef FAKE_REMOTE_H
#define FAKE_REMOTE_H

#include <cstdint>
#include <vector>
#include "irrx.h"

typedef std::vector<uint16_t> Frame;  // mark, space, mark, ... (µs), ends with a mark

static Frame halves_to_frame(const std::vector<int> &halves, uint16_t t) {
    Frame f;
    size_t i = 0;
    while (i < halves.size() && halves[i] == 0) i++;  // leading space = idle line
    while (i < halves.size()) {
        int lvl = halves[i];
        uint32_t len = 0;
        while (i < halves.size() && halves[i] == lvl) { len += t; i++; }
        f.push_back((uint16_t)len);
    }
    if (!f.empty() && f.size() % 2 == 0) f.pop_back();  // trailing space = idle line
    return f;
}

struct Remote {
    static Frame pulse_distance(uint16_t hm, uint16_t hs, const uint8_t b[4]) {
        Frame f = {hm, hs};
        for (int i = 0; i < 32; i++) {
            f.push_back(560);
            f.push_back(((b[i / 8] >> (i % 8)) & 1) ? 1690 : 560);
        }
        f.push_back(560);
        return f;
    }
    static Frame nec(uint16_t addr, uint8_t cmd, bool extended = false) {
        uint8_t b[4] = {(uint8_t)addr, (uint8_t)(extended ? addr >> 8 : ~addr), cmd, (uint8_t)~cmd};
        return pulse_distance(9000, 4500, b);
    }
    static Frame nec_repeat() { return {9000, 2250, 560}; }
    static Frame samsung(uint8_t addr, uint8_t cmd) {
        uint8_t b[4] = {addr, addr, cmd, (uint8_t)~cmd};
        return pulse_distance(4500, 4500, b);
    }
    static Frame sony(uint8_t cmd, uint16_t addr, int bits) {
        uint32_t v = (cmd & 0x7Fu) | ((uint32_t)addr << 7);
        Frame f = {2400, 600};
        for (int i = 0; i < bits; i++) {
            f.push_back(((v >> i) & 1) ? 1200 : 600);
            if (i + 1 < bits) f.push_back(600);
        }
        return f;
    }
    static Frame rc5(uint8_t addr, uint8_t cmd, int toggle) {
        std::vector<int> bits = {1, !((cmd >> 6) & 1), toggle};
        for (int i = 4; i >= 0; i--) bits.push_back((addr >> i) & 1);
        for (int i = 5; i >= 0; i--) bits.push_back((cmd >> i) & 1);
        std::vector<int> h;
        for (int b : bits) { h.push_back(b ? 0 : 1); h.push_back(b ? 1 : 0); }  // 1 = space-mark
        return halves_to_frame(h, 889);
    }
    static Frame rc6(uint8_t addr, uint8_t cmd, int toggle) {
        std::vector<int> h = {1, 0};                                  // start bit 1
        for (int i = 0; i < 3; i++) { h.push_back(0); h.push_back(1); }  // mode 000
        h.push_back(toggle); h.push_back(toggle); h.push_back(!toggle); h.push_back(!toggle);
        uint16_t v = (uint16_t)(addr << 8 | cmd);
        for (int i = 15; i >= 0; i--) { int b = (v >> i) & 1; h.push_back(b); h.push_back(!b); }  // 1 = mark-space
        Frame f = halves_to_frame(h, 444);
        f.insert(f.begin(), {2666, 889});
        return f;
    }
};

struct Receiver {
    irrx rx;
    uint32_t now = 1000000;
    uint32_t seed = 7;
    int stretch = 0, jitter = 0;
    Receiver() { irrx_init(&rx); }

    int rnd(int range) {  // -range..range
        seed = seed * 1103515245u + 12345u;
        return range ? (int)((seed >> 16) % (2u * range + 1u)) - range : 0;
    }
    // play a frame; returns the frame's end time
    void feed(const Frame &f) {
        irrx_edge(&rx, now, 0);
        for (size_t i = 0; i < f.size(); i++) {
            int d = (int)f[i] + ((i % 2 == 0) ? stretch : -stretch) + rnd(jitter);
            if (d < 50) d = 50;
            now += (uint32_t)d;
            irrx_edge(&rx, now, (i % 2 == 0) ? 1 : 0);
        }
    }
    // let the line rest ms milliseconds, then poll like loop() would
    int idle(uint32_t ms, irrx_result *out) {
        now += ms * 1000u;
        if (!irrx_check(&rx, now)) return 0;
        return irrx_read(&rx, out);
    }
    int play(const Frame &f, irrx_result *out, uint32_t pause_ms = 20) {
        feed(f);
        return idle(pause_ms, out);
    }
};

#endif
