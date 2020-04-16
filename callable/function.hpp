#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/cppvtable.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/platform.hpp"

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

#ifdef __GNUG__
#  include <new>  // for placement new
#endif

namespace v1util {

namespace detail {
using PFunctionDeleterFunc = void (*)(void*);
struct FunctionHeapHeader;

class FunctionBase {
 protected:
  static constexpr const auto kStorageAlignment = alignof(PFunctionDeleterFunc);
  static constexpr const auto kStorageSize = 1 * sizeof(uintptr_t);

  FunctionBase() = default;

  FunctionBase(const FunctionBase& other) noexcept { copyObject(other); }

  FunctionBase(FunctionBase&& other) noexcept {
    memcpy(mStorage.data, other.mStorage.data, sizeof(mStorage));

    mpTrampoline = other.mpTrampoline;
    other.mpTrampoline = 0;
  }

  FunctionBase& operator=(const FunctionBase& other) noexcept {
    // keep old object for self-assignment correctness
    auto pOldObject = hasHeapObject() ? mStorage.pObject : nullptr;
    copyObject(other);
    if(pOldObject) freeHeapObject(pOldObject);

    return *this;
  }

  FunctionBase& operator=(FunctionBase&& other) noexcept {
    auto pOldObject = hasHeapObject() ? mStorage.pObject : nullptr;

    memcpy(mStorage.data, other.mStorage.data, sizeof(mStorage));

    mpTrampoline = other.mpTrampoline;
    if(pOldObject) freeHeapObject(pOldObject);
    other.mpTrampoline = 0;
    return *this;
  }

  ~FunctionBase() {
    if(hasHeapObject()) freeHeapObject(mStorage.pObject);
#ifdef V1_DEBUG
    mpTrampoline = uintptr_t(~0ULL);
#endif
  }

  V1_PUBLIC void createHeapObject(size_t objectSize,
      size_t objectAlignment,
      void* pTrampoline,
      PFunctionDeleterFunc pDeleter) noexcept;
  V1_PUBLIC void copyObject(const FunctionBase& other) noexcept;
  V1_PUBLIC static void freeHeapObject(void* pHeapObject) noexcept;
  void setTrampoline(void* pTrampoline, bool hasHeapObject, bool hasEmbeddedStorage) {
    mpTrampoline = uintptr_t(pTrampoline) | (hasHeapObject ? kHasHeapObjectMask : 0)
                   | (hasEmbeddedStorage ? kUseAddressOfStorage : 0);
  }

  inline bool hasHeapObject() const noexcept { return mpTrampoline & kHasHeapObjectMask; }
  inline bool hasEmbeddedStorage() const noexcept { return mpTrampoline & kUseAddressOfStorage; }
  inline void* storage() const noexcept {
    return hasEmbeddedStorage() ? (void*)&mStorage.data : mStorage.pObject;
  }
  inline void* trampoline() const noexcept {
    return (void*)(mpTrampoline & ~kTrampolineStolenBitsMask);
  }
  V1_PUBLIC size_t _refCount() const noexcept;

  union {
    void* pObject = {};  // pointer to heap allocation or static data/function ptr
    alignas(kStorageAlignment) uint8_t data[kStorageSize];  // embedded storage for small Lambdas
  } mStorage = {};

  static_assert(alignof(PFunctionDeleterFunc) >= 4,
      "Storing additional data in mpTrampoline requires alignment of functions pointers");
  static constexpr const uintptr_t kHasHeapObjectMask = 0b01;
  static constexpr const uintptr_t kUseAddressOfStorage = 0b10;
  static constexpr const uintptr_t kTrampolineStolenBitsMask = 0b11;

  //! Trampoline function or target function (plus bits of storage):
  uintptr_t mpTrampoline = 0;
};
}  // namespace detail


/*! An owning reference to a type-erased function.
 *
 * Like std::function as of C++17, just with reduced functionality. Differences:
 * - move operations may+do not throw
 * - could support equality comparison with caveats
 * - small-Lambda optimization is obvious and guaranteed
 *
 * Lambdas are stored inside iff:
 * - Lambda size fits kStorageSize, alignment doesn't exceed that of a function pointer
 * - the Lambda is trivially copyable and destructable
 *   (caveat: copyable is not met on MSVC 2019; thus, on Windows, Lambdas are copied
 *    onto the heap)
 *
 * References to static functions and member functions are always stored inside.
 *
 */
template <typename Signature>
class Function;

template <typename RetType, typename... Args>
class Function<RetType(Args...)> : detail::FunctionBase {
 public:
  using result_type = RetType;

  Function() = default;
  V1_DEFAULT_CP_MV(Function);

  //! Construct from static function pointer
  Function(RetType (*pFreeFunction)(Args...)) noexcept {
    auto pTrampoline = (void*)(PTrampolineType)[](void* pFunction, Args... args) {
      return (*(RetType(*)(Args...))pFunction)(std::forward<Args>(args)...);
    };

    setTrampoline(pTrampoline, !K(hasHeapObject), !K(hasEmbeddedStorage));
    mStorage.pObject = (void*)pFreeFunction;
  }

