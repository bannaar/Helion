#include "server/security.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace helion::security {
namespace {
constexpr std::uint64_t kN = 1 << 15;
constexpr std::uint64_t kR = 8;
constexpr std::uint64_t kP = 1;
constexpr std::uint64_t kMaxMemory = 64 * 1024 * 1024;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;
constexpr std::string_view kPrefix = "$scrypt$32768$8$1$";
constexpr std::string_view kCompanionHashPrefix = "$sha256$";

std::string toHex(const unsigned char* bytes, std::size_t length) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(length * 2);
  for (std::size_t i = 0; i < length; ++i) {
    out.push_back(digits[bytes[i] >> 4]);
    out.push_back(digits[bytes[i] & 15]);
  }
  return out;
}

bool fromHex(std::string_view text, unsigned char* out, std::size_t length) {
  if (text.size() != length * 2) return false;
  for (std::size_t i = 0; i < length; ++i) {
    auto digit = [](char ch) -> int {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
      return -1;
    };
    const int high = digit(text[i * 2]);
    const int low = digit(text[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    out[i] = static_cast<unsigned char>((high << 4) | low);
  }
  return true;
}

std::array<unsigned char, 32> sha256(std::string_view value) {
  std::array<unsigned char, 32> digest{};
  unsigned int length = 0;
  if (EVP_Digest(value.data(), value.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1 ||
      length != digest.size()) {
    OPENSSL_cleanse(digest.data(), digest.size());
    throw std::runtime_error("companion token hashing unavailable");
  }
  return digest;
}

std::string encodedCompanionHash(std::string_view token) {
  auto digest = sha256(token);
  std::string encoded(kCompanionHashPrefix);
  encoded += toHex(digest.data(), digest.size());
  OPENSSL_cleanse(digest.data(), digest.size());
  return encoded;
}

std::string hashAnyPassword(std::string_view password) {
  if (password.empty() || password.size() > kMaxPassword) throw std::invalid_argument("invalid password length");
  std::array<unsigned char, kSaltBytes> salt{};
  std::array<unsigned char, kHashBytes> derived{};
  if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1 ||
      EVP_PBE_scrypt(password.data(), password.size(), salt.data(), salt.size(),
                     kN, kR, kP, kMaxMemory, derived.data(), derived.size()) != 1) {
    OPENSSL_cleanse(derived.data(), derived.size());
    throw std::runtime_error("password hashing unavailable");
  }
  std::string encoded(kPrefix);
  encoded += toHex(salt.data(), salt.size()) + "$" + toHex(derived.data(), derived.size());
  OPENSSL_cleanse(derived.data(), derived.size());
  return encoded;
}
} // namespace

std::string hashPassword(std::string_view password) {
  if (password.size() < kMinNewPassword || password.size() > kMaxPassword)
    throw std::invalid_argument("invalid password length");
  return hashAnyPassword(password);
}

bool isEncodedHash(std::string_view value) { return value.substr(0, 1) == "$"; }

bool verifyPassword(std::string_view password, std::string_view encoded) {
  if (password.empty() || password.size() > kMaxPassword || encoded.substr(0, kPrefix.size()) != kPrefix) return false;
  const auto rest = encoded.substr(kPrefix.size());
  const auto separator = rest.find('$');
  if (separator == std::string_view::npos) return false;
  std::array<unsigned char, kSaltBytes> salt{};
  std::array<unsigned char, kHashBytes> expected{};
  std::array<unsigned char, kHashBytes> derived{};
  if (!fromHex(rest.substr(0, separator), salt.data(), salt.size()) ||
      !fromHex(rest.substr(separator + 1), expected.data(), expected.size())) return false;
  if (EVP_PBE_scrypt(password.data(), password.size(), salt.data(), salt.size(),
                     kN, kR, kP, kMaxMemory, derived.data(), derived.size()) != 1) return false;
  const bool match = CRYPTO_memcmp(expected.data(), derived.data(), derived.size()) == 0;
  OPENSSL_cleanse(derived.data(), derived.size());
  return match;
}

bool migrateLegacyCredential(std::string& credential, bool legacyRecord) {
  if (!legacyRecord) return false;
  std::string encoded = hashAnyPassword(credential);
  std::fill(credential.begin(), credential.end(), '\0');
  credential = std::move(encoded);
  return true;
}

CompanionTokenMaterial issueCompanionToken() {
  std::array<unsigned char, kCompanionTokenIdBytes> idBytes{};
  std::array<unsigned char, kCompanionTokenSecretBytes> secretBytes{};
  if (RAND_bytes(idBytes.data(), static_cast<int>(idBytes.size())) != 1 ||
      RAND_bytes(secretBytes.data(), static_cast<int>(secretBytes.size())) != 1) {
    OPENSSL_cleanse(secretBytes.data(), secretBytes.size());
    throw std::runtime_error("companion token generation unavailable");
  }

  CompanionTokenMaterial material;
  material.id = toHex(idBytes.data(), idBytes.size());
  material.token = "hc1." + material.id + "." + toHex(secretBytes.data(), secretBytes.size());
  OPENSSL_cleanse(secretBytes.data(), secretBytes.size());
  material.hash = encodedCompanionHash(material.token);
  return material;
}

bool isEncodedCompanionTokenHash(std::string_view value) {
  if (value.substr(0, kCompanionHashPrefix.size()) != kCompanionHashPrefix) return false;
  std::array<unsigned char, 32> decoded{};
  return fromHex(value.substr(kCompanionHashPrefix.size()), decoded.data(), decoded.size());
}

bool verifyCompanionToken(
    std::string_view token,
    std::string_view expectedId,
    std::string_view encodedHash) {
  constexpr std::string_view tokenPrefix = "hc1.";
  if (token.substr(0, tokenPrefix.size()) != tokenPrefix ||
      encodedHash.substr(0, kCompanionHashPrefix.size()) != kCompanionHashPrefix) return false;

  const auto idStart = tokenPrefix.size();
  const auto separator = token.find('.', idStart);
  if (separator == std::string_view::npos || token.substr(idStart, separator - idStart) != expectedId ||
      token.size() != tokenPrefix.size() + kCompanionTokenIdBytes * 2 + 1 + kCompanionTokenSecretBytes * 2) {
    return false;
  }

  std::array<unsigned char, 32> expected{};
  if (!fromHex(encodedHash.substr(kCompanionHashPrefix.size()), expected.data(), expected.size())) return false;

  auto actual = sha256(token);
  const bool match = CRYPTO_memcmp(expected.data(), actual.data(), actual.size()) == 0;
  OPENSSL_cleanse(actual.data(), actual.size());
  OPENSSL_cleanse(expected.data(), expected.size());
  return match;
}

} // namespace helion::security
