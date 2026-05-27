#pragma once
#include <stddef.h>
#include <stdint.h>

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef float F32;
typedef double F64;
typedef uint32_t B32;

#define Assert(c)                                                              \
  if (!(c))                                                                    \
  {                                                                            \
    __builtin_trap();                                                          \
  }
#define AlignPow2(x, p) (((x) + (p) - 1) & ~((p) - 1))
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
