#pragma once

#include <cstddef>
#include <cstdint>

#include "controlled_aux_query.hpp"
#include "operating_mode.hpp"

namespace nexstar {

enum class AuxTxState : std::uint8_t {
  kIdle,
  kBusWait,
  kBusyClaim,
  kClaimDelay,
  kTxEnable,
  kWrite,
  kUartDrain,
  kEchoWait,
  kResponseWait,
  kBackoff,
  kFault,
};

enum class AuxTxAuthorization : std::uint8_t {
  kProhibited,
  kControlledTest,
  kUsbBridge,
};

enum class AuxTxFault : std::uint8_t {
  kNone,
  kContentionTimeout,
  kWriteFailure,
  kUartDrainTimeout,
  kEchoTimeout,
  kTransactionTimeout,
  kResponseTimeout,
};

struct AuxTxMetrics {
  std::uint32_t start_requests{0};
  std::uint32_t rejected_not_idle{0};
  std::uint32_t rejected_unauthorized{0};
  std::uint32_t rejected_policy{0};
  std::uint32_t rejected_checksum{0};
  std::uint32_t contention_retries{0};
  std::uint32_t completed_packets{0};
  std::uint32_t faults{0};
  std::uint32_t busy_timeouts{0};
  std::uint32_t recoveries{0};
  std::uint32_t maximum_busy_hold_us{0};
};

class AuxTxIo {
 public:
  virtual ~AuxTxIo() = default;
  virtual bool busBusy() const = 0;
  virtual void setBusyAsserted(bool asserted) = 0;
  virtual void setTxEnabled(bool enabled) = 0;
  virtual std::size_t write(const std::uint8_t* bytes, std::size_t size) = 0;
  virtual bool txComplete() const = 0;
};

struct AuxTxTiming {
  std::uint32_t claim_delay_us{100};
  std::uint32_t bus_wait_timeout_us{5000};
  std::uint32_t uart_drain_timeout_us{20000};
  std::uint32_t echo_timeout_us{150000};
  std::uint32_t response_timeout_us{250000};
  std::uint32_t transaction_timeout_us{175000};
  std::uint32_t backoff_us{2000};
  std::uint8_t maximum_attempts{3};
};

class AuxTransmitter {
 public:
  explicit AuxTransmitter(AuxTxIo& io, const AuxTxTiming timing = {})
      : io_(io), timing_(timing) {
    forceSafe();
  }

  bool start(const AuxPacket& packet, const OperatingMode mode,
             const AuxTxAuthorization authorization,
             const std::uint32_t now_us) {
    ++metrics_.start_requests;
    if (state_ != AuxTxState::kIdle) {
      ++metrics_.rejected_not_idle;
      return false;
    }
    const bool controlled = mode == OperatingMode::kControlledTest &&
                            authorization == AuxTxAuthorization::kControlledTest;
    const bool bridge = mode == OperatingMode::kUsbBridge &&
                        authorization == AuxTxAuthorization::kUsbBridge;
    if (!MayTransmitAux(mode) || (!controlled && !bridge)) {
      ++metrics_.rejected_unauthorized;
      return false;
    }
    if (!HasValidAuxChecksum(packet)) {
      ++metrics_.rejected_checksum;
      return false;
    }
    if (controlled && !IsControlledReadOnlyQuery(packet)) {
      ++metrics_.rejected_policy;
      return false;
    }
    packet_ = packet;
    controlled_transaction_ = controlled;
    attempts_ = 0;
    transition(AuxTxState::kBusWait, now_us);
    return true;
  }

  void tick(const std::uint32_t now_us) {
    if (busy_claimed_ &&
        Delta(now_us, busy_claimed_us_) >= timing_.transaction_timeout_us) {
      enterFault(now_us, AuxTxFault::kTransactionTimeout);
      return;
    }

    switch (state_) {
      case AuxTxState::kIdle:
      case AuxTxState::kFault:
        return;

      case AuxTxState::kBusWait:
        if (!io_.busBusy()) {
          transition(AuxTxState::kBusyClaim, now_us);
        } else if (elapsed(now_us) >= timing_.bus_wait_timeout_us) {
          retryOrFault(now_us);
        }
        return;

      case AuxTxState::kBusyClaim:
        io_.setBusyAsserted(true);
        busy_claimed_ = true;
        busy_claimed_us_ = now_us;
        transition(AuxTxState::kClaimDelay, now_us);
        return;

      case AuxTxState::kClaimDelay:
        if (elapsed(now_us) >= timing_.claim_delay_us) {
          transition(AuxTxState::kTxEnable, now_us);
        }
        return;

      case AuxTxState::kTxEnable:
        io_.setTxEnabled(true);
        transition(AuxTxState::kWrite, now_us);
        return;

      case AuxTxState::kWrite:
        if (io_.write(packet_.bytes.data(), packet_.size) != packet_.size) {
          enterFault(now_us, AuxTxFault::kWriteFailure);
        } else {
          transition(AuxTxState::kUartDrain, now_us);
        }
        return;

      case AuxTxState::kUartDrain:
        if (io_.txComplete()) {
          // Once the UART confirms that the final stop bit has drained, the
          // bridge no longer needs to own the bus. Disable TX and release
          // BUSY immediately; the responder can start before the echoed byte
          // reaches the application-level RX queue.
          io_.setTxEnabled(false);
          releaseBusy(now_us);
          if (controlled_transaction_ && echo_complete_) {
            transition(AuxTxState::kResponseWait, now_us);
          } else {
            transition(AuxTxState::kEchoWait, now_us);
          }
        } else if (elapsed(now_us) >= timing_.uart_drain_timeout_us) {
          enterFault(now_us, AuxTxFault::kUartDrainTimeout);
        }
        return;

      case AuxTxState::kEchoWait:
        if (echo_complete_) {
          if (controlled_transaction_) {
            transition(AuxTxState::kResponseWait, now_us);
          } else {
            ++metrics_.completed_packets;
            transition(AuxTxState::kIdle, now_us);
          }
        } else if (elapsed(now_us) >= timing_.echo_timeout_us) {
          enterFault(now_us, AuxTxFault::kEchoTimeout);
        }
        return;

      case AuxTxState::kResponseWait:
        if (response_complete_) {
          ++metrics_.completed_packets;
          transition(AuxTxState::kIdle, now_us);
        } else if (elapsed(now_us) >= timing_.response_timeout_us) {
          enterFault(now_us, AuxTxFault::kResponseTimeout);
        }
        return;

      case AuxTxState::kBackoff:
        if (elapsed(now_us) >= timing_.backoff_us) {
          transition(AuxTxState::kBusWait, now_us);
        }
        return;
    }
  }

