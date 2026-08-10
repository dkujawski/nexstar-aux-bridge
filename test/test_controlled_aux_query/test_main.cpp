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

void test_accepts_only_matching_checksum_valid_version_response() {
  const auto query = nexstar::BuildControlledVersionQuery(0x03, 0x10);
  nexstar::AuxPacket response{};
  constexpr std::uint8_t bytes[]{0x3B, 0x05, 0x10, 0x03,
                                 0xFE, 0x05, 0x14, 0xD1};
  response.size = sizeof(bytes);
  response.origin = nexstar::PacketOrigin::kAuxBus;
  for (std::size_t index = 0; index < sizeof(bytes); ++index) {
    response.bytes[index] = bytes[index];
  }
  TEST_ASSERT_TRUE(nexstar::IsControlledVersionResponse(query, response));

  response.bytes[3] = 0x04;
  response.bytes[7] = nexstar::CalculateAuxChecksum(response.bytes.data(), 7);
  TEST_ASSERT_FALSE(nexstar::IsControlledVersionResponse(query, response));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_builds_checksum_valid_payload_free_version_query);
  RUN_TEST(test_rejects_mutating_payload_and_non_version_commands);
  RUN_TEST(test_accepts_only_matching_checksum_valid_version_response);
  return UNITY_END();
}
