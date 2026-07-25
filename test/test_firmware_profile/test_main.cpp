#include <unity.h>

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
  TEST_ASSERT_FALSE(nexstar::IsValidProfile(3));
}

void test_only_bridge_profile_may_transmit_aux() {
  TEST_ASSERT_TRUE(nexstar::MayTransmitAux(nexstar::FirmwareProfile::kBridge));
  TEST_ASSERT_FALSE(nexstar::MayTransmitAux(nexstar::FirmwareProfile::kListenOnly));
}

void test_display_model_describes_current_safe_state() {
  nexstar::DisplaySnapshot snapshot{};
  snapshot.profile = nexstar::FirmwareProfile::kBridge;
  snapshot.state = nexstar::ProjectState::kSafeBaseline;
  snapshot.host_ready = true;

  TEST_ASSERT_EQUAL_STRING("BRIDGE", nexstar::ProfileLabel(snapshot.profile));
  TEST_ASSERT_EQUAL_STRING("SAFE BASELINE", nexstar::ProjectStateLabel(snapshot.state));
  TEST_ASSERT_FALSE(snapshot.aux_enabled);
}

void test_display_snapshot_change_detection_covers_status_fields() {
  nexstar::DisplaySnapshot before{};
  nexstar::DisplaySnapshot after = before;
  TEST_ASSERT_TRUE(nexstar::SnapshotsEqual(before, after));

  after.error_count = 1;
  TEST_ASSERT_FALSE(nexstar::SnapshotsEqual(before, after));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_known_profiles_are_valid);
  RUN_TEST(test_only_bridge_profile_may_transmit_aux);
  RUN_TEST(test_display_model_describes_current_safe_state);
  RUN_TEST(test_display_snapshot_change_detection_covers_status_fields);
  return UNITY_END();
}
