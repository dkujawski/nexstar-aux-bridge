#include <unity.h>

#include "board_support.hpp"
#include "diagnostics.hpp"
#include "display_model.hpp"
#include "firmware_profile.hpp"

void setUp() {}

void tearDown() {}

void test_known_profiles_are_valid() {
  TEST_ASSERT_TRUE(nexstar::IsValidProfile(
      static_cast<std::uint8_t>(nexstar::FirmwareProfile::kBridge)));
  TEST_ASSERT_TRUE(nexstar::IsValidProfile(
      static_cast<std::uint8_t>(nexstar::FirmwareProfile::kListenOnly)));
  TEST_ASSERT_FALSE(nexstar::IsValidProfile(0));
  TEST_ASSERT_TRUE(nexstar::IsValidProfile(
      static_cast<std::uint8_t>(nexstar::FirmwareProfile::kDiagnostic)));
  TEST_ASSERT_TRUE(nexstar::IsValidProfile(
      static_cast<std::uint8_t>(nexstar::FirmwareProfile::kControlledTest)));
  TEST_ASSERT_FALSE(nexstar::IsValidProfile(5));
}

void test_only_authorized_active_profiles_may_transmit_aux() {
  TEST_ASSERT_TRUE(nexstar::MayTransmitAux(nexstar::FirmwareProfile::kBridge));
  TEST_ASSERT_FALSE(nexstar::MayTransmitAux(nexstar::FirmwareProfile::kListenOnly));
  TEST_ASSERT_FALSE(nexstar::MayTransmitAux(nexstar::FirmwareProfile::kDiagnostic));
  TEST_ASSERT_TRUE(
      nexstar::MayTransmitAux(nexstar::FirmwareProfile::kControlledTest));
}

void test_diagnostics_are_bounded_and_counters_saturate() {
  nexstar::Diagnostics diagnostics;
  diagnostics.increment(nexstar::DiagnosticCounter::kParserFailures);
  TEST_ASSERT_EQUAL_UINT32(
      1, diagnostics.count(nexstar::DiagnosticCounter::kParserFailures));

  for (std::size_t index = 0;
       index < nexstar::Diagnostics::kEventCapacity + 3; ++index) {
    diagnostics.record(
        {static_cast<std::uint32_t>(index),
         nexstar::DiagnosticEventCode::kParserFailure,
         static_cast<std::uint16_t>(index)});
  }
  TEST_ASSERT_EQUAL_UINT32(nexstar::Diagnostics::kEventCapacity,
                           diagnostics.eventCount());
  TEST_ASSERT_EQUAL_UINT32(3, diagnostics.event(0).timestamp_ms);
  TEST_ASSERT_EQUAL_UINT32(
      nexstar::Diagnostics::kEventCapacity + 2,
      diagnostics.event(nexstar::Diagnostics::kEventCapacity - 1).timestamp_ms);
}

void test_peripheral_pin_contract_is_unique_and_safe() {
  TEST_ASSERT_TRUE(nexstar::PinsAreDistinct());
  TEST_ASSERT_TRUE(nexstar::PeripheralPinsAreSafe());
  TEST_ASSERT_TRUE(nexstar::BoardPolarity::kAuxTxDisabledLevel);
  TEST_ASSERT_TRUE(nexstar::BoardPolarity::kAuxBusyReleasedLevel);
  TEST_ASSERT_FALSE(nexstar::BoardPolarity::kAuxBusyAssertActiveHigh);
  TEST_ASSERT_EQUAL_UINT8(1, nexstar::BoardPins::kHostUartTx);
  TEST_ASSERT_EQUAL_UINT8(3, nexstar::BoardPins::kHostUartRx);
  TEST_ASSERT_EQUAL_UINT8(16, nexstar::BoardPins::kAuxUartRx);
  TEST_ASSERT_EQUAL_UINT8(17, nexstar::BoardPins::kAuxUartTx);
  TEST_ASSERT_EQUAL_UINT8(34, nexstar::BoardPins::kAuxCtsIn);
}

void test_display_model_describes_current_safe_state() {
  nexstar::DisplaySnapshot snapshot{};
  snapshot.profile = nexstar::FirmwareProfile::kBridge;
  snapshot.state = nexstar::ProjectState::kSafeBaseline;
  snapshot.host_ready = true;

  TEST_ASSERT_EQUAL_STRING("USB BRIDGE", nexstar::ProfileLabel(snapshot.profile));
  TEST_ASSERT_EQUAL_STRING("SAFE BASELINE", nexstar::ProjectStateLabel(snapshot.state));
  TEST_ASSERT_FALSE(snapshot.aux_enabled);
}

