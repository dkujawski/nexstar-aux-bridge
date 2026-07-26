#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "echo_tracker.hpp"

namespace {

nexstar::AuxPacket VersionPacket() {
  nexstar::AuxPacket packet{};
  constexpr std::array<std::uint8_t, 6> bytes{
      0x3B, 0x03, 0x04, 0x10, 0xFE, 0xEB,
  };
  packet.size = bytes.size();
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    packet.bytes[index] = bytes[index];
  }
  return packet;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_perfect_fragmented_echo_is_consumed() {
  const auto packet = VersionPacket();
  nexstar::EchoTracker tracker;
  TEST_ASSERT_TRUE(tracker.begin(packet, 10));

  for (std::size_t index = 0; index < packet.size; ++index) {
    const auto forward = tracker.feed(packet.bytes[index], 11 + index);
    TEST_ASSERT_EQUAL_UINT16(0, forward.size);
  }
  TEST_ASSERT_FALSE(tracker.active());
  TEST_ASSERT_EQUAL_UINT32(1, tracker.matches());
}

void test_packet_immediately_after_echo_is_forwarded() {
  const auto packet = VersionPacket();
  nexstar::EchoTracker tracker;
  tracker.begin(packet, 0);
  for (std::size_t index = 0; index < packet.size; ++index) {
    tracker.feed(packet.bytes[index], index);
  }

  const auto forward = tracker.feed(0x3B, 7);
  TEST_ASSERT_EQUAL_UINT16(1, forward.size);
  TEST_ASSERT_EQUAL_HEX8(0x3B, forward.bytes[0]);
}

void test_partial_corrupt_echo_releases_every_consumed_byte() {
  const auto packet = VersionPacket();
  nexstar::EchoTracker tracker;
  tracker.begin(packet, 0);
  tracker.feed(packet.bytes[0], 1);
  tracker.feed(packet.bytes[1], 2);
  const auto forward = tracker.feed(0x99, 3);

  TEST_ASSERT_EQUAL_UINT16(3, forward.size);
  TEST_ASSERT_EQUAL_HEX8(packet.bytes[0], forward.bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(packet.bytes[1], forward.bytes[1]);
  TEST_ASSERT_EQUAL_HEX8(0x99, forward.bytes[2]);
  TEST_ASSERT_EQUAL_UINT32(1, tracker.mismatches());
}

void test_missing_and_partial_echo_timeout_are_bounded() {
  const auto packet = VersionPacket();
  nexstar::EchoTracker tracker;
  tracker.begin(packet, 100);
  auto forward =
      tracker.poll(100 + nexstar::EchoTracker::EchoWindowMs(packet.size));
  TEST_ASSERT_EQUAL_UINT16(0, forward.size);
  TEST_ASSERT_EQUAL_UINT32(1, tracker.timeouts());

  tracker.begin(packet, 200);
  tracker.feed(packet.bytes[0], 201);
  tracker.feed(packet.bytes[1], 202);
  forward = tracker.poll(
      200 + nexstar::EchoTracker::EchoWindowMs(packet.size));
  TEST_ASSERT_EQUAL_UINT16(2, forward.size);
  TEST_ASSERT_EQUAL_HEX8(packet.bytes[0], forward.bytes[0]);
  TEST_ASSERT_EQUAL_HEX8(packet.bytes[1], forward.bytes[1]);
}

void test_oversized_packet_metadata_is_rejected() {
  auto packet = VersionPacket();
  packet.size = nexstar::kAuxMaximumPacketSize + 1;
  nexstar::EchoTracker tracker;
  TEST_ASSERT_FALSE(tracker.begin(packet, 0));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_perfect_fragmented_echo_is_consumed);
  RUN_TEST(test_packet_immediately_after_echo_is_forwarded);
  RUN_TEST(test_partial_corrupt_echo_releases_every_consumed_byte);
  RUN_TEST(test_missing_and_partial_echo_timeout_are_bounded);
  RUN_TEST(test_oversized_packet_metadata_is_rejected);
  return UNITY_END();
}
