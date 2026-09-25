#include "server/environment.h"

#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* label) {
  if (!condition) {
    std::cerr << "FAILED: " << label << '\n';
    std::exit(1);
  }
}
}

int main() {
  using helion::server::RuntimeEnvironment;
  using helion::server::parseRuntimeEnvironment;
  using helion::server::purchasePrice;
  using helion::server::runtimeEnvironmentName;
  using helion::server::usesTestEconomy;

  check(parseRuntimeEnvironment("development") == RuntimeEnvironment::development,
        "development environment parses");
  check(parseRuntimeEnvironment("test") == RuntimeEnvironment::private_test,
        "private test environment parses");
  check(parseRuntimeEnvironment("production") == RuntimeEnvironment::production,
        "production environment parses");
  check(!parseRuntimeEnvironment("stable") && !parseRuntimeEnvironment("") &&
        !parseRuntimeEnvironment("PRODUCTION"), "unknown environment is rejected");
  check(runtimeEnvironmentName(RuntimeEnvironment::development) == "development" &&
        runtimeEnvironmentName(RuntimeEnvironment::private_test) == "test" &&
        runtimeEnvironmentName(RuntimeEnvironment::production) == "production",
        "environment names are stable persistence identifiers");
  check(runtimeEnvironmentName(static_cast<RuntimeEnvironment>(255)) == "unknown",
        "invalid in-memory environment cannot silently become development");
  check(!usesTestEconomy(RuntimeEnvironment::development) &&
        usesTestEconomy(RuntimeEnvironment::private_test) &&
        !usesTestEconomy(RuntimeEnvironment::production),
        "only the private TEST environment selects convenience economy policy");
  check(purchasePrice(RuntimeEnvironment::private_test, 24000, true) == 100 &&
        purchasePrice(RuntimeEnvironment::development, 24000, true) == 24000 &&
        purchasePrice(RuntimeEnvironment::production, 24000, true) == 24000 &&
        purchasePrice(RuntimeEnvironment::private_test, 24000, false) == 24000,
        "TEST price is isolated from canonical development and production prices");
  check(!purchasePrice(static_cast<RuntimeEnvironment>(255), 100, true) &&
        !purchasePrice(RuntimeEnvironment::production, -1, true),
        "ambiguous environment and invalid canonical prices fail closed");

  std::cout << "runtime environment tests passed\n";
}
