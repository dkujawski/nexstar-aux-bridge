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
constexpr std::uint32_t kBootDurationMs = 1800;

Adafruit_ST7735 tft(BoardPins::kTftChipSelect, BoardPins::kTftDataCommand,
                    BoardPins::kTftReset);

}  // namespace

bool DisplayController::begin(const DisplaySnapshot& snapshot,
                              const std::uint32_t now_ms) {
  // SPI TFT modules do not provide a reliable presence probe. Keep setup
  // bounded and best-effort so an unplugged optional panel cannot delay or
  // prevent the bridge from starting.
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
  last_view_ = model_.update(snapshot, now_ms);
  showing_boot_ = true;
  boot_started_ms_ = now_ms;
  last_render_ms_ = now_ms;
  renderBoot(snapshot);
  digitalWrite(BoardPins::kTftBacklight, HIGH);
  return true;
}

bool DisplayController::ViewsEqual(const DisplayViewModel& lhs,
                                   const DisplayViewModel& rhs) {
  return SnapshotsEqual(lhs.snapshot, rhs.snapshot) &&
         lhs.show_rx_activity == rhs.show_rx_activity &&
         lhs.show_tx_activity == rhs.show_tx_activity &&
         lhs.show_fault_overlay == rhs.show_fault_overlay;
}

void DisplayController::renderBoot(const DisplaySnapshot& snapshot) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 4);
  tft.print("NEXSTAR AUX BRIDGE");
  tft.setCursor(4, 20);
  tft.print("FW " NEXSTAR_FIRMWARE_VERSION);
  tft.setCursor(4, 34);
  tft.print("ESP32 DEVKIT V1");
  tft.setCursor(4, 48);
  tft.print("MODE ");
  tft.print(ProfileLabel(snapshot.profile));
  tft.setCursor(4, 66);
  tft.print("AUX OUTPUTS SAFE");
}

void DisplayController::renderMain(const DisplayViewModel& view) {
  const DisplaySnapshot& snapshot = view.snapshot;
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 4);
  tft.print(ProfileLabel(snapshot.profile));
  tft.setCursor(4, 18);
  tft.printf("AUX:%s", snapshot.aux_enabled ? "ACTIVE" : "SAFE");
  tft.setCursor(4, 30);
  tft.printf("HOST:%s%s", snapshot.host_ready ? "READY" : "WAIT",
             snapshot.host_active ? " *" : "");
  tft.setCursor(4, 44);
  tft.printf("RX%c %lu TX%c %lu", view.show_rx_activity ? '*' : ':',
             static_cast<unsigned long>(snapshot.rx_packets),
             view.show_tx_activity ? '*' : ':',
             static_cast<unsigned long>(snapshot.tx_packets));
  tft.setCursor(4, 58);
  tft.printf("ERR:%lu BUSY:%lu", static_cast<unsigned long>(snapshot.error_count),
             static_cast<unsigned long>(snapshot.busy_timeout_count));
  tft.setCursor(4, 72);
  tft.print(snapshot.profile == FirmwareProfile::kListenOnly
                ? "LISTEN-ONLY: TX LOCKED"
                : "STATUS SNAPSHOT");
}

void DisplayController::renderFault(const DisplayViewModel& view) {
  const DisplaySnapshot& snapshot = view.snapshot;
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 160, 14, ST77XX_RED);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 3);
  tft.print("AUX FAULT - SAFE");
  tft.setCursor(4, 24);
  tft.printf("CODE:%u ERR:%lu", snapshot.fault_code,
             static_cast<unsigned long>(snapshot.error_count));
  tft.setCursor(4, 40);
  tft.printf("BUSY TIMEOUTS:%lu",
             static_cast<unsigned long>(snapshot.busy_timeout_count));
  tft.setCursor(4, 56);
  tft.print("TX DISABLED");
}

void DisplayController::update(const DisplaySnapshot& snapshot,
                               const std::uint32_t now_ms) {
  if (!available_) {
    return;
  }
  const DisplayViewModel view = model_.update(snapshot, now_ms);
  if (showing_boot_) {
    if (now_ms - boot_started_ms_ < kBootDurationMs) {
      return;
    }
    showing_boot_ = false;
  } else if (ViewsEqual(view, last_view_)) {
    return;
  }
  if (now_ms - last_render_ms_ < kMinimumRenderIntervalMs) {
    return;
  }
  last_view_ = view;
  last_render_ms_ = now_ms;
  if (view.show_fault_overlay) {
    renderFault(view);
  } else {
    renderMain(view);
  }
}

#else

bool DisplayController::begin(const DisplaySnapshot&, std::uint32_t) {
  available_ = false;
  return false;
}

void DisplayController::update(const DisplaySnapshot&, std::uint32_t) {}

void DisplayController::renderBoot(const DisplaySnapshot&) {}

void DisplayController::renderMain(const DisplayViewModel&) {}

void DisplayController::renderFault(const DisplayViewModel&) {}

bool DisplayController::ViewsEqual(const DisplayViewModel&,
                                   const DisplayViewModel&) {
  return true;
}

#endif

}  // namespace nexstar
