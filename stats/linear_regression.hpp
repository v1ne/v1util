#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/math.hpp"
#include "v1util/container/range.hpp"

#include <numeric>
#include <type_traits>


namespace v1util { namespace stats {

//! Coefficients for y(x) = ax + b
template <typename T>
struct StraitCoefficients {
  T a = {};
  T b = {};

  T at(T x) const { return a * x + b; }

  bool operator==(const StraitCoefficients& o) const { return a == o.a && b == o.b; }
  bool operator!=(const StraitCoefficients& o) const { return a != o.a && b != o.b; }
  bool operator<(const StraitCoefficients& o) const { return a < o.a || (a == o.a && b < o.b); }
};

//! Estimate coefficients a, b for y(x) = ax + b
template <typename Container>
auto linearRegression(const Container& dataX, const Container& dataY) {
  using T = std::remove_cv_t<typename Container::value_type>;
  V1_ASSERT(dataX.size() == dataY.size());

  T avgX;
  T avgY;
  if constexpr(std::is_integral_v<T>) {
    avgX = roundIntDiv(pmrange::accumulate(dataX, T{0}), T(dataX.size()));
    avgY = roundIntDiv(pmrange::accumulate(dataY, T{0}), T(dataY.size()));
  } else {
    avgX = pmrange::accumulate(dataX, T{0}) / T(dataX.size());
    avgY = pmrange::accumulate(dataY, T{0}) / T(dataY.size());
  }

  T deltaXYSum = 0;
  T deltaXSumSq = 0;
  auto iX = dataX.begin();
  auto iY = dataY.begin();
  for(size_t i = 0; i < dataX.size(); ++i) {
    auto deltaX = *iX - avgX;
    auto deltaY = *iY - avgY;
    deltaXYSum += deltaX * deltaY;
    deltaXSumSq += deltaX * deltaX;
  }

  if(deltaXSumSq == T{0}) {
    V1_INVALID();
    return StraitCoefficients<T>{};
  }

  T a;
  if constexpr(std::is_integral_v<T>)
    a = roundIntDiv(deltaXYSum, deltaXSumSq);
  else
    a = deltaXYSum / deltaXSumSq;

  T b = avgY - a * avgX;
  return StraitCoefficients<T>{a, b};
}


//! Estimate coefficients a, b for y(x) = ax + b, with exponential averaging of measurements
template <typename T>
class LinearSeriesEstimator {
 public:
  LinearSeriesEstimator() = default;
  V1_DEFAULT_CP_MV(LinearSeriesEstimator);

  //! Set alpha as used in exponential smoothing
  void setAlpha(T alpha) { mAlpha = alpha; }

  void feed(T x, T y) {
    if(x != mLastX) {
      T avgX = (x + mLastX) / 2;
      T avgY = (y + mLastY) / 2;
      auto a = ((x - avgX) * (y - avgY) + (mLastX - avgX) * (mLastY - avgY))
               / (squared(x - avgX) + squared(mLastX - avgX));
      auto b = avgY - a * avgX;

      applyExpAvg(&mCoefficients.a, mAlpha, a);
      applyExpAvg(&mCoefficients.b, mAlpha, b);
    }

    mLastX = x, mLastY = y;
  }

  template <typename Container>
  void feedMany(const Container& dataX, const Container& dataY) {
    V1_ASSERT(dataX.size() == dataY.size());

    if(dataX.size() == 1) {
      feed(dataX.front(), dataY.front());
      return;
    }

    auto newCoeff = linearRegression(dataX, dataY);
    auto alpha = T(dataX.size()) * mAlpha;
    applyExpAvg(&mCoefficients.a, alpha, newCoeff.a);
    applyExpAvg(&mCoefficients.b, alpha, newCoeff.b);

    mLastX = dataX.back();
    mLastY = newCoeff.at(mLastX);
  }

  StraitCoefficients<T> currentCoefficients() const { return mCoefficients; }

 private:
  static_assert(std::is_floating_point_v<T>);
  StraitCoefficients<T> mCoefficients;
  T mAlpha = 1;
  T mLastX = {}, mLastY = {};
};

}}  // namespace v1util::stats
