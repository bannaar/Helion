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
  using helion::server::runtimeEnvironmentName;

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

  std::cout << "runtime environment tests passed\n";
}
