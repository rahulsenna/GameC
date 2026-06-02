#pragma once
#include <math.h>

struct alignas(8) Vec2
{
  float x, y;
};

struct alignas(16) Vec3
{
  float x, y, z;
};

struct alignas(16) Vec4
{
  float x, y, z, w;
};

struct Mat4
{
  Vec4 columns[4];
};

// --- Vector Operations ---

inline Vec3 operator+(Vec3 a, Vec3 b)
{
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(Vec3 a, Vec3 b)
{
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(Vec3 a, float s)
{
  return {a.x * s, a.y * s, a.z * s};
}

inline Vec3 operator*(float s, Vec3 a)
{
  return {a.x * s, a.y * s, a.z * s};
}

inline Vec3 operator/(Vec3 a, float s)
{
  return {a.x / s, a.y / s, a.z / s};
}

inline Vec3 &operator+=(Vec3 &a, Vec3 b)
{
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

inline Vec3 &operator-=(Vec3 &a, Vec3 b)
{
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
  return a;
}

// --- Math Functions ---

Vec3 math_normalize(Vec3 v);
Vec3 math_cross(Vec3 a, Vec3 b);
float math_dot(Vec3 a, Vec3 b);

Mat4 math_make_translation(Vec3 t);
Mat4 math_make_rotation_x(float angle);
Mat4 math_make_rotation_y(float angle);
Mat4 math_make_rotation_z(float angle);
Mat4 math_make_scale(Vec3 s);
Mat4 math_make_perspective(float fovy_radians, float aspect, float nearZ,
                           float farZ);
Mat4 math_make_orthographic(float left, float right, float bottom, float top,
                            float nearZ, float farZ);
Mat4 math_make_look_at(Vec3 eye, Vec3 target, Vec3 up);

void math_extract_frustum_planes(const Mat4 &vp, Vec4 planes[6]);
bool math_test_sphere_frustum(Vec3 center, float radius, const Vec4 planes[6]);

Mat4 operator*(const Mat4 &a, const Mat4 &b);
Vec4 operator*(const Mat4 &m, const Vec4 &v);
