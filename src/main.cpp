#include <Arduino.h>
#include <driver/uart.h>

#include "board_io.hpp"
#include "aux_protocol.hpp"
#include "aux_transmitter.hpp"
#include "controlled_aux_query.hpp"
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

#ifndef NEXSTAR_AUX_CONTROLLED_TEST_ENABLED
#define NEXSTAR_AUX_CONTROLLED_TEST_ENABLED 0
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

#if NEXSTAR_AUX_CONTROLLED_TEST_ENABLED
static_assert(NEXSTAR_FIRMWARE_PROFILE == 4,
              "Controlled AUX test requires the NEX-16 profile");

constexpr std::uint32_t kAuxBaud = 19200;
constexpr std::uint8_t kControlledQuerySource = nexstar::kControlledTestSource;
constexpr std::size_t kControlLineCapacity = 24;

class ArduinoAuxTxIo final : public nexstar::AuxTxIo {
 public:
  explicit ArduinoAuxTxIo(HardwareSerial& uart) : uart_(uart) {}

  bool busBusy() const override {
    const bool clear_to_send =
        digitalRead(nexstar::BoardPins::kAuxCtsIn) ==
        (nexstar::BoardPolarity::kAuxCtsActiveLow ? LOW : HIGH);
    return !clear_to_send;
  }

  void setBusyAsserted(const bool asserted) override {
    digitalWrite(nexstar::BoardPins::kAuxBusyAssert,
                 asserted == nexstar::BoardPolarity::kAuxBusyAssertActiveHigh
                     ? HIGH
                     : LOW);
  }

  void setTxEnabled(const bool enabled) override {
    digitalWrite(nexstar::BoardPins::kAuxTxEnable,
                 enabled == nexstar::BoardPolarity::kAuxTxEnableActiveHigh
                     ? HIGH
                     : LOW);
  }

  std::size_t write(const std::uint8_t* bytes, const std::size_t size) override {
    return uart_.write(bytes, size);
  }

  bool txComplete() const override {
    return uart_wait_tx_done(UART_NUM_2, 0) == ESP_OK;
  }

 private:
  HardwareSerial& uart_;
};

HardwareSerial aux_uart(2);
ArduinoAuxTxIo aux_tx_io(aux_uart);
nexstar::AuxTransmitter aux_transmitter(aux_tx_io);
nexstar::EchoTracker echo_tracker;
nexstar::AuxStreamDecoder controlled_aux_decoder;
nexstar::AuxPacket active_query;
nexstar::AuxPacket pending_response;
nexstar::AuxTxState last_tx_state = nexstar::AuxTxState::kIdle;
char control_line[kControlLineCapacity]{};
std::size_t control_line_size = 0;
bool query_active = false;
bool echo_tracking = false;
bool destination_selected = false;
bool pending_response_valid = false;
std::uint8_t selected_destination = 0;
std::uint32_t echo_matches_at_start = 0;

void PrintControlledTestStatus() {
  const auto& metrics = aux_transmitter.metrics();
  Serial.printf(
      "NEX16: STATUS state=%u active=%u destination=%s%02X starts=%lu rejected_unauthorized=%lu "
      "rejected_policy=%lu rejected_checksum=%lu completed=%lu faults=%lu "
      "recoveries=%lu max_busy_us=%lu\n",
      static_cast<unsigned>(aux_transmitter.state()), query_active ? 1U : 0U,
      destination_selected ? "" : "--", selected_destination,
      static_cast<unsigned long>(metrics.start_requests),
      static_cast<unsigned long>(metrics.rejected_unauthorized),
      static_cast<unsigned long>(metrics.rejected_policy),
      static_cast<unsigned long>(metrics.rejected_checksum),
      static_cast<unsigned long>(metrics.completed_packets),
      static_cast<unsigned long>(metrics.faults),
      static_cast<unsigned long>(metrics.recoveries),
      static_cast<unsigned long>(metrics.maximum_busy_hold_us));
}

