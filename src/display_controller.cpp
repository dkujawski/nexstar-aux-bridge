#include "display_controller.hpp"

#if NEXSTAR_DISPLAY_ENABLED
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <SPI.h>

#include "board_support.hpp"
#endif

namespace nexstar {

DisplayController::DisplayController() = default;

#if NEXSTAR_DISPLAY_ENABLED
namespace {

constexpr std::uint32_t kTftSpiHz = 4000000;
constexpr std::uint32_t kMinimumRenderIntervalMs = 500;

Adafruit_ST7735 tft(BoardPins::kTftChipSelect, BoardPins::kTftDataCommand,
                    BoardPins::kTftReset);

}  // namespace

bool DisplayController::begin(const DisplaySnapshot& snapshot,
                              const std::uint32_t now_ms) {
  // Keep the panel dark and deselected while its control pins and SPI bus are
  // established. This diagnostic profile is the only profile that reaches
  // this code until NEX-21 bench validation is complete.
  pinMode(BoardPins::kTftBacklight, OUTPUT);
  digitalWrite(BoardPins::kTftBacklight, LOW);
  pinMode(BoardPins::kTftChipSelect, OUTPUT);
  digitalWrite(BoardPins::kTftChipSelect, HIGH);
  pinMode(BoardPins::kTftDataCommand, OUTPUT);
  digitalWrite(BoardPins::kTftDataCommand, LOW);
  pinMode(BoardPins::kTftReset, OUTPUT);
  digitalWrite(BoardPins::kTftReset, HIGH);

  SPI.begin(BoardPins::kTftClock, -1, BoardPins::kTftMosi,
            BoardPins::kTftChipSelect);
  tft.initR(INITR_MINI160x80);
  tft.setSPISpeed(kTftSpiHz);
  tft.setRotation(1);
  tft.setTextWrap(false);

  available_ = true;
  last_snapshot_ = snapshot;
  last_render_ms_ = now_ms;
  render(snapshot);
  digitalWrite(BoardPins::kTftBacklight, HIGH);
  return true;
}

void DisplayController::render(const DisplaySnapshot& snapshot) {
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 160, 12, ST77XX_RED);
  tft.fillRect(0, 12, 160, 12, ST77XX_GREEN);
  tft.fillRect(0, 24, 160, 12, ST77XX_BLUE);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 42);
  tft.print("NEXSTAR TFT TEST");
  tft.setCursor(4, 54);
  tft.print(ProfileLabel(snapshot.profile));
  renderStatusLine(snapshot);
}

void DisplayController::renderStatusLine(const DisplaySnapshot& snapshot) {
  // Updating only this row avoids a visible full-screen flash whenever AUX
  // traffic changes the packet counter.
  tft.fillRect(0, 64, 160, 16, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 66);
  tft.printf("RX:%lu SAFE", static_cast<unsigned long>(snapshot.rx_packets));
}

void DisplayController::update(const DisplaySnapshot& snapshot,
                               const std::uint32_t now_ms) {
  if (!available_ || SnapshotsEqual(snapshot, last_snapshot_) ||
      now_ms - last_render_ms_ < kMinimumRenderIntervalMs) {
    return;
  }
  const bool static_content_changed =
      snapshot.profile != last_snapshot_.profile ||
      snapshot.state != last_snapshot_.state;
  last_snapshot_ = snapshot;
  last_render_ms_ = now_ms;
  if (static_content_changed) {
    render(snapshot);
  } else {
    renderStatusLine(snapshot);
  }
}

#else

bool DisplayController::begin(const DisplaySnapshot&, std::uint32_t) {
  available_ = false;
  return false;
}

void DisplayController::update(const DisplaySnapshot&, std::uint32_t) {}

void DisplayController::render(const DisplaySnapshot&) {}

void DisplayController::renderStatusLine(const DisplaySnapshot&) {}

#endif

}  // namespace nexstar
