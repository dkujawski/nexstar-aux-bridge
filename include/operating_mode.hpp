#pragma once

#include <cstdint>

namespace nexstar {

enum class OperatingMode : std::uint8_t {
  kBridge = 1,
  kUsbBridge = kBridge,
  kListenOnly = 2,
  kDiagnostic = 3,
  kControlledTest = 4,
};

constexpr bool IsValidOperatingMode(const std::uint8_t mode) {
  return mode >= static_cast<std::uint8_t>(OperatingMode::kUsbBridge) &&
         mode <= static_cast<std::uint8_t>(OperatingMode::kControlledTest);
}

constexpr bool MayTransmitAux(const OperatingMode mode) {
  return mode == OperatingMode::kUsbBridge ||
         mode == OperatingMode::kControlledTest;
}

constexpr const char* OperatingModeLabel(const OperatingMode mode) {
  switch (mode) {
    case OperatingMode::kBridge:
      return "USB BRIDGE";
    case OperatingMode::kListenOnly:
      return "LISTEN ONLY";
    case OperatingMode::kDiagnostic:
      return "DIAGNOSTIC";
    case OperatingMode::kControlledTest:
      return "NEX16 CONTROLLED TEST";
  }
  return "UNKNOWN";
}

}  // namespace nexstar