void PrintControlledTestLimits() {
  const auto& timing = aux_transmitter.timing();
  Serial.printf(
      "NEX16: LIMITS busy_deadline_us=%lu claim_delay_us=%lu "
      "uart_drain_timeout_us=%lu echo_timeout_us=%lu backoff_us=%lu "
      "response_timeout_us=%lu maximum_attempts=%u\n",
      static_cast<unsigned long>(timing.transaction_timeout_us),
      static_cast<unsigned long>(timing.claim_delay_us),
      static_cast<unsigned long>(timing.uart_drain_timeout_us),
      static_cast<unsigned long>(timing.echo_timeout_us),
      static_cast<unsigned long>(timing.backoff_us),
      static_cast<unsigned long>(timing.response_timeout_us),
      static_cast<unsigned>(timing.maximum_attempts));
}

void SelectDestination(const std::uint8_t destination) {
  if (query_active || aux_transmitter.state() != nexstar::AuxTxState::kIdle) {
    Serial.println("NEX16: REJECT destination selection requires idle transmitter");
    return;
  }
  selected_destination = destination;
  destination_selected = true;
  Serial.printf("NEX16: SELECTED destination=%02X transmission=unauthorized\n",
                selected_destination);
}

void ArmSingleVersionQuery() {
  if (query_active || aux_transmitter.state() != nexstar::AuxTxState::kIdle) {
    Serial.println("NEX16: REJECT arm requires idle, disarmed transmitter");
    return;
  }
  if (!destination_selected) {
    Serial.println("NEX16: REJECT select destination before ARM");
    return;
  }

  active_query = nexstar::BuildControlledVersionQuery(kControlledQuerySource,
                                                       selected_destination);
  // Authorization exists only for this one start call. Boot, fault recovery,
  // and idle operation all remain prohibited.
  const bool accepted = aux_transmitter.start(
      active_query, nexstar::OperatingMode::kControlledTest,
      nexstar::AuxTxAuthorization::kControlledTest, micros());
  if (!accepted) {
    Serial.println("NEX16: REJECT query policy/authorization failure");
    return;
  }

  query_active = true;
  echo_tracking = false;
  pending_response_valid = false;
  Serial.printf("NEX16: ARMED one GET_VERSION source=%02X destination=%02X\n",
                kControlledQuerySource, selected_destination);
}

void HandleControlLine() {
  control_line[control_line_size] = '\0';
  if (strcmp(control_line, "STATUS") == 0) {
    PrintControlledTestStatus();
  } else if (strcmp(control_line, "RECOVER") == 0) {
    aux_transmitter.recover(micros());
    query_active = false;
    echo_tracking = false;
    pending_response_valid = false;
    destination_selected = false;
    Serial.println("NEX16: RECOVERED transmission=unauthorized");
  } else if (strcmp(control_line, "ARM") == 0) {
    ArmSingleVersionQuery();
  } else if (strncmp(control_line, "SELECT ", 7) == 0) {
    char* end = nullptr;
    const unsigned long destination = strtoul(control_line + 7, &end, 16);
    if (end == control_line + 7 || *end != '\0' || destination > 0xFFU) {
      Serial.println("NEX16: REJECT expected SELECT <destination-hex>");
    } else {
      SelectDestination(static_cast<std::uint8_t>(destination));
    }
  } else if (control_line_size != 0) {
    Serial.println("NEX16: REJECT only SELECT <destination-hex>, ARM, STATUS, RECOVER");
  }
  control_line_size = 0;
}

void PollControlledTestCommands() {
  while (Serial.available() > 0) {
    const char byte = static_cast<char>(Serial.read());
    if (byte == '\r') {
      continue;
    }
    if (byte == '\n') {
      HandleControlLine();
    } else if (control_line_size + 1 < kControlLineCapacity) {
      control_line[control_line_size++] = byte;
    } else {
      control_line_size = 0;
      Serial.println("NEX16: REJECT command too long");
    }
  }
}

