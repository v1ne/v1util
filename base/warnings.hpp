#pragma once

#include "platform.hpp"

#if defined(_MSC_VER)
#  define V1_NO_WARNINGS __pragma(warning(push, 1));

#  define V1_NO_WARN_TRUNCATE_CONSTS   \
    __pragma(warning(push));           \
    __pragma(warning(disable : 4310)); \
    __pragma(warning(disable : 26450));

#  define V1_NO_WARN_SELF_ASSIGNMENT __pragma(warning(push, 1));

#  define V1_RESTORE_WARNINGS __pragma(warning(pop));

#elif defined(__clang__)

#  define V1_NO_WARNINGS _Pragma("clang diagnostic push")

#  define V1_NO_WARN_TRUNCATE_CONSTS _Pragma("clang diagnostic push")
#  define V1_NO_WARN_SELF_ASSIGNMENT  \
    _Pragma("clang diagnostic push"); \
    _Pragma("clang diagnostic ignored \"-Wself-assign-overloaded\"")

#  define V1_RESTORE_WARNINGS _Pragma("clang diagnostic pop")

#elif defined(__GNUG__)

#  define V1_NO_WARNINGS _Pragma("GCC diagnostic push")

#  define V1_NO_WARN_TRUNCATE_CONSTS _Pragma("GCC diagnostic push")
#  define V1_NO_WARN_SELF_ASSIGNMENT _Pragma("GCC diagnostic push")

#  define V1_RESTORE_WARNINGS _Pragma("GCC diagnostic pop")
#endif
