#pragma once

#include <array>

namespace helion::client::math {

struct Mat4 {
  // Column-major storage matches GLSL's mat4 layout.
  std::array<float, 16> value{};
};

Mat4 identity();
Mat4 orthographic(float left, float right, float bottom, float top,
                  float nearPlane, float farPlane);
Mat4 translation(float x, float y, float z = 0.0f);
Mat4 rotationZ(float radians);
Mat4 multiply(const Mat4& left, const Mat4& right);
std::array<float, 4> transform(const Mat4& matrix, const std::array<float, 4>& point);

} // namespace helion::client::math
