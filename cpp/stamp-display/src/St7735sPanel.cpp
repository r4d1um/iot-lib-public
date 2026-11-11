#include "St7735sPanel.h"

namespace stampdisplay {

namespace {

// ST7735S commands
constexpr uint8_t SWRESET = 0x01;
constexpr uint8_t SLPIN   = 0x10;
constexpr uint8_t SLPOUT  = 0x11;
constexpr uint8_t NORON   = 0x13;
constexpr uint8_t INVOFF  = 0x20;
constexpr uint8_t INVON   = 0x21;
constexpr uint8_t DISPON  = 0x29;
constexpr uint8_t CASET   = 0x2A;
constexpr uint8_t RASET   = 0x2B;
constexpr uint8_t RAMWR   = 0x2C;
constexpr uint8_t MADCTL  = 0x36;
constexpr uint8_t COLMOD  = 0x3A;

constexpr uint8_t MADCTL_MY  = 0x80;
constexpr uint8_t MADCTL_MX  = 0x40;
constexpr uint8_t MADCTL_MV  = 0x20;
constexpr uint8_t MADCTL_BGR = 0x08;

// Init sequence: command, argument count (| DELAY if a delay byte follows),
// arguments, optional delay in ms (255 = 500 ms). Terminated by 0x00, 0x00.
constexpr uint8_t DELAY = 0x80;
const uint8_t INIT_SEQUENCE[] = {
  SWRESET, DELAY, 150,
  SLPOUT,  DELAY, 255,
  0xB1, 3, 0x01, 0x2C, 0x2D,                    // FRMCTR1 frame rate, normal mode
  0xB2, 3, 0x01, 0x2C, 0x2D,                    // FRMCTR2 idle mode
  0xB3, 6, 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D,  // FRMCTR3 partial mode
  0xB4, 1, 0x07,                                // INVCTR no line inversion
  0xC0, 3, 0xA2, 0x02, 0x84,                    // PWCTR1 -4.6 V, auto mode
  0xC1, 1, 0xC5,                                // PWCTR2 VGH25 2.4 V, VGSEL -10 V
  0xC2, 2, 0x0A, 0x00,                          // PWCTR3 opamp current small
  0xC3, 2, 0x8A, 0x2A,                          // PWCTR4 boost frequency
  0xC4, 2, 0x8A, 0xEE,                          // PWCTR5
  0xC5, 1, 0x0E,                                // VMCTR1 VCOM
  COLMOD, 1, 0x05,                              // 16 bit per pixel (RGB565)
  0xE0, 16, 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,  // gamma +
            0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
  0xE1, 16, 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,  // gamma -
            0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
  NORON,  DELAY, 10,
  DISPON, DELAY, 100,
  0x00, 0x00,
};

constexpr size_t CHUNK_PIXELS = 64;

}  // namespace

void St7735sPanel::initPanel() {
  if (bus_.hasReset()) {
    bus_.reset(true);
    bus_.delayMs(10);
    bus_.reset(false);
    bus_.delayMs(10);
    bus_.reset(true);
    bus_.delayMs(120);
  }
  runInitSequence(INIT_SEQUENCE);
  command(config_.invert ? INVON : INVOFF);
  setRotation(config_.rotation);
}

void St7735sPanel::setRotation(uint8_t rotation) {
  config_.rotation = rotation & 3;
  uint8_t madctl = 0;
  switch (config_.rotation) {
    case 0: madctl = MADCTL_MX | MADCTL_MY; break;
    case 1: madctl = MADCTL_MY | MADCTL_MV; break;
    case 2: madctl = 0; break;
    case 3: madctl = MADCTL_MX | MADCTL_MV; break;
  }
  if (config_.bgr) madctl |= MADCTL_BGR;

  const bool landscape = config_.rotation & 1;
  width_ = landscape ? NATIVE_HEIGHT : NATIVE_WIDTH;
  height_ = landscape ? NATIVE_WIDTH : NATIVE_HEIGHT;
  colStart_ = landscape ? config_.yOffset : config_.xOffset;
  rowStart_ = landscape ? config_.xOffset : config_.yOffset;

  alignas(4) uint8_t arg[4] = {madctl};
  commandWithData(MADCTL, arg, 1);
}

void St7735sPanel::sleep(bool enable) {
  command(enable ? SLPIN : SLPOUT);
  bus_.delayMs(120);
}

void St7735sPanel::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  alignas(4) uint8_t chunk[CHUNK_PIXELS * 2];  // aligned: the ESP8266 sends it straight from here
  for (size_t i = 0; i < CHUNK_PIXELS; ++i) {
    chunk[i * 2] = static_cast<uint8_t>(color >> 8);
    chunk[i * 2 + 1] = static_cast<uint8_t>(color & 0xFF);
  }

