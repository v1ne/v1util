#pragma once

#include "platform.hpp"

#include <string>

namespace v1util {
V1_PUBLIC std::string printf2string(const char* pFormat...);

//! return the number of characters that the formatted string will need
V1_PUBLIC int vscprintf(const char* pFormat...);
}  // namespace v1util
