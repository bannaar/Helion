#pragma once

#include "shared/flight.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace helion::protocol::binary {

inline constexpr std::uint8_t kVersion = 1;
inline constexpr std::size_t kHeaderSize = 14;
inline constexpr std::size_t kMaxPayload = 4096;
inline constexpr std::size_t kMaxFrameSize = kHeaderSize + kMaxPayload;

enum class MessageType : std::uint8_t {
  textRequest = 1,
  textResponse = 2,
  input = 3,
  flightSnapshot = 4
};

struct Frame {
  MessageType type = MessageType::textRequest;
  std::uint16_t flags = 0;
  std::uint32_t requestId = 0;
  std::vector<std::uint8_t> payload;
};

enum class DecodeKind {
  frame,
  truncated,
  invalidMagic,
  incompatibleVersion,
  invalidMessageType,
  invalidFlags,
  oversizedPayload
};

struct DecodeEvent {
  DecodeKind kind = DecodeKind::frame;
  Frame frame;
  std::uint8_t receivedVersion = 0;
};

class Decoder {
 public:
  Decoder();
  std::vector<DecodeEvent> feed(std::string_view bytes);
  std::optional<DecodeEvent> finish();
  bool failed() const { return failed_; }
  std::size_t bufferedBytes() const { return pending_.size(); }

 private:
  std::vector<std::uint8_t> pending_;
  std::size_t expectedSize_ = 0;
  bool failed_ = false;
};

bool knownMessageType(std::uint8_t value);
const char* decodeKindName(DecodeKind value);
std::optional<std::string> encodeFrame(MessageType type, std::uint32_t requestId,
                                       std::string_view payload, std::uint16_t flags = 0);

std::string encodeInputPayload(const flight::Input& input);
std::optional<flight::Input> decodeInputPayload(std::string_view payload);

std::optional<std::string> encodeFlightSnapshotPayload(const flight::State& state,
                                                       int credits, int experience);
bool decodeFlightSnapshotPayload(std::string_view payload, flight::State& state,
                                 int& credits, int& experience);

} // namespace helion::protocol::binary
