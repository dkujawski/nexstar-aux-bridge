#pragma once

#include <cstddef>
#include <cstdint>

#include "aux_protocol.hpp"
#include "packet_router.hpp"

namespace nexstar {

// The USB-UART connection has no reliable physical disconnect indication on
// ESP32. This deliberately small interface makes the byte stream testable and
// keeps all I/O nonblocking: a zero-byte write simply defers the remainder.
class HostByteStream {
 public:
  virtual ~HostByteStream() = default;
  virtual int available() const = 0;
  virtual int read() = 0;
  virtual std::size_t write(const std::uint8_t* bytes, std::size_t size) = 0;
};

struct HostTransportMetrics {
  std::uint32_t received_bytes{0};
  std::uint32_t decoded_packets{0};
  std::uint32_t malformed_packets{0};
  std::uint32_t inter_byte_timeouts{0};
  std::uint32_t rejected_packets{0};
  std::uint32_t transmitted_bytes{0};
  std::uint32_t write_backpressure{0};
};

template <std::size_t AuxTxCapacity = 8, std::size_t HostTxCapacity = 8>
class HostTransport {
 public:
  HostTransport(HostByteStream& stream,
                PacketRouter<AuxTxCapacity, HostTxCapacity>& router)
      : stream_(stream), router_(router) {}

  // Limits each call so USB input or a stalled host can never monopolize AUX
  // service. Call it frequently from the bridge loop.
  void service(const std::uint32_t now_ms,
               const std::size_t max_read_bytes = 64) {
    if (decoder_.expire(now_ms)) {
      ++metrics_.inter_byte_timeouts;
    }
    for (std::size_t count = 0;
         count < max_read_bytes && stream_.available() > 0; ++count) {
      const int value = stream_.read();
      if (value < 0) {
        break;
      }
      ++metrics_.received_bytes;
      AuxPacket packet{};
      const DecodeResult result = decoder_.feed(
          static_cast<std::uint8_t>(value), now_ms, PacketOrigin::kHost, packet);
      if (result == DecodeResult::kRejected) {
        ++metrics_.malformed_packets;
      } else if (result == DecodeResult::kPacket) {
        ++metrics_.decoded_packets;
        if (!router_.routeFromHost(packet)) {
          ++metrics_.rejected_packets;
        }
      }
    }
    drainHostOutput();
  }

  [[nodiscard]] const HostTransportMetrics& metrics() const { return metrics_; }
  [[nodiscard]] std::size_t pendingOutputBytes() const {
    return output_active_ ? output_.size - output_offset_ : 0;
  }

 private:
  void drainHostOutput() {
    if (!output_active_) {
      if (!router_.takeForHost(output_)) {
        return;
      }
      output_offset_ = 0;
      output_active_ = true;
    }

    const std::size_t remaining = output_.size - output_offset_;
    const std::size_t written = stream_.write(output_.bytes.data() + output_offset_,
                                              remaining);
    if (written == 0) {
      ++metrics_.write_backpressure;
      return;
    }
    const std::size_t accepted = written > remaining ? remaining : written;
    output_offset_ += accepted;
    metrics_.transmitted_bytes += static_cast<std::uint32_t>(accepted);
    if (output_offset_ == output_.size) {
      output_active_ = false;
    }
  }

  HostByteStream& stream_;
  PacketRouter<AuxTxCapacity, HostTxCapacity>& router_;
  AuxStreamDecoder decoder_;
  HostTransportMetrics metrics_{};
  AuxPacket output_{};
  std::size_t output_offset_{0};
  bool output_active_{false};
};

}  // namespace nexstar
