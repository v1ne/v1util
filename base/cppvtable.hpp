#pragma once

#include "platform.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace v1util {
struct UntypedMemberFnCall {
  void* pObject;
  void* pMemFn;
};

template <typename Class, typename MemberClass, typename MemberFunc>
UntypedMemberFnCall resolveUntypedMemberFn(
    Class* pClass, MemberFunc MemberClass::*memfun) noexcept {
  static_assert(
      std::is_base_of_v<MemberClass, Class>, "Member function must belong to class or base");
  auto pMemberClass = static_cast<MemberClass*>(pClass);
#ifdef V1_ARCH_X86
#  ifdef V1_OS_WIN
  if constexpr(sizeof(memfun) == sizeof(void*)) {
    return UntypedMemberFnCall{(void*)pMemberClass, (void*)*(uintptr_t*)(void*)&memfun};
  } else if constexpr(sizeof(memfun) == 2 * sizeof(void*)) {
    // pMemberClass has multiple base classes
    struct FuncAndOffset {
      void* pTarget;
      int thisOffset;
    };
    auto pFuncAndOffset = (const FuncAndOffset*)(void*)&memfun;
    return UntypedMemberFnCall{
        (void*)((intptr_t)pMemberClass + pFuncAndOffset->thisOffset), pFuncAndOffset->pTarget};
  } else {
    static_assert(false, "unsupported inheritance mode");
  }
#  elif defined(V1_OS_POSIX)
  static_assert(sizeof(memfun) == 2 * sizeof(void*));
  // Itanium ABI is used on most Unix systems
  struct ItaniumMemFn {
    intptr_t pFuncOrOffset;
    size_t thisOffset;
  };
  auto pMemFn = (const ItaniumMemFn*)(void*)&memfun;
  if(pMemFn->pFuncOrOffset & 1) {
    // function is a virtual function
    auto pVtbl = *(void***)pMemberClass;
    return UntypedMemberFnCall{(void*)((intptr_t)pMemberClass + pMemFn->thisOffset),
        (pVtbl)[(pMemFn->pFuncOrOffset - 1) / sizeof(void*)]};
  } else {
    // function is non-virtual
    return UntypedMemberFnCall{
        (void*)((intptr_t)pMemberClass + pMemFn->thisOffset), (void*)pMemFn->pFuncOrOffset};
  }
#  else
#    error unsupported OS
#  endif
#elif defined(V1_ARCH_ARM) && defined(V1_OS_POSIX)
  static_assert(sizeof(memfun) == 2 * sizeof(void*));
  struct MemFnLayout {
    intptr_t pFuncOrOffset;
    intptr_t isVirtual;
  };
  auto pMemFn = (const MemFnLayout*)(void*)&memfun;
  void* pFunction;
  if(pMemFn->isVirtual) {
    auto pVtbl = *(void***)pMemberClass;
    pFunction = (pVtbl)[(pMemFn->pFuncOrOffset) / sizeof(void*)];
  } else {
    pFunction = (void*)pMemFn->pFuncOrOffset;
  }
  return UntypedMemberFnCall{(void*)((intptr_t)pMemberClass), pFunction};
#else
#  error unsupported CPU architecture
#endif
}


//! Template to determine the class from a member function
template <typename MemberClass, typename MemFuncSignature>
MemberClass* getMemberClassPtr(MemFuncSignature MemberClass::*pMemFn) {
  return nullptr;
}

}  // namespace v1util