  bus_.beginWrite();
  setWindow(x, y, w, h);
  uint32_t remaining = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
  while (remaining > 0) {
    const size_t n = remaining > CHUNK_PIXELS ? CHUNK_PIXELS : remaining;
    bus_.write(chunk, n * 2);
    remaining -= n;
  }
  bus_.endWrite();
}

void St7735sPanel::blit(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) {
  alignas(4) uint8_t chunk[CHUNK_PIXELS * 2];

  bus_.beginWrite();
  setWindow(x, y, w, h);
  const uint32_t total = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
  uint32_t done = 0;
  while (done < total) {
    const size_t n = total - done > CHUNK_PIXELS ? CHUNK_PIXELS : total - done;
    for (size_t i = 0; i < n; ++i) {
      chunk[i * 2] = static_cast<uint8_t>(pixels[done + i] >> 8);  // panel expects big-endian RGB565
      chunk[i * 2 + 1] = static_cast<uint8_t>(pixels[done + i] & 0xFF);
    }
    bus_.write(chunk, n * 2);
    done += n;
  }
  bus_.endWrite();
}

void St7735sPanel::command(uint8_t cmd) {
  alignas(4) uint8_t b[4] = {cmd};
  bus_.beginWrite();
  bus_.dc(false);
  bus_.write(b, 1);
  bus_.dc(true);
  bus_.endWrite();
}

void St7735sPanel::commandWithData(uint8_t cmd, const uint8_t* data, uint8_t len) {
  alignas(4) uint8_t b[4] = {cmd};
  bus_.beginWrite();
  bus_.dc(false);
  bus_.write(b, 1);
  bus_.dc(true);
  bus_.write(data, len);
  bus_.endWrite();
}

void St7735sPanel::runInitSequence(const uint8_t* seq) {
  while (true) {
    const uint8_t cmd = *seq++;
    const uint8_t argInfo = *seq++;
    if (cmd == 0x00 && argInfo == 0x00) break;

    const uint8_t argCount = argInfo & static_cast<uint8_t>(~DELAY);
    commandWithData(cmd, seq, argCount);
    seq += argCount;

    if (argInfo & DELAY) {
      const uint16_t ms = *seq++;
      bus_.delayMs(ms == 255 ? 500 : ms);
    }
  }
}

// Called between beginWrite()/endWrite(): sets the RAM window and starts RAMWR.
void St7735sPanel::setWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
  const uint16_t x0 = static_cast<uint16_t>(x + colStart_), x1 = static_cast<uint16_t>(x + w - 1 + colStart_);
  const uint16_t y0 = static_cast<uint16_t>(y + rowStart_), y1 = static_cast<uint16_t>(y + h - 1 + rowStart_);
  alignas(4) uint8_t cols[4] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
  alignas(4) uint8_t rows[4] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
  alignas(4) uint8_t c[4] = {CASET};

  bus_.dc(false); bus_.write(c, 1); bus_.dc(true); bus_.write(cols, 4);
  c[0] = RASET;
  bus_.dc(false); bus_.write(c, 1); bus_.dc(true); bus_.write(rows, 4);
  c[0] = RAMWR;
  bus_.dc(false); bus_.write(c, 1); bus_.dc(true);
}

}  // namespace stampdisplay
