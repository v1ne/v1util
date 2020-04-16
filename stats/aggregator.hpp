#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/container/array_view.hpp"
#include "v1util/container/range.hpp"

#include <algorithm>
#include <cfloat>
#include <numeric>


namespace v1util { namespace stats {

struct DblMinMaxAvg {
  size_t count = 0;
  double min = 0.;
  double avg = 0.;
  double max = 0.;
};

class MinAvgMaxAggregator {
 public:
  MinAvgMaxAggregator() = default;
  V1_DEFAULT_CP_MV(MinAvgMaxAggregator);

  template <typename T>
  void feed(v1util::ArrayView<T> elements) {
    mGlobalCount += elements.size();
    mGlobalSum = pmrange::accumulate(elements, mGlobalSum);
    auto minmax = pmrange::minmax_element(elements);
    mGlobalMin = std::min(double(*minmax.first), mGlobalMin);
    mGlobalMax = std::max(double(*minmax.second), mGlobalMax);
  }

  bool hasStats() const { return mGlobalCount > 0; }

  DblMinMaxAvg stats() const {
    V1_ASSERT(hasStats());
    return {mGlobalCount, mGlobalMin, mGlobalSum / double(mGlobalCount), mGlobalMax};
  }


  void _overrideData(size_t count, double sum, double max, double min) {
    mGlobalCount = count;
    mGlobalSum = sum;
    mGlobalMax = max;
    mGlobalMin = min;
  }

 private:
  size_t mGlobalCount = 0;
  double mGlobalSum = 0;
  double mGlobalMax = -DBL_MAX;
  double mGlobalMin = DBL_MAX;
};


}}  // namespace v1util::stats
