#include "thread.hpp"

#include "debug.hpp"
#include "platform.hpp"

#if defined(V1_OS_WIN)
#  include <Windows.h>
#  undef min
#  undef max
#elif defined(V1_OS_POSIX)
#  if defined(V1_OS_LINUX)
#    ifndef _GNU_SOURCE
#      define _GNU_SOURCE
#    endif
#    include <pthread.h>
#  elif defined(V1_OS_FREEBSD)
#    include <pthread_np.h>
#  endif
#  include <sched.h>
#  include <unistd.h>
#  include <cstring>
#endif

namespace v1util {


/*
 * platform-dependent:
 */

#if defined(V1_OS_WIN)

void sleepMs(unsigned int dT) {
  ::Sleep(dT);
}

void yield() {
  ::Sleep(0);
}

void Thread::setName(std::string_view name) {
  if(name.empty()) {
    ::SetThreadDescription(::GetCurrentThread(), L"");
    return;
  }

  const auto nameSize = int(name.size() + 1);
  auto pWName = (wchar_t*)malloc(2 * nameSize);
  if(pWName && ::MultiByteToWideChar(CP_UTF8, 0, name.data(), nameSize, pWName, nameSize)) {
    ::SetThreadDescription(::GetCurrentThread(), pWName);
  } else
    V1_INVALID();
  free(pWName);
}

#elif defined(V1_OS_POSIX)

void sleepMs(unsigned int dT) {
  ::usleep(1000 * dT);
}

void yield() {
  ::sched_yield();
}

void Thread::setName(std::string_view name) {
  auto pBuf = (char*)alloca(name.size() + 1);
  memcpy(pBuf, name.data(), name.size());
  pBuf[name.size()] = '\0';
#  if defined(V1_OS_LINUX)
  ::pthread_setname_np(pthread_self(), pBuf);
#  elif defined(V1_OS_FREEBSD)
  ::pthread_set_name_np(pthread_self(), pBuf);
#  endif
}

#else
#  error unknown platform
#endif

}  // namespace v1util
