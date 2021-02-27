#include "time.hpp"

#include "debug.hpp"
#include "platform.hpp"


#if defined(V1_OS_WIN)
#  include <Windows.h>
#  undef min
#  undef max
#elif defined(V1_OS_LINUX)
extern "C" {
#  include <stdio.h>
#  include <stdlib.h>
#  include <unistd.h>
#  ifdef V1_ARCH_X86
#    include <x86intrin.h>  // GCC
#  else
#    include <time.h>
#  endif
}
#elif defined(V1_OS_FREEBSD)
extern "C" {
#  include <sys/types.h>

#  include <sys/sysctl.h>
#  include <sys/user.h>
#  include <unistd.h>
}
#endif

namespace v1util {


/*
 * platform-dependent:
 */

#if defined(V1_OS_WIN)

int64_t tscTicksPerSecond() {
  static const auto sTicksPerSecond = []() {
    LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
  }();
  return sTicksPerSecond;
}


uint64_t tscStamp() {
  LARGE_INTEGER count;
  static_assert(sizeof(uint64_t) == sizeof(LARGE_INTEGER), "");
  ::QueryPerformanceCounter(&count);
  return uint64_t(count.QuadPart);
}

// for normal clock: GetSystemTimePreciseAsFileTime

#elif defined(V1_OS_POSIX) && defined(V1_ARCH_X86)

int64_t tscTicksPerSecond() {
  static const auto sTicksPerSecond = []() -> int64_t {
#  if defined(V1_OS_FREEBSD)
    int64_t ticksPerSecond;
    size_t size = sizeof(ticksPerSecond);
    int failure = sysctlbyname("machdep.tsc_freq", &ticksPerSecond, &size, nullptr, 0);
    if(failure || size != sizeof(ticksPerSecond)) {
      V1_DEBUGBREAK();
      return 0;
    }
    return ticksPerSecond;
#  elif defined(V1_OS_LINUX)
    const auto numberFromFile = [](const char* pFilename) -> int64_t {
      FILE* pFile = ::fopen(pFilename, "r");
      if(!pFile) return -1;

      char buf[32];
      buf[31] = '\0';
      auto numRead = ::fread(&buf, 1, 31, pFile);
      if(numRead <= 0) return -1;
      fclose(pFile);

      return ::atoll(buf);
    };

    auto kHz = numberFromFile("/sys/devices/system/cpu/cpu0/tsc_freq_khz");
    if(kHz > 0) return 1000 * kHz;
    kHz = numberFromFile("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if(kHz > 0) return 1000 * kHz;

    V1_DEBUGBREAK();
    return 0;
#  else
#    error unknown OS
#  endif
  }();
  return sTicksPerSecond;
}


uint64_t tscStamp() {
  return __rdtsc();
}

#elif defined(V1_OS_POSIX) && defined(V1_ARCH_ARM)

int64_t tscTicksPerSecond() {
  return 1'000'000'000LL;
}

uint64_t tscStamp() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return uint64_t(int64_t(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec);
}
#else
#  error unsupported platform
#endif


/*
 * platform-independent
 */


TscDiff tscDiffFromDblS(double s) {
  return {int64_t(0.5 + s * tscTicksPerSecond())};
}

TscDiff tscDiffFromS(int64_t s) {
  return {s * tscTicksPerSecond()};
}


TscDiff tscDiffFromMs(int64_t ms) {
  const auto divisor = 1'000;
  return {(ms * tscTicksPerSecond() + divisor / 2) / divisor};
}


TscDiff tscDiffFromUs(int64_t us) {
  const auto divisor = 1'000'000;
  return {(us * tscTicksPerSecond() + divisor / 2) / divisor};
}


int64_t toS(TscDiff tscDiff) {
  const auto divisor = tscTicksPerSecond();
  return (tscDiff.mDiff + divisor / 2) / divisor;
}


double toDblS(TscDiff tscDiff) {
  const auto divisor = tscTicksPerSecond();
  return double(tscDiff.mDiff) / divisor;
}


int64_t toMs(TscDiff tscDiff) {
  const auto divisor = (tscTicksPerSecond() + 500) / 1'000;
  return (tscDiff.mDiff + divisor / 2) / divisor;
}


int64_t toUs(TscDiff tscDiff) {
  const auto divisor = (tscTicksPerSecond() + 500'000) / 1'000'000;
  return (tscDiff.mDiff + divisor / 2) / divisor;
}


}  // namespace v1util
