#pragma once

#include "array_view.hpp"
#include "v1util/base/debug.hpp"

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace v1util {

/*
 * A reference to a region of contiguous memory. It's like std::span,
 * but only for non-const elements and more simple.
 *
 * Caveat: Never ever capture temporary values via auto x = make_span({1, 2, 3, 4})!
 */
template <typename T>
class Span {
 public:
  static_assert(
      std::is_same_v<std::remove_const_t<T>, T>, "Span is only meant for non-const elements");
  using value_type = T;
  using pointer = value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using iterator_category = std::random_access_iterator_tag;
  using iterator = pointer;
  using const_iterator = const value_type*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  Span() {}

  Span(pointer pBuf, std::size_t numElements) : mpBegin(pBuf), mpEnd(pBuf + numElements) {}

  template <typename Iterator>
  Span(Iterator iBegin, Iterator iEnd) {
    static_assert(
        std::is_same_v<
            std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<decltype(&*iBegin)>>>,
            decltype(&*iBegin)>,
        "Span is only meant for non-const elements");
    V1_ASSERT(iBegin <= iEnd);
    if constexpr(std::is_same_v<std::decay_t<Iterator>, pointer>) {
      mpBegin = iBegin;
      mpEnd = iEnd;
    } else {
      const auto size = iEnd - iBegin;
      if(size) {
        mpBegin = &*iBegin;
        mpEnd = mpBegin + size;
      }
    }
  }

  template <typename Container>
  explicit Span(Container& c) : Span(c.begin(), c.end()) {}

#ifdef V1_DEBUG
  ~Span() { mpBegin = mpEnd = nullptr; }
#endif

  Span(const Span&) = default;
  Span& operator=(const Span&) = default;
  Span& operator=(std::nullptr_t) {
    mpBegin = mpEnd = nullptr;
    return *this;
  }

#ifdef V1_DEBUG
  Span(Span&& src) noexcept : mpBegin(src.mpBegin), mpEnd(src.mpEnd) {
    src.mpBegin = src.mpEnd = nullptr;
  }

  Span& operator=(Span&& src) noexcept {
    mpBegin = src.mpBegin;
    mpEnd = src.mpEnd;
    src.mpBegin = src.mpEnd = nullptr;

    return *this;
  }
#else
  Span(Span&&) noexcept = default;
  Span& operator=(Span&&) noexcept = default;
#endif

  void swap(Span& other) noexcept {
    const auto copy = *this;
    *this = other;
    other = copy;
  }

  iterator begin() const { return mpBegin; }
  const_iterator cbegin() const { return mpBegin; }
  iterator end() const { return mpEnd; }
  const_iterator cend() const { return mpEnd; }

  reverse_iterator rbegin() const { return std::make_reverse_iterator(mpEnd); }
  const_reverse_iterator crbegin() const { return std::make_reverse_iterator(mpEnd); }
  reverse_iterator rend() const { return std::make_reverse_iterator(mpBegin); }
  const_reverse_iterator crend() const { return std::make_reverse_iterator(mpBegin); }

  pointer data() const { return mpBegin; }
  std::size_t size() const { return mpEnd - mpBegin; }
  bool empty() const { return mpEnd == mpBegin; }
  reference front() const {
    V1_ASSERT(mpBegin);
    return *mpBegin;
  }
  reference back() const {
    V1_ASSERT(mpBegin);
    return *(mpEnd - 1);
  }
  reference operator[](size_type offset) const {
    const auto pEntry = mpBegin + offset;
    V1_ASSERT(pEntry < mpEnd);
    return *pEntry;
  }

  //! return span for first @p count elements
  Span first(size_type count) const {
    V1_ASSERT(count <= size());
    return {mpBegin, mpBegin + count};
  }

  //! return span, skipping first @p count elements
  Span skip(size_type count) const {
    V1_ASSERT(count <= size());
    return {mpBegin + count, mpEnd};
  }

  //! return span for last @p count elements
  Span last(size_type count) const {
    const auto numElements = size();
    V1_ASSERT(count <= numElements);
    return {mpBegin + numElements - count, mpEnd};
  }

  //! return span, shrunk by @p count elements from the back
  Span shrink(size_type count) const {
    V1_ASSERT(count <= size());
    return {mpBegin, mpEnd - count};
  }

  Span subspan(size_type start, size_type count) const {
#ifdef V1_DEBUG
    const auto numEntries = size();
    V1_ASSERT(start <= numEntries && count <= numEntries && start + count <= numEntries);
#endif
    return {mpBegin + start, mpBegin + start + count};
  }

  ArrayView<T> view() const { return {mpBegin, mpEnd}; }
  operator ArrayView<T>() const { return {mpBegin, mpEnd}; }

  bool operator==(const Span& other) const {
    return mpBegin == other.mpBegin && mpEnd == other.mpEnd;
  }

  bool operator!=(const Span& other) const { return !operator==(other); }

  bool operator<(const Span& other) const {
    return mpBegin < other.mpBegin || (mpBegin == other.mpBegin && mpEnd < other.mpEnd);
  }

 private:
  T* mpBegin = nullptr;
  T* mpEnd = nullptr;
};

template <typename Container>
inline auto make_span(Container& c) {
  return Span<typename Container::value_type>(c);
}

template <typename Iterator>
inline auto make_span(Iterator iBegin, Iterator iEnd) {
  return Span<typename std::iterator_traits<Iterator>::value_type>(
      std::move(iBegin), std::move(iEnd));
}

template <typename Element>
inline auto make_span(Element* pStart, std::size_t size) {
  return Span<Element>(pStart, size);
}

}  // namespace v1util
