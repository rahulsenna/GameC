#include "math_utils.h"
#include <math.h>

simd_float4x4 math_make_translation(simd_float3 t)
{
  return simd_matrix(simd_make_float4(1, 0, 0, 0), simd_make_float4(0, 1, 0, 0),
                     simd_make_float4(0, 0, 1, 0),
                     simd_make_float4(t.x, t.y, t.z, 1));
}

simd_float4x4 math_make_rotation_x(float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  return simd_matrix(simd_make_float4(1, 0, 0, 0), simd_make_float4(0, c, s, 0),
                     simd_make_float4(0, -s, c, 0),
                     simd_make_float4(0, 0, 0, 1));
}

simd_float4x4 math_make_rotation_y(float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  return simd_matrix(simd_make_float4(c, 0, -s, 0),
                     simd_make_float4(0, 1, 0, 0), simd_make_float4(s, 0, c, 0),
                     simd_make_float4(0, 0, 0, 1));
}

simd_float4x4 math_make_rotation_z(float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  return simd_matrix(
      simd_make_float4(c, s, 0, 0), simd_make_float4(-s, c, 0, 0),
      simd_make_float4(0, 0, 1, 0), simd_make_float4(0, 0, 0, 1));
}

simd_float4x4 math_make_scale(simd_float3 s)
{
  return simd_matrix(
      simd_make_float4(s.x, 0, 0, 0), simd_make_float4(0, s.y, 0, 0),
      simd_make_float4(0, 0, s.z, 0), simd_make_float4(0, 0, 0, 1));
}

simd_float4x4 math_make_perspective(float fovy_radians, float aspect,
                                    float nearZ, float farZ)
{
  float ys = 1.0f / tanf(fovy_radians * 0.5f);
  float xs = ys / aspect;
  float zs = farZ / (nearZ - farZ);

  // Metal depth ranges from 0 to 1
  return simd_matrix(
      simd_make_float4(xs, 0, 0, 0), simd_make_float4(0, ys, 0, 0),
      simd_make_float4(0, 0, zs, -1), simd_make_float4(0, 0, nearZ * zs, 0));
}
