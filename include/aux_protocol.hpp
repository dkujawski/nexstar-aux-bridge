#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexstar {

constexpr std::uint8_t kAuxPreamble = 0x3B;
constexpr std::uint8_t kAuxMinimumMessageLength = 3;
constexpr std::size_t kAuxMaximumPacketSize = 258;

enum class PacketOrigin : std::uint8_t {
  kHost,
  kAuxBus,
};

struct AuxPacket {
  std::array<std::uint8_t, kAuxMaximumPacketSize> bytes{};
  std::uint16_t size{0};
  PacketOrigin origin{PacketOrigin::kAuxBus};
  std::uint32_t timestamp_ms{0};

  [[nodiscard]] std::uint8_t messageLength() const {
    return size >= 2 ? bytes[1] : 0;
  }
};

constexpr std::uint8_t CalculateAuxChecksum(const std::uint8_t* bytes,
                                            const std::size_t size_without_checksum) {
  std::uint8_t sum = 0;
  for (std::size_t index = 1; index < size_without_checksum; ++index) {
    sum = static_cast<std::uint8_t>(sum + bytes[index]);
  }
  return static_cast<std::uint8_t>(0U - sum);
}

constexpr bool HasValidAuxChecksum(const AuxPacket& packet) {
  if (packet.size < kAuxMinimumMessageLength + 3 ||
      packet.bytes[0] != kAuxPreamble ||
      packet.size != static_cast<std::size_t>(packet.bytes[1]) + 3) {
    return false;
  }
  std::uint8_t sum = 0;
  for (std::size_t index = 1; index < packet.size; ++index) {
    sum = static_cast<std::uint8_t>(sum + packet.bytes[index]);
  }
  return sum == 0;
}

enum class DecodeResult : std::uint8_t {
  kNone,
  kPacket,
  kRejected,
};

class AuxStreamDecoder {
 public:
  static constexpr std::uint32_t kDefaultInterByteTimeoutMs = 20;

  explicit AuxStreamDecoder(
      const std::uint32_t inter_byte_timeout_ms = kDefaultInterByteTimeoutMs)
      : inter_byte_timeout_ms_(inter_byte_timeout_ms) {}

  DecodeResult feed(const std::uint8_t byte, const std::uint32_t now_ms,
                    const PacketOrigin origin, AuxPacket& output) {
    if (size_ != 0 && Elapsed(now_ms, last_byte_ms_) > inter_byte_timeout_ms_) {
      reset();
    }
    last_byte_ms_ = now_ms;

    if (size_ == 0) {
      if (byte == kAuxPreamble) {
        buffer_[size_++] = byte;
      }
      return DecodeResult::kNone;
    }

    if (size_ == 1) {
      if (byte < kAuxMinimumMessageLength) {
        reset();
        return DecodeResult::kRejected;
      }
      expected_size_ = static_cast<std::size_t>(byte) + 3;
      if (expected_size_ > buffer_.size()) {
        reset();
        return DecodeResult::kRejected;
      }
      buffer_[size_++] = byte;
      return DecodeResult::kNone;
    }

    if (size_ >= buffer_.size() || size_ >= expected_size_) {
      reset();
      return DecodeResult::kRejected;
    }
    buffer_[size_++] = byte;

    if (size_ != expected_size_) {
      return DecodeResult::kNone;
    }

    AuxPacket candidate{};
    candidate.size = static_cast<std::uint16_t>(size_);
    candidate.origin = origin;
    candidate.timestamp_ms = now_ms;
    for (std::size_t index = 0; index < size_; ++index) {
      candidate.bytes[index] = buffer_[index];
    }

    const bool valid = HasValidAuxChecksum(candidate);
    reset();
    if (!valid) {
      if (byte == kAuxPreamble) {
        buffer_[size_++] = byte;
        last_byte_ms_ = now_ms;
      }
      return DecodeResult::kRejected;
    }

    output = candidate;
    return DecodeResult::kPacket;
  }

  void reset() {
    size_ = 0;
    expected_size_ = 0;
  }

  [[nodiscard]] std::size_t bufferedSize() const { return size_; }

 private:
  static constexpr std::uint32_t Elapsed(const std::uint32_t now,
                                         const std::uint32_t then) {
    return now - then;
  }

  std::array<std::uint8_t, kAuxMaximumPacketSize> buffer_{};
  std::size_t size_{0};
  std::size_t expected_size_{0};
  std::uint32_t last_byte_ms_{0};
  std::uint32_t inter_byte_timeout_ms_;
};

}  // namespace nexstar
