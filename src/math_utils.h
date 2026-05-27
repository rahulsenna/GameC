#pragma once
#include <simd/simd.h>

simd_float3 math_normalize(simd_float3 v);
simd_float3 math_cross(simd_float3 a, simd_float3 b);
float math_dot(simd_float3 a, simd_float3 b);

simd_float4x4 math_make_translation(simd_float3 t);
simd_float4x4 math_make_rotation_x(float angle);
simd_float4x4 math_make_rotation_y(float angle);
simd_float4x4 math_make_rotation_z(float angle);
simd_float4x4 math_make_scale(simd_float3 s);
simd_float4x4 math_make_perspective(float fovy_radians, float aspect,
                                    float nearZ, float farZ);
simd_float4x4 math_make_look_at(simd_float3 eye, simd_float3 target,
                                simd_float3 up);
