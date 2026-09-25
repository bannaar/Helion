#include "shared/binary_protocol.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace helion::protocol::binary {
namespace {
constexpr std::uint8_t kMagicFirst = 'H';
constexpr std::uint8_t kMagicSecond = 'B';
constexpr std::size_t kFlightPayloadSize = 57;

std::uint16_t readU16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8) |
                                    static_cast<std::uint16_t>(bytes[1]));
}

std::uint32_t readU32(const std::uint8_t* bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
         (static_cast<std::uint32_t>(bytes[1]) << 16) |
         (static_cast<std::uint32_t>(bytes[2]) << 8) |
         static_cast<std::uint32_t>(bytes[3]);
}

void appendU16(std::string& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<char>((value >> 8) & 0xff));
  bytes.push_back(static_cast<char>(value & 0xff));
}

void appendU32(std::string& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<char>((value >> 24) & 0xff));
  bytes.push_back(static_cast<char>((value >> 16) & 0xff));
  bytes.push_back(static_cast<char>((value >> 8) & 0xff));
  bytes.push_back(static_cast<char>(value & 0xff));
}

void appendFloat(std::string& bytes, double value) {
  const float narrowed = static_cast<float>(value);
  std::uint32_t wire = 0;
  static_assert(sizeof(wire) == sizeof(narrowed), "binary float must be 32 bits");
  static_assert(std::numeric_limits<float>::is_iec559, "binary float requires IEEE-754");
  std::memcpy(&wire, &narrowed, sizeof(wire));
  appendU32(bytes, wire);
}

float readFloat(const std::uint8_t* bytes) {
  const std::uint32_t wire = readU32(bytes);
  float value = 0;
  std::memcpy(&value, &wire, sizeof(value));
  return value;
}

bool validFlightState(const flight::State& state, int credits, int experience) {
  return std::isfinite(state.x) && std::isfinite(state.y) && std::isfinite(state.vx) &&
    std::isfinite(state.vy) && std::isfinite(state.yaw) && std::isfinite(state.cooldown) &&
    std::isfinite(state.fuel) && std::isfinite(state.maxFuel) &&
    std::isfinite(state.weaponCooldown) && std::abs(state.x) <= 1201 &&
    std::abs(state.y) <= 1201 && std::abs(state.vx) <= flight::kMaximumSupportedSpeed &&
    std::abs(state.vy) <= flight::kMaximumSupportedSpeed &&
    std::abs(state.yaw) <= flight::kPi + 0.001 && state.cooldown >= 0 &&
    state.cooldown <= 1.251 && state.cargo >= 0 && state.cargo <= flight::kMaximumSupportedCargo &&
    state.food >= 0 && state.food <= flight::kMaximumSupportedCargo && state.parts >= 0 &&
    state.parts <= flight::kMaximumSupportedCargo &&
    flight::cargoUsed(state) <= flight::kMaximumSupportedCargo && state.station >= 0 &&
    state.station < static_cast<int>(flight::kStations.size()) && state.hull >= 0 &&
    state.maxHull >= 1 && state.maxHull <= flight::kMaximumSupportedHull &&
    state.hull <= state.maxHull && state.fuel >= 0 && state.maxFuel > 0 &&
    state.maxFuel <= 1000000 && state.fuel <= state.maxFuel && state.weaponCooldown >= 0 &&
    state.weaponCooldown <= 10 && credits >= 0 && experience >= 0;
}

DecodeEvent errorEvent(DecodeKind kind, std::uint8_t version = 0) {
  DecodeEvent event;
  event.kind = kind;
  event.receivedVersion = version;
  return event;
}
}

Decoder::Decoder() { pending_.reserve(kMaxFrameSize); }

