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

} // namespace helion::server
