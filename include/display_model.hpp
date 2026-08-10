#pragma once

#include <cstdint>

#include "firmware_profile.hpp"

namespace nexstar {

enum class ProjectState : std::uint8_t {
  kSafeBaseline,
  kOperational,
  kFault,
};

struct DisplaySnapshot {
  FirmwareProfile profile{FirmwareProfile::kBridge};
  ProjectState state{ProjectState::kSafeBaseline};
  bool host_ready{false};
  bool host_active{false};
  bool aux_enabled{false};
  bool rx_active{false};
  bool tx_active{false};
  std::uint32_t rx_packets{0};
  std::uint32_t tx_packets{0};
  std::uint32_t error_count{0};
  std::uint32_t busy_timeout_count{0};
  std::uint8_t fault_code{0};
};

struct DisplayViewModel {
  DisplaySnapshot snapshot{};
  bool show_rx_activity{false};
  bool show_tx_activity{false};
  bool show_fault_overlay{false};
};

class DisplayModel {
 public:
  static constexpr std::uint32_t kActivityHoldMs = 750;
  static constexpr std::uint32_t kFaultOverlayMs = 2000;

  DisplayViewModel update(const DisplaySnapshot& snapshot, std::uint32_t now_ms) {
    if (snapshot.rx_active || snapshot.rx_packets != last_snapshot_.rx_packets) {
      rx_activity_until_ms_ = now_ms + kActivityHoldMs;
    }
    if (snapshot.tx_active || snapshot.tx_packets != last_snapshot_.tx_packets) {
      tx_activity_until_ms_ = now_ms + kActivityHoldMs;
    }
    if ((snapshot.state == ProjectState::kFault &&
         last_snapshot_.state != ProjectState::kFault) ||
        snapshot.error_count != last_snapshot_.error_count) {
      fault_overlay_until_ms_ = now_ms + kFaultOverlayMs;
    }

    last_snapshot_ = snapshot;
    return {
        snapshot,
        DeadlinePending(now_ms, rx_activity_until_ms_),
        DeadlinePending(now_ms, tx_activity_until_ms_),
        DeadlinePending(now_ms, fault_overlay_until_ms_),
    };
  }

 private:
  static constexpr bool DeadlinePending(const std::uint32_t now_ms,
                                        const std::uint32_t deadline_ms) {
    return static_cast<std::int32_t>(deadline_ms - now_ms) > 0;
  }

  DisplaySnapshot last_snapshot_{};
  std::uint32_t rx_activity_until_ms_{0};
  std::uint32_t tx_activity_until_ms_{0};
  std::uint32_t fault_overlay_until_ms_{0};
};

constexpr const char* ProfileLabel(const FirmwareProfile profile) {
  return OperatingModeLabel(profile);
}

constexpr const char* ProjectStateLabel(const ProjectState state) {
  switch (state) {
    case ProjectState::kSafeBaseline:
      return "SAFE BASELINE";
    case ProjectState::kOperational:
      return "OPERATIONAL";
    case ProjectState::kFault:
      return "FAULT";
  }
  return "UNKNOWN";
}

constexpr bool SnapshotsEqual(const DisplaySnapshot& lhs, const DisplaySnapshot& rhs) {
  return lhs.profile == rhs.profile && lhs.state == rhs.state &&
         lhs.host_ready == rhs.host_ready && lhs.host_active == rhs.host_active &&
         lhs.aux_enabled == rhs.aux_enabled &&
         lhs.rx_active == rhs.rx_active && lhs.tx_active == rhs.tx_active &&
         lhs.rx_packets == rhs.rx_packets && lhs.tx_packets == rhs.tx_packets &&
         lhs.error_count == rhs.error_count &&
         lhs.busy_timeout_count == rhs.busy_timeout_count &&
         lhs.fault_code == rhs.fault_code;
}

}  // namespace nexstar
