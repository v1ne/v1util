#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/cppvtable.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/platform.hpp"

#include <type_traits>
#include <utility>


namespace v1util {

/*! A non-owning reference to a type-erased function
 *
 * Sometimes this is called a delegate, sometimes it's a FunctionView, non-owning callback, ...
 *
 * This reference tries to be lean and fast. It achieves this quite well, but it has to work around
 * some specialties of MSVC.
 *
 * If you bind to virtual methods on classes, behaviour during destruction of the classes is
 * not clearly defined: Once the original target of the delegate is destroyed, either the derived
 * or base class method may be called.
 *
 * Be careful about lambdas: Delegate does not extend the lifetime of a lambda! This is invalid:
 * auto x = Delegate<...>([&]() { return foo; });
 * process(x); // -> Probably crashes when calling x(). The referenced lambda went out of scope.
 *
 * The only valid pattern is: process(Delegate<...>([&]() { return foo; });
 * In this case, the delegate may again not be stored beyond the end of the statement.
 */
template <typename Signature>
class Delegate;

template <typename RetType, typename... Args>
class Delegate<RetType(Args...)> {
 public:
  using result_type = RetType;

  V1_DEFAULT_CP_MV_CTOR(Delegate);

  // clang-format off

  //! Construct from static function pointer
  Delegate(RetType (*pFreeFunction)(Args...)) noexcept {
    mpObject = (void*)pFreeFunction;
    mpTrampoline = [](void* pFunction, Args... args) {
      return (*(RetType(*)(Args...))pFunction)(std::forward<Args>(args)...);
    };
  }

  /*! Construct from a Callable
   *
   * This is one of the few exceptions where a single-argument non-explicit constructor
   * is used.
   */
  template <typename Invocable, typename =
    std::enable_if_t<std::is_invocable_r_v<RetType, Invocable, Args...>
      && !std::is_same_v<std::decay_t<Invocable>, Delegate>>>
  Delegate(Invocable&& invocable) noexcept {
    mpObject = (void*)std::addressof(invocable);
    mpTrampoline = [](void* pFunction, Args... args) {
      return (*(std::add_pointer_t<Invocable>)pFunction)(std::forward<Args>(args)...);
    };
  }

  //! Create from static function pointer
  static inline Delegate bind(RetType (*pFreeFunction)(Args...)) noexcept {
    return Delegate((void*)pFreeFunction, [](void* pFunction, Args... args) {
      return (*(RetType(*)(Args...))pFunction)(std::forward<Args>(args)...);
    });
  }

  //! Create from a Callable
  template <typename Invocable, typename = std::enable_if_t<std::is_invocable_r_v<RetType,
     Invocable, Args...> && !std::is_same_v<std::decay_t<Invocable>, Delegate>>>
  static inline Delegate bind(Invocable&& invocable) noexcept {
    return Delegate((void*)std::addressof(invocable), [](void* pFunction, Args... args) {
      return (*(std::add_pointer_t<Invocable>)pFunction)(std::forward<Args>(args)...);
    });
  }

  // clang-format on


  /*! Bind to a member function.
   *
   * It tries to avoid jumping through an additional trampoline, unless it's forced to,
   * on Windows.
   */
  template <auto pMemFn, typename Class>
  static inline Delegate bind(Class* pClass) noexcept {
    using MemberClass = std::remove_pointer_t<decltype(getMemberClassPtr(pMemFn))>;

#if defined(V1_OS_WIN)
    /*
     * https://docs.microsoft.com/de-de/cpp/build/x64-calling-convention?view=vs-2019
     * For every return value that is not passed by value, pRetVal comes first, pThis comes
     * second. This is contrary to what happens on SYSV and binds type erasure and a uniform
     * function call ABI harder.
     *
     * It is not easy to discern the different return type classes of the calling convention
     * in C++. That is because C++ type_traits are only a poor match to what is required by
     * the calling convention. To be on the safe side, generate an interposer for every
     * non-arithmetic return type. See the test for all the different cases.
     */

    if constexpr(std::is_void_v<RetType> || std::is_arithmetic_v<RetType>) {
      return bindMemFnFast<MemberClass, pMemFn>(pClass);
    } else {
      return bindMemFnSafe<MemberClass, pMemFn>(pClass);
    }
#else
    return bindMemFnFast<MemberClass, pMemFn>(pClass);
#endif
  }


