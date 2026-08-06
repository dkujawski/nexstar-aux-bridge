#pragma once

#include <array>
#include <cstdint>

namespace nexstar {

// Generic 30-pin ESP32 DevKit V1 with an ESP32-WROOM-32 module and CP2102.
// AUX assignments remain electrically inactive until the NEX-14 bench gates
// in docs/pin-allocation.md have passed.
struct BoardPins {
  // UART0 is hard-wired to the onboard CP2102.
  static constexpr std::uint8_t kHostUartTx = 1;
  static constexpr std::uint8_t kHostUartRx = 3;

  // UART2 and ordinary GPIOs reserved for the external AUX interface.
  static constexpr std::uint8_t kAuxUartRx = 16;
  static constexpr std::uint8_t kAuxUartTx = 17;
  // RJ12 pin 1 is CTS from the mount/interconnect. It is active-low: LOW
  // grants permission to transmit; it does not mean the bus is busy.
  static constexpr std::uint8_t kAuxCtsIn = 34;
  static constexpr std::uint8_t kAuxBusyAssert = 26;
  static constexpr std::uint8_t kAuxTxEnable = 27;

  // External 0.96-inch 80x160 ST7735S display. The display remains disabled
  // until NEX-21 verifies its electrical and initialization behavior.
  static constexpr std::uint8_t kTftClock = 18;
  static constexpr std::uint8_t kTftMosi = 23;
  static constexpr std::uint8_t kTftChipSelect = 33;
  static constexpr std::uint8_t kTftDataCommand = 32;
  static constexpr std::uint8_t kTftReset = 25;
  static constexpr std::uint8_t kTftBacklight = 13;
};

struct BoardPolarity {
  // The proposed 74AHCT125 /OE connection is active-low. HIGH is safe.
  static constexpr bool kAuxTxEnableActiveHigh = false;
  static constexpr bool kAuxTxDisabledLevel = true;

  // BUSY uses a second SN74AHCT125 channel with A tied LOW. Its active-low
  // /OE asserts BUSY when LOW and releases the output to high impedance when
  // HIGH.
  static constexpr bool kAuxBusyAssertActiveHigh = false;
  static constexpr bool kAuxBusyReleasedLevel = true;

  // The conditioned RJ12 pin-1 CTS input is asserted (clear-to-send) LOW.
  static constexpr bool kAuxCtsActiveLow = true;

  // Provisional until NEX-21 bench validation.
  static constexpr bool kTftBacklightActiveHigh = true;
};

constexpr bool PinsAreDistinct() {
  constexpr std::array<std::uint8_t, 11> pins{
      BoardPins::kAuxUartRx,
      BoardPins::kAuxUartTx,
      BoardPins::kAuxCtsIn,
      BoardPins::kAuxBusyAssert,
      BoardPins::kAuxTxEnable,
      BoardPins::kTftClock,
      BoardPins::kTftMosi,
      BoardPins::kTftChipSelect,
      BoardPins::kTftDataCommand,
      BoardPins::kTftReset,
      BoardPins::kTftBacklight,
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

constexpr bool IsHostUartPin(const std::uint8_t pin) {
  return pin == BoardPins::kHostUartTx || pin == BoardPins::kHostUartRx;
}

constexpr bool IsStrappingPin(const std::uint8_t pin) {
  return pin == 0 || pin == 2 || pin == 5 || pin == 12 || pin == 15;
}

constexpr bool PeripheralPinsAreSafe() {
  constexpr std::array<std::uint8_t, 11> pins{
      BoardPins::kAuxUartRx,      BoardPins::kAuxUartTx,
      BoardPins::kAuxCtsIn,      BoardPins::kAuxBusyAssert,
      BoardPins::kAuxTxEnable,   BoardPins::kTftClock,
      BoardPins::kTftMosi,       BoardPins::kTftChipSelect,
      BoardPins::kTftDataCommand, BoardPins::kTftReset,
      BoardPins::kTftBacklight,
  };
  for (const std::uint8_t pin : pins) {
    if (IsHostUartPin(pin) || IsStrappingPin(pin) ||
        (pin >= 6 && pin <= 11)) {
      return false;
    }
  }
  return true;
}

static_assert(PinsAreDistinct(), "AUX and TFT pins must be unique");
static_assert(PeripheralPinsAreSafe(),
              "Peripheral pins conflict with UART0, flash, or boot straps");

}  // namespace nexstar
