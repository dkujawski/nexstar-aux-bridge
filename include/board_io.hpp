#pragma once

#include <Arduino.h>

#include "board_support.hpp"

namespace nexstar {

inline void ForceAuxOutputsSafe() {
  digitalWrite(BoardPins::kAuxTxEnable,
               BoardPolarity::kAuxTxDisabledLevel ? HIGH : LOW);
  digitalWrite(BoardPins::kAuxBusyAssert,
               BoardPolarity::kAuxBusyReleasedLevel ? HIGH : LOW);
}

inline void InitializeAuxOutputsSafe() {
  // Preload the output latches before changing direction. External resistors
  // hold the same safe levels while reset still leaves these pins high-Z.
  ForceAuxOutputsSafe();
  pinMode(BoardPins::kAuxTxEnable, OUTPUT);
  pinMode(BoardPins::kAuxBusyAssert, OUTPUT);
  pinMode(BoardPins::kAuxUartRx, INPUT);
  pinMode(BoardPins::kAuxBusyIn, INPUT);
  pinMode(BoardPins::kAuxUartTx, INPUT);
}

}  // namespace nexstar