std::vector<DecodeEvent> Decoder::feed(std::string_view bytes) {
  std::vector<DecodeEvent> events;
  if (failed_) return events;
  for (unsigned char byte : bytes) {
    pending_.push_back(byte);
    if (pending_.size() == kHeaderSize) {
      bool invalidHeader = false;
      if (pending_[0] != kMagicFirst || pending_[1] != kMagicSecond) {
        events.push_back(errorEvent(DecodeKind::invalidMagic));
        invalidHeader = true;
      } else if (pending_[2] != kVersion) {
        events.push_back(errorEvent(DecodeKind::incompatibleVersion, pending_[2]));
        invalidHeader = true;
      } else if (!knownMessageType(pending_[3])) {
        events.push_back(errorEvent(DecodeKind::invalidMessageType));
        invalidHeader = true;
      } else if (readU16(pending_.data() + 4) != 0) {
        events.push_back(errorEvent(DecodeKind::invalidFlags));
        invalidHeader = true;
      } else {
        const std::uint32_t payloadSize = readU32(pending_.data() + 6);
        if (payloadSize > kMaxPayload) {
          events.push_back(errorEvent(DecodeKind::oversizedPayload));
          invalidHeader = true;
        } else expectedSize_ = kHeaderSize + payloadSize;
      }
      if (invalidHeader) {
        failed_ = true;
        pending_.clear();
        expectedSize_ = 0;
        break;
      }
    }
    if (expectedSize_ != 0 && pending_.size() == expectedSize_) {
      DecodeEvent event;
      event.kind = DecodeKind::frame;
      event.frame.type = static_cast<MessageType>(pending_[3]);
      event.frame.flags = readU16(pending_.data() + 4);
      event.frame.requestId = readU32(pending_.data() + 10);
      event.frame.payload.assign(pending_.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), pending_.end());
      events.push_back(std::move(event));
      pending_.clear();
      expectedSize_ = 0;
    }
  }
  return events;
}

std::optional<DecodeEvent> Decoder::finish() {
  if (failed_ || pending_.empty()) return std::nullopt;
  failed_ = true;
  pending_.clear();
  expectedSize_ = 0;
  return errorEvent(DecodeKind::truncated);
}

bool knownMessageType(std::uint8_t value) {
  return value >= static_cast<std::uint8_t>(MessageType::textRequest) &&
         value <= static_cast<std::uint8_t>(MessageType::flightSnapshot);
}

const char* decodeKindName(DecodeKind value) {
  switch (value) {
    case DecodeKind::frame: return "FRAME";
    case DecodeKind::truncated: return "TRUNCATED";
    case DecodeKind::invalidMagic: return "INVALID_MAGIC";
    case DecodeKind::incompatibleVersion: return "CLIENT_UPDATE_REQUIRED";
    case DecodeKind::invalidMessageType: return "INVALID_MESSAGE_TYPE";
    case DecodeKind::invalidFlags: return "INVALID_FLAGS";
    case DecodeKind::oversizedPayload: return "OVERSIZED_PAYLOAD";
  }
  return "UNKNOWN";
}

std::optional<std::string> encodeFrame(MessageType type, std::uint32_t requestId,
                                       std::string_view payload, std::uint16_t flags) {
  if (!knownMessageType(static_cast<std::uint8_t>(type)) || flags != 0 || payload.size() > kMaxPayload)
    return std::nullopt;
  std::string bytes;
  bytes.reserve(kHeaderSize + payload.size());
  bytes.push_back(static_cast<char>(kMagicFirst));
  bytes.push_back(static_cast<char>(kMagicSecond));
  bytes.push_back(static_cast<char>(kVersion));
  bytes.push_back(static_cast<char>(type));
  appendU16(bytes, flags);
  appendU32(bytes, static_cast<std::uint32_t>(payload.size()));
  appendU32(bytes, requestId);
  bytes.append(payload.data(), payload.size());
  return bytes;
}

std::string encodeInputPayload(const flight::Input& input) {
  if ((input.thrust != 0 && input.thrust != 1) || input.turn < -1 || input.turn > 1 ||
      (input.brake != 0 && input.brake != 1)) return {};
  std::string payload;
  payload.reserve(3);
  payload.push_back(static_cast<char>(input.thrust));
  payload.push_back(static_cast<char>(static_cast<std::int8_t>(input.turn)));
  payload.push_back(static_cast<char>(input.brake));
  return payload;
}

