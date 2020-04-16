#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"

#include <algorithm>
#include <numeric>


/*! Poor Man's Range routines
 *
 * A stop-gap measure to make C++ more bearable by writing less code that deals
 * with individual iterators.
 *
 * The algorithmy work with rangey types, like Range, ArrayView, Span or containers.
 */

namespace v1util { namespace pmrange {
template <typename I>
//! Right-open range of iterators
class ItRange {
 public:
  ItRange() {}
  ItRange(const I& begin, const I& end) : mBegin(begin), mEnd(end) {}
  ItRange(I&& begin, I&& end) : mBegin(std::move(begin)), mEnd(std::move(end)) {}
  V1_DEFAULT_CP_MV(ItRange);

  // @{ getters
  auto begin() const { return mBegin; }
  auto end() const { return mEnd; }

  bool empty() const { return mBegin == mEnd; }
  bool size() const { return mEnd - mBegin; }
  auto front() const { return *mBegin; }
  auto back() const { return *(mEnd - 1); }
  // @}

  // @{ subranges, can be expensive
  ItRange first(size_t count) const {
    V1_ASSERT(count <= size());
    return {mBegin, mBegin + count};
  }
  ItRange last(size_t count) const {
    V1_ASSERT(count <= size());
    return {mBegin + count, mEnd};
  }
  ItRange subrange(size_t start, size_t count) const {
#ifdef V1_DEBUG
    const auto numEntries = size();
    V1_ASSERT(start <= numEntries && count <= numEntries && start + count <= numEntries);
#endif
    const auto newStart = mBegin + start;
    return {newStart, newStart + count};
  }
  // @}

  // @{ data members
  I mBegin = {};
  I mEnd = {};
  // @}
};

template <typename Container>
auto makeRange(Container& container) {
  return ItRange<typename Container::iterator>{container.begin(), container.end()};
}

template <typename Container>
auto makeConstRange(const Container& container) {
  return ItRange<typename Container::const_iterator>{container.begin(), container.end()};
}

/************ own  A L G O R I T H M S , in alphabetical order *****************/

template <typename Range, typename Element>
inline bool contains(const Range& haystack, const Element& needle) {
  for(const auto& straw : haystack)
    if(straw == needle) return true;
  return false;
}

template <typename InRange, typename OutRange, typename T>
void copyScaled(const InRange& in, OutRange&& out, T scale) {
  V1_ASSERT(out.size() >= in.size());
  auto iOut = out.begin();
  for(const auto& inElement : in) {
    *iOut = inElement * scale;
    iOut++;
  }
}

//! Returns the index of @p needle in @p haystack or the number of elements if not found
template <typename Range, typename Element>
inline auto indexOf(const Range& haystack, const Element& needle) {
  return Range::size_type(std::find(haystack.begin(), haystack.end(), needle) - haystack.begin());
}

//! Returns the index of @p needle in @p haystack or the number of elements if not found
template <typename Range, typename Predicate>
inline auto indexOfFirst(const Range& haystack, Predicate pred) {
  return Range::size_type(std::find_if(haystack.begin(), haystack.end(), pred) - haystack.begin());
}


/*** Range version of  S T L  A L G O R I T H M S , in alphabetical order ***/

template <class Range, class Predicate>
bool all_of(Range&& range, Predicate pred) {
  return std::find_if_not(range.begin(), range.end(), std::forward<Predicate>(pred)) == range.end();
}

template <class Range, class Predicate>
bool any_of(Range&& range, Predicate pred) {
  return std::find_if(range.begin(), range.end(), std::forward<Predicate>(pred)) != range.end();
}

template <class Range, class T>
T accumulate(Range&& range, T init) {
  return std::accumulate(range.begin(), range.end(), std::move(init));
}

template <typename InRange, typename OutRange>
void copy(const InRange& in, OutRange&& out) {
  V1_ASSERT(out.size() >= in.size());
  auto iOut = out.begin();
  for(const auto& inElement : in) {
    *iOut = inElement;
    iOut++;
  }
}

template <class Range, class Predicate>
size_t count_if(Range&& range, Predicate pred) {
  return (size_t)std::count_if(range.begin(), range.end(), std::forward<Predicate>(pred));
}

template <typename RangeA, typename RangeB>
inline bool equal(RangeA&& rangeA, RangeB&& rangeB) {
  return std::equal(rangeA.begin(), rangeA.end(), rangeB.begin(), rangeB.end());
}

template <typename Range, typename T>
inline void fill(Range&& out, const T& value) {
  for(auto& element : out) element = value;
}

template <typename Range>
inline auto min_element(Range&& range) {
  return std::min_element(range.begin(), range.end());
}

template <typename Range>
inline auto minmax_element(Range&& range) {
  return std::minmax_element(range.begin(), range.end());
}

template <typename Range>
inline auto max_element(Range&& range) {
  return std::max_element(range.begin(), range.end());
}

template <class Range, class Predicate>
bool none_of(Range&& range, Predicate pred) {
  return std::find_if(range.begin(), range.end(), std::forward<Predicate>(pred)) == range.end();
}

}}  // namespace v1util::pmrange
