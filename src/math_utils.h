#pragma once
#include <simd/simd.h>

simd_float4x4 math_make_translation(simd_float3 t);
simd_float4x4 math_make_rotation_x(float angle);
simd_float4x4 math_make_rotation_y(float angle);
simd_float4x4 math_make_rotation_z(float angle);
simd_float4x4 math_make_scale(simd_float3 s);
simd_float4x4 math_make_perspective(float fovy_radians, float aspect,
                                    float nearZ, float farZ);
