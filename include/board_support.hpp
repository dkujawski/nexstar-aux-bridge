#pragma once

#include <array>
#include <cstdint>

namespace nexstar {

// Heltec WiFi LoRa 32 V3 / ESP32-S3 pin contract. AUX assignments are
// candidates until the checks in docs/pin-allocation.md have passed.
struct BoardPins {
  static constexpr std::uint8_t kHostUartTx = 43;
  static constexpr std::uint8_t kHostUartRx = 44;

  static constexpr std::uint8_t kAuxUartRx = 4;
  static constexpr std::uint8_t kAuxUartTx = 5;
  static constexpr std::uint8_t kAuxBusyIn = 6;
  static constexpr std::uint8_t kAuxBusyAssert = 7;
  static constexpr std::uint8_t kAuxTxEnable = 2;

  static constexpr std::uint8_t kBatteryAdc = 1;
  static constexpr std::uint8_t kVextControl = 36;
  static constexpr std::uint8_t kLed = 35;
  static constexpr std::uint8_t kOledSda = 17;
  static constexpr std::uint8_t kOledScl = 18;
  static constexpr std::uint8_t kOledReset = 21;

  static constexpr std::array<std::uint8_t, 7> kLoraPins{
      8, 9, 10, 11, 12, 13, 14,
  };
};

struct BoardPolarity {
  // The proposed 74AHCT125 /OE connection is active-low. HIGH is safe.
  static constexpr bool kAuxTxEnableActiveHigh = false;
  static constexpr bool kAuxTxDisabledLevel = true;

  // The proposed open-drain BUSY MOSFET gate is active-high. LOW releases BUSY.
  static constexpr bool kAuxBusyAssertActiveHigh = true;
  static constexpr bool kAuxBusyReleasedLevel = false;

  // External level shifting must present true when the shared BUSY wire is low.
  static constexpr bool kAuxBusyInputActiveLow = true;
};

constexpr bool AuxPinsAreDistinct() {
  constexpr std::array<std::uint8_t, 5> pins{
      BoardPins::kAuxUartRx,
      BoardPins::kAuxUartTx,
      BoardPins::kAuxBusyIn,
      BoardPins::kAuxBusyAssert,
      BoardPins::kAuxTxEnable,
  };
  for (std::size_t first = 0; first < pins.size(); ++first) {
    for (std::size_t second = first + 1; second < pins.size(); ++second) {
      if (pins[first] == pins[second]) {
        return false;
      }
    }
  }
  return true;
}

constexpr bool IsBoardOwnedPin(const std::uint8_t pin) {
  if (pin == BoardPins::kHostUartTx || pin == BoardPins::kHostUartRx ||
      pin == BoardPins::kBatteryAdc || pin == BoardPins::kVextControl ||
      pin == BoardPins::kLed || pin == BoardPins::kOledSda ||
      pin == BoardPins::kOledScl || pin == BoardPins::kOledReset) {
    return true;
  }
  for (const std::uint8_t lora_pin : BoardPins::kLoraPins) {
    if (pin == lora_pin) {
      return true;
    }
  }
  return false;
}

constexpr bool AuxPinsAvoidBoardFunctions() {
  return !IsBoardOwnedPin(BoardPins::kAuxUartRx) &&
         !IsBoardOwnedPin(BoardPins::kAuxUartTx) &&
         !IsBoardOwnedPin(BoardPins::kAuxBusyIn) &&
         !IsBoardOwnedPin(BoardPins::kAuxBusyAssert) &&
         !IsBoardOwnedPin(BoardPins::kAuxTxEnable);
}

static_assert(AuxPinsAreDistinct(), "AUX pins must be unique");
static_assert(AuxPinsAvoidBoardFunctions(), "AUX pins conflict with Heltec peripherals");

}  // namespace nexstar
