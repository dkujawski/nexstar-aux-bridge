#pragma once

#include <cstdint>

#include "aux_protocol.hpp"

namespace nexstar {

// NEX-16 initially permits only the payload-free GET_VERSION command. This
// builder does not authorize transmission; AuxTransmitter separately requires
// an explicit controlled-test authorization on every start request.
constexpr std::uint8_t kAuxCommandGetVersion = 0xFE;

constexpr AuxPacket BuildControlledVersionQuery(const std::uint8_t source,
                                                const std::uint8_t destination) {
  AuxPacket packet{};
  packet.bytes[0] = kAuxPreamble;
  packet.bytes[1] = 3;
  packet.bytes[2] = source;
  packet.bytes[3] = destination;
  packet.bytes[4] = kAuxCommandGetVersion;
  packet.bytes[5] = CalculateAuxChecksum(packet.bytes.data(), 5);
  packet.size = 6;
  packet.origin = PacketOrigin::kHost;
  return packet;
}

constexpr bool IsControlledReadOnlyQuery(const AuxPacket& packet) {
  return HasValidAuxChecksum(packet) && packet.origin == PacketOrigin::kHost &&
         packet.size == 6 && packet.messageLength() == 3 &&
         packet.bytes[4] == kAuxCommandGetVersion;
}

}  // namespace nexstar
