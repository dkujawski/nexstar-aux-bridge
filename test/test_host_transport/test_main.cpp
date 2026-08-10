#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "host_transport.hpp"

namespace {

class FakeStream final : public nexstar::HostByteStream {
 public:
  int available() const override {
    return static_cast<int>(input_size_ - input_offset_);
  }
  int read() override {
    return input_offset_ < input_size_ ? input_[input_offset_++] : -1;
  }
  std::size_t write(const std::uint8_t* bytes, const std::size_t size) override {
    const std::size_t accepted = write_limit_ < size ? write_limit_ : size;
    for (std::size_t index = 0; index < accepted; ++index) {
      output_[output_size_++] = bytes[index];
    }
    return accepted;
  }
  void append(const std::uint8_t* bytes, const std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
      input_[input_size_++] = bytes[index];
    }
  }
  void setWriteLimit(const std::size_t limit) { write_limit_ = limit; }
  [[nodiscard]] std::size_t outputSize() const { return output_size_; }
  [[nodiscard]] std::uint8_t outputAt(const std::size_t index) const {
    return output_[index];
  }

 private:
  std::array<std::uint8_t, 64> input_{};
  std::array<std::uint8_t, 64> output_{};
  std::size_t input_size_{0};
  std::size_t input_offset_{0};
  std::size_t output_size_{0};
  std::size_t write_limit_{64};
};

nexstar::AuxPacket Packet(const nexstar::PacketOrigin origin) {
  nexstar::AuxPacket packet{};
  constexpr std::array<std::uint8_t, 6> bytes{0x3B, 0x03, 0x04, 0x10, 0xFE, 0xEB};
  packet.size = bytes.size();
  packet.origin = origin;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    packet.bytes[index] = bytes[index];
  }
  return packet;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_host_input_is_decoded_before_entering_aux_queue() {
  FakeStream stream;
  nexstar::PacketRouter<2, 2> router;
  nexstar::HostTransport<2, 2> transport(stream, router);
  const auto packet = Packet(nexstar::PacketOrigin::kHost);
  stream.append(packet.bytes.data(), packet.size);
  transport.service(100);

  nexstar::AuxPacket output{};
  TEST_ASSERT_TRUE(router.takeForAux(output));
  TEST_ASSERT_EQUAL_UINT16(packet.size, output.size);
  TEST_ASSERT_EQUAL_UINT32(packet.size, transport.metrics().received_bytes);
  TEST_ASSERT_EQUAL_UINT32(1, transport.metrics().decoded_packets);
}

void test_malformed_host_packet_never_enters_aux_queue() {
  FakeStream stream;
  nexstar::PacketRouter<2, 2> router;
  nexstar::HostTransport<2, 2> transport(stream, router);
  auto packet = Packet(nexstar::PacketOrigin::kHost);
  packet.bytes[5] ^= 1;
  stream.append(packet.bytes.data(), packet.size);
  transport.service(100);

  nexstar::AuxPacket output{};
  TEST_ASSERT_FALSE(router.takeForAux(output));
  TEST_ASSERT_EQUAL_UINT32(1, transport.metrics().malformed_packets);
}

void test_partial_host_writes_are_deferred_without_blocking() {
  FakeStream stream;
  stream.setWriteLimit(2);
  nexstar::PacketRouter<2, 2> router;
  nexstar::HostTransport<2, 2> transport(stream, router);
  const auto packet = Packet(nexstar::PacketOrigin::kAuxBus);
  TEST_ASSERT_TRUE(router.routeFromAux(packet));

  transport.service(100);
  TEST_ASSERT_EQUAL_UINT32(2, transport.metrics().transmitted_bytes);
  TEST_ASSERT_EQUAL_UINT32(packet.size - 2, transport.pendingOutputBytes());
  transport.service(101);
  transport.service(102);
  TEST_ASSERT_EQUAL_UINT32(packet.size, transport.metrics().transmitted_bytes);
  TEST_ASSERT_EQUAL_UINT32(0, transport.pendingOutputBytes());
  TEST_ASSERT_EQUAL_UINT32(packet.size, stream.outputSize());
  TEST_ASSERT_EQUAL_HEX8(0x3B, stream.outputAt(0));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_host_input_is_decoded_before_entering_aux_queue);
  RUN_TEST(test_malformed_host_packet_never_enters_aux_queue);
  RUN_TEST(test_partial_host_writes_are_deferred_without_blocking);
  return UNITY_END();
}
