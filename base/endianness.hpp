#pragma once

#include "platform.hpp"

#include <cstdint>


#define V1_ENDIANNESS_LITTLE 0
#define V1_ENDIANNESS_BIG 1

#if defined(__LITTLE_ENDIAN__) || defined(_WIN32) || defined(_WIN64) || defined(__ARMEL__)     \
    || defined(__THUMBEL__) || defined(__AARCH64EL__) || defined(_MIPSEL) || defined(__MIPSEL) \
    || defined(__MIPSEL__)                                                                     \
    || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#  define V1_IS_LITTLE_ENDIAN
#  define V1_ENDIANNESS V1_ENDIANNESS_LITTLE
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__)                   \
    || defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB) || defined(__MIPSEB__) \
    || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  define V1_IS_BIG_ENDIAN
#  define V1_ENDIANNESS V1_ENDIANNESS_BIG
#else
#  error "Can't determine endianness of your target platform."
#endif

#ifdef V1_OS_WIN
#  include <intrin.h>
#endif

namespace v1util::endianness {
template <typename UnsignedT>
UnsignedT swap_endianness(UnsignedT) noexcept;


template <>
inline int8_t swap_endianness<int8_t>(int8_t value) noexcept {
  return value;
}
template <>
inline uint8_t swap_endianness<uint8_t>(uint8_t value) noexcept {
  return value;
}

#if defined(V1_OS_WIN)

template <>
inline uint16_t swap_endianness<uint16_t>(uint16_t value) noexcept {
  return _byteswap_ushort(value);
}
template <>
inline int16_t swap_endianness<int16_t>(int16_t value) noexcept {
  return (int16_t)_byteswap_ushort((uint16_t)value);
}
template <>
inline uint32_t swap_endianness<uint32_t>(uint32_t value) noexcept {
  return _byteswap_ulong(value);
}
template <>
inline int32_t swap_endianness<int32_t>(int32_t value) noexcept {
  return (int32_t)_byteswap_ulong((uint32_t)value);
}
template <>
inline uint64_t swap_endianness<uint64_t>(uint64_t value) noexcept {
  return _byteswap_uint64(value);
}
template <>
inline int64_t swap_endianness<int64_t>(int64_t value) noexcept {
  return (int64_t)_byteswap_uint64((uint64_t)value);
}

#elif defined(V1_OS_POSIX)

template <>
inline uint16_t swap_endianness<uint16_t>(uint16_t value) noexcept {
  return __builtin_bswap16(value);
}
template <>
inline int16_t swap_endianness<int16_t>(int16_t value) noexcept {
  return (int16_t)__builtin_bswap16((uint16_t)value);
}
template <>
inline uint32_t swap_endianness<uint32_t>(uint32_t value) noexcept {
  return __builtin_bswap32(value);
}
template <>
inline int32_t swap_endianness<int32_t>(int32_t value) noexcept {
  return (int32_t)__builtin_bswap32((uint32_t)value);
}
template <>
inline uint64_t swap_endianness<uint64_t>(uint64_t value) noexcept {
  return __builtin_bswap64(value);
}
template <>
inline int64_t swap_endianness<int64_t>(int64_t value) noexcept {
  return (int64_t)__builtin_bswap64((uint64_t)value);
}

#else
#  error unsupported OS
#endif


/*
 * The following macros use these abbreviations:
 * be: big endian,  le: little endian,  nat: native,  net: network (big endian)
 */
#ifdef V1_IS_BIG_ENDIAN
#  define nat2be(x) (x)
#  define nat2le(x) v1util::endianness::swap_endianness(x)
#  define le2nat(x) v1util::endianness::swap_endianness(x)
#  define be2nat(x) (x)
#  define nat2net(x) (x)
#  define net2nat(x) (x)
#else  // little endian -- assumed, mixed endian doesn't exist ;):
#  define nat2le(x) (x)
#  define nat2be(x) v1util::endianness::swap_endianness(x)
#  define be2nat(x) v1util::endianness::swap_endianness(x)
#  define le2nat(x) (x)
#  define nat2net(x) v1util::endianness::swap_endianness(x)
#  define net2nat(x) v1util::endianness::swap_endianness(x)
#endif

}  // namespace v1util::endianness
