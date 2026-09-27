#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace helion::security {

inline constexpr std::size_t kMinNewPassword = 12;
inline constexpr std::size_t kMaxPassword = 128;
inline constexpr std::size_t kCompanionTokenIdBytes = 8;
inline constexpr std::size_t kCompanionTokenSecretBytes = 32;

struct CompanionTokenMaterial {
  std::string id;
  std::string token;
  std::string hash;
};

// Uses OpenSSL's scrypt KDF, RAND_bytes salt, and CRYPTO_memcmp verification.
std::string hashPassword(std::string_view password);
bool verifyPassword(std::string_view password, std::string_view encoded);
bool isEncodedHash(std::string_view value);
// The caller identifies a legacy plaintext record by its on-disk record type.
// Legacy records may contain shorter passwords; hash them before accepting clients.
bool migrateLegacyCredential(std::string& credential, bool legacyRecord);

// Companion bearer tokens are random 256-bit secrets. Only their SHA-256
// verifier is persisted; the plaintext token is returned once at issuance.
CompanionTokenMaterial issueCompanionToken();
std::string companionTokenId(std::string_view token);
bool verifyCompanionToken(std::string_view token, std::string_view expectedId, std::string_view encodedHash);
bool isEncodedCompanionTokenHash(std::string_view value);

} // namespace helion::security
