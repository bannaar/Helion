#include "shared/binary_protocol.h"
#include "shared/protocol.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
constexpr int kIterations = 100000;
void check(bool valid, const char* label) {
  if (!valid) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
}

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
  const std::string routineFlight = helion::flight::snapshot(state, credits, experience) + "\n" +
    helion::flight::fuelLine(state) + "\n";
  const std::string textFlight = routineFlight + helion::flight::combatStatusLine(state) + "\n";
  const auto binaryFlightPayload = binary::encodeFlightSnapshotPayload(state, credits, experience);
  check(binaryInput && binaryFlightPayload, "benchmark sample encodes");
  const auto binaryFlight = binary::encodeFrame(binary::MessageType::flightSnapshot, 1,
                                                 *binaryFlightPayload);
  check(binaryFlight.has_value(), "flight frame encodes");
  helion::flight::State verified;
  int verifiedCredits = 0, verifiedExperience = 0;
  check(binary::decodeFlightSnapshotPayload(*binaryFlightPayload, verified, verifiedCredits, verifiedExperience) &&
        verified.x == state.x && verified.weaponCooldown == state.weaponCooldown &&
        verified.fuel == state.fuel && verifiedCredits == credits && verifiedExperience == experience,
        "benchmark validates decoded semantics, not just throughput");
  check(binaryInput->size() == 17 && binaryFlight->size() == 71 &&
        binaryFlight->size() < routineFlight.size(), "typed frame sizes and representative snapshot savings");

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
      helion::flight::fuelLine(state) + "\n" + helion::flight::combatStatusLine(state) + "\n";
    checksum += encoded.size();
  });
  const double binaryFlightEncode = nanosecondsPerOperation([&] {
    const auto payload = binary::encodeFlightSnapshotPayload(state, credits, experience);
    const auto encoded = binary::encodeFrame(binary::MessageType::flightSnapshot, 1, *payload);
    checksum += encoded->size();
  });
  const auto newline = textFlight.find('\n');
  const std::string snapshotLine = textFlight.substr(0, newline);
  const std::string fuelLine = helion::flight::fuelLine(state);
  const std::string combatLine = helion::flight::combatStatusLine(state);
  const double textFlightDecode = nanosecondsPerOperation([&] {
    helion::flight::State decoded;
    int decodedCredits = 0, decodedExperience = 0;
    const bool valid = helion::flight::readSnapshot(snapshotLine, decoded, decodedCredits, decodedExperience) &&
      helion::flight::readFuelLine(fuelLine, decoded) && helion::flight::readCombatStatus(combatLine, decoded);
    check(valid && std::abs(decoded.weaponCooldown - state.weaponCooldown) <= 0.0051,
          "text benchmark round-trip within its two-decimal precision");
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
    check(valid && decodedCredits == credits, "binary benchmark round-trip");
    checksum += static_cast<std::uint64_t>(valid ? decoded.hull : 0);
  });

  constexpr std::size_t intervalMessages = 100; // Ten seconds at the current 10 Hz input cadence.
  const std::size_t textInterval = intervalMessages * (textInput.size() + routineFlight.size());
  const std::size_t binaryInterval = intervalMessages * (binaryInput->size() + binaryFlight->size());
  // ACTIVE SHIP is still emitted for every input update, even in binary mode.
  // Include its envelope cost; do not advertise codec-only savings as total wire savings.
  const std::string activeShip = "ACTIVE SHIP instance=ship-benchmark-0001 hull=SIDEWINDER display=Sidewinder "
    "manufacturer=INDEPENDENT_YARDS operator=CIVILIAN cargo-capacity=8 speed=200 acceleration=130 "
    "handling=6 hull-capacity=125 pad=SMALL mass=25 livery=independent_civilian wear=USED";
  const auto activeFrame = binary::encodeFrame(binary::MessageType::textResponse, 1, activeShip);
  check(activeFrame.has_value(), "representative active-ship metadata fits bounded envelope");
  const auto fullTextInterval = textInterval + intervalMessages * (activeShip.size() + 1);
  const auto fullBinaryInterval = binaryInterval + intervalMessages * activeFrame->size();
  check(fullBinaryInterval < fullTextInterval && checksum != 0 &&
        inputDecoder.bufferedBytes() == 0 && flightDecoder.bufferedBytes() == 0,
        "complete sampled update remains a byte win and decoders retain no completed frame");
  std::cout << "PROTOCOL_BENCH iterations=" << kIterations << " checksum=" << checksum << '\n'
            << "INPUT bytes-text=" << textInput.size() << " bytes-binary=" << binaryInput->size()
            << " encode-ns-text=" << textInputEncode << " encode-ns-binary=" << binaryInputEncode
            << " decode-ns-text=" << textInputDecode << " decode-ns-binary=" << binaryInputDecode << '\n'
            << "FLIGHT bytes-text=" << textFlight.size() << " bytes-binary=" << binaryFlight->size()
            << " encode-ns-text=" << textFlightEncode << " encode-ns-binary=" << binaryFlightEncode
            << " decode-ns-text=" << textFlightDecode << " decode-ns-binary=" << binaryFlightDecode << '\n'
            << "INTERVAL seconds=10 input-hz=10 snapshot-hz=10 codec-bytes-text=" << textInterval
            << " codec-bytes-binary=" << binaryInterval << " full-update-bytes-text=" << fullTextInterval
            << " full-update-bytes-binary=" << fullBinaryInterval << '\n'
            << "BUFFER retained-after-frame=0 maximum-frame=" << binary::kMaxFrameSize
            << " allocation-count=not-instrumented tls-overhead=excluded\n";
}
