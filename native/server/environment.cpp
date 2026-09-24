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

} // namespace helion::server
