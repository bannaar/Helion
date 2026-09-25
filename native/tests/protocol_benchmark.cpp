#include "shared/binary_protocol.h"
#include "shared/protocol.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
constexpr int kIterations = 100000;

template <typename Function>
double nanosecondsPerOperation(Function function) {
  const auto start = std::chrono::steady_clock::now();
  for (int iteration = 0; iteration < kIterations; ++iteration) function();
  const auto elapsed = std::chrono::duration<double, std::nano>(
    std::chrono::steady_clock::now() - start).count();
  return elapsed / kIterations;
}
}

int main() {
  namespace binary = helion::protocol::binary;
  helion::flight::State state;
  state.x = 312.125; state.y = -184.75; state.vx = 41.5; state.vy = -22.25;
  state.yaw = 1.125; state.docked = false; state.cargo = 5; state.food = 2;
  state.parts = 1; state.station = 0; state.cooldown = 0.625; state.hull = 93;
  state.maxHull = 125; state.fuel = 68.5; state.maxFuel = 110;
  state.weaponCooldown = 0.375;
  constexpr int credits = 18425;
  constexpr int experience = 735;

  const std::string textInput = "INPUT 1 -1 0\n";
  const auto binaryInputPayload = binary::encodeInputPayload({1, -1, 0});
  const auto binaryInput = binary::encodeFrame(binary::MessageType::input, 1, binaryInputPayload);
  const std::string textFlight = helion::flight::snapshot(state, credits, experience) + "\n" +
    helion::flight::fuelLine(state) + "\n";
  const auto binaryFlightPayload = binary::encodeFlightSnapshotPayload(state, credits, experience);
  const auto binaryFlight = binary::encodeFrame(binary::MessageType::flightSnapshot, 1,
                                                 *binaryFlightPayload);
  if (!binaryInput || !binaryFlightPayload || !binaryFlight) return 1;

  std::uint64_t checksum = 0;
  const double textInputEncode = nanosecondsPerOperation([&] {
    const std::string encoded = "INPUT " + std::to_string(state.input.thrust + 1) + " -1 0\n";
    checksum += encoded.size();
  });
  const double binaryInputEncode = nanosecondsPerOperation([&] {
    const auto payload = binary::encodeInputPayload({1, -1, 0});
    const auto encoded = binary::encodeFrame(binary::MessageType::input, 1, payload);
    checksum += encoded->size();
  });
  const double textInputDecode = nanosecondsPerOperation([&] {
    const auto request = helion::protocol::parseRequest("INPUT 1 -1 0");
    checksum += static_cast<std::uint64_t>(request.thrust + request.brake + request.turn + 1);
  });
  binary::Decoder inputDecoder;
  const double binaryInputDecode = nanosecondsPerOperation([&] {
    const auto events = inputDecoder.feed(*binaryInput);
    const auto input = binary::decodeInputPayload(std::string_view(
      reinterpret_cast<const char*>(events[0].frame.payload.data()), events[0].frame.payload.size()));
    checksum += static_cast<std::uint64_t>(input->thrust + input->brake + input->turn + 1);
  });

  const double textFlightEncode = nanosecondsPerOperation([&] {
    const auto encoded = helion::flight::snapshot(state, credits, experience) + "\n" +
      helion::flight::fuelLine(state) + "\n";
    checksum += encoded.size();
  });
  const double binaryFlightEncode = nanosecondsPerOperation([&] {
    const auto payload = binary::encodeFlightSnapshotPayload(state, credits, experience);
    const auto encoded = binary::encodeFrame(binary::MessageType::flightSnapshot, 1, *payload);
    checksum += encoded->size();
  });
  const auto newline = textFlight.find('\n');
  const std::string snapshotLine = textFlight.substr(0, newline);
  const std::string fuelLine = textFlight.substr(newline + 1, textFlight.size() - newline - 2);
  const double textFlightDecode = nanosecondsPerOperation([&] {
    helion::flight::State decoded;
    int decodedCredits = 0, decodedExperience = 0;
    const bool valid = helion::flight::readSnapshot(snapshotLine, decoded, decodedCredits, decodedExperience) &&
      helion::flight::readFuelLine(fuelLine, decoded);
    checksum += static_cast<std::uint64_t>(valid ? decoded.hull : 0);
  });
  binary::Decoder flightDecoder;
  const double binaryFlightDecode = nanosecondsPerOperation([&] {
    const auto events = flightDecoder.feed(*binaryFlight);
    helion::flight::State decoded;
    int decodedCredits = 0, decodedExperience = 0;
    const bool valid = binary::decodeFlightSnapshotPayload(std::string_view(
      reinterpret_cast<const char*>(events[0].frame.payload.data()), events[0].frame.payload.size()),
      decoded, decodedCredits, decodedExperience);
    checksum += static_cast<std::uint64_t>(valid ? decoded.hull : 0);
  });

  constexpr std::size_t intervalMessages = 100; // Ten seconds at the current 10 Hz input cadence.
  const std::size_t textInterval = intervalMessages * (textInput.size() + textFlight.size());
  const std::size_t binaryInterval = intervalMessages * (binaryInput->size() + binaryFlight->size());
  std::cout << "PROTOCOL_BENCH iterations=" << kIterations << " checksum=" << checksum << '\n'
            << "INPUT bytes-text=" << textInput.size() << " bytes-binary=" << binaryInput->size()
            << " encode-ns-text=" << textInputEncode << " encode-ns-binary=" << binaryInputEncode
            << " decode-ns-text=" << textInputDecode << " decode-ns-binary=" << binaryInputDecode << '\n'
            << "FLIGHT bytes-text=" << textFlight.size() << " bytes-binary=" << binaryFlight->size()
            << " encode-ns-text=" << textFlightEncode << " encode-ns-binary=" << binaryFlightEncode
            << " decode-ns-text=" << textFlightDecode << " decode-ns-binary=" << binaryFlightDecode << '\n'
            << "INTERVAL seconds=10 input-hz=10 snapshot-hz=10 bytes-text=" << textInterval
            << " bytes-binary=" << binaryInterval << '\n';
}
