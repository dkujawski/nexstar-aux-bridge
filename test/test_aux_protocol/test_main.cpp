#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "aux_protocol.hpp"

namespace {

template <std::size_t MessageSize>
std::array<std::uint8_t, MessageSize + 3> MakePacket(
    const std::array<std::uint8_t, MessageSize>& message) {
  std::array<std::uint8_t, MessageSize + 3> packet{};
  packet[0] = nexstar::kAuxPreamble;
  packet[1] = static_cast<std::uint8_t>(MessageSize);
  for (std::size_t index = 0; index < MessageSize; ++index) {
    packet[index + 2] = message[index];
  }
  packet[MessageSize + 2] =
      nexstar::CalculateAuxChecksum(packet.data(), packet.size() - 1);
  return packet;
}

template <std::size_t Size>
nexstar::DecodeResult FeedPacket(nexstar::AuxStreamDecoder& decoder,
                                 const std::array<std::uint8_t, Size>& bytes,
                                 nexstar::AuxPacket& output,
                                 std::uint32_t start_ms = 0) {
  nexstar::DecodeResult result = nexstar::DecodeResult::kNone;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    result = decoder.feed(bytes[index], start_ms + index,
                          nexstar::PacketOrigin::kAuxBus, output);
  }
  return result;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_checksum_matches_published_version_query() {
  constexpr std::array<std::uint8_t, 5> without_checksum{
      0x3B, 0x03, 0x04, 0x10, 0xFE,
  };
  TEST_ASSERT_EQUAL_HEX8(
      0xEB, nexstar::CalculateAuxChecksum(without_checksum.data(),
                                          without_checksum.size()));
}

void test_valid_fragmented_packet_preserves_metadata() {
  const auto bytes = MakePacket<3>({0x04, 0x10, 0xFE});
  nexstar::AuxStreamDecoder decoder;
  nexstar::AuxPacket output{};

  for (std::size_t index = 0; index + 1 < bytes.size(); ++index) {
    TEST_ASSERT_EQUAL(
        static_cast<int>(nexstar::DecodeResult::kNone),
        static_cast<int>(decoder.feed(bytes[index], 100 + index,
                                      nexstar::PacketOrigin::kHost, output)));
  }
  TEST_ASSERT_EQUAL(
      static_cast<int>(nexstar::DecodeResult::kPacket),
      static_cast<int>(decoder.feed(bytes.back(), 105,
                                    nexstar::PacketOrigin::kHost, output)));
  TEST_ASSERT_EQUAL_UINT16(bytes.size(), output.size);
  TEST_ASSERT_EQUAL_UINT32(105, output.timestamp_ms);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::PacketOrigin::kHost),
                    static_cast<int>(output.origin));
}

void test_adjacent_packets_and_noise_decode_independently() {
  const auto first = MakePacket<3>({0x04, 0x10, 0xFE});
  const auto second = MakePacket<4>({0x10, 0x04, 0x01, 0xAA});
  nexstar::AuxStreamDecoder decoder;
  nexstar::AuxPacket output{};

  decoder.feed(0x00, 0, nexstar::PacketOrigin::kAuxBus, output);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::DecodeResult::kPacket),
                    static_cast<int>(FeedPacket(decoder, first, output, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::DecodeResult::kPacket),
                    static_cast<int>(FeedPacket(decoder, second, output, 20)));
  TEST_ASSERT_EQUAL_HEX8(0xAA, output.bytes[5]);
}

void test_bad_checksum_is_rejected_then_decoder_recovers() {
  auto bad = MakePacket<3>({0x04, 0x10, 0xFE});
  bad.back() ^= 0x01;
  const auto good = MakePacket<3>({0x04, 0x11, 0xFE});
  nexstar::AuxStreamDecoder decoder;
  nexstar::AuxPacket output{};

  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::DecodeResult::kRejected),
                    static_cast<int>(FeedPacket(decoder, bad, output)));
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::DecodeResult::kPacket),
                    static_cast<int>(FeedPacket(decoder, good, output, 20)));
}

void test_invalid_length_and_truncated_timeout_recover() {
  nexstar::AuxStreamDecoder decoder(5);
  nexstar::AuxPacket output{};
  decoder.feed(0x3B, 0, nexstar::PacketOrigin::kAuxBus, output);
  TEST_ASSERT_EQUAL(
      static_cast<int>(nexstar::DecodeResult::kRejected),
      static_cast<int>(
          decoder.feed(2, 1, nexstar::PacketOrigin::kAuxBus, output)));

  const auto good = MakePacket<3>({0x04, 0x10, 0xFE});
  decoder.feed(good[0], 10, nexstar::PacketOrigin::kAuxBus, output);
  decoder.feed(good[1], 11, nexstar::PacketOrigin::kAuxBus, output);
  decoder.feed(good[2], 12, nexstar::PacketOrigin::kAuxBus, output);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::DecodeResult::kPacket),
                    static_cast<int>(FeedPacket(decoder, good, output, 30)));
}

void test_maximum_declared_length_is_bounds_safe() {
  std::array<std::uint8_t, nexstar::kAuxMaximumPacketSize> bytes{};
  bytes[0] = nexstar::kAuxPreamble;
  bytes[1] = 255;
  bytes[2] = 0x04;
  bytes[3] = 0x10;
  bytes[4] = 0xFE;
  bytes.back() =
      nexstar::CalculateAuxChecksum(bytes.data(), bytes.size() - 1);

  nexstar::AuxStreamDecoder decoder;
  nexstar::AuxPacket output{};
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::DecodeResult::kPacket),
                    static_cast<int>(FeedPacket(decoder, bytes, output)));
  TEST_ASSERT_EQUAL_UINT16(nexstar::kAuxMaximumPacketSize, output.size);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_matches_published_version_query);
  RUN_TEST(test_valid_fragmented_packet_preserves_metadata);
  RUN_TEST(test_adjacent_packets_and_noise_decode_independently);
  RUN_TEST(test_bad_checksum_is_rejected_then_decoder_recovers);
  RUN_TEST(test_invalid_length_and_truncated_timeout_recover);
  RUN_TEST(test_maximum_declared_length_is_bounds_safe);
  return UNITY_END();
}
