#pragma once

/* operating systems: */

#if defined(_WINDOWS) || defined(_WIN32)
#  define V1_OS_WIN 1

#  if defined(V1_DLLEXPORT)
#    define V1_PUBLIC __declspec(dllexport)
#  elif defined(V1_DLLIMPORT)
#    define V1_PUBLIC __declspec(dllimport)
#  else
#    define V1_PUBLIC
#  endif

#elif defined(__linux__) || defined(__unix__)
#  define V1_OS_POSIX 1
#  define V1_PUBLIC
#  if defined(__linux__)
#    define V1_OS_LINUX
#  elif defined(__FreeBSD__)
#    define V1_OS_FREEBSD
#  else
#    error unknown Unix
#  endif


#else
#  error unknown operating system
#endif


/* architectures: */
#if defined(__x86_64__)
#  define V1_ARCH_AMD64
#  define V1_ARCH_X86
#elif defined(__i386__)
#  define V1_ARCH_X86_32
#  define V1_ARCH_X86
#elif defined(__aarch64__)
#  define V1_ARCH_ARM64
#  define V1_ARCH_ARM
#elif defined(__arm__)
#  define V1_ARCH_ARM32
#  define V1_ARCH_ARM
#else
#  error unknown CPU architecture
#endif
