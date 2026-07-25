#include "display_controller.hpp"

#if NEXSTAR_DISPLAY_ENABLED

#include <Arduino.h>
#include <Wire.h>

#include <cstdio>

namespace {

constexpr std::uint8_t kDisplayAddress = 0x3C;
constexpr std::int16_t kDisplayWidth = 128;
constexpr std::int16_t kDisplayHeight = 64;
constexpr std::uint32_t kBootDurationMs = 1800;
constexpr std::uint32_t kMinimumRenderIntervalMs = 250;
constexpr std::uint32_t kPowerOffDelayMs = 20;
constexpr std::uint32_t kPowerSettleDelayMs = 100;
constexpr std::uint32_t kProbeRetryDelayMs = 25;
constexpr std::uint8_t kProbeAttempts = 20;

void DrawHeader(Adafruit_SSD1306& display, const char* text) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(text);
  display.drawFastHLine(0, 9, kDisplayWidth, SSD1306_WHITE);
}

}  // namespace

namespace nexstar {

DisplayController::DisplayController()
    : display_(kDisplayWidth, kDisplayHeight, &Wire, RST_OLED) {}

bool DisplayController::begin(const DisplaySnapshot& snapshot, const std::uint32_t now_ms) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
  delay(kPowerOffDelayMs);
  digitalWrite(Vext, LOW);

  pinMode(RST_OLED, OUTPUT);
  digitalWrite(RST_OLED, LOW);
  delay(20);
  digitalWrite(RST_OLED, HIGH);
  delay(kPowerSettleDelayMs);

  Wire.end();
  Wire.begin(SDA_OLED, SCL_OLED);
  Wire.setTimeOut(20);

  bool acknowledged = false;
  for (std::uint8_t attempt = 0; attempt < kProbeAttempts; ++attempt) {
    Wire.beginTransmission(kDisplayAddress);
    if (Wire.endTransmission() == 0) {
      acknowledged = true;
      break;
    }
    delay(kProbeRetryDelayMs);
  }
  if (!acknowledged) {
    digitalWrite(Vext, HIGH);
    return false;
  }

  if (!display_.begin(SSD1306_SWITCHCAPVCC, kDisplayAddress, false, false)) {
    digitalWrite(Vext, HIGH);
    return false;
  }

  display_.dim(false);
  display_.invertDisplay(false);
  display_.ssd1306_command(SSD1306_SETCONTRAST);
  display_.ssd1306_command(0xFF);
  available_ = true;
  boot_started_ms_ = now_ms;
  last_render_ms_ = now_ms;
  showing_boot_ = true;
  last_snapshot_ = snapshot;
  renderBoot(snapshot);
  return true;
}

void DisplayController::update(const DisplaySnapshot& snapshot, const std::uint32_t now_ms) {
  if (!available_) {
    return;
  }

  if (showing_boot_) {
    if (now_ms - boot_started_ms_ < kBootDurationMs) {
      return;
    }
    showing_boot_ = false;
  } else if (SnapshotsEqual(snapshot, last_snapshot_)) {
    return;
  }

  if (now_ms - last_render_ms_ < kMinimumRenderIntervalMs) {
    return;
  }

  if (snapshot.state == ProjectState::kFault) {
    renderFault(snapshot);
  } else {
    renderMain(snapshot);
  }
  last_snapshot_ = snapshot;
  last_render_ms_ = now_ms;
}

void DisplayController::renderBoot(const DisplaySnapshot& snapshot) {
  display_.clearDisplay();
  DrawHeader(display_, "NEXSTAR AUX BRIDGE");
  display_.setCursor(0, 14);
  display_.println("HELTEC V3 / ESP32-S3");
  display_.print("FW ");
  display_.println(NEXSTAR_FIRMWARE_VERSION);
  display_.print("MODE ");
  display_.println(ProfileLabel(snapshot.profile));
  display_.println("STARTING DISPLAY");
  display_.println("AUX OUTPUTS SAFE");
  display_.display();
}

void DisplayController::renderMain(const DisplaySnapshot& snapshot) {
  char counters[22];
  std::snprintf(counters, sizeof(counters), "RX:%lu TX:%lu",
                static_cast<unsigned long>(snapshot.rx_packets),
                static_cast<unsigned long>(snapshot.tx_packets));

  display_.clearDisplay();
  DrawHeader(display_, "NEXSTAR AUX");
  display_.setCursor(0, 13);
  display_.println(ProjectStateLabel(snapshot.state));
  display_.print("USB: ");
  display_.println(snapshot.host_ready ? "READY" : "WAITING");
  display_.print("AUX: ");
  display_.println(snapshot.aux_enabled ? "ENABLED" : "DISABLED");

  if (snapshot.state == ProjectState::kSafeBaseline) {
    display_.println("NEXT: NEX-6 PINS");
  } else {
    display_.println(counters);
    display_.print("ERR: ");
    display_.println(snapshot.error_count);
  }
  display_.display();
}

void DisplayController::renderFault(const DisplaySnapshot& snapshot) {
  display_.clearDisplay();
  DrawHeader(display_, "NEXSTAR AUX FAULT");
  display_.setTextSize(2);
  display_.setCursor(0, 17);
  display_.println("SAFE");
  display_.setTextSize(1);
  display_.println("AUX DISABLED");
  display_.print("ERRORS: ");
  display_.println(snapshot.error_count);
  display_.display();
}

}  // namespace nexstar

#else

namespace nexstar {

DisplayController::DisplayController() = default;

bool DisplayController::begin(const DisplaySnapshot&, std::uint32_t) {
  return false;
}

void DisplayController::update(const DisplaySnapshot&, std::uint32_t) {}

}  // namespace nexstar

#endif
