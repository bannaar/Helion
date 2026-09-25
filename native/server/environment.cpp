#include "server/environment.h"

namespace helion::server {

std::optional<RuntimeEnvironment> parseRuntimeEnvironment(std::string_view value) {
  if (value == "development") return RuntimeEnvironment::development;
  if (value == "test") return RuntimeEnvironment::private_test;
  if (value == "production") return RuntimeEnvironment::production;
  return std::nullopt;
}

std::string_view runtimeEnvironmentName(RuntimeEnvironment environment) {
  switch (environment) {
    case RuntimeEnvironment::development: return "development";
    case RuntimeEnvironment::private_test: return "test";
    case RuntimeEnvironment::production: return "production";
  }
  return "unknown";
}

bool usesTestEconomy(RuntimeEnvironment environment) {
  return environment == RuntimeEnvironment::private_test;
}

std::optional<int> purchasePrice(RuntimeEnvironment environment, int canonicalPrice, bool testAvailable) {
  if (canonicalPrice < 0 || runtimeEnvironmentName(environment) == "unknown") return std::nullopt;
  if (environment == RuntimeEnvironment::private_test && testAvailable) return 100;
  return canonicalPrice;
}

} // namespace helion::server
