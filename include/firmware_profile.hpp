#pragma once

#include <cstdint>

namespace nexstar {

enum class FirmwareProfile : std::uint8_t {
  kBridge = 1,
  kListenOnly = 2,
};

constexpr bool IsValidProfile(const std::uint8_t profile) {
  return profile == static_cast<std::uint8_t>(FirmwareProfile::kBridge) ||
         profile == static_cast<std::uint8_t>(FirmwareProfile::kListenOnly);
}

constexpr bool MayTransmitAux(const FirmwareProfile profile) {
  return profile == FirmwareProfile::kBridge;
}

}  // namespace nexstar
