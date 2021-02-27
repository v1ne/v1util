#pragma once
#include <filesystem>
#include "v1util/base/warnings.hpp"

/*
 * Forward-declare std::filesystem::path because <filesystem> includes every part of the STL but the
 * kitchen sink:
 *   algorithm, chrono, iomanip, list, locale, memory, string, string_view, system_error, utility,
 *   vector
 *
 * On GCC libstdc++, using fs_fwd.h could do the trick, too. It's more light-weight, only including
 * chrono and system_error.
 *
 * Caveat emptor: Forward-declaring elements of the STL namespace is forbidden by the standard.
 * Of course. But since this is not a production code base, I can get away with ignoring this
 * instead of wrapping std::filesystem::path into a struct that I can forward-declare... Or just
 * use pointers.
 */

#ifdef __GNUG__

namespace std::filesystem {
inline namespace __cxx11 {
class path;
}

using path = __cxx11::path;
}  // namespace std::filesystem

#else

V1_NO_WARNINGS
namespace std::filesystem {
class path;
}
V1_RESTORE_WARNINGS

#endif
