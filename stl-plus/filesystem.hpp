#pragma once

// Ensure that forward-declaring works:
#include "filesystem-fwd.hpp"

// That's what you get for using C++17 features in 2020. :<
#ifdef __GNUG__
#  include <experimental/filesystem>
namespace stdfs = std::experimental::filesystem;
#else
#  include <filesystem>
namespace stdfs = std::filesystem;
#endif

#include <string_view>

namespace v1util {
//! @return the absolute path to the v1util source repository or an empty path.
stdfs::path repoPath();
//! @return the absolute path to the v1util test files or an empty path.
stdfs::path testFilesPath();

//! Generates prefix + (something) + suffix so that the result doesn't exist.
stdfs::path unique_path(stdfs::path prefix, std::string_view suffix);
}  // namespace v1util