  // clang-format off
  //! Construct from a Callable
  template <typename Invocable, typename =
    std::enable_if_t<std::is_invocable_r_v<RetType, Invocable, Args...>
      && !std::is_same_v<std::decay_t<Invocable>, Function>>>
  Function(Invocable&& invocable) noexcept {
    using BareInvocable = std::remove_reference_t<std::remove_cv_t<Invocable>>;
    auto pTrampoline = (void*) (PTrampolineType)[](void* pInvocable, Args... args) {
      return (*(std::add_pointer_t<Invocable>)pInvocable)(std::forward<Args>(args)...);
    };

    if constexpr(std::is_trivially_destructible_v<BareInvocable>
              && std::is_trivially_copyable_v<BareInvocable> // mStorage is only trivially copied
              && sizeof(invocable) <= sizeof(mStorage.data)
              && kStorageAlignment % alignof(BareInvocable) == 0) {
        setTrampoline(pTrampoline, !K(hasHeapObject), K(hasEmbeddedStorage));
        new(mStorage.data) BareInvocable(std::forward<Invocable>(invocable));
    } else {
      // If all fails, just heap-allocate space for the Callable:
      auto pDeleter = (detail::PFunctionDeleterFunc)[](void* pInvokable){
        auto pLambda = (BareInvocable*)pInvokable;
        pLambda->~BareInvocable();
      };

      createHeapObject(sizeof(BareInvocable), alignof(BareInvocable), pTrampoline, pDeleter);
      new(mStorage.pObject) BareInvocable(std::forward<Invocable>(invocable));
    }
  }
  // clang-format on


  /*! Bind to a member function.
   *
   * It tries to avoid jumping through an additional trampoline, unless it's forced to,
   * on Windows.
   */
  template <auto pMemFn, typename Class>
  static inline Function bind(Class* pClass) noexcept {
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
  static inline Function bindMemFnFast(Class* pClass) noexcept {
#ifdef V1_OS_WIN
    static_assert(std::is_void_v<RetType> || std::is_arithmetic_v<RetType>);
#endif

    auto memFnCall = resolveUntypedMemberFn(pClass, pMemFn);
    return {memFnCall.pObject, (void*)(PTrampolineType)memFnCall.pMemFn};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Function bindMemFnFast(Class* pClass) noexcept {
#ifdef V1_OS_WIN
    static_assert(std::is_void_v<RetType> || std::is_arithmetic_v<RetType>);
#endif

    auto memFnCall = resolveUntypedMemberFn(pClass, pMemFn);
    return {memFnCall.pObject, (void*)(PTrampolineType)memFnCall.pMemFn};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Function bindMemFnFast(const Class* pClass) noexcept {
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
  static inline Function bindMemFnSafe(Class* pClass) noexcept {
    using RealFuncType = RetType (*)(void*, Args...);
    const auto pTrampoline = (void*)(RealFuncType)[](void* pThis, Args... args)->RetType {
      return (((Class*)pThis)->*pMemFn)(std::forward<Args>(args)...);
    };
    return {(void*)pClass, pTrampoline};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Function bindMemFnSafe(Class* pClass) noexcept {
    using RealFuncType = RetType (*)(void*, Args...);
    const auto pTrampoline = (void*)(RealFuncType)[](void* pThis, Args... args)->RetType {
      return (((Class*)pThis)->*pMemFn)(std::forward<Args>(args)...);
    };
    return {(void*)pClass, pTrampoline};
  }

  template <typename Class, RetType (Class::*pMemFn)(Args...) const>
  static inline Function bindMemFnSafe(const Class* pClass) noexcept {
    using RealFuncType = RetType (*)(void*, Args...);
    const auto pTrampoline = (void*)(RealFuncType)[](void* pThis, Args... args)->RetType {
      return (((const Class*)pThis)->*pMemFn)(std::forward<Args>(args)...);
    };
    return {(void*)pClass, pTrampoline};
  }


  ~Function() = default;

  void swap(Function& other) noexcept {
    auto tmp = other;
    other = *this;
    *this = tmp;
  }

  decltype(auto) operator()(Args... args) const {
    V1_ASSERT(mpTrampoline & ~kTrampolineStolenBitsMask);
    V1_ASSERT(!hasHeapObject() || !hasEmbeddedStorage());
    return typedTrampoline()(storage(), std::forward<Args>(args)...);
  }

  inline operator bool() const noexcept { return mpTrampoline; }
  inline bool isHeapBased() const noexcept { return mpTrampoline && hasHeapObject(); }
  inline size_t _refCount() const noexcept { return FunctionBase::_refCount(); }

 private:
  using PTrampolineType = RetType (*)(void*, Args...);

  //! Private setter for bindMemFn*
  Function(void* pObject, void* pTrampoline) noexcept {
    setTrampoline(pTrampoline, !K(hasHeapObject), !K(hasEmbeddedStorage));
    mStorage.pObject = pObject;
  }

  inline PTrampolineType typedTrampoline() const {
    return (PTrampolineType)(mpTrampoline & ~kTrampolineStolenBitsMask);
  }
};

}  // namespace v1util
