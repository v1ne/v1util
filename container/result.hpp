#pragma once

#include "v1util/base/debug.hpp"

#include <new>
#include <system_error>
#include <type_traits>


namespace v1util {

/** Holder for a value or an error
 *
 * Initialised with an "is empty" error.
 * Provides for chaining of transformations.
 * 
 */
template <typename T>
class Result {
 private:
  template <typename X>
  struct IsResult {
    static constexpr bool value = false;
  };
  template <typename Y>
  struct IsResult<Result<Y>> {
    static constexpr bool value = true;
  };

 public:
  using value_type = T;
  using error_type = std::error_code;

  static_assert(!std::is_same_v<value_type, const T>,
      "For const values, use \"const Result<T>\" instead of \"Result<const T>\"");

  Result() { setEmptyError(); }
  Result(const T& value) : Result(std::in_place, value) {}
  Result(T&& value) noexcept : Result(std::in_place, std::move(value)) {}
  Result(const error_type& error) { new(&mStorage) error_type(error); }
  Result(error_type&& error) noexcept { new(&mStorage) error_type(std::move(error)); }

  template <typename... Args>
  Result(std::in_place_t, Args&&... args) {
    new(&mStorage) T(std::forward<Args>(args)...);
    mHasValue = true;
  }

  Result(const Result& other) {
    if(other.hasValue())
      new(&mStorage) T(other.value());
    else
      new(&mStorage) error_type(other.error());
    mHasValue = other.hasValue();
  }

  Result(Result&& other) {
    mHasValue = other.hasValue();
    if(other.hasValue()) {
      new(&mStorage) T(std::move(other.value()));
      other.mHasValue = false;
      other.destroy();
      other.setEmptyError();
    } else
      new(&mStorage) error_type(other.error());
  }

  ~Result() {
    destroy();
#ifdef V1_DEBUG
    mHasValue = false;
#endif
  }

  Result& operator=(const Result& other) {
    destroy();
    mHasValue = false;
    if(other.hasValue())
      new(&mStorage) T(other.value());
    else
      new(&mStorage) error_type(other.error());
    mHasValue = other.hasValue();
    return *this;
  }

  Result& operator=(Result&& other) noexcept {
    destroy();
    mHasValue = other.hasValue();
    if(other.hasValue()) {
      new(&mStorage) T(std::move(other.value()));
      other.mHasValue = false;
      other.destroy();
      other.setEmptyError();
    } else
      new(&mStorage) error_type(other.error());
    return *this;
  }

  Result& operator=(const T& value) {
    destroy();
    new(&mStorage) T(value);
    mHasValue = true;
    return *this;
  }

  Result& operator=(T&& value) noexcept {
    destroy();
    new(&mStorage) T(std::move(value));
    mHasValue = true;
    return *this;
  }

  Result& operator=(const error_type& value) {
    destroy();
    new(&mStorage) error_type(value);
    mHasValue = false;
    return *this;
  }

  Result& operator=(error_type&& value) noexcept {
    destroy();
    new(&mStorage) error_type(std::move(value));
    mHasValue = false;
    return *this;
  }


  bool hasValue() const { return mHasValue; }
  operator bool() const { return mHasValue; }

  const T& value() const {
    V1_ASSERT(hasValue());
    return *std::launder<const T>(reinterpret_cast<const T*>(&mStorage));
  }
  T& value() {
    V1_ASSERT(hasValue());
    return *std::launder<T>(reinterpret_cast<T*>(&mStorage));
  }

  const error_type& error() const {
    V1_ASSERT(!hasValue());
    return *std::launder<const error_type>(reinterpret_cast<const error_type*>(&mStorage));
  }
  error_type& error() {
    V1_ASSERT(!hasValue());
    return *std::launder<error_type>(reinterpret_cast<error_type*>(&mStorage));
  }

  /** Apply @p functor to the value (if present), yielding its return value or pass on the error
   *
   * If @p functor returns a Result, that Result is returned. If "this" is empty, its error is
   * returned.
   */
  template <typename Functor>
  auto apply(Functor&& functor) const {
    static_assert(
        std::is_invocable_v<Functor, const T&>, "Unable to invoke functor with value type");
    using FunctorReturnT =
        std::remove_cv_t<std::remove_reference_t<std::invoke_result_t<Functor, const T&>>>;
    if constexpr(std::is_void_v<FunctorReturnT>) {
      functor(value());
    } else {
      using ReturnT = std::
          conditional_t<IsResult<FunctorReturnT>::value, FunctorReturnT, Result<FunctorReturnT>>;
      return hasValue() ? ReturnT(functor(value())) : ReturnT(error());
    }
  }

  template <typename Functor>
  auto apply(Functor&& functor) {
    static_assert(std::is_invocable_v<Functor, T&>, "Unable to invoke functor with value type");
    using FunctorReturnT =
        std::remove_cv_t<std::remove_reference_t<std::invoke_result_t<Functor, T&>>>;
    if constexpr(std::is_void_v<FunctorReturnT>) {
      functor(value());
    } else {
      using ReturnT = std::
          conditional_t<IsResult<FunctorReturnT>::value, FunctorReturnT, Result<FunctorReturnT>>;
      return hasValue() ? ReturnT(functor(value())) : ReturnT(error());
    }
  }

 private:
  void destroy() {
    if(hasValue())
      value().~value_type();
    else
      error().~error_type();
  }

  void setEmptyError() { new(&mStorage) error_type(1, std::system_category()); }

  std::aligned_storage_t<std::max(sizeof(value_type), sizeof(error_type)),
      std::max(alignof(value_type), alignof(error_type))>
      mStorage;
  bool mHasValue = false;
};

}  // namespace v1util
