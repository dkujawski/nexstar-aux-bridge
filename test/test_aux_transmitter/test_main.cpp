#include <unity.h>

#include <array>
#include <cstddef>

#include "aux_transmitter.hpp"

namespace {

class FakeIo : public nexstar::AuxTxIo {
 public:
  bool busBusy() const override { return bus_busy; }
  void setBusyAsserted(const bool asserted) override { busy_asserted = asserted; }
  void setTxEnabled(const bool enabled) override { tx_enabled = enabled; }
  std::size_t write(const std::uint8_t*, const std::size_t size) override {
    ++writes;
    return short_write ? size - 1 : size;
  }
  bool txComplete() const override { return tx_complete; }

  bool bus_busy{false};
  bool busy_asserted{false};
  bool tx_enabled{false};
  bool tx_complete{false};
  bool short_write{false};
  std::uint32_t writes{0};
};

nexstar::AuxPacket Packet() {
  nexstar::AuxPacket packet{};
  constexpr std::array<std::uint8_t, 6> bytes{
      0x3B, 0x03, 0x04, 0x10, 0xFE, 0xEB,
  };
  packet.size = bytes.size();
  packet.origin = nexstar::PacketOrigin::kHost;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    packet.bytes[index] = bytes[index];
  }
  return packet;
}

nexstar::AuxPacket Response() {
  nexstar::AuxPacket packet{};
  constexpr std::array<std::uint8_t, 8> bytes{
      0x3B, 0x05, 0x10, 0x04, 0xFE, 0x04, 0x03, 0xE2,
  };
  packet.size = bytes.size();
  packet.origin = nexstar::PacketOrigin::kAuxBus;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    packet.bytes[index] = bytes[index];
  }
  return packet;
}

void AdvanceToDrain(nexstar::AuxTransmitter& transmitter) {
  transmitter.tick(0);    // bus wait -> claim
  transmitter.tick(1);    // assert BUSY -> delay
  transmitter.tick(101);  // delay -> enable
  transmitter.tick(102);  // enable -> write
  transmitter.tick(103);  // write -> drain
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_listen_only_and_invalid_packets_never_transmit() {
  FakeIo io;
  nexstar::AuxTransmitter transmitter(io);
  TEST_ASSERT_FALSE(
      transmitter.start(Packet(), nexstar::OperatingMode::kListenOnly,
                        nexstar::AuxTxAuthorization::kControlledTest, 0));
  TEST_ASSERT_FALSE(
      transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                        nexstar::AuxTxAuthorization::kProhibited, 0));
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);

  auto bad = Packet();
  bad.bytes[5] ^= 1;
  TEST_ASSERT_FALSE(
      transmitter.start(bad, nexstar::OperatingMode::kControlledTest,
                        nexstar::AuxTxAuthorization::kControlledTest, 0));

  auto mutating = Packet();
  mutating.bytes[4] = 0x24;
  mutating.bytes[5] = nexstar::CalculateAuxChecksum(
      mutating.bytes.data(), mutating.size - 1);
  TEST_ASSERT_FALSE(transmitter.start(
      mutating, nexstar::OperatingMode::kControlledTest,
      nexstar::AuxTxAuthorization::kControlledTest, 0));
  TEST_ASSERT_EQUAL_UINT32(1, transmitter.metrics().rejected_policy);
}

void test_success_path_releases_at_uart_drain_then_requires_echo_and_response() {
  FakeIo io;
  nexstar::AuxTransmitter transmitter(io);
  TEST_ASSERT_TRUE(
      transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                        nexstar::AuxTxAuthorization::kControlledTest, 0));
  AdvanceToDrain(transmitter);
  TEST_ASSERT_TRUE(io.tx_enabled);
  TEST_ASSERT_TRUE(io.busy_asserted);

  transmitter.tick(104);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kUartDrain),
                    static_cast<int>(transmitter.state()));
  io.tx_complete = true;
  transmitter.tick(105);
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kEchoWait),
                    static_cast<int>(transmitter.state()));

  transmitter.notifyEchoComplete();
  transmitter.tick(106);
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kResponseWait),
                    static_cast<int>(transmitter.state()));
  TEST_ASSERT_TRUE(transmitter.notifyResponse(Response()));
  transmitter.tick(107);
  TEST_ASSERT_EQUAL_UINT32(1, transmitter.completedPackets());
}

void test_response_timeout_faults_after_busy_is_safely_released() {
  FakeIo io;
  io.tx_complete = true;
  nexstar::AuxTxTiming timing{};
  timing.response_timeout_us = 10;
  nexstar::AuxTransmitter transmitter(io, timing);
  transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                    nexstar::AuxTxAuthorization::kControlledTest, 0);
  transmitter.tick(0);
  transmitter.tick(1);
  transmitter.tick(101);
  transmitter.tick(102);
  transmitter.tick(103);
  transmitter.tick(104);
  TEST_ASSERT_FALSE(io.busy_asserted);
  transmitter.notifyEchoComplete();
  transmitter.tick(105);
  transmitter.tick(115);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxFault::kResponseTimeout),
                    static_cast<int>(transmitter.lastFault()));
  TEST_ASSERT_EQUAL_UINT32(0, transmitter.completedPackets());
}

