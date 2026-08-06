#include <unity.h>

#include "controlled_aux_query.hpp"

void setUp() {}
void tearDown() {}

void test_builds_checksum_valid_payload_free_version_query() {
  const auto packet = nexstar::BuildControlledVersionQuery(0x04, 0x10);
  TEST_ASSERT_TRUE(nexstar::HasValidAuxChecksum(packet));
  TEST_ASSERT_TRUE(nexstar::IsControlledReadOnlyQuery(packet));
  TEST_ASSERT_EQUAL_HEX8(0xFE, packet.bytes[4]);
  TEST_ASSERT_EQUAL_UINT16(6, packet.size);
}

void test_rejects_mutating_payload_and_non_version_commands() {
  auto packet = nexstar::BuildControlledVersionQuery(0x04, 0x10);
  packet.bytes[4] = 0x24;
  packet.bytes[5] =
      nexstar::CalculateAuxChecksum(packet.bytes.data(), packet.size - 1);
  TEST_ASSERT_FALSE(nexstar::IsControlledReadOnlyQuery(packet));

  packet = nexstar::BuildControlledVersionQuery(0x04, 0x10);
  packet.origin = nexstar::PacketOrigin::kAuxBus;
  TEST_ASSERT_FALSE(nexstar::IsControlledReadOnlyQuery(packet));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_builds_checksum_valid_payload_free_version_query);
  RUN_TEST(test_rejects_mutating_payload_and_non_version_commands);
  return UNITY_END();
}
