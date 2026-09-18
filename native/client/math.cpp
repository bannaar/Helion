#include "client/math.h"

#include <cmath>

namespace helion::client::math {

Mat4 identity() {
  Mat4 result;
  result.value = {1.0f, 0.0f, 0.0f, 0.0f,
                  0.0f, 1.0f, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 0.0f,
                  0.0f, 0.0f, 0.0f, 1.0f};
  return result;
}

Mat4 orthographic(float left, float right, float bottom, float top,
                  float nearPlane, float farPlane) {
  Mat4 result{};
  result.value[0] = 2.0f / (right - left);
  result.value[5] = 2.0f / (top - bottom);
  result.value[10] = -2.0f / (farPlane - nearPlane);
  result.value[12] = -(right + left) / (right - left);
  result.value[13] = -(top + bottom) / (top - bottom);
  result.value[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
  result.value[15] = 1.0f;
  return result;
}

Mat4 translation(float x, float y, float z) {
  auto result = identity();
  result.value[12] = x;
  result.value[13] = y;
  result.value[14] = z;
  return result;
}

Mat4 rotationZ(float radians) {
  Mat4 result = identity();
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  result.value[0] = cosine;
  result.value[1] = sine;
  result.value[4] = -sine;
  result.value[5] = cosine;
  return result;
}

Mat4 multiply(const Mat4& left, const Mat4& right) {
  Mat4 result{};
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row)
      for (int index = 0; index < 4; ++index)
        result.value[static_cast<std::size_t>(column * 4 + row)] +=
          left.value[static_cast<std::size_t>(index * 4 + row)] *
          right.value[static_cast<std::size_t>(column * 4 + index)];
  return result;
}

std::array<float, 4> transform(const Mat4& matrix, const std::array<float, 4>& point) {
  std::array<float, 4> result{};
  for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
      result[static_cast<std::size_t>(row)] +=
        matrix.value[static_cast<std::size_t>(column * 4 + row)] * point[static_cast<std::size_t>(column)];
  return result;
}

} // namespace helion::client::math