void test_contention_retries_are_bounded_then_fault_safe() {
  FakeIo io;
  io.bus_busy = true;
  nexstar::AuxTxTiming timing{};
  timing.bus_wait_timeout_us = 10;
  timing.backoff_us = 5;
  timing.maximum_attempts = 2;
  nexstar::AuxTransmitter transmitter(io, timing);
  transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                    nexstar::AuxTxAuthorization::kControlledTest, 0);

  transmitter.tick(10);
  transmitter.tick(15);
  transmitter.tick(25);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kFault),
                    static_cast<int>(transmitter.state()));
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);
}

void test_write_drain_and_echo_timeouts_force_safe_fault() {
  FakeIo io;
  io.short_write = true;
  nexstar::AuxTransmitter transmitter(io);
  transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                    nexstar::AuxTxAuthorization::kControlledTest, 0);
  AdvanceToDrain(transmitter);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kFault),
                    static_cast<int>(transmitter.state()));
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);

  transmitter.recover(200);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kIdle),
                    static_cast<int>(transmitter.state()));
}

void test_uart_drain_and_echo_timeout_paths_force_safe_fault() {
  FakeIo io;
  nexstar::AuxTxTiming timing{};
  timing.uart_drain_timeout_us = 10;
  timing.echo_timeout_us = 10;
  nexstar::AuxTransmitter transmitter(io, timing);
  transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                    nexstar::AuxTxAuthorization::kControlledTest, 0);
  AdvanceToDrain(transmitter);
  transmitter.tick(113);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kFault),
                    static_cast<int>(transmitter.state()));
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);

  transmitter.recover(200);
  io.tx_complete = true;
  transmitter.start(Packet(), nexstar::OperatingMode::kControlledTest,
                    nexstar::AuxTxAuthorization::kControlledTest, 200);
  transmitter.tick(200);
  transmitter.tick(201);
  transmitter.tick(301);
  transmitter.tick(302);
  transmitter.tick(303);
  transmitter.tick(304);
  transmitter.tick(305);
  transmitter.tick(315);
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kFault),
                    static_cast<int>(transmitter.state()));
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);
}

void test_transaction_timeout_bounds_busy_hold_and_records_fault() {
  FakeIo io;
  nexstar::AuxTxTiming timing{};
  timing.transaction_timeout_us = 25;
  timing.claim_delay_us = 100;
  nexstar::AuxTransmitter transmitter(io, timing);
  TEST_ASSERT_TRUE(transmitter.start(
      Packet(), nexstar::OperatingMode::kControlledTest,
      nexstar::AuxTxAuthorization::kControlledTest, 0));
  transmitter.tick(0);
  transmitter.tick(1);
  transmitter.tick(26);

  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxState::kFault),
                    static_cast<int>(transmitter.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(nexstar::AuxTxFault::kTransactionTimeout),
                    static_cast<int>(transmitter.lastFault()));
  TEST_ASSERT_FALSE(io.tx_enabled);
  TEST_ASSERT_FALSE(io.busy_asserted);
  TEST_ASSERT_EQUAL_UINT32(25, transmitter.metrics().maximum_busy_hold_us);
  TEST_ASSERT_EQUAL_UINT32(1, transmitter.metrics().busy_timeouts);
}

void test_one_thousand_simulated_version_transactions_complete() {
  FakeIo io;
  nexstar::AuxTransmitter transmitter(io);
  for (std::uint32_t exchange = 0; exchange < 1000; ++exchange) {
    const std::uint32_t base = exchange * 200;
    io.tx_complete = false;
    TEST_ASSERT_TRUE(transmitter.start(
        Packet(), nexstar::OperatingMode::kControlledTest,
        nexstar::AuxTxAuthorization::kControlledTest, base));
    transmitter.tick(base);
    transmitter.tick(base + 1);
    transmitter.tick(base + 101);
    transmitter.tick(base + 102);
    transmitter.tick(base + 103);
    io.tx_complete = true;
    transmitter.tick(base + 104);
    transmitter.notifyEchoComplete();
    transmitter.tick(base + 105);
    TEST_ASSERT_TRUE(transmitter.notifyResponse(Response()));
    transmitter.tick(base + 106);
  }
  TEST_ASSERT_EQUAL_UINT32(1000, transmitter.completedPackets());
  TEST_ASSERT_EQUAL_UINT32(0, transmitter.faults());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_listen_only_and_invalid_packets_never_transmit);
  RUN_TEST(
      test_success_path_releases_at_uart_drain_then_requires_echo_and_response);
  RUN_TEST(test_contention_retries_are_bounded_then_fault_safe);
  RUN_TEST(test_write_drain_and_echo_timeouts_force_safe_fault);
  RUN_TEST(test_uart_drain_and_echo_timeout_paths_force_safe_fault);
  RUN_TEST(test_transaction_timeout_bounds_busy_hold_and_records_fault);
  RUN_TEST(test_response_timeout_faults_after_busy_is_safely_released);
  RUN_TEST(test_one_thousand_simulated_version_transactions_complete);
  return UNITY_END();
}
