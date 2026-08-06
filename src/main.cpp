#include <Arduino.h>

#include "board_io.hpp"
#include "aux_protocol.hpp"
#include "aux_transmitter.hpp"
#include "diagnostics.hpp"
#include "echo_tracker.hpp"
#include "display_model.hpp"
#include "display_service.hpp"
#include "firmware_profile.hpp"
#include "packet_router.hpp"

#ifndef NEXSTAR_FIRMWARE_VERSION
#define NEXSTAR_FIRMWARE_VERSION "unknown"
#endif

#ifndef NEXSTAR_FIRMWARE_PROFILE
#error "NEXSTAR_FIRMWARE_PROFILE must be defined by the PlatformIO environment"
#endif

#ifndef NEXSTAR_FIRMWARE_PROFILE_NAME
#define NEXSTAR_FIRMWARE_PROFILE_NAME "unknown"
#endif

#ifndef NEXSTAR_DISPLAY_DIAGNOSTICS
#define NEXSTAR_DISPLAY_DIAGNOSTICS 0
#endif

#ifndef NEXSTAR_AUX_CAPTURE_ENABLED
#define NEXSTAR_AUX_CAPTURE_ENABLED 0
#endif

#pragma message("Nexstar AUX Bridge firmware " NEXSTAR_FIRMWARE_VERSION " profile " \
                NEXSTAR_FIRMWARE_PROFILE_NAME)

static_assert(nexstar::IsValidProfile(NEXSTAR_FIRMWARE_PROFILE), "Invalid firmware profile");

namespace {

// Force the external AUX controls safe before any optional service starts.
// UART0 remains reserved for the CP2102-facing binary host transport.
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(1);
constexpr std::uint32_t kDisplayPublishIntervalMs = 250;

nexstar::DisplayService display;
nexstar::DisplaySnapshot snapshot;
nexstar::Diagnostics diagnostics;
std::uint32_t last_display_publish_ms = 0;

#if NEXSTAR_AUX_CAPTURE_ENABLED
static_assert(NEXSTAR_FIRMWARE_PROFILE == 2,
              "AUX capture is restricted to the listen-only profile");

constexpr std::uint32_t kAuxBaud = 19200;
constexpr std::uint32_t kCaptureStatusIntervalMs = 1000;
HardwareSerial aux_uart(2);
nexstar::AuxStreamDecoder aux_decoder;
std::uint32_t aux_bytes = 0;
std::uint32_t aux_frames = 0;
std::uint32_t aux_rejected = 0;
std::uint32_t last_capture_status_ms = 0;

void PrintAuxPacket(const nexstar::AuxPacket& packet) {
  Serial.printf("AUX %lu", static_cast<unsigned long>(packet.timestamp_ms));
  for (std::uint16_t index = 0; index < packet.size; ++index) {
    Serial.printf(" %02X", packet.bytes[index]);
  }
  Serial.println();
}

void PollAuxCapture() {
  // Listen-only is enforced both electrically and in software. UART2 is
  // created without a TX pin, and the two external active-low enables are
  // continuously returned to their released levels.
  nexstar::ForceAuxOutputsSafe();

  while (aux_uart.available() > 0) {
    const int value = aux_uart.read();
    if (value < 0) {
      break;
    }

    ++aux_bytes;
    nexstar::AuxPacket packet{};
    const auto result = aux_decoder.feed(static_cast<std::uint8_t>(value), millis(),
                                         nexstar::PacketOrigin::kAuxBus, packet);
    if (result == nexstar::DecodeResult::kPacket) {
      ++aux_frames;
      snapshot.rx_packets = aux_frames;
      snapshot.rx_active = true;
      PrintAuxPacket(packet);
    } else if (result == nexstar::DecodeResult::kRejected) {
      ++aux_rejected;
    }
  }

  const std::uint32_t now = millis();
  if (now - last_capture_status_ms >= kCaptureStatusIntervalMs) {
    last_capture_status_ms = now;
    const bool cts_asserted =
        digitalRead(nexstar::BoardPins::kAuxCtsIn) == LOW;
    Serial.printf("STAT %lu bytes=%lu frames=%lu rejected=%lu cts=%u oe_tx=%u oe_busy=%u\n",
                  static_cast<unsigned long>(now),
                  static_cast<unsigned long>(aux_bytes),
                  static_cast<unsigned long>(aux_frames),
                  static_cast<unsigned long>(aux_rejected), cts_asserted ? 1U : 0U,
                  digitalRead(nexstar::BoardPins::kAuxTxEnable),
                  digitalRead(nexstar::BoardPins::kAuxBusyAssert));
  }
}
#endif

}  // namespace

void setup() {
  nexstar::InitializeAuxOutputsSafe();
  snapshot.profile = static_cast<nexstar::FirmwareProfile>(NEXSTAR_FIRMWARE_PROFILE);
  snapshot.state = nexstar::ProjectState::kSafeBaseline;
  snapshot.host_ready = true;
  diagnostics.record({millis(), nexstar::DiagnosticEventCode::kBoot, 0});
  diagnostics.record({millis(), nexstar::DiagnosticEventCode::kSafeOutputsForced, 0});
  diagnostics.record({millis(), nexstar::DiagnosticEventCode::kModeSelected,
                      static_cast<std::uint16_t>(NEXSTAR_FIRMWARE_PROFILE)});

#if NEXSTAR_DISPLAY_DIAGNOSTICS || NEXSTAR_AUX_CAPTURE_ENABLED
  Serial.begin(115200);
  delay(200);
#endif

#if NEXSTAR_AUX_CAPTURE_ENABLED
  aux_uart.setRxBufferSize(2048);
  aux_uart.begin(kAuxBaud, SERIAL_8N1, nexstar::BoardPins::kAuxUartRx, -1);
  Serial.printf("CAPTURE:READY profile=%s aux_baud=%lu rx=%u tx=disconnected\n",
                NEXSTAR_FIRMWARE_PROFILE_NAME,
                static_cast<unsigned long>(kAuxBaud),
                nexstar::BoardPins::kAuxUartRx);
#endif

  const bool display_ready = display.begin(snapshot);

#if NEXSTAR_DISPLAY_DIAGNOSTICS
  Serial.println(display_ready ? "DISPLAY:OK" : "DISPLAY:NOT_FOUND");
#else
  (void)display_ready;
#endif
}

void loop() {
#if NEXSTAR_AUX_CAPTURE_ENABLED
  PollAuxCapture();
#endif

  const std::uint32_t now = millis();
  if (now - last_display_publish_ms >= kDisplayPublishIntervalMs) {
    last_display_publish_ms = now;
    display.publish(snapshot);
  }
  vTaskDelay(kIdleDelay);
}
