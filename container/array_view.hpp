#pragma once

#include "v1util/base/debug.hpp"

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace v1util {

/*
 * A view on a region of contiguous memory. It's like std::string_view, but for memory.
 * In contrast to GSL::span, elements may not be modified.
 *
 * Caveat: Never ever capture temporary values via make_array_view({1, 2, 3, 4})!
 */
template <typename T>
class ArrayView {
 public:
  using value_type = const T;
  using pointer = value_type*;
  using reference = value_type&;
  using const_reference = reference;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using iterator_category = std::random_access_iterator_tag;
  using iterator = pointer;
  using const_iterator = pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  static_assert(!std::is_same_v<value_type, T>, "ArrayView may only be used on non-const types");

  ArrayView() {}

  ArrayView(pointer pBuf, std::size_t numElements) : mpBegin(pBuf), mpEnd(pBuf + numElements) {}

  template <typename ConstIterator>
  ArrayView(ConstIterator iBegin, ConstIterator iEnd) {
    V1_ASSERT(iBegin <= iEnd);
    if constexpr(std::is_pointer_v<std::decay_t<ConstIterator>>) {
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
  explicit ArrayView(const Container& c) : ArrayView(c.cbegin(), c.cend()) {}

#ifdef V1_DEBUG
  ~ArrayView() { mpBegin = mpEnd = nullptr; }
#endif

  ArrayView(const ArrayView&) = default;
  ArrayView& operator=(const ArrayView&) = default;
  ArrayView& operator=(std::nullptr_t) {
    mpBegin = mpEnd = nullptr;
    return *this;
  }

#ifdef V1_DEBUG
  ArrayView(ArrayView&& src) noexcept : mpBegin(src.mpBegin), mpEnd(src.mpEnd) {
    src.mpBegin = src.mpEnd = nullptr;
  }

  ArrayView& operator=(ArrayView&& src) noexcept {
    mpBegin = src.mpBegin;
    mpEnd = src.mpEnd;
    src.mpBegin = src.mpEnd = nullptr;

    return *this;
  }
#else
  ArrayView(ArrayView&&) noexcept = default;
  ArrayView& operator=(ArrayView&&) noexcept = default;
#endif

  void swap(ArrayView& other) noexcept {
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
  const_reference front() const {
    V1_ASSERT(mpBegin);
    return *mpBegin;
  }
  const_reference back() const {
    V1_ASSERT(mpBegin);
    return *(mpEnd - 1);
  }
  const_reference operator[](size_type offset) const {
    const auto pEntry = mpBegin + offset;
    V1_ASSERT(pEntry < mpEnd);
    return *pEntry;
  }

  //! return view for first @p count elements
  ArrayView first(size_type count) const {
    V1_ASSERT(count <= size());
    return {mpBegin, mpBegin + count};
  }

  //! return view, skipping first @p count elements
  ArrayView skip(size_type count) const {
    V1_ASSERT(count <= size());
    return {mpBegin + count, mpEnd};
  }

  //! return view for last @p count elements
  ArrayView last(size_type count) const {
    const auto numElements = size();
    V1_ASSERT(count <= numElements);
    return {mpBegin + numElements - count, mpEnd};
  }

  //! return view, shrunk by @p count elements from the back
  ArrayView shrink(size_type count) const {
    V1_ASSERT(count <= size());
    return {mpBegin, mpEnd - count};
  }


  ArrayView subview(size_type start, size_type count) const {
#ifdef V1_DEBUG
    const auto numEntries = size();
    V1_ASSERT(start <= numEntries && count <= numEntries && start + count <= numEntries);
#endif
    return {mpBegin + start, mpBegin + start + count};
  }

  bool operator==(const ArrayView& other) const {
    return mpBegin == other.mpBegin && mpEnd == other.mpEnd;
  }

  bool operator!=(const ArrayView& other) const { return !operator==(other); }

  bool operator<(const ArrayView& other) const {
    return mpBegin < other.mpBegin || (mpBegin == other.mpBegin && mpEnd < other.mpEnd);
  }

 private:
  const T* mpBegin = nullptr;
  const T* mpEnd = nullptr;
};

template <typename Container>
inline auto make_array_view(const Container& c) {
  return ArrayView<std::remove_const_t<typename Container::value_type>>(c);
}

template <typename ConstIterator>
inline auto make_array_view(ConstIterator iBegin, ConstIterator iEnd) {
  return ArrayView<std::remove_const_t<typename std::iterator_traits<ConstIterator>::value_type>>(
      std::move(iBegin), std::move(iEnd));
}

template <typename Element>
inline auto make_array_view(const Element* pStart, std::size_t size) {
  return ArrayView<Element>(pStart, size);
}

template <typename Element>
inline auto make_array_view(std::initializer_list<Element> list) {
  return ArrayView<std::remove_const_t<Element>>(list.begin(), list.size());
}

}  // namespace v1util
