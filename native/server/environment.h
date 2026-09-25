#pragma once

#include <optional>
#include <string_view>

namespace helion::server {

enum class RuntimeEnvironment {
  development,
  private_test,
  production
};

std::optional<RuntimeEnvironment> parseRuntimeEnvironment(std::string_view value);
std::string_view runtimeEnvironmentName(RuntimeEnvironment environment);
bool usesTestEconomy(RuntimeEnvironment environment);
std::optional<int> purchasePrice(RuntimeEnvironment environment, int canonicalPrice, bool testAvailable);

} // namespace helion::server
