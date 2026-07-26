#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aux_protocol.hpp"

namespace nexstar {

template <typename Item, std::size_t Capacity>
class BoundedQueue {
 public:
  static_assert(Capacity > 0, "Queue capacity must be positive");

  bool push(const Item& item) {
    if (size_ == Capacity) {
      ++overflows_;
      return false;
    }
    items_[(head_ + size_) % Capacity] = item;
    ++size_;
    return true;
  }

  bool pop(Item& item) {
    if (size_ == 0) {
      return false;
    }
    item = items_[head_];
    head_ = (head_ + 1) % Capacity;
    --size_;
    return true;
  }

  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] std::size_t capacity() const { return Capacity; }
  [[nodiscard]] std::uint32_t overflows() const { return overflows_; }

 private:
  std::array<Item, Capacity> items_{};
  std::size_t head_{0};
  std::size_t size_{0};
  std::uint32_t overflows_{0};
};

template <std::size_t AuxTxCapacity = 8, std::size_t HostTxCapacity = 8>
class PacketRouter {
 public:
  bool routeFromHost(const AuxPacket& packet) {
    if (packet.origin != PacketOrigin::kHost || !HasValidAuxChecksum(packet)) {
      ++invalid_host_packets_;
      return false;
    }
    return aux_tx_.push(packet);
  }

  bool routeFromAux(const AuxPacket& packet) {
    if (packet.origin != PacketOrigin::kAuxBus || !HasValidAuxChecksum(packet)) {
      ++invalid_aux_packets_;
      return false;
    }
    return host_tx_.push(packet);
  }

  bool takeForAux(AuxPacket& packet) { return aux_tx_.pop(packet); }
  bool takeForHost(AuxPacket& packet) { return host_tx_.pop(packet); }

  [[nodiscard]] const BoundedQueue<AuxPacket, AuxTxCapacity>& auxQueue() const {
    return aux_tx_;
  }
  [[nodiscard]] const BoundedQueue<AuxPacket, HostTxCapacity>& hostQueue() const {
    return host_tx_;
  }
  [[nodiscard]] std::uint32_t invalidHostPackets() const {
    return invalid_host_packets_;
  }
  [[nodiscard]] std::uint32_t invalidAuxPackets() const {
    return invalid_aux_packets_;
  }

 private:
  BoundedQueue<AuxPacket, AuxTxCapacity> aux_tx_{};
  BoundedQueue<AuxPacket, HostTxCapacity> host_tx_{};
  std::uint32_t invalid_host_packets_{0};
  std::uint32_t invalid_aux_packets_{0};
};

}  // namespace nexstar
