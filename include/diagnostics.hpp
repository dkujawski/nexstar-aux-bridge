#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexstar {

enum class DiagnosticCounter : std::uint8_t {
  kParserFailures,
  kQueueOverflows,
  kBusyTimeouts,
  kTxFailures,
  kEchoFailures,
  kUsbFailures,
  kCount,
};

enum class DiagnosticEventCode : std::uint8_t {
  kBoot,
  kModeSelected,
  kSafeOutputsForced,
  kParserFailure,
  kQueueOverflow,
  kBusyTimeout,
  kTxFailure,
  kEchoFailure,
  kUsbFailure,
};

struct DiagnosticEvent {
  std::uint32_t timestamp_ms{0};
  DiagnosticEventCode code{DiagnosticEventCode::kBoot};
  std::uint16_t detail{0};
};

class Diagnostics {
 public:
  static constexpr std::size_t kEventCapacity = 32;

  void increment(const DiagnosticCounter counter) {
    std::uint32_t& value = counters_[static_cast<std::size_t>(counter)];
    if (value != UINT32_MAX) {
      ++value;
    }
  }

  [[nodiscard]] std::uint32_t count(const DiagnosticCounter counter) const {
    return counters_[static_cast<std::size_t>(counter)];
  }

  void record(const DiagnosticEvent event) {
    events_[next_event_] = event;
    next_event_ = (next_event_ + 1) % kEventCapacity;
    if (event_count_ < kEventCapacity) {
      ++event_count_;
    }
  }

  [[nodiscard]] std::size_t eventCount() const { return event_count_; }

  [[nodiscard]] DiagnosticEvent event(const std::size_t oldest_first_index) const {
    if (oldest_first_index >= event_count_) {
      return {};
    }
    const std::size_t oldest =
        event_count_ == kEventCapacity ? next_event_ : 0;
    return events_[(oldest + oldest_first_index) % kEventCapacity];
  }

 private:
  std::array<std::uint32_t,
             static_cast<std::size_t>(DiagnosticCounter::kCount)>
      counters_{};
  std::array<DiagnosticEvent, kEventCapacity> events_{};
  std::size_t next_event_{0};
  std::size_t event_count_{0};
};

}  // namespace nexstar
