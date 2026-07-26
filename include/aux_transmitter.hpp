#pragma once

#include <cstddef>
#include <cstdint>

#include "aux_protocol.hpp"
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
  kTxDisable,
  kEchoWait,
  kBusyRelease,
  kBackoff,
  kFault,
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
             const std::uint32_t now_us) {
    if (state_ != AuxTxState::kIdle || !MayTransmitAux(mode) ||
        !HasValidAuxChecksum(packet)) {
      return false;
    }
    packet_ = packet;
    attempts_ = 0;
    transition(AuxTxState::kBusWait, now_us);
    return true;
  }

  void tick(const std::uint32_t now_us) {
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
          enterFault(now_us);
        } else {
          transition(AuxTxState::kUartDrain, now_us);
        }
        return;

      case AuxTxState::kUartDrain:
        if (io_.txComplete()) {
          transition(AuxTxState::kTxDisable, now_us);
        } else if (elapsed(now_us) >= timing_.uart_drain_timeout_us) {
          enterFault(now_us);
        }
        return;

      case AuxTxState::kTxDisable:
        io_.setTxEnabled(false);
        transition(AuxTxState::kEchoWait, now_us);
        return;

      case AuxTxState::kEchoWait:
        if (echo_complete_) {
          transition(AuxTxState::kBusyRelease, now_us);
        } else if (elapsed(now_us) >= timing_.echo_timeout_us) {
          enterFault(now_us);
        }
        return;

      case AuxTxState::kBusyRelease:
        io_.setBusyAsserted(false);
        ++completed_packets_;
        transition(AuxTxState::kIdle, now_us);
        return;

      case AuxTxState::kBackoff:
        if (elapsed(now_us) >= timing_.backoff_us) {
          transition(AuxTxState::kBusWait, now_us);
        }
        return;
    }
  }

  void notifyEchoComplete() { echo_complete_ = true; }

  void recover(const std::uint32_t now_us) {
    forceSafe();
    transition(AuxTxState::kIdle, now_us);
  }

  [[nodiscard]] AuxTxState state() const { return state_; }
  [[nodiscard]] std::uint32_t completedPackets() const {
    return completed_packets_;
  }
  [[nodiscard]] std::uint32_t faults() const { return faults_; }

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
    }
  }

  void retryOrFault(const std::uint32_t now_us) {
    ++attempts_;
    if (attempts_ >= timing_.maximum_attempts) {
      enterFault(now_us);
    } else {
      forceSafe();
      transition(AuxTxState::kBackoff, now_us);
    }
  }

  void enterFault(const std::uint32_t now_us) {
    forceSafe();
    ++faults_;
    transition(AuxTxState::kFault, now_us);
  }

  void forceSafe() {
    io_.setTxEnabled(false);
    io_.setBusyAsserted(false);
  }

  AuxTxIo& io_;
  AuxTxTiming timing_;
  AuxPacket packet_{};
  AuxTxState state_{AuxTxState::kIdle};
  std::uint32_t state_entered_us_{0};
  std::uint32_t completed_packets_{0};
  std::uint32_t faults_{0};
  std::uint8_t attempts_{0};
  bool echo_complete_{false};
};

}  // namespace nexstar
