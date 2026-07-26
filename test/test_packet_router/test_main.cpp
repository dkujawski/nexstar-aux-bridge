#include <unity.h>

#include <array>
#include <cstddef>

#include "packet_router.hpp"

namespace {

nexstar::AuxPacket Packet(const nexstar::PacketOrigin origin,
                          const std::uint8_t destination) {
  nexstar::AuxPacket packet{};
  constexpr std::array<std::uint8_t, 6> base{
      0x3B, 0x03, 0x04, 0x10, 0xFE, 0xEB,
  };
  packet.size = base.size();
  packet.origin = origin;
  for (std::size_t index = 0; index < base.size(); ++index) {
    packet.bytes[index] = base[index];
  }
  packet.bytes[3] = destination;
  packet.bytes[5] =
      nexstar::CalculateAuxChecksum(packet.bytes.data(), packet.size - 1);
  return packet;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_routes_each_direction_without_loopback() {
  nexstar::PacketRouter<2, 2> router;
  const auto host_packet = Packet(nexstar::PacketOrigin::kHost, 0x10);
  const auto aux_packet = Packet(nexstar::PacketOrigin::kAuxBus, 0x04);

  TEST_ASSERT_TRUE(router.routeFromHost(host_packet));
  TEST_ASSERT_TRUE(router.routeFromAux(aux_packet));

  nexstar::AuxPacket output{};
  TEST_ASSERT_TRUE(router.takeForAux(output));
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::PacketOrigin::kHost),
                    static_cast<int>(output.origin));
  TEST_ASSERT_TRUE(router.takeForHost(output));
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::PacketOrigin::kAuxBus),
                    static_cast<int>(output.origin));
}

void test_queue_overflow_drops_newest_and_is_observable() {
  nexstar::PacketRouter<1, 1> router;
  const auto first = Packet(nexstar::PacketOrigin::kHost, 0x10);
  const auto second = Packet(nexstar::PacketOrigin::kHost, 0x11);
  TEST_ASSERT_TRUE(router.routeFromHost(first));
  TEST_ASSERT_FALSE(router.routeFromHost(second));
  TEST_ASSERT_EQUAL_UINT32(1, router.auxQueue().overflows());

  nexstar::AuxPacket output{};
  TEST_ASSERT_TRUE(router.takeForAux(output));
  TEST_ASSERT_EQUAL_HEX8(0x10, output.bytes[3]);
}

void test_invalid_checksum_and_wrong_origin_never_reach_aux() {
  nexstar::PacketRouter<2, 2> router;
  auto bad = Packet(nexstar::PacketOrigin::kHost, 0x10);
  bad.bytes[5] ^= 1;
  TEST_ASSERT_FALSE(router.routeFromHost(bad));

  const auto wrong_origin = Packet(nexstar::PacketOrigin::kAuxBus, 0x10);
  TEST_ASSERT_FALSE(router.routeFromHost(wrong_origin));
  TEST_ASSERT_EQUAL_UINT32(2, router.invalidHostPackets());

  nexstar::AuxPacket output{};
  TEST_ASSERT_FALSE(router.takeForAux(output));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_routes_each_direction_without_loopback);
  RUN_TEST(test_queue_overflow_drops_newest_and_is_observable);
  RUN_TEST(test_invalid_checksum_and_wrong_origin_never_reach_aux);
  return UNITY_END();
}
