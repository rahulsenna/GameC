#include "math_utils.h"
#include <math.h>

Vec3 math_normalize(Vec3 v)
{
  float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len > 0.00001f)
  {
    return {v.x / len, v.y / len, v.z / len};
  }
  return {0, 0, 0};
}

Vec3 math_cross(Vec3 a, Vec3 b)
{
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float math_dot(Vec3 a, Vec3 b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Mat4 math_make_translation(Vec3 t)
{
  return Mat4{Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}, Vec4{0, 0, 1, 0},
              Vec4{t.x, t.y, t.z, 1}};
}

Mat4 math_make_rotation_x(float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  return Mat4{Vec4{1, 0, 0, 0}, Vec4{0, c, s, 0}, Vec4{0, -s, c, 0},
              Vec4{0, 0, 0, 1}};
}

Mat4 math_make_rotation_y(float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  return Mat4{Vec4{c, 0, -s, 0}, Vec4{0, 1, 0, 0}, Vec4{s, 0, c, 0},
              Vec4{0, 0, 0, 1}};
}

Mat4 math_make_rotation_z(float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  return Mat4{Vec4{c, s, 0, 0}, Vec4{-s, c, 0, 0}, Vec4{0, 0, 1, 0},
              Vec4{0, 0, 0, 1}};
}

Mat4 math_make_scale(Vec3 s)
{
  return Mat4{Vec4{s.x, 0, 0, 0}, Vec4{0, s.y, 0, 0}, Vec4{0, 0, s.z, 0},
              Vec4{0, 0, 0, 1}};
}

Mat4 math_make_perspective(float fovy_radians, float aspect, float nearZ,
                           float farZ)
{
  float ys = 1.0f / tanf(fovy_radians * 0.5f);
  float xs = ys / aspect;
  float zs = farZ / (nearZ - farZ);

  return Mat4{Vec4{xs, 0, 0, 0}, Vec4{0, ys, 0, 0}, Vec4{0, 0, zs, -1},
              Vec4{0, 0, nearZ * zs, 0}};
}

Mat4 math_make_look_at(Vec3 eye, Vec3 target, Vec3 up)
{
  Vec3 f = math_normalize(target - eye);
  Vec3 r = math_normalize(math_cross(f, up));
  Vec3 u = math_cross(r, f);

  return Mat4{
      Vec4{r.x, u.x, -f.x, 0.0f}, Vec4{r.y, u.y, -f.y, 0.0f},
      Vec4{r.z, u.z, -f.z, 0.0f},
      Vec4{-math_dot(r, eye), -math_dot(u, eye), math_dot(f, eye), 1.0f}};
}

Mat4 operator*(const Mat4 &a, const Mat4 &b)
{
  Mat4 res;
  for (int col = 0; col < 4; ++col)
  {
    for (int row = 0; row < 4; ++row)
    {
      float sum = 0.0f;
      for (int i = 0; i < 4; ++i)
      {
        // b.columns[col] is the 'i'th element of the column
        // a.columns[i] gives us the 'i'th column of 'a'.
        // So we want the 'row'th element of the 'i'th column of 'a'.
        float b_val = (i == 0)   ? b.columns[col].x
                      : (i == 1) ? b.columns[col].y
                      : (i == 2) ? b.columns[col].z
                                 : b.columns[col].w;

        float a_val = (row == 0)   ? a.columns[i].x
                      : (row == 1) ? a.columns[i].y
                      : (row == 2) ? a.columns[i].z
                                   : a.columns[i].w;

        sum += a_val * b_val;
      }
      if (row == 0)
        res.columns[col].x = sum;
      else if (row == 1)
        res.columns[col].y = sum;
      else if (row == 2)
        res.columns[col].z = sum;
      else if (row == 3)
        res.columns[col].w = sum;
    }
  }
  return res;
}

Mat4 math_make_orthographic(float left, float right, float bottom, float top,
                            float nearZ, float farZ)
{
  Mat4 m = {};
  float ral = right + left;
  float rsl = right - left;
  float tab = top + bottom;
  float tsb = top - bottom;
  float fan = farZ + nearZ;
  float fsn = farZ - nearZ;

  m.columns[0].x = 2.0f / rsl;
  m.columns[1].y = 2.0f / tsb;
  m.columns[2].z = -1.0f / fsn; // -1/(far-near) for Metal depth 0..1 in RH
  m.columns[3].x = -ral / rsl;
  m.columns[3].y = -tab / tsb;
  m.columns[3].z = -nearZ / fsn; // -near/(far-near) for Metal
  m.columns[3].w = 1.0f;

  return m;
}
