#pragma once

#include "platform.hpp"

#include <cassert>

/*
 * V1_DEBUGBREAK(): Halt in the debugger (if attached). Otherwise, terminates the program.
 * V1_DEBUG/V1_RELEASE: Indicates whether debug or release mode was selected
 */
#ifdef V1_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#  define V1_DEBUGBREAK() ::DebugBreak()
#  if defined(DEBUG) || defined(_DEBUG)
#    define V1_DEBUG
#  else
#    define V1_RELEASE
#  endif
#  define _V1_ASSUME(pred) __assume(pred)

#elif V1_OS_POSIX
#  define V1_DEBUGBREAK() __asm__("int3")
#  if defined(DEBUG) || !defined(NDEBUG)
#    define V1_DEBUG
#  else
#    define V1_RELEASE
#  endif
#  define _V1_ASSUME(pred)                 \
    do {                                   \
      if(!(pred)) __builtin_unreachable(); \
    } while(false)

#else
#  error Unsupported platform
#endif

namespace v1util {
V1_PUBLIC bool isDebuggerPresent();
V1_PUBLIC void printfToDebugger(const char* pFormat...);
}  // namespace v1util

/*! Assert that x is true, also in release mode.
 *
 * Halts in the debugger, if attached. Otherwise, terminates the program.
 */
#define V1_ASSERT_RELEASE(x)          \
  do {                                \
    if(!(x)) {                        \
      if(v1util::isDebuggerPresent()) \
        V1_DEBUGBREAK();              \
      else                            \
        assert(false);                \
    }                                 \
  } while(false)
#ifdef V1_DEBUG
//! Asserts that x is true, only in debug mode.
#  define V1_ASSERT(x) V1_ASSERT_RELEASE(x)
#else
//! Asserts that x is true, only in debug mode.
#  define V1_ASSERT(x) \
    do {               \
    } while(false)
#endif

/*! Assume that "x" is true, instruct the compiler/analyzer to think the same.
 *
 * This is checked in debug mode, but not in release mode. There, you have to know
 * that your assumption holds because the optimizer will use this knowledge!
 */
#define V1_ASSUME(x) \
  do {               \
    V1_ASSERT(x);    \
    _V1_ASSUME(x);   \
  } while(false)

//! Indicates that reaching this statement is not meant to happen
#define V1_INVALID() V1_ASSERT(false)

//! Indicates that you reached a dead end
#define V1_CODEMISSING() V1_ASSERT(false)
