#include "function.hpp"

#include "v1util/base/bitop.hpp"
#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/platform.hpp"

#include <algorithm>
#include <atomic>

#ifdef V1_OS_WIN
#  include <malloc.h>
#else
#  include <cstdlib>
#endif

namespace v1util { namespace detail {

namespace {

constexpr const auto kValidHeapBlockMarker = 0x1337CAFE;
constexpr const auto kDeletedHeapBlockMarker = 0x66D00D66;

struct FunctionHeapHeader {
  FunctionHeapHeader() = default;
  V1_NO_CP_NO_MV(FunctionHeapHeader);

  PFunctionDeleterFunc pDeleter = nullptr;
  size_t objectSize = 0;
  size_t objectAlignment = 0;
  std::atomic_uint_fast32_t refCount = 1;
#ifdef V1_DEBUG
  int canary = kValidHeapBlockMarker;
#endif
};

FunctionHeapHeader* headerFromHeapObj(void* pHeapObject) noexcept {
  return ((FunctionHeapHeader*)pHeapObject) - 1;
};

void* mallocHeapObject(size_t objectSize, size_t objectAlignment) noexcept {
  V1_ASSERT(isPow2(objectAlignment) && isPow2(alignof(FunctionHeapHeader)));
  auto allocAlignment = std::max(objectAlignment, alignof(FunctionHeapHeader));
  auto allocSize = alignUp(objectSize + sizeof(FunctionHeapHeader), allocAlignment);

#ifdef V1_OS_WIN
  auto pAlloc = _aligned_malloc(allocSize, allocAlignment);
#else
  auto pAlloc = std::aligned_alloc(allocAlignment, allocSize);
#endif

  auto pHeader = new(pAlloc) FunctionHeapHeader();
  pHeader->objectSize = objectSize;
  pHeader->objectAlignment = objectAlignment;
  return pHeader + 1;
}
}  // namespace


size_t FunctionBase::_refCount() const noexcept {
  return hasHeapObject() ? headerFromHeapObj(mStorage.pObject)->refCount.load() : 0;
}

void FunctionBase::createHeapObject(size_t objectSize, size_t objectAlignment, void* pTrampoline,
    PFunctionDeleterFunc pDeleter) noexcept {
  mStorage.pObject = mallocHeapObject(objectSize, objectAlignment);
  setTrampoline(pTrampoline, K(hasHeapObject), !K(hasEmbeddedStorage));

  auto pHeader = headerFromHeapObj(mStorage.pObject);
  pHeader->pDeleter = pDeleter;
}

void FunctionBase::copyObject(const FunctionBase& other) noexcept {
  if(other.hasHeapObject()) {
    auto pOtherHeader = headerFromHeapObj(other.mStorage.pObject);
    V1_ASSERT(pOtherHeader->canary == kValidHeapBlockMarker);
    pOtherHeader->refCount++;
    mStorage.pObject = other.mStorage.pObject;
  } else
    memcpy(mStorage.data, other.mStorage.data, sizeof(mStorage.data));

  mpTrampoline = other.mpTrampoline;
}


void FunctionBase::freeHeapObject(void* pHeapObject) noexcept {
  auto pHeader = headerFromHeapObj(pHeapObject);
  V1_ASSERT(pHeader->canary == kValidHeapBlockMarker);

  if(--pHeader->refCount > 0) return;

#ifdef V1_DEBUG
  pHeader->canary = kDeletedHeapBlockMarker;
#endif

  if(pHeader->pDeleter) pHeader->pDeleter(pHeapObject);
  pHeader->~FunctionHeapHeader();

  auto pAlloc = pHeader;
#ifdef V1_OS_WIN
  _aligned_free(pAlloc);
#else
  std::free(pAlloc);
#endif
}

}}  // namespace v1util::detail