  /*! Bind to a class + member function, going the fast route
   *
   * This means that no trampoline is used, assuming we know where to jump.
   * This requires special support for every platform ABI.
   *
   */
  template <typename Class, RetType (Class::*pMemFn)(Args...)>
  static inline Delegate bindMemFnFast(Class* pClass) noexcept {
#ifdef V1_OS_WIN
    static_assert(std::is_void_v<RetType> || std::is_arithmetic_v<RetType>);
#endif

    auto memFnCall = resolveUntypedMemberFn(pClass, pMemFn);
    return {memFnCall.pObject, (void*)(PTrampolineType)memFnCall.pMemFn};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Delegate bindMemFnFast(Class* pClass) noexcept {
#ifdef V1_OS_WIN
    static_assert(std::is_void_v<RetType> || std::is_arithmetic_v<RetType>);
#endif

    auto memFnCall = resolveUntypedMemberFn(pClass, pMemFn);
    return {memFnCall.pObject, (void*)(PTrampolineType)memFnCall.pMemFn};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Delegate bindMemFnFast(const Class* pClass) noexcept {
#ifdef V1_OS_WIN
    static_assert(std::is_void_v<RetType> || std::is_arithmetic_v<RetType>);
#endif
    using NonConstClass = std::remove_const_t<Class>;
    auto memFnCall = resolveUntypedMemberFn<NonConstClass>((NonConstClass*)pClass, pMemFn);
    return {memFnCall.pObject, (void*)(PTrampolineType)memFnCall.pMemFn};
  }


  /*! Construct from a class + member function, always giving standard-conformant behaviour
   *
   * This binding calls the expected target, as in standard C++. Not the fastest,
   * but the safest.
   */
  template <typename Class, RetType (Class::*pMemFn)(Args...)>
  static inline Delegate bindMemFnSafe(Class* pClass) noexcept {
    using RealFuncType = RetType (*)(void*, Args...);
    const auto pTrampoline = (void*)(RealFuncType)[](void* pThis, Args... args)->RetType {
      return (((Class*)pThis)->*pMemFn)(std::forward<Args>(args)...);
    };
    return {(void*)pClass, pTrampoline};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Delegate bindMemFnSafe(Class* pClass) noexcept {
    using RealFuncType = RetType (*)(void*, Args...);
    const auto pTrampoline = (void*)(RealFuncType)[](void* pThis, Args... args)->RetType {
      return (((Class*)pThis)->*pMemFn)(std::forward<Args>(args)...);
    };
    return {(void*)pClass, pTrampoline};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Delegate bindMemFnSafe(const Class* pClass) noexcept {
    using RealFuncType = RetType (*)(void*, Args...);
    const auto pTrampoline = (void*)(RealFuncType)[](void* pThis, Args... args)->RetType {
      return (((const Class*)pThis)->*pMemFn)(std::forward<Args>(args)...);
    };
    return {(void*)pClass, pTrampoline};
  }


  ~Delegate() {
#ifdef V1_DEBUG
    mpObject = (void*)uintptr_t(~0ULL);
    mpTrampoline = (PTrampolineType)uintptr_t(~0ULL);
#endif
  }

  void swap(Delegate& other) noexcept {
    auto tmp = other;
    other = *this;
    *this = tmp;
  }

  decltype(auto) inline operator()(Args... args) const {
    V1_ASSUME(mpObject);
    V1_ASSUME(mpTrampoline);
    return mpTrampoline(mpObject, std::forward<Args>(args)...);
  }

/*
 * Equality comparisons would follow these rules:
 * - Two Delegates created on the same static function are equal.
 * - Two Delegates created on the same lambda are unrelated.
 * - Two Delegates created on the same member function with "bindMemFnFast" are equal.
 * - Two Delegates created on the same member function with "bindMemFnSafe" are unrelated.
 * - Two Delegates created on the same member function with "bind" are equal,
     except on Windows(*) where they can be unrelated.
 * - Two Delegates created on different entities are unequal, except if they have the
 *   same address. Your compiler or linker might unify identical but different
 *   functions, breaking this behaviour.
 *
 * "Unrelated" means that the result of the equality comparison is not specified.
 *
 * (*): Only true on non-Windows platforms. On Windows, due to a peculiarity of the
 * calling convention, "bindMemFnSafe" is used for complex return types, breaking equality
 * comparability.
 *
 * Due to this convoluted situation, equality comparisons and ordering are not supported.
 * Support can be added by also storing the address of the actual member function, but this
 * would increase the size of Delegate on Windows.
 */
#ifndef V1_DELEGATE_BROKEN_COMPARISONS
  bool operator==(const Delegate& b) = delete;
  bool operator!=(const Delegate& b) = delete;
  bool operator<(const Delegate& b) = delete;
#else
  friend bool operator==(const Delegate& a, const Delegate& b) {
    return a.mpTrampoline == b.mpTrampoline && a.mpObject == b.mpObject;
  }

  friend bool operator!=(const Delegate& a, const Delegate& b) {
    return a.mpTrampoline != b.mpTrampoline || a.mpObject != b.mpObject;
  }

  friend bool operator<(const Delegate& a, const Delegate& b) {
    return a.mpTrampoline < b.mpTrampoline
           || (a.mpTrampoline == b.mpTrampoline && a.mpObject < b.mpObject);
  }
#endif


  operator bool() const noexcept { return mpTrampoline; }
  void* target() const noexcept { return mpObject; }

 private:
  using PTrampolineType = RetType (*)(void*, Args...);

  //! Private setter for bindMemFn*
  Delegate(void* pObject, void* pTrampoline) noexcept {
    mpObject = pObject;
    mpTrampoline = (PTrampolineType)pTrampoline;
  }

  void* mpObject = nullptr;
  PTrampolineType mpTrampoline = nullptr;
};

}  // namespace v1util
