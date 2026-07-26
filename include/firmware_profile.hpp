#pragma once

#include "operating_mode.hpp"

namespace nexstar {

using FirmwareProfile = OperatingMode;

constexpr bool IsValidProfile(const std::uint8_t profile) {
  return IsValidOperatingMode(profile);
}

}  // namespace nexstar
