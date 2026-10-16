#include "shared/binary_protocol.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {
void check(bool condition, const char* label) {
  if (!condition) { std::cerr << "FAILED: " << label << '\n'; std::exit(1); }
}
}

int main() {
  namespace binary = helion::protocol::binary;
  using binary::DecodeKind;
  using binary::MessageType;

  const auto inputPayload = binary::encodeInputPayload({1, -1, 0});
  check(inputPayload.size() == 3, "valid input uses three explicit bytes");
  const auto encoded = binary::encodeFrame(MessageType::input, 0x01020304, inputPayload);
  check(encoded && encoded->size() == binary::kHeaderSize + 3 && (*encoded)[0] == 'H' &&
        (*encoded)[1] == 'B' && static_cast<unsigned char>((*encoded)[2]) == binary::kVersion &&
        static_cast<unsigned char>((*encoded)[3]) == static_cast<unsigned char>(MessageType::input) &&
        static_cast<unsigned char>((*encoded)[6]) == 0 && static_cast<unsigned char>((*encoded)[9]) == 3 &&
        static_cast<unsigned char>((*encoded)[10]) == 1 && static_cast<unsigned char>((*encoded)[13]) == 4,
        "header layout uses explicit magic version type flags length and big-endian request id");

  binary::Decoder partial;
  for (std::size_t i = 0; i + 1 < encoded->size(); ++i)
    check(partial.feed(std::string_view(encoded->data() + i, 1)).empty(),
          "partial binary frame waits without speculative decode");
  auto events = partial.feed(std::string_view(encoded->data() + encoded->size() - 1, 1));
  check(events.size() == 1, "final byte completes exactly one binary frame");
  const auto decodedInput = binary::decodeInputPayload(std::string_view(
    reinterpret_cast<const char*>(events[0].frame.payload.data()), events[0].frame.payload.size()));
  check(events[0].kind == DecodeKind::frame && events[0].frame.type == MessageType::input &&
        events[0].frame.requestId == 0x01020304 &&
        decodedInput && decodedInput->thrust == 1 && decodedInput->turn == -1 && decodedInput->brake == 0,
        "partial frame reconstructs exact typed input");

  const auto text = binary::encodeFrame(MessageType::textRequest, 5, "PROFILE");
  binary::Decoder multiple;
  events = multiple.feed(*encoded + *text);
  check(events.size() == 2 && events[0].frame.type == MessageType::input &&
        events[1].frame.type == MessageType::textRequest && events[1].frame.requestId == 5,
        "multiple frames in one TLS read decode independently");

  binary::Decoder truncated;
  check(truncated.feed(std::string_view(encoded->data(), 7)).empty() &&
        truncated.finish() && truncated.failed(), "truncated header fails closed at end of stream");
  for (std::size_t split = 0; split <= encoded->size(); ++split) {
    binary::Decoder fragmented;
    auto first = fragmented.feed(std::string_view(*encoded).substr(0, split));
    auto second = fragmented.feed(std::string_view(*encoded).substr(split));
    check(first.size() + second.size() == 1 && !fragmented.finish() && !fragmented.failed(),
          "every header/payload fragmentation boundary yields exactly one complete frame");
    if (split > 0 && split < encoded->size()) {
      binary::Decoder incomplete;
      check(incomplete.feed(std::string_view(*encoded).substr(0, split)).empty(),
            "incomplete header or payload never emits a request");
      const auto end = incomplete.finish();
      check(end && end->kind == DecodeKind::truncated && incomplete.failed() &&
            incomplete.bufferedBytes() == 0 && incomplete.feed(*encoded).empty() && !incomplete.finish(),
            "truncation failure is terminal and releases buffered bytes");
    }
  }

  auto corrupt = [&](std::size_t index, unsigned char value, DecodeKind expected) {
    std::string bytes = *encoded;
    bytes[index] = static_cast<char>(value);
    binary::Decoder decoder;
    const auto result = decoder.feed(bytes);
    check(result.size() == 1 && result[0].kind == expected && decoder.failed() &&
          decoder.bufferedBytes() == 0, "invalid binary header fails without retained payload");
  };
  corrupt(0, 'X', DecodeKind::invalidMagic);
  corrupt(1, 'X', DecodeKind::invalidMagic);
  corrupt(2, 0, DecodeKind::incompatibleVersion);
  corrupt(2, binary::kVersion + 1, DecodeKind::incompatibleVersion);
  corrupt(3, 255, DecodeKind::invalidMessageType);
  corrupt(3, 0, DecodeKind::invalidMessageType);
  corrupt(4, 1, DecodeKind::invalidFlags);
  corrupt(5, 1, DecodeKind::invalidFlags);
  corrupt(6, 1, DecodeKind::oversizedPayload);
  check(std::string(binary::decodeKindName(DecodeKind::incompatibleVersion)) == "CLIENT_UPDATE_REQUIRED",
        "incompatible binary version has deterministic update-required result");

  check(!binary::encodeFrame(MessageType::textRequest, 1,
                             std::string(binary::kMaxPayload + 1, 'x')),
        "encoder rejects oversized payload without truncation");
  const auto maximum = binary::encodeFrame(MessageType::textRequest, 1,
                                            std::string(binary::kMaxPayload, 'x'));
  binary::Decoder maximumDecoder;
  events = maximumDecoder.feed(*maximum);
  check(events.size() == 1 && events[0].frame.payload.size() == binary::kMaxPayload &&
        maximumDecoder.bufferedBytes() == 0, "maximum bounded payload round-trips");
  auto overMaximum = *maximum;
  overMaximum[9] = 1; // 4097, not merely an obviously enormous length.
  binary::Decoder orderedFailure;
  events = orderedFailure.feed(*text + overMaximum + *text);
  check(events.size() == 2 && events[0].kind == DecodeKind::frame &&
        events[1].kind == DecodeKind::oversizedPayload && orderedFailure.feed(*text).empty(),
        "a valid prefix precedes the terminal error and no trailing frame escapes failure");
  check(!binary::encodeFrame(static_cast<MessageType>(255), 1, "") &&
        !binary::encodeFrame(MessageType::textRequest, 1, "", 1),
        "encoder rejects unknown types and reserved flags");

  check(!binary::decodeInputPayload(std::string("\1\2\0", 3)) &&
        !binary::decodeInputPayload(std::string("\1\0", 2)) &&
        binary::encodeInputPayload({2, 0, 0}).empty(), "malformed input fields reject");
  for (int byte = 0; byte < 256; ++byte) {
    for (std::size_t field = 0; field < 3; ++field) {
      std::string payload(3, '\0');
      payload[field] = static_cast<char>(byte);
      const bool valid = field == 1 ? byte == 0 || byte == 1 || byte == 255 : byte <= 1;
      check(binary::decodeInputPayload(payload).has_value() == valid,
            "every possible byte in every typed input field has defined acceptance");
    }
  }

  helion::flight::State flight;
  flight.x = 123.125; flight.y = -456.5; flight.vx = 12.25; flight.vy = -9.5;
  flight.yaw = 1.25; flight.docked = false; flight.cargo = 4; flight.food = 2;
  flight.parts = 1; flight.station = 1; flight.cooldown = 0.5; flight.hull = 87;
  flight.maxHull = 125; flight.fuel = 73.25; flight.maxFuel = 110;
  flight.destroyed = false; flight.weaponCooldown = 0.75;
  const auto flightPayload = binary::encodeFlightSnapshotPayload(flight, 123456, 789);
  helion::flight::State restored;
  int credits = 0, experience = 0;
  check(flightPayload && flightPayload->size() == 57 &&
        binary::decodeFlightSnapshotPayload(*flightPayload, restored, credits, experience) &&
        std::abs(restored.x - flight.x) < 0.001 && std::abs(restored.y - flight.y) < 0.001 &&
        restored.cargo == flight.cargo && restored.food == flight.food && restored.parts == flight.parts &&
        restored.hull == flight.hull && restored.maxHull == flight.maxHull &&
        std::abs(restored.fuel - flight.fuel) < 0.001 && credits == 123456 && experience == 789,
        "flight snapshot uses explicit fixed-width fields and preserves authoritative state");
  check(!binary::decodeFlightSnapshotPayload(flightPayload->substr(1), restored, credits, experience),
        "malformed flight payload length rejects");
  const auto rejectFlight = [&](std::string payload) {
    const auto previous = restored;
    const int previousCredits = credits, previousExperience = experience;
    check(!binary::decodeFlightSnapshotPayload(payload, restored, credits, experience) &&
          restored.x == previous.x && restored.fuel == previous.fuel && restored.hull == previous.hull &&
          credits == previousCredits && experience == previousExperience,
          "malformed binary state rejects atomically without partially replacing client state");
  };
  for (const auto index : {36u, 56u}) {
    auto bad = *flightPayload;
    bad[index] = 2;
    rejectFlight(bad);
  }
  auto badStation = *flightPayload;
  badStation[51] = static_cast<char>(255);
  rejectFlight(badStation);
  for (const auto index : {39u, 43u}) {
    auto badInteger = *flightPayload;
    badInteger[index] = static_cast<char>(255);
    rejectFlight(badInteger);
  }
  for (std::size_t index = 0; index < 36; index += 4) {
    auto nonfinite = *flightPayload;
    nonfinite[index] = 0x7f;
    nonfinite[index + 1] = static_cast<char>(0xc0); // quiet NaN in each float field
    nonfinite[index + 2] = nonfinite[index + 3] = 0;
    rejectFlight(nonfinite);
  }
  flight.cooldown = 1.251;
  flight.yaw = helion::flight::kPi + 0.001;
  const auto boundary = binary::encodeFlightSnapshotPayload(flight, std::numeric_limits<int>::max(), 0);
  check(boundary && binary::decodeFlightSnapshotPayload(*boundary, restored, credits, experience) &&
        restored.cooldown <= 1.251 && restored.yaw <= helion::flight::kPi + 0.001 &&
        credits == std::numeric_limits<int>::max(),
        "float narrowing does not push valid boundary states out of the decoder's domain");
  flight.x = std::numeric_limits<double>::quiet_NaN();
  check(!binary::encodeFlightSnapshotPayload(flight, 1, 1), "nonfinite flight values reject");

  std::uint32_t random = 0x13579bdf;
  for (int sample = 0; sample < 2000; ++sample) {
    random = random * 1664525u + 1013904223u;
    const std::size_t size = random % 96;
    std::string bytes(size, '\0');
    for (char& byte : bytes) {
      random = random * 1664525u + 1013904223u;
      byte = static_cast<char>(random >> 24);
    }
    binary::Decoder fuzzed;
    (void)fuzzed.feed(bytes);
    check(fuzzed.bufferedBytes() <= binary::kMaxFrameSize,
          "random input never grows decoder beyond one bounded frame");
    (void)fuzzed.finish();
  }

  std::cout << "bounded ABI-independent binary protocol tests passed\n";
}
