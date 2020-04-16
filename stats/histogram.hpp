#pragma once

#include "v1util/base/debug.hpp"
#include "v1util/container/array_view.hpp"
#include "v1util/container/range.hpp"
#include "v1util/container/span.hpp"

#include <algorithm>
#include <cmath>

namespace v1util { namespace stats {

template <typename T>
struct HistogramBin {
  T start = {};
  T width = {};
  size_t count = 0;
};

template <typename T>
void makeSimpleFloatHistogram(ArrayView<T> in, Span<HistogramBin<T>> outBins) {
  V1_ASSERT(!outBins.empty());

  auto iMinmax = pmrange::minmax_element(in);
  auto min = *iMinmax.first;
  auto max = *iMinmax.second;

  if(outBins.size() == 1) {
    outBins.front() = HistogramBin<T>{min, max - min, in.size()};
    return;
  }

  auto binWidth = (max - min) / T(outBins.size() - 1);
  min -= binWidth / 2;
  max += binWidth / 2;

  auto iBin = outBins.begin();
  for(size_t i = 0; i < outBins.size(); ++i)
    *iBin++ = HistogramBin<T>{min + T(i) * binWidth, binWidth, 0};

  for(const auto& datapoint : in) {
    auto binIdx = size_t(std::floor((datapoint - min) / binWidth));
    outBins[binIdx].count++;
  }
}

}}  // namespace v1util::stats
