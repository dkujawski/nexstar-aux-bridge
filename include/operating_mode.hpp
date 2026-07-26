#pragma once

#include <cstdint>

namespace nexstar {

enum class OperatingMode : std::uint8_t {
  kBridge = 1,
  kUsbBridge = kBridge,
  kListenOnly = 2,
  kDiagnostic = 3,
};

constexpr bool IsValidOperatingMode(const std::uint8_t mode) {
  return mode >= static_cast<std::uint8_t>(OperatingMode::kUsbBridge) &&
         mode <= static_cast<std::uint8_t>(OperatingMode::kDiagnostic);
}

constexpr bool MayTransmitAux(const OperatingMode mode) {
  return mode == OperatingMode::kBridge;
}

constexpr const char* OperatingModeLabel(const OperatingMode mode) {
  switch (mode) {
    case OperatingMode::kBridge:
      return "USB BRIDGE";
    case OperatingMode::kListenOnly:
      return "LISTEN ONLY";
    case OperatingMode::kDiagnostic:
      return "DIAGNOSTIC";
  }
  return "UNKNOWN";
}

}  // namespace nexstar
