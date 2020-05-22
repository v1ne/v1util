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
