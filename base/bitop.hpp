#pragma once

#include <cstdint>

namespace v1util {

//! return @p val, aligned upwards to meet @p alignment (must be a power of two).
inline constexpr uint8_t alignUp(uint8_t val, uint8_t alignment) {
  auto mask = alignment - 1;
  return uint8_t(uint8_t(val + mask) & ~mask);
}
inline constexpr uint16_t alignUp(uint16_t val, uint16_t alignment) {
  auto mask = alignment - 1;
  return uint8_t(uint8_t(val + mask) & ~mask);
}
inline constexpr uint32_t alignUp(uint32_t val, uint32_t alignment) {
  auto mask = alignment - 1;
  return (val + mask) & ~mask;
}
inline constexpr uint64_t alignUp(uint64_t val, uint64_t alignment) {
  auto mask = alignment - 1;
  return (val + mask) & ~mask;
}

inline constexpr bool isPow2(uint8_t x) {
  return x && !((x - 1) & x);
}
inline constexpr bool isPow2(uint16_t x) {
  return x && !((x - 1) & x);
}
inline constexpr bool isPow2(uint32_t x) {
  return x && !((x - 1) & x);
}
inline constexpr bool isPow2(uint64_t x) {
  return x && !((x - 1) & x);
}

inline constexpr uint32_t nextPow2(uint32_t x) {
  x -= 1;
  x |= x >> 1U;
  x |= x >> 2U;
  x |= x >> 4U;
  x |= x >> 8U;
  x |= x >> 16U;
  return x + 1;
}
inline constexpr uint64_t nextPow2(uint64_t x) {
  x -= 1;
  x |= x >> 1U;
  x |= x >> 2U;
  x |= x >> 4U;
  x |= x >> 8U;
  x |= x >> 16U;
  x |= x >> 32U;
  return x + 1;
}

}  // namespace v1util
