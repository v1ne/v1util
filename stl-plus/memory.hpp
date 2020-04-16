#pragma once

/* The missing parts of STL's <memory> */

#include <memory>

namespace v1util {
//! Cast a unique_ptr to another type, yielding a unique_ptr
template <typename To, typename From>
std::unique_ptr<To> unique_cast(From pFrom) {
  const auto pTo = dynamic_cast<To*>(pFrom.get());
  if(!pTo) return nullptr;

  pFrom.release();
  return std::unique_ptr<To>(pTo);
}

}  // namespace v1util
