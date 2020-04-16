#pragma once

#include "platform.hpp"

#include <cstddef>
#include <cstdint>


namespace v1util {

/*** TSC: Time Stamp Counter, high-precision/short-duration time stamping and time intervals ***/

// fwd decls:
class TscStamp;
TscStamp tscNow();

//! Return the number of ticks the TSC advances per second
V1_PUBLIC int64_t tscTicksPerSecond();
//! Return the current value of the Time Stamp Counter. Low overhead.
V1_PUBLIC uint64_t tscStamp();

/**
 * Difference between two time stamps of the TSC.
 *
 * Can be converted to and from seconds of wall clock time.
 */
class TscDiff {
 public:
  TscDiff() = default;
  TscDiff(int64_t diff) : mDiff(diff) {}

  friend inline TscDiff operator-(const TscDiff& l, const TscDiff& r) { return l.mDiff - r.mDiff; }
  friend inline TscDiff operator+(const TscDiff& l, const TscDiff& r) { return l.mDiff + r.mDiff; }
  friend inline TscDiff operator*(const TscDiff& l, int64_t r) { return l.mDiff * r; }
  friend inline TscDiff operator*(int64_t l, const TscDiff& r) { return l * r.mDiff; }
  friend inline TscDiff operator/(const TscDiff& l, int64_t r) { return l.mDiff / r; }
  friend inline TscDiff operator-(const TscDiff& l) { return -l.mDiff; }

  friend inline bool operator<(const TscDiff& l, const TscDiff& r) { return l.mDiff < r.mDiff; }
  friend inline bool operator>(const TscDiff& l, const TscDiff& r) { return l.mDiff > r.mDiff; }
  friend inline bool operator<=(const TscDiff& l, const TscDiff& r) { return l.mDiff <= r.mDiff; }
  friend inline bool operator>=(const TscDiff& l, const TscDiff& r) { return l.mDiff >= r.mDiff; }
  friend inline bool operator==(const TscDiff& l, const TscDiff& r) { return l.mDiff == r.mDiff; }
  friend inline bool operator!=(const TscDiff& l, const TscDiff& r) { return l.mDiff != r.mDiff; }

  // special comparisons to zero:
  friend inline bool operator<(const TscDiff& left, const std::nullptr_t&) {
    return left.mDiff < 0;
  }
  friend inline bool operator>(const TscDiff& left, const std::nullptr_t&) {
    return left.mDiff > 0;
  }
  friend inline bool operator<=(const TscDiff& left, const std::nullptr_t&) {
    return left.mDiff <= 0;
  }
  friend inline bool operator>=(const TscDiff& left, const std::nullptr_t&) {
    return left.mDiff >= 0;
  }
  friend inline bool operator==(const TscDiff& left, const std::nullptr_t&) {
    return left.mDiff == 0;
  }
  friend inline bool operator!=(const TscDiff& left, const std::nullptr_t&) {
    return left.mDiff != 0;
  }

  //! Wind back this time difference by @p diff.
  TscDiff& operator-=(const TscDiff& diff) {
    mDiff -= diff.mDiff;
    return *this;
  }

  //! Advance this time difference by @p diff.
  TscDiff& operator+=(const TscDiff& diff) {
    mDiff += diff.mDiff;
    return *this;
  }

  TscDiff& operator*=(int64_t multiplier) {
    mDiff *= multiplier;
    return *this;
  }

  TscDiff& operator/=(int64_t divisor) {
    mDiff /= divisor;
    return *this;
  }

  friend V1_PUBLIC TscDiff tscDiffFromDblS(double us);
  friend V1_PUBLIC TscDiff tscDiffFromS(int64_t us);
  friend V1_PUBLIC TscDiff tscDiffFromMs(int64_t us);
  friend V1_PUBLIC TscDiff tscDiffFromUs(int64_t us);

  friend V1_PUBLIC double toDblS(TscDiff tscDiff);
  friend V1_PUBLIC int64_t toS(TscDiff tscDiff);
  friend V1_PUBLIC int64_t toMs(TscDiff tscDiff);
  friend V1_PUBLIC int64_t toUs(TscDiff tscDiff);

  int64_t raw() const { return mDiff; }

 private:
  int64_t mDiff = 0;
};


/**
 * A time stamp that represents a point in wall clock time. Derived from TSC.
 *
 * The Time Stamp Counter is running at a frequency of the same order of magnitude as the
 * CPU itself. Querying the counter is supposed to have low overhead.
 */
class TscStamp {
 public:
  TscStamp() = default;
  TscStamp(uint64_t initial) : mValue(initial) {}

  //! Set or override this stamp with the current value of the time stamp counter.
  void stamp() { mValue = tscStamp(); }

  //! Return how much wall clock time (s) has passed since the time stamp was set.
  auto diffToNowS() const { return toS(TscDiff(int64_t(tscStamp() - mValue))); }
  //! Return how much wall clock time (ms) has passed since the time stamp was set.
  auto diffToNowMs() const { return toMs(TscDiff(int64_t(tscStamp() - mValue))); }
  //! Return how much wall clock time (us) has passed since the time stamp was set.
  auto diffToNowUs() const { return toUs(TscDiff(int64_t(tscStamp() - mValue))); }

  //! Return the difference in wall clock time between two time stamps.
  friend inline TscDiff operator-(const TscStamp& after, const TscStamp& before) {
    return int64_t(after.mValue - before.mValue);
  }
  //! Return a time stamp that is @p diff in the past from the point in time @pt.
  friend inline TscStamp operator-(const TscStamp& pt, const TscDiff& diff) {
    return uint64_t(pt.mValue - diff.raw());
  }
  //! Return a time stamp that is @p diff in the future from the point in time @pt.
  friend inline TscStamp operator+(const TscStamp& pt, const TscDiff& diff) {
    return uint64_t(pt.mValue + diff.raw());
  }

  //! Wind back this time stamp by @p diff.
  TscStamp& operator-=(const TscDiff& diff) {
    mValue -= diff.raw();
    return *this;
  }

  //! Advance this time stamp by @p diff.
  TscStamp& operator+=(const TscDiff& diff) {
    mValue += diff.raw();
    return *this;
  }

  uint64_t inline raw() const { return mValue; }

  //! Heuristic. Could be false positive, but highly unlikely.
  bool inline isSet() const { return mValue; }

  // @{ Heuristics, assuming time difference < valueRange/2; @see isAtOrAfterInRing
  friend inline bool operator<(const TscStamp& l, const TscStamp& r) {
    return r.mValue != l.mValue && r.mValue - l.mValue < uint64_t(9223372036854775808ULL);
  }
  friend inline bool operator>(const TscStamp& l, const TscStamp& r) {
    return l.mValue != r.mValue && l.mValue - r.mValue < uint64_t(9223372036854775808ULL);
  }
  friend inline bool operator<=(const TscStamp& l, const TscStamp& r) {
    return r.mValue - l.mValue < uint64_t(9223372036854775808ULL);
  }
  friend inline bool operator>=(const TscStamp& l, const TscStamp& r) {
    return l.mValue - r.mValue < uint64_t(9223372036854775808ULL);
  }
  friend inline bool operator==(const TscStamp& l, const TscStamp& r) {
    return l.mValue == r.mValue;
  }
  friend inline bool operator!=(const TscStamp& l, const TscStamp& r) {
    return l.mValue != r.mValue;
  }
  // @}

 private:
  uint64_t mValue = 0U;
};

//! Return a time stamp for the current wall clock time.
inline TscStamp tscNow() {
  return tscStamp();
}

}  // namespace v1util
