// Host tests for StampDisplay's core: Gfx on the in-memory panels, and the ST7735S protocol against
// a recording fake bus (the bytes the real display would receive).
#include <StampDisplay.h>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace stampdisplay;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

// Records what the panel sends: commands (DC low) and data bytes (DC high).
struct FakeBus : St7735sBus {
  struct Item { bool data; uint8_t b; };
  std::vector<Item> log;
  bool dcHigh = true, inWrite = false;
  int writes = 0, resets = 0, delayTotal = 0;
  bool wired = true;
  void beginWrite() override { CHECK(!inWrite); inWrite = true; ++writes; }
  void endWrite() override { CHECK(inWrite); inWrite = false; }
  void dc(bool d) override { dcHigh = d; }
  void write(const uint8_t* p, size_t n) override {
    CHECK(inWrite);
    for (size_t i = 0; i < n; ++i) log.push_back({dcHigh, p[i]});
  }
  bool hasReset() const override { return wired; }
  void reset(bool) override { ++resets; }
  void delayMs(uint16_t ms) override { delayTotal += ms; }

  // data bytes following the last occurrence of command `cmd`
  std::vector<uint8_t> after(uint8_t cmd) const {
    std::vector<uint8_t> out;
    for (size_t i = log.size(); i-- > 0;) {
      if (!log[i].data && log[i].b == cmd) {
        for (size_t j = i + 1; j < log.size() && log[j].data; ++j) out.push_back(log[j].b);
        break;
      }
    }
    return out;
  }
};

static void testInit() {
  FakeBus bus;
  St7735sConfig c{};
  c.pinCs = 1; c.pinDc = 2; c.pinRst = 3;
  c.rotation = 1;
  St7735sPanel p(bus, c);
  p.initPanel();
  CHECK(bus.resets == 3);
  CHECK(bus.delayTotal >= 150 + 500 + 10 + 100);
  CHECK(!bus.log.empty() && !bus.log[0].data && bus.log[0].b == 0x01);  // SWRESET first
  CHECK(bus.after(0x3A) == std::vector<uint8_t>{0x05});                 // RGB565
  CHECK(bus.after(0x36) == std::vector<uint8_t>{0x80 | 0x20});          // rotation 1: MY|MV
  CHECK(p.width() == 160 && p.height() == 128);

  bus.wired = false; bus.resets = 0;
  p.initPanel();
  CHECK(bus.resets == 0);
}

static void testWindowAndPixels() {
  FakeBus bus;
  St7735sConfig c{};
  c.pinCs = 1; c.pinDc = 2; c.pinRst = -1;
  c.rotation = 0; c.xOffset = 2; c.yOffset = 1; c.bgr = true;
  St7735sPanel p(bus, c);
  p.setRotation(0);
  CHECK(bus.after(0x36) == std::vector<uint8_t>{0x40 | 0x80 | 0x08});
  bus.log.clear();
  p.fillRect(3, 4, 2, 1, 0xF800);
  CHECK(bus.after(0x2A) == (std::vector<uint8_t>{0, 5, 0, 6}));   // x + xOffset
  CHECK(bus.after(0x2B) == (std::vector<uint8_t>{0, 5, 0, 5}));   // y + yOffset
  CHECK(bus.after(0x2C) == (std::vector<uint8_t>{0xF8, 0x00, 0xF8, 0x00}));
  bus.log.clear();
  const uint16_t px[3] = {0x1234, 0xABCD, 0x0001};
  p.blit(0, 0, 3, 1, px);
  CHECK(bus.after(0x2C) == (std::vector<uint8_t>{0x12, 0x34, 0xAB, 0xCD, 0x00, 0x01}));
  // a large fill is sent in chunks but completely
  bus.log.clear();
  p.fillRect(0, 0, 128, 160, 0x0000);
  CHECK(bus.after(0x2C).size() == 128u * 160u * 2u);
}

static void testGfx() {
  FramebufferPanel fb(160, 128);
  Gfx g(fb);
  g.fillScreen(color::BLUE);
  CHECK(fb.pixel(0, 0) == color::BLUE && fb.pixel(159, 127) == color::BLUE);
  int16_t x = g.drawText(0, 0, "Hi", color::WHITE, color::BLACK);
  CHECK(x == 2 * CHAR_W);
  bool lit = false;
  for (int yy = 0; yy < CHAR_H; ++yy) for (int xx = 0; xx < CHAR_W; ++xx) lit |= fb.pixel(xx, yy) == color::WHITE;
  CHECK(lit);
  g.drawField(0, 20, "toolongtext", 4, color::WHITE, color::RED);
  CHECK(fb.pixel(4 * CHAR_W, 20) == color::BLUE);      // nothing drawn past the field
  g.fillRect(-10, -10, 20, 20, color::GREEN);           // clipped, must not crash
  CHECK(fb.pixel(9, 9) == color::GREEN);
  CHECK(Gfx::textWidth("abc", 2) == 3 * CHAR_W * 2);

  MonoPanel<128, 64> mono;
  Gfx m(mono);
  m.fillRect(0, 0, 8, 8, color::WHITE);
  CHECK(mono.pixel(7, 7) && !mono.pixel(8, 8));
}

int main() {
  testInit();
  testWindowAndPixels();
  testGfx();
  if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
  std::printf("StampDisplay: all tests passed\n");
  return 0;
}
