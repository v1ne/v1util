#pragma once

#include "v1util/base/warnings.hpp"

/*
 * Forward-declare std::filesystem::path because <filesystem> includes every part of the STL but the
 * kitchen sink:
 *   algorithm, chrono, iomanip, list, locale, memory, string, string_view, system_error, utility,
 *   vector
 *
 * Caveat emptor: Forward-declaring elements of the STL namespace is forbidden by the standard.
 * Of course. But since this is not a production code base, I can get away with ignoring this
 * instead of wrapping std::filesystem::path into a struct that I can forward-declare... Or just
 * use pointers.
 */

#ifdef __GNUG__

namespace std::experimental::filesystem { inline namespace v1 { inline namespace __cxx11 {
class path;
}}}  // namespace std::experimental::filesystem::v1::__cxx11
namespace stdfs = std::experimental::filesystem;

#else

V1_NO_WARNINGS
namespace std::filesystem {
class path;
}
V1_RESTORE_WARNINGS
namespace stdfs = std::filesystem;

#endif