std::optional<flight::Input> decodeInputPayload(std::string_view payload) {
  if (payload.size() != 3) return std::nullopt;
  const int thrust = static_cast<unsigned char>(payload[0]);
  const int turn = static_cast<std::int8_t>(static_cast<unsigned char>(payload[1]));
  const int brake = static_cast<unsigned char>(payload[2]);
  if ((thrust != 0 && thrust != 1) || turn < -1 || turn > 1 || (brake != 0 && brake != 1))
    return std::nullopt;
  return flight::Input{thrust, turn, brake};
}

std::optional<std::string> encodeFlightSnapshotPayload(const flight::State& state,
                                                       int credits, int experience) {
  if (!validFlightState(state, credits, experience)) return std::nullopt;
  std::string payload;
  payload.reserve(kFlightPayloadSize);
  appendFloat(payload, state.x);
  appendFloat(payload, state.y);
  appendFloat(payload, state.vx);
  appendFloat(payload, state.vy);
  appendFloat(payload, state.yaw);
  appendFloat(payload, state.cooldown);
  appendFloat(payload, state.fuel);
  appendFloat(payload, state.maxFuel);
  appendFloat(payload, state.weaponCooldown);
  payload.push_back(static_cast<char>(state.docked));
  appendU16(payload, static_cast<std::uint16_t>(state.cargo));
  appendU32(payload, static_cast<std::uint32_t>(credits));
  appendU32(payload, static_cast<std::uint32_t>(experience));
  appendU16(payload, static_cast<std::uint16_t>(state.food));
  appendU16(payload, static_cast<std::uint16_t>(state.parts));
  payload.push_back(static_cast<char>(state.station));
  appendU16(payload, static_cast<std::uint16_t>(state.hull));
  appendU16(payload, static_cast<std::uint16_t>(state.maxHull));
  payload.push_back(static_cast<char>(state.destroyed));
  return payload;
}

bool decodeFlightSnapshotPayload(std::string_view payload, flight::State& state,
                                 int& credits, int& experience) {
  if (payload.size() != kFlightPayloadSize) return false;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(payload.data());
  flight::State next;
  std::size_t offset = 0;
  auto takeFloat = [&] {
    const float value = readFloat(bytes + offset);
    offset += 4;
    return static_cast<double>(value);
  };
  next.x = takeFloat(); next.y = takeFloat(); next.vx = takeFloat(); next.vy = takeFloat();
  next.yaw = takeFloat(); next.cooldown = takeFloat(); next.fuel = takeFloat();
  next.maxFuel = takeFloat(); next.weaponCooldown = takeFloat();
  const std::uint8_t docked = bytes[offset++];
  next.cargo = readU16(bytes + offset); offset += 2;
  const std::uint32_t nextCredits = readU32(bytes + offset); offset += 4;
  const std::uint32_t nextExperience = readU32(bytes + offset); offset += 4;
  next.food = readU16(bytes + offset); offset += 2;
  next.parts = readU16(bytes + offset); offset += 2;
  next.station = bytes[offset++];
  next.hull = readU16(bytes + offset); offset += 2;
  next.maxHull = readU16(bytes + offset); offset += 2;
  const std::uint8_t destroyed = bytes[offset++];
  if (offset != payload.size() || docked > 1 || destroyed > 1 ||
      nextCredits > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      nextExperience > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) return false;
  next.docked = docked != 0;
  next.destroyed = destroyed != 0;
  if (!validFlightState(next, static_cast<int>(nextCredits), static_cast<int>(nextExperience))) return false;
  state = next;
  credits = static_cast<int>(nextCredits);
  experience = static_cast<int>(nextExperience);
  return true;
}

} // namespace helion::protocol::binary
