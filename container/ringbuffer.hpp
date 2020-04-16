#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/container/array_view.hpp"
#include "v1util/container/span.hpp"

namespace v1util {

/*! SPSC queue
 *
 * TODO: use std::atomic acquire/release (currently works only on x64 because of luck)
 */
template <typename T>
class ChunkedRingBuffer {
 public:
  ChunkedRingBuffer() = default;
  ChunkedRingBuffer(const ChunkedRingBuffer& other) = delete;
  ChunkedRingBuffer(ChunkedRingBuffer&& other) noexcept { *this = std::move(other); }

  ChunkedRingBuffer& operator=(const ChunkedRingBuffer& other) = delete;
  ChunkedRingBuffer& operator=(ChunkedRingBuffer&& other) noexcept {
    delete mpData;
    mCapacity = other.mCapacity;
    mpData = other.mpData;
    mpHead = other.mpHead;
    mpTail = other.mpTail;

    other.mpData = nullptr;
#ifdef V1_DEBUG
    other.mCapacity = ~0ULL;
    other.mpData = nullptr;
    other.mpHead = nullptr;
    other.mpTail = nullptr;
#endif

    return *this;
  }


  ~ChunkedRingBuffer() {
    delete mpData;
#ifdef V1_DEBUG
    mCapacity = ~0ULL;
    mpData = nullptr;
    mpHead = nullptr;
    mpTail = nullptr;
#endif
  }

  void setCapacity(uint64_t capacity) {
    delete mpData;

    mCapacity = capacity + 1;
    mpHead = mpTail = mpData = new T[mCapacity]();
  }

  void fillFrom(ArrayView<T> data) {
    const auto count = data.size();
    auto pData = data.data();

    V1_ASSERT(availableSize() >= count);
#ifdef V1_DEBUG
    const auto oldSize = size();
#endif

    const auto pEnd = mpData + mCapacity;
    const auto pNewHead = mpHead + count;
    if(pNewHead <= pEnd) {
      for(uint64_t i = 0; i < count; i++) mpHead[i] = pData[i];

      mpHead = pNewHead == pEnd ? mpData : pNewHead;
    } else {
      const auto numCopyToEnd = uint64_t(pEnd - mpHead);
      for(uint64_t i = 0; i < numCopyToEnd; i++) mpHead[i] = pData[i];
      pData += numCopyToEnd;

      const auto numCopyToStart = uint64_t(count - numCopyToEnd);
      for(uint64_t i = 0; i < numCopyToStart; i++) mpData[i] = pData[i];

      mpHead = mpData + numCopyToStart;
    }

    V1_ASSERT(size() <= oldSize + count);  // not equal because of concurrency
    V1_ASSERT(size() <= capacity());
  }

  void drainTo(Span<T> data) {
    const auto count = data.size();
    auto pData = data.data();

    V1_ASSERT(size() >= count);
#ifdef V1_DEBUG
    const auto oldSize = size();
#endif

    const auto pEnd = mpData + mCapacity;
    const auto pNewTail = mpTail + count;
    if(pNewTail <= pEnd) {
      for(uint64_t i = 0; i < count; i++) pData[i] = mpTail[i];
      mpTail = pNewTail == pEnd ? mpData : pNewTail;
    } else {
      const auto numCopyFromEnd = uint64_t(pEnd - mpTail);
      for(uint64_t i = 0; i < numCopyFromEnd; i++) pData[i] = mpTail[i];
      pData += numCopyFromEnd;

      const auto numCopyFromStart = uint64_t(count - numCopyFromEnd);
      for(uint64_t i = 0; i < numCopyFromStart; i++) pData[i] = mpData[i];

      mpTail = mpData + numCopyFromStart;
    }

    V1_ASSERT(size() >= oldSize - count);  // not equal because of concurrency
    V1_ASSERT(size() <= capacity());
  }

  void push(const T& data) {
    V1_ASSERT(!full());
    const auto pEnd = mpData + mCapacity;
    auto pNewHead = mpHead + 1;
    if(pNewHead == pEnd) pNewHead = mpData;
    *mpHead = data;
    mpHead = pNewHead;
  }

  T pop() {
    V1_ASSERT(mpHead != mpTail);
    const auto pEnd = mpData + mCapacity;
    const auto pNewTail = mpTail + 1;
    T data = *mpTail;
    mpTail = pNewTail == pEnd ? mpData : pNewTail;
    return data;
  }

  T& peek() {
    V1_ASSERT(mpHead != mpTail);
    return *mpTail;
  }

  T* peekPtr() { return mpHead != mpTail ? mpTail : nullptr; }

  void drop() {
    V1_ASSERT(mpHead != mpTail);
    const auto pEnd = mpData + mCapacity;
    const auto pNewTail = mpTail + 1;
    mpTail = pNewTail >= pEnd ? mpData : pNewTail;
  }

  T& get() {
    V1_ASSERT(mpHead != mpTail);
    const auto pEnd = mpData + mCapacity;
    const auto pNewTail = mpTail + 1;
    T& data = *mpTail;
    mpTail = pNewTail >= pEnd ? mpData : pNewTail;
    return data;
  }

