#pragma once

#include "v1util/base/cpppainrelief.hpp"

#include <atomic>

namespace v1util {
/*! A simple RAII object that sets a variable to a given value for as long as the object exists.
 *
 * The object is reset to its original value on destruction.
 */
template <typename AtomicT>
class AtomicValueScope {
 public:
  AtomicValueScope(AtomicT& variable, typename AtomicT::value_type newValue)
      : mVariable(variable), mOldValue(mVariable.load(std::memory_order_acquire)) {
    mVariable.store(newValue, std::memory_order_release);
  }
  ~AtomicValueScope() { mVariable.store(mOldValue, std::memory_order_release); }
  V1_NO_CP_NO_MV(AtomicValueScope);

 private:
  AtomicT& mVariable;
  typename AtomicT::value_type mOldValue;
};

/*! A simple RAII object that treats a variable as ref counter
 *
 * The object incrmeents the ref count on construction and decrements it on destruction.
 * See scope.hpp for non-atomic variants.
 */
template <typename AtomicT>
class AtomicRefCountingScope {
 public:
  AtomicRefCountingScope(AtomicT& variable) : mVariable(variable) {
    mVariable.fetch_add(1, std::memory_order_acq_rel);
  }
  ~AtomicRefCountingScope() { mVariable.fetch_sub(1, std::memory_order_acq_rel); }
  V1_NO_CP_NO_MV(AtomicRefCountingScope);

 private:
  AtomicT& mVariable;
};

}  // namespace v1util
