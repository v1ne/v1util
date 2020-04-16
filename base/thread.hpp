#pragma once

#include "platform.hpp"

#include <string_view>
#include <thread>

namespace v1util {

V1_PUBLIC void sleepMs(unsigned int dT);
V1_PUBLIC void yield();

//! The thread without pitfalls
class Thread : public std::thread {
 public:
  Thread() = default;
  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  //! will join automatically
  ~Thread() {
    if(joinable()) join();
  }

  template <typename Function, typename... Args>
  Thread(Function&& f, Args&&... args) : std::thread(f, args...) {}

  Thread(Thread&&) noexcept = default;
  Thread& operator=(Thread&& other) noexcept {
    if(joinable()) join();
    std::thread::operator=(std::move(other));
    return *this;
  }

  void setName(std::string_view name);

  // TODO: Add support for priorities
};
}  // namespace v1util
