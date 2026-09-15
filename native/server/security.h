#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace helion::security {

inline constexpr std::size_t kMinNewPassword = 12;
inline constexpr std::size_t kMaxPassword = 128;

// Uses OpenSSL's scrypt KDF, RAND_bytes salt, and CRYPTO_memcmp verification.
std::string hashPassword(std::string_view password);
bool verifyPassword(std::string_view password, std::string_view encoded);
bool isEncodedHash(std::string_view value);
// The caller identifies a legacy plaintext record by its on-disk record type.
// Legacy records may contain shorter passwords; hash them before accepting clients.
bool migrateLegacyCredential(std::string& credential, bool legacyRecord);

} // namespace helion::security
