#ifndef REDREAM_MATH_H
#define REDREAM_MATH_H

#include <float.h>
#include <math.h>
#include <stdint.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(x, lo, hi) MAX((lo), MIN((hi), (x)))
#define ABS(x) ((x) < 0 ? -(x) : (x))

#define ALIGN_UP(v, alignment) (((v) + (alignment)-1) & ~((alignment)-1))
#define ALIGN_DOWN(v, alignment) ((v) & ~((alignment)-1))

static inline uint32_t bswap24(uint32_t v) {
  return ((v & 0xff) << 16) | (v & 0x00ff00) | ((v & 0xff0000) >> 16);
}

static inline int popcnt32(uint32_t v) {
  /* avoid using popcnt intrinsics to support older processors such as the
     core 2 duo which are plenty fast enough to support */
  v = (v & 0x55555555) + ((v >> 1) & 0x55555555);
  v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
  v = (v & 0x0f0f0f0f) + ((v >> 4) & 0x0f0f0f0f);
  v = (v & 0x00ff00ff) + ((v >> 8) & 0x00ff00ff);
  v = (v & 0x0000ffff) + ((v >> 16) & 0x0000ffff);
  return (int)v;
}

#if COMPILER_MSVC

#include <intrin.h>

static inline int clz32(uint32_t v) {
  unsigned long r = 0;
  _BitScanReverse(&r, v);
  return 31 - r;
}
static inline int clz64(uint64_t v) {
  unsigned long r = 0;
  _BitScanReverse64(&r, v);
  return 63 - r;
}
static inline int ctz32(uint32_t v) {
  unsigned long r = 0;
  _BitScanForward(&r, v);
  return r;
}
static inline int ctz64(uint64_t v) {
  unsigned long r = 0;
  _BitScanForward64(&r, v);
  return r;
}

static inline uint16_t bswap16(uint16_t v) {
  return _byteswap_ushort(v);
}
static inline uint32_t bswap32(uint32_t v) {
  return _byteswap_ulong(v);
}

#else

static inline int clz32(uint32_t v) {
  return __builtin_clz(v);
}
static inline int clz64(uint64_t v) {
  return __builtin_clzll(v);
}
static inline int ctz32(uint32_t v) {
  return __builtin_ctz(v);
}
static inline int ctz64(uint64_t v) {
  return __builtin_ctzll(v);
}

static inline uint16_t bswap16(uint16_t v) {
  return __builtin_bswap16(v);
}
static inline uint32_t bswap32(uint32_t v) {
  return __builtin_bswap32(v);
}

#endif

#endif