void test_display_snapshot_change_detection_covers_status_fields() {
  nexstar::DisplaySnapshot before{};
  nexstar::DisplaySnapshot after = before;
  TEST_ASSERT_TRUE(nexstar::SnapshotsEqual(before, after));

  after.error_count = 1;
  TEST_ASSERT_FALSE(nexstar::SnapshotsEqual(before, after));

  after = before;
  after.host_active = true;
  TEST_ASSERT_FALSE(nexstar::SnapshotsEqual(before, after));

  after = before;
  after.busy_timeout_count = 1;
  TEST_ASSERT_FALSE(nexstar::SnapshotsEqual(before, after));

  after = before;
  after.fault_code = 6;
  TEST_ASSERT_FALSE(nexstar::SnapshotsEqual(before, after));
}

void test_display_model_latches_packet_activity_without_per_byte_redraw_state() {
  nexstar::DisplayModel model;
  nexstar::DisplaySnapshot snapshot{};

  auto view = model.update(snapshot, 100);
  TEST_ASSERT_FALSE(view.show_rx_activity);
  TEST_ASSERT_FALSE(view.show_tx_activity);

  snapshot.rx_packets = 1;
  view = model.update(snapshot, 200);
  TEST_ASSERT_TRUE(view.show_rx_activity);
  TEST_ASSERT_FALSE(view.show_tx_activity);

  view = model.update(snapshot, 949);
  TEST_ASSERT_TRUE(view.show_rx_activity);
  view = model.update(snapshot, 950);
  TEST_ASSERT_FALSE(view.show_rx_activity);
}

void test_display_model_fault_overlay_is_temporary() {
  nexstar::DisplayModel model;
  nexstar::DisplaySnapshot snapshot{};
  model.update(snapshot, 100);

  snapshot.state = nexstar::ProjectState::kFault;
  snapshot.error_count = 1;
  auto view = model.update(snapshot, 200);
  TEST_ASSERT_TRUE(view.show_fault_overlay);

  view = model.update(snapshot, 2199);
  TEST_ASSERT_TRUE(view.show_fault_overlay);
  view = model.update(snapshot, 2200);
  TEST_ASSERT_FALSE(view.show_fault_overlay);
  TEST_ASSERT_EQUAL_STRING("FAULT", nexstar::ProjectStateLabel(view.snapshot.state));
}

void test_display_model_copies_full_status_snapshot() {
  nexstar::DisplayModel model;
  nexstar::DisplaySnapshot snapshot{};
  snapshot.host_ready = true;
  snapshot.host_active = true;
  snapshot.aux_enabled = false;
  snapshot.rx_packets = 12;
  snapshot.tx_packets = 7;
  snapshot.error_count = 2;
  snapshot.busy_timeout_count = 1;
  snapshot.fault_code = 6;

  const auto view = model.update(snapshot, 100);
  TEST_ASSERT_TRUE(view.snapshot.host_ready);
  TEST_ASSERT_TRUE(view.snapshot.host_active);
  TEST_ASSERT_EQUAL_UINT32(12, view.snapshot.rx_packets);
  TEST_ASSERT_EQUAL_UINT32(7, view.snapshot.tx_packets);
  TEST_ASSERT_EQUAL_UINT32(2, view.snapshot.error_count);
  TEST_ASSERT_EQUAL_UINT32(1, view.snapshot.busy_timeout_count);
  TEST_ASSERT_EQUAL_UINT8(6, view.snapshot.fault_code);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_known_profiles_are_valid);
  RUN_TEST(test_only_authorized_active_profiles_may_transmit_aux);
  RUN_TEST(test_peripheral_pin_contract_is_unique_and_safe);
  RUN_TEST(test_diagnostics_are_bounded_and_counters_saturate);
  RUN_TEST(test_display_model_describes_current_safe_state);
  RUN_TEST(test_display_snapshot_change_detection_covers_status_fields);
  RUN_TEST(test_display_model_latches_packet_activity_without_per_byte_redraw_state);
  RUN_TEST(test_display_model_fault_overlay_is_temporary);
  RUN_TEST(test_display_model_copies_full_status_snapshot);
  return UNITY_END();
}
