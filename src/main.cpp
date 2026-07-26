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

#pragma message("Nexstar AUX Bridge firmware " NEXSTAR_FIRMWARE_VERSION " profile " \
                NEXSTAR_FIRMWARE_PROFILE_NAME)

static_assert(nexstar::IsValidProfile(NEXSTAR_FIRMWARE_PROFILE), "Invalid firmware profile");

namespace {

// This bootstrap initializes only the board-owned OLED pins. AUX GPIOs remain
// in their reset state, and the CP2102-facing UART stays binary-clean. NEX-6
// will define safe AUX pin states.
constexpr TickType_t kIdleDelay = pdMS_TO_TICKS(250);

nexstar::DisplayService display;
nexstar::DisplaySnapshot snapshot;
nexstar::Diagnostics diagnostics;

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

#if NEXSTAR_DISPLAY_DIAGNOSTICS
  Serial.begin(115200);
  delay(200);
#endif

  const bool display_ready = display.begin(snapshot);

#if NEXSTAR_DISPLAY_DIAGNOSTICS
  Serial.println(display_ready ? "DISPLAY:OK" : "DISPLAY:NOT_FOUND");
#else
  (void)display_ready;
#endif
}

void loop() {
  display.publish(snapshot);
  vTaskDelay(kIdleDelay);
}
