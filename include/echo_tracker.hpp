#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aux_protocol.hpp"

namespace nexstar {

struct EchoForward {
  std::array<std::uint8_t, kAuxMaximumPacketSize> bytes{};
  std::uint16_t size{0};
};

class EchoTracker {
 public:
  static constexpr std::uint32_t kBaud = 19200;
  static constexpr std::uint32_t kBitsPerCharacter = 10;
  static constexpr std::uint32_t kDeadlineMarginMs = 5;

  bool begin(const AuxPacket& transmitted, const std::uint32_t now_ms) {
    if (transmitted.size == 0 || transmitted.size > expected_.size()) {
      return false;
    }
    expected_size_ = transmitted.size;
    matched_size_ = 0;
    for (std::size_t index = 0; index < expected_size_; ++index) {
      expected_[index] = transmitted.bytes[index];
    }
    deadline_ms_ = now_ms + EchoWindowMs(expected_size_);
    active_ = true;
    return true;
  }

  EchoForward feed(const std::uint8_t byte, const std::uint32_t now_ms) {
    EchoForward forward{};
    if (!active_) {
      forward.bytes[0] = byte;
      forward.size = 1;
      return forward;
    }

    if (DeadlineReached(now_ms, deadline_ms_)) {
      releasePartial(forward);
      ++timeouts_;
      active_ = false;
      if (forward.size < forward.bytes.size()) {
        forward.bytes[forward.size++] = byte;
      }
      return forward;
    }

    if (byte == expected_[matched_size_]) {
      ++matched_size_;
      if (matched_size_ == expected_size_) {
        ++matches_;
        active_ = false;
      }
      return forward;
    }

    releasePartial(forward);
    if (forward.size < forward.bytes.size()) {
      forward.bytes[forward.size++] = byte;
    }
    ++mismatches_;
    active_ = false;
    return forward;
  }

  EchoForward poll(const std::uint32_t now_ms) {
    EchoForward forward{};
    if (active_ && DeadlineReached(now_ms, deadline_ms_)) {
      releasePartial(forward);
      ++timeouts_;
      active_ = false;
    }
    return forward;
  }

  // A transmitter recovery abandons any partial physical echo. Do not forward
  // it into the packet decoder: the failed transaction must not influence the
  // next host request.
  void cancel() {
    expected_size_ = 0;
    matched_size_ = 0;
    active_ = false;
  }

  [[nodiscard]] bool active() const { return active_; }
  [[nodiscard]] std::uint32_t matches() const { return matches_; }
  [[nodiscard]] std::uint32_t mismatches() const { return mismatches_; }
  [[nodiscard]] std::uint32_t timeouts() const { return timeouts_; }

  static constexpr std::uint32_t EchoWindowMs(const std::size_t packet_size) {
    const std::uint32_t transmission_ms = static_cast<std::uint32_t>(
        (packet_size * kBitsPerCharacter * 1000U + kBaud - 1U) / kBaud);
    return transmission_ms + kDeadlineMarginMs;
  }

 private:
  static constexpr bool DeadlineReached(const std::uint32_t now_ms,
                                        const std::uint32_t deadline_ms) {
    return static_cast<std::int32_t>(now_ms - deadline_ms) >= 0;
  }

  void releasePartial(EchoForward& forward) const {
    for (std::size_t index = 0; index < matched_size_; ++index) {
      forward.bytes[forward.size++] = expected_[index];
    }
  }

  std::array<std::uint8_t, kAuxMaximumPacketSize> expected_{};
  std::size_t expected_size_{0};
  std::size_t matched_size_{0};
  std::uint32_t deadline_ms_{0};
  std::uint32_t matches_{0};
  std::uint32_t mismatches_{0};
  std::uint32_t timeouts_{0};
  bool active_{false};
};

}  // namespace nexstar