  void notifyEchoComplete() { echo_complete_ = true; }

  bool notifyResponse(const AuxPacket& response) {
    if (!controlled_transaction_ || state_ != AuxTxState::kResponseWait ||
        !IsControlledVersionResponse(packet_, response)) {
      return false;
    }
    response_complete_ = true;
    return true;
  }

  void recover(const std::uint32_t now_us) {
    forceSafe();
    last_fault_ = AuxTxFault::kNone;
    ++metrics_.recoveries;
    transition(AuxTxState::kIdle, now_us);
  }

  [[nodiscard]] AuxTxState state() const { return state_; }
  [[nodiscard]] std::uint32_t completedPackets() const {
    return metrics_.completed_packets;
  }
  [[nodiscard]] std::uint32_t faults() const { return metrics_.faults; }
  [[nodiscard]] AuxTxFault lastFault() const { return last_fault_; }
  [[nodiscard]] const AuxTxMetrics& metrics() const { return metrics_; }
  [[nodiscard]] const AuxTxTiming& timing() const { return timing_; }

 private:
  static constexpr std::uint32_t Delta(const std::uint32_t now,
                                       const std::uint32_t then) {
    return now - then;
  }

  [[nodiscard]] std::uint32_t elapsed(const std::uint32_t now_us) const {
    return Delta(now_us, state_entered_us_);
  }

  void transition(const AuxTxState state, const std::uint32_t now_us) {
    state_ = state;
    state_entered_us_ = now_us;
    if (state == AuxTxState::kBusWait) {
      echo_complete_ = false;
      response_complete_ = false;
    }
  }

  void retryOrFault(const std::uint32_t now_us) {
    ++attempts_;
    if (attempts_ >= timing_.maximum_attempts) {
      enterFault(now_us, AuxTxFault::kContentionTimeout);
    } else {
      ++metrics_.contention_retries;
      forceSafe();
      transition(AuxTxState::kBackoff, now_us);
    }
  }

  void enterFault(const std::uint32_t now_us, const AuxTxFault fault) {
    recordBusyHold(now_us);
    forceSafe();
    last_fault_ = fault;
    ++metrics_.faults;
    if (fault == AuxTxFault::kTransactionTimeout) {
      ++metrics_.busy_timeouts;
    }
    transition(AuxTxState::kFault, now_us);
  }

  void recordBusyHold(const std::uint32_t now_us) {
    if (!busy_claimed_) {
      return;
    }
    const std::uint32_t held_us = Delta(now_us, busy_claimed_us_);
    if (held_us > metrics_.maximum_busy_hold_us) {
      metrics_.maximum_busy_hold_us = held_us;
    }
    busy_claimed_ = false;
  }

  void releaseBusy(const std::uint32_t now_us) {
    recordBusyHold(now_us);
    io_.setBusyAsserted(false);
  }

  void forceSafe() {
    io_.setTxEnabled(false);
    io_.setBusyAsserted(false);
    busy_claimed_ = false;
  }

  AuxTxIo& io_;
  AuxTxTiming timing_;
  AuxPacket packet_{};
  AuxTxState state_{AuxTxState::kIdle};
  std::uint32_t state_entered_us_{0};
  AuxTxMetrics metrics_{};
  AuxTxFault last_fault_{AuxTxFault::kNone};
  std::uint32_t busy_claimed_us_{0};
  std::uint8_t attempts_{0};
  bool echo_complete_{false};
  bool response_complete_{false};
  bool busy_claimed_{false};
  bool controlled_transaction_{false};
};

}  // namespace nexstar
