#pragma once

#include "v1util/base/cpppainrelief.hpp"

namespace v1util {
/*! A simple RAII object that sets a variable to a given value for as long as the object exists.
 *
 * The object is reset to its original value on destruction.
 */
template <typename T>
class ValueScope {
 public:
  ValueScope(T& variable, T newValue) : mVariable(variable), mOldValue(mVariable) {
    mVariable = newValue;
  }
  ~ValueScope() { mVariable = mOldValue; }
  V1_NO_CP_NO_MV(ValueScope);

 private:
  T& mVariable;
  T mOldValue;
};

/*! A simple RAII object that treats a variable as ref counter
 *
 * The object incrmeents the ref count on construction and decrements it on destruction.
 * See atomic.hpp for an atomic scope.
 */
template <typename T>
class RefCountingScope;

template <>
class RefCountingScope<bool> {
 public:
  RefCountingScope(bool& variable) : mVariable(variable) { variable = true; }
  ~RefCountingScope() { mVariable = false; }
  V1_NO_CP_NO_MV(RefCountingScope);

 private:
  bool& mVariable;
};

template <typename T>
class RefCountingScope {
 public:
  RefCountingScope(T& variable) : mVariable(variable) { ++variable; }
  ~RefCountingScope() { --mVariable; }
  V1_NO_CP_NO_MV(RefCountingScope);

 private:
  T& mVariable;
};

}  // namespace v1util
