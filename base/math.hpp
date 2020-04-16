#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

namespace v1util {

static constexpr double kE = 2.71828182845904523536;
static constexpr float kfE = 2.71828182845904523536f;
static constexpr double kPi = 3.14159265358979323846;
static constexpr float kfPi = 3.14159265358979323846f;
static constexpr double kSqrt2 = 1.41421356237309504880;
static constexpr float kfSqrt2 = 1.41421356237309504880f;


/*! Return the Alpha for a moving average that needs @p stepsToReach63 steps to reach 1-1/e
 *
 * The Alpha leads to a moving average that goes from 0 to 1-1/e (63%) for a unity step (0->1)
 * input in @p stepsToReach63 steps.
 *
 * See: https://en.wikipedia.org/wiki/Exponential_smoothing
 */
template <typename Num>
inline Num alphaForExpAvgFromSteps(Num stepsToReach63) {
  return Num(1) - std::exp(-Num(1) / stepsToReach63);
}

/*! Return the Alpha for a moving average that needs @p stepsToReachTarget steps to reach @p
 * targetAmount
 *
 * The Alpha leads to a moving average that goes from 0 to @p targetAmount for a unity step (0->1)
 * input in @p stepsToReachTarget steps. In other words: It reaches @p targetAmount% of the target
 * value after @p stepsToReachTarget steps.
 *
 * @p targetAmount has to be within (0, 1) for reasonable alphas.
 *
 * See: https://en.wikipedia.org/wiki/Exponential_smoothing
 */
template <typename Num>
inline Num alphaForExpAvgFromStepsToAmount(Num stepsToReachTarget, Num targetAmount) {
  return Num(1 - std::exp(1 / Num(stepsToReachTarget) * std::log(Num(1) - targetAmount)));
}

/*! Apply exponential smoothing / exponential moving average to @p pCurrentValue
 *
 * @p alpha is the exponential smoothing alpha, as usual and as returned by alphaForExpAvgFromSteps
 * or alphaForExpAvgFromSteps. The value @p newValue is incorporated into @p pCurrentValue.
 */
template <typename Num>
inline void applyExpAvg(Num* pCurrentValue, Num alpha, Num newValue) {
  *pCurrentValue = (Num(1) - alpha) * *pCurrentValue + alpha * newValue;
}

//! Return the result of the integer division, rounded towards 1
template <typename Int>
inline Int ceilIntDiv(Int dividend, Int divisor) {
  static_assert(std::is_integral_v<Int>);
  if constexpr(std::is_unsigned_v<Int>) {
    return (dividend + divisor - 1) / divisor;
  } else {
    const auto offset =
        ((dividend > 0) != (divisor > 0)) ? 0 : dividend > 0 ? divisor - 1 : divisor + 1;
    return (dividend + offset) / divisor;
  }
}


/*! Return smaller signed distance a-b for unsigned numbers @p a, @p b.
 *
 * It operates on the unsigned ring and returns the smaller distance between
 * the two, taking the direction into consideration. Actually, it's just subtraction.
 */
template <typename UInt>
inline auto ringDistance(UInt a, UInt b) {
  return std::make_signed_t<UInt>(a - b);
}

/*! probe > reference: Dedice whether @p probe does not precede @p reference in the ring.
 *
 * It is the decision counter-part to @see ringDistance.
 * The ring is split in half at @p reference. The first half starts at
 * @p reference and contains the succeeding ringSize/2-1 values. The second half
 * starts at reference + ringSize/2 and contains the remaining elements.
 *
 * @returns true if @p probe is in the half of the ring starting at @p reference
 */
template <typename UInt>
inline bool isAtOrAfterInRing(UInt reference, UInt probe) {
  static_assert(std::is_unsigned_v<UInt>);

  constexpr const UInt kHalfValueRangeMinusOne = std::numeric_limits<UInt>::max() / 2;
  const UInt forwardDistance = probe - reference;
  return forwardDistance <= kHalfValueRangeMinusOne;
}

//! Returns a < b, looking at both in a ring
template <typename UInt>
inline bool ringLess(UInt a, UInt b) {
  static_assert(std::is_unsigned_v<UInt>);

  constexpr const UInt kHalfValueRangeMinusOne = std::numeric_limits<UInt>::max() / 2;
  const UInt forwardDistance = b - a;
  return a != b && forwardDistance <= kHalfValueRangeMinusOne;
}

//! Returns a <= b, looking at both in a ring
template <typename UInt>
inline bool ringLessEq(UInt a, UInt b) {
  static_assert(std::is_unsigned_v<UInt>);

  constexpr const UInt kHalfValueRangeMinusOne = std::numeric_limits<UInt>::max() / 2;
  const UInt forwardDistance = b - a;
  return forwardDistance <= kHalfValueRangeMinusOne;
}

//! Returns a > b, looking at both in a ring
template <typename UInt>
inline bool ringGreater(UInt a, UInt b) {
  static_assert(std::is_unsigned_v<UInt>);

  constexpr const UInt kHalfValueRangeMinusOne = std::numeric_limits<UInt>::max() / 2;
  const UInt forwardDistance = a - b;
  return a != b && forwardDistance <= kHalfValueRangeMinusOne;
}

//! Returns a >= b, looking at both in a ring
template <typename UInt>
inline bool ringGreaterEq(UInt a, UInt b) {
  static_assert(std::is_unsigned_v<UInt>);

  constexpr const UInt kHalfValueRangeMinusOne = std::numeric_limits<UInt>::max() / 2;
  const UInt forwardDistance = a - b;
  return forwardDistance <= kHalfValueRangeMinusOne;
}


//! Return the rounded result of the integer division
template <typename Int>
inline Int roundIntDiv(Int dividend, Int divisor) {
  static_assert(std::is_integral_v<Int>);
  if constexpr(std::is_unsigned_v<Int>) {
    return (dividend + divisor / 2) / divisor;
  } else {
    const auto offset = (((dividend > 0) != (divisor > 0)) ? -divisor : divisor) / 2;
    return (dividend + offset) / divisor;
  }
}

template <typename Num>
inline int sgn(Num value) {
  return (Num(0) < value) - (value < Num(0));
}

template <typename Num>
inline Num squared(Num value) {
  return value * value;
}

}  // namespace v1util
