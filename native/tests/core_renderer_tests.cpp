#include "client/core_renderer.h"
#include "shared/flight.h"

#include <cmath>
#include <iostream>

int main() {
  using helion::client::firstMissingCoreFunction;
  using helion::client::math::identity;
  using helion::client::math::multiply;
  using helion::client::math::rotationZ;
  using helion::client::math::transform;
  using helion::client::math::translation;

  if (firstMissingCoreFunction({"glCreateShader", "glShaderSource"}) != "glCompileShader") return 1;
  if (firstMissingCoreFunction({}) != "glCreateShader") return 1;
  if (!firstMissingCoreFunction({
    "glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv",
    "glGetShaderInfoLog", "glDeleteShader", "glCreateProgram", "glAttachShader",
    "glLinkProgram", "glGetProgramiv", "glGetProgramInfoLog", "glUseProgram",
    "glDeleteProgram", "glGenVertexArrays", "glBindVertexArray", "glDeleteVertexArrays",
    "glGenBuffers", "glBindBuffer", "glBufferData", "glDeleteBuffers",
    "glEnableVertexAttribArray", "glVertexAttribPointer", "glGetUniformLocation",
    "glUniformMatrix4fv", "glDrawArrays"}).empty()) return 1;

  const auto unit = identity();
  const auto unchanged = transform(unit, {1.0f, 2.0f, 3.0f, 1.0f});
  const std::array<float, 4> expected{1.0f, 2.0f, 3.0f, 1.0f};
  if (unchanged != expected) return 1;

  const auto moved = transform(translation(2.0f, -1.0f), {1.0f, 2.0f, 0.0f, 1.0f});
  if (std::fabs(moved[0] - 3.0f) >= 0.0001f || std::fabs(moved[1] - 1.0f) >= 0.0001f) return 1;

  const auto quarterTurn = transform(rotationZ(helion::flight::kPi / 2.0f), {1.0f, 0.0f, 0.0f, 1.0f});
  if (std::fabs(quarterTurn[0]) >= 0.0001f || std::fabs(quarterTurn[1] - 1.0f) >= 0.0001f) return 1;

  const auto composed = transform(multiply(translation(2.0f, 0.0f), rotationZ(helion::flight::kPi / 2.0f)),
                                  {1.0f, 0.0f, 0.0f, 1.0f});
  if (std::fabs(composed[0] - 2.0f) >= 0.0001f || std::fabs(composed[1] - 1.0f) >= 0.0001f) return 1;
  std::cout << "core renderer policy and math tests passed\n";
}
