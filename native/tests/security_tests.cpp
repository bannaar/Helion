#include "server/connection_limit.h"
#include "server/security.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* label) {
  if (!condition) {
    std::cerr << "FAILED: " << label << '\n';
    std::exit(1);
  }
}
}

int main() {
  const std::string password = "synthetic-test-password";
  const std::string first = helion::security::hashPassword(password);
  const std::string second = helion::security::hashPassword(password);
  check(first != second, "unique random salts");
  check(helion::security::verifyPassword(password, first), "correct password verifies");
  check(!helion::security::verifyPassword("wrong-password", first), "invalid credentials rejected");
  check(!helion::security::verifyPassword(password, "$scrypt$malformed"), "malformed stored hash rejected");
  check(helion::security::isEncodedHash(first), "encoded credential recognized");
  for (const std::string& candidate : {std::string(11, 'a'), std::string(129, 'a')}) {
    bool rejected = false;
    try { (void)helion::security::hashPassword(candidate); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "new password limits enforced");
  }
  check(helion::security::verifyPassword(std::string(128, 'a'),
        helion::security::hashPassword(std::string(128, 'a'))), "maximum password accepted");

  std::string legacy = "oldpass";
  check(helion::security::migrateLegacyCredential(legacy, true), "legacy credential migrated");
  check(legacy != "oldpass" && helion::security::verifyPassword("oldpass", legacy), "legacy login preserved after migration");
  check(!helion::security::migrateLegacyCredential(legacy, false), "hash not migrated twice");
  std::string prefixedLegacy = "$legacy-password";
  check(helion::security::migrateLegacyCredential(prefixedLegacy, true) &&
        helion::security::verifyPassword("$legacy-password", prefixedLegacy), "dollar-prefixed legacy password migrates");
  std::string corrupt = "$scrypt$invalid";
  check(!helion::security::migrateLegacyCredential(corrupt, false) &&
        !helion::security::verifyPassword("oldpass", corrupt), "corrupt hash fails closed");

  const auto companion = helion::security::issueCompanionToken();
  const auto companion2 = helion::security::issueCompanionToken();
  check(companion.id.size() == helion::security::kCompanionTokenIdBytes * 2, "companion token id length");
  check(companion.token.rfind("hc1." + companion.id + ".", 0) == 0, "companion token embeds public id");
  check(helion::security::isEncodedCompanionTokenHash(companion.hash), "companion token hash recognized");
  check(helion::security::companionTokenId(companion.token) == companion.id, "companion token id parses");
  check(helion::security::companionTokenId("hc1.bad.token").empty(), "malformed companion token id rejected");
  check(helion::security::verifyCompanionToken(companion.token, companion.id, companion.hash),
        "issued companion token verifies");
  check(!helion::security::verifyCompanionToken(companion2.token, companion.id, companion.hash),
        "different companion token rejected");
  check(!helion::security::verifyCompanionToken(companion.token, companion2.id, companion.hash),
        "wrong companion token id rejected");
  check(!helion::security::isEncodedCompanionTokenHash("$sha256$bad"), "malformed companion hash rejected");

  helion::server::ConnectionLimit limit(2);
  check(limit.tryAcquire() && limit.tryAcquire(), "available slots acquired");
  check(!limit.tryAcquire() && limit.active() == 2, "connection limit enforced");
  limit.release();
  check(limit.tryAcquire() && limit.active() == 2, "released slot reused");
  limit.release();
  limit.release();
  check(limit.active() == 0, "all slots released");

  std::cout << "security tests passed\n";
  return 0;
}
