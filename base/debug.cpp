#include "debug.hpp"

#include "platform.hpp"

#if defined(V1_OS_WIN)
#  include "alloca.hpp"

#  include <Windows.h>
#  undef min
#  undef max
#  include <malloc.h>
#  include <cstdarg>
#  include <cstdio>
#elif defined(V1_OS_LINUX)
extern "C" {
#  include <sys/stat.h>
#  include <sys/types.h>

#  include <fcntl.h>
#  include <stdio.h>
#  include <unistd.h>
}
#  include <cstdarg>
#  include <cstdlib>
#  include <string_view>
#elif defined(V1_OS_FREEBSD)
extern "C" {
#  include <sys/types.h>

#  include <stdarg.h>
#  include <stdio.h>
#  include <sys/sysctl.h>
#  include <sys/user.h>
#  include <unistd.h>
}
#endif

namespace v1util {

#ifdef V1_OS_WIN
bool isDebuggerPresent() {
  return ::IsDebuggerPresent();
}

void printfToDebugger(const char* pFormat...) {
  va_list args;
  va_start(args, pFormat);

  if(isDebuggerPresent()) {

    const auto bufSize = size_t(_vscprintf(pFormat, args)) + 2;
    const auto pBuf = (char*)V1_ALLOCA(bufSize);
    V1_ASSUME(pBuf);
    auto written = vsnprintf(pBuf, bufSize, pFormat, args);
    *(pBuf + written) = '\n';
    *(pBuf + written + 1) = '\0';
    ::OutputDebugStringA(pBuf);
  }
  va_end(args);
}


#elif defined(V1_OS_LINUX)

bool isDebuggerPresent() {
  int fd = ::open("/proc/self/status", O_RDONLY);
  if(fd < 0) return false;

  constexpr const auto kBufSize = 4096;
  char buf[kBufSize];
  int bytesRead = ::read(fd, buf, kBufSize);
  close(fd);
  if(bytesRead > 0) {
    auto bufView = std::string_view(buf, bytesRead);
    auto tracerPos = bufView.find("TracerPid:");
    if(tracerPos != std::string_view::npos) {
      bufView = bufView.substr(tracerPos + 10);
      auto tracerPid = std::atoll(bufView.data());
      return tracerPid > 0;
    }
  }

  V1_DEBUGBREAK();
  return false;
}

void printfToDebugger(const char* pFormat...) {
  /*
   * isDebuggerPresent() is very expensive on Linux (*SIGH*).
   * Cache the result.
   */
  static bool debuggerIsAttached = isDebuggerPresent();

  va_list args;
  va_start(args, pFormat);
  if(debuggerIsAttached) {
    vfprintf(stderr, pFormat, args);
    fputc('\n', stderr);
  }
  va_end(args);
}


#elif defined(V1_OS_FREEBSD)

bool isDebuggerPresent() {
  struct kinfo_proc procInfo;
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t size = sizeof(procInfo);
  int failure = sysctl(mib, 4, &procInfo, &size, nullptr, 0);
  if(failure || procInfo.ki_structsize != sizeof(procInfo)) {
    V1_DEBUGBREAK();
    return false;
  }

  return procInfo.ki_tracer != 0;
}

void printfToDebugger(const char* pFormat...) {
  va_list args;
  va_start(args, pFormat);
  if(isDebuggerPresent()) {
    vfprintf(stderr, pFormat, args);
    fputc('\n', stderr);
  }
  va_end(args);
}


#endif

}  // namespace v1util
