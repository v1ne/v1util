#include "format.hpp"

#include "platform.hpp"

#include <cstdarg>
#include <cstdio>

namespace v1util {
int vscprintf(const char* pFormat...) {
  va_list args;
  va_start(args, pFormat);
#ifdef V1_OS_WIN
  return _vscprintf(pFormat, args);
#else
  return vsnprintf(nullptr, 0, pFormat, args);
#endif
}

std::string printf2string(const char* pFormat...) {
  va_list args;
  va_start(args, pFormat);
  va_list argsCopy;
  va_copy(argsCopy, args);

  std::string result;
  result.resize(size_t(vscprintf(pFormat, args)));
#ifndef V1_OS_WIN
  va_end(argsCopy);
#endif

  /*
   * Overwrite trailing \0 that is part of the string by definition (because .c_str() is a valid
   * C string).
   */
  vsnprintf(result.data(), result.size() + 1, pFormat, args);
  va_end(args);

  return result;
}

}  // namespace v1util