  //! returns how much free space is occupied in the buffer
  uint64_t size() const {
    const auto pHead = mpHead;
    const auto pTail = mpTail;
    return pHead >= pTail ? pHead - pTail : mCapacity - (pTail - pHead);
  }

  //! returns how much free space is left in the buffer
  inline uint64_t availableSize() const { return capacity() - size(); }
  inline uint64_t capacity() const { return mCapacity - 1; }
  inline bool empty() const { return mpHead == mpTail; }
  inline bool full() const { return size() == capacity(); }

 protected:
  uint64_t mCapacity = 0;
  T* mpData = nullptr;
  T* mpHead = nullptr;
  T* mpTail = nullptr;
};


/*! Bare-bones deque that doesn't dynamically allocate memory */
template <typename T>
class FixedSizeDeque : public ChunkedRingBuffer<T> {
  using ChunkedRingBuffer<T>::mCapacity;
  using ChunkedRingBuffer<T>::mpHead;
  using ChunkedRingBuffer<T>::mpTail;
  using ChunkedRingBuffer<T>::mpData;
  using ChunkedRingBuffer<T>::full;

 public:
  inline T& back() { return ChunkedRingBuffer<T>::peek(); }
  inline const T& back() const {
    V1_ASSERT(mpHead != mpTail);
    return *mpTail;
  }
  inline void pop_back() { return ChunkedRingBuffer<T>::drop(); }
  inline void push_back(const T& data) {
    V1_ASSERT(!full());
    auto pNewTail = (mpTail == mpData ? mpData + mCapacity : mpTail) - 1;
    *pNewTail = data;
    mpTail = pNewTail;
  }
  template <typename... Args>
  inline void emplace_back(Args... args) {
    V1_ASSERT(!full());
    auto pNewTail = (mpTail == mpData ? mpData + mCapacity : mpTail) - 1;
    *pNewTail = T{args...};
    mpTail = pNewTail;
  }

  inline T& front() {
    V1_ASSERT(mpHead != mpTail);
    auto pWrappedHead = mpHead == mpData ? mpData + mCapacity : mpHead;
    return *(pWrappedHead - 1);
  }
  inline void pop_front() {
    V1_ASSERT(mpHead != mpTail);
    auto pNewHead = (mpHead == mpData ? mpData + mCapacity : mpHead) - 1;
    mpHead = pNewHead;
  }
  inline void push_front(const T& data) { ChunkedRingBuffer<T>::push(data); }
};


/*! An iterator that treats the underlying container as a ring
 *
 * This means that you can walk indefinitely into both directions (until int64_t is exceeded),
 * without hitting the end. It is useful if you don't want to model the underlying container
 * as a ring buffer.
 */
template <typename Iter>
class RingIterator {
 public:
  using iterator_category = typename std::iterator_traits<Iter>::iterator_category;
  using difference_type = int64_t;
  using value_type = typename std::iterator_traits<Iter>::value_type;
  using pointer = typename std::iterator_traits<Iter>::pointer;
  using reference = typename std::iterator_traits<Iter>::reference;

  RingIterator() = default;
  RingIterator(Iter iBegin, Iter iEnd)
      : mOffset(0), mSize(iEnd - iBegin), mIter(iBegin), mBegin(iBegin) {}

  V1_DEFAULT_CP_MV(RingIterator);

  RingIterator& operator++() {
    ++mOffset;
    ++mIter;
    if(mIter == mBegin + mSize) mIter = mBegin;
    return *this;
  }

  RingIterator operator++(int) {
    auto old = *this;
    operator++();
    return old;
  }

  RingIterator& operator--() {
    --mOffset;
    if(mIter == mBegin) mIter = mBegin + mSize;
    --mIter;
    return *this;
  }

  RingIterator operator--(int) {
    auto old = *this;
    operator--();
    return old;
  }

  RingIterator& operator+=(int64_t distance) {
    mOffset += distance;
    const auto simpleDistance = mOffset % int64_t(mSize);
    mIter = simpleDistance < 0 ? mBegin + mSize + simpleDistance : mBegin + simpleDistance;
    return *this;
  }

  RingIterator& operator-=(int64_t distance) { return operator+=(-distance); }

  RingIterator operator+(int64_t distance) const {
    auto iRing = *this;
    iRing += distance;
    return iRing;
  }

  RingIterator operator-(int64_t distance) const { return operator+(-distance); }

  int64_t operator-(const RingIterator& other) const { return int64_t(mOffset - other.mOffset); }
  bool operator==(const RingIterator& other) const { return other.mOffset == mOffset; }
  bool operator!=(const RingIterator& other) const { return other.mOffset != mOffset; }


  decltype(auto) operator*() { return *mIter; }
  decltype(auto) operator*() const { return *mIter; }

  auto base() const { return mIter; }

 private:
  int64_t mOffset = 0U;
  size_t mSize = 0U;
  Iter mIter = {};
  Iter mBegin = {};
};

}  // namespace v1util
