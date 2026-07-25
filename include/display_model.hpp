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
  bool aux_enabled{false};
  bool rx_active{false};
  bool tx_active{false};
  std::uint32_t rx_packets{0};
  std::uint32_t tx_packets{0};
  std::uint32_t error_count{0};
  bool battery_valid{false};
  std::uint16_t battery_millivolts{0};
};

constexpr const char* ProfileLabel(const FirmwareProfile profile) {
  return profile == FirmwareProfile::kListenOnly ? "LISTEN ONLY" : "BRIDGE";
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
         lhs.host_ready == rhs.host_ready && lhs.aux_enabled == rhs.aux_enabled &&
         lhs.rx_active == rhs.rx_active && lhs.tx_active == rhs.tx_active &&
         lhs.rx_packets == rhs.rx_packets && lhs.tx_packets == rhs.tx_packets &&
         lhs.error_count == rhs.error_count && lhs.battery_valid == rhs.battery_valid &&
         lhs.battery_millivolts == rhs.battery_millivolts;
}

}  // namespace nexstar
