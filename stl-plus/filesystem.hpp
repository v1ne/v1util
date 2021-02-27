#pragma once

// Ensure that forward-declaring works:
#include "filesystem-fwd.hpp"

#include <string_view>

namespace v1util {
//! @return the absolute path to the v1util source repository or an empty path.
std::filesystem::path repoPath();
//! @return the absolute path to the v1util test files or an empty path.
std::filesystem::path testFilesPath();

//! Generates prefix + (something) + suffix so that the result doesn't exist.
std::filesystem::path unique_path(std::filesystem::path prefix, std::string_view suffix);
}  // namespace v1util
