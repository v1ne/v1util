#pragma once

//! Ableton-style tagged booleans
#define K(x) true

//! Default copy, move + default constructor shortcut
#define V1_DEFAULT_CP_MV_CTOR(T)             \
  T() noexcept = default;                    \
  T(const T&) noexcept = default;            \
  T(T&&) noexcept = default;                 \
  T& operator=(const T&) noexcept = default; \
  T& operator=(T&&) noexcept = default;

//! Default copy & move constructor shortcut
#define V1_DEFAULT_CP_MV(T)                  \
  T(const T&) noexcept = default;            \
  T(T&&) noexcept = default;                 \
  T& operator=(const T&) noexcept = default; \
  T& operator=(T&&) noexcept = default;

//! Default copy & move constructor shortcut (without noexcept)
#define V1_DEFAULT_CP_MV_MAYTHROW(T) \
  T(const T&) = default;             \
  T(T&&) = default;                  \
  T& operator=(const T&) = default;  \
  T& operator=(T&&) = default;

//! Non-copyable; default move constructor shortcut
#define V1_NO_CP_DEFAULT_MV(T)              \
  T(const T&) noexcept = delete;            \
  T(T&&) noexcept = default;                \
  T& operator=(const T&) noexcept = delete; \
  T& operator=(T&&) noexcept = default;

//! Non-copyable; default move & default constructor shortcut
#define V1_NO_CP_DEFAULT_MV_CTOR(T)         \
  T() noexcept = default;                   \
  T(const T&) noexcept = delete;            \
  T(T&&) noexcept = default;                \
  T& operator=(const T&) noexcept = delete; \
  T& operator=(T&&) noexcept = default;

//! Explicitly mark a class as non-copyable, non-moveable
#define V1_NO_CP_NO_MV(T)          \
  T(const T&) = delete;            \
  T(T&&) = delete;                 \
  T& operator=(const T&) = delete; \
  T& operator=(T&&) = delete;

//! Explicitly mark a class as non-copyable
#define V1_NO_CP(T)     \
  T(const T&) = delete; \
  T& operator=(const T&) = delete;