void PollControlledAux() {
  while (aux_uart.available() > 0) {
    const int value = aux_uart.read();
    if (value < 0) {
      break;
    }
    const std::uint8_t byte = static_cast<std::uint8_t>(value);
    nexstar::EchoForward forward{};
    if (echo_tracking) {
      forward = echo_tracker.feed(byte, millis());
      if (!echo_tracker.active()) {
        echo_tracking = false;
        if (echo_tracker.matches() > echo_matches_at_start) {
          aux_transmitter.notifyEchoComplete();
        }
      }
    } else {
      forward.bytes[0] = byte;
      forward.size = 1;
    }

    for (std::uint16_t index = 0; index < forward.size; ++index) {
      nexstar::AuxPacket packet{};
      if (controlled_aux_decoder.feed(forward.bytes[index], millis(),
                                      nexstar::PacketOrigin::kAuxBus,
                                      packet) == nexstar::DecodeResult::kPacket) {
        // A response can finish within microseconds of BUSY release. Preserve
        // it until the transmitter has crossed into kResponseWait instead of
        // discarding it when notifyResponse observes the preceding state.
        if (nexstar::IsControlledVersionResponse(active_query, packet)) {
          pending_response = packet;
          pending_response_valid = true;
        }
      }
    }
  }
}

void ServiceControlledTest() {
  PollControlledTestCommands();
  PollControlledAux();
  aux_transmitter.tick(micros());

  if (pending_response_valid &&
      aux_transmitter.notifyResponse(pending_response)) {
    pending_response_valid = false;
  }

  // Begin matching before the UART write. The physical echo can arrive while
  // the transmitter is still waiting for the UART drain; starting in
  // kEchoWait would consume and discard those bytes before matching begins.
  if (query_active && !echo_tracking &&
      aux_transmitter.state() == nexstar::AuxTxState::kWrite) {
    echo_matches_at_start = echo_tracker.matches();
    echo_tracking = echo_tracker.begin(active_query, millis());
    if (!echo_tracking) {
      Serial.println("NEX16: FAULT unable to start echo tracker");
    }
  }

  const auto state = aux_transmitter.state();
  if (state != last_tx_state) {
    if (state == nexstar::AuxTxState::kIdle && query_active) {
      query_active = false;
      pending_response_valid = false;
      Serial.println("NEX16: COMPLETE transmission=unauthorized");
      PrintControlledTestStatus();
    } else if (state == nexstar::AuxTxState::kFault) {
      query_active = false;
      echo_tracking = false;
      pending_response_valid = false;
      Serial.printf("NEX16: FAULT code=%u transmission=unauthorized\n",
                    static_cast<unsigned>(aux_transmitter.lastFault()));
      PrintControlledTestStatus();
    }
    last_tx_state = state;
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

#if NEXSTAR_AUX_CONTROLLED_TEST_ENABLED
  Serial.begin(115200);
  delay(200);
  aux_uart.setRxBufferSize(2048);
  aux_uart.begin(kAuxBaud, SERIAL_8N1, nexstar::BoardPins::kAuxUartRx,
                 nexstar::BoardPins::kAuxUartTx);
  Serial.println("NEX16: CONTROLLED-TEST READY transmission=unauthorized");
  Serial.println("NEX16: commands: SELECT <destination-hex>, ARM, STATUS, RECOVER");
  PrintControlledTestLimits();
  PrintControlledTestStatus();
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

#if NEXSTAR_AUX_CONTROLLED_TEST_ENABLED
  ServiceControlledTest();
#endif

  const std::uint32_t now = millis();
  if (now - last_display_publish_ms >= kDisplayPublishIntervalMs) {
    last_display_publish_ms = now;
    display.publish(snapshot);
  }
#if NEXSTAR_AUX_CONTROLLED_TEST_ENABLED
  // The transmitter's safety timing is in microseconds. A one-tick sleep here
  // adds about 1 ms to every state transition, stretching the 100 us claim
  // delay and holding BUSY after the echo. Yield without a timed delay so the
  // controlled state machine can honor its configured bounds.
  taskYIELD();
#else
  vTaskDelay(kIdleDelay);
#endif
}
