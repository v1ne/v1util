#include "peakfinder.hpp"

#include "v1util/container/array_view.hpp"

#include "doctest/doctest.h"

#include <vector>


namespace v1util { namespace dsp { namespace test {
const auto sMultiPeakSequence = {
    /* 0 */ 0.0f,
    /* 1 */ 1.0f,
    /* 2 */ 1.0f,
    /* 3 */ 1.0f,
    /* 4 */ 0.8f,
    /* 5 */ 0.7f,
    /* 6 */ 0.5f,
    /* 7 */ 0.5f,
    /* 8 */ 0.5f,
    /* 9 */ 3.0f,
    /* A */ 0.0f,
    /* B */ 4.0f,
    /* C */ 0.0f,
    /* D */ 2.0f,
    /* E */ 0.5f,
    /* F */ 1.0f};


std::vector<size_t> runPeakFinderOn(ArrayView<float> input,
    size_t lockoutDistance,
    size_t blockSize,
    float peakThreshold = 0.f,
    size_t rounds = 1) {
  std::vector<size_t> peaks;

  BufferedPeakFinder bpf(2 * lockoutDistance);
  size_t inputPos = 0;
  for(size_t round = 0; round < rounds; ++round) {
    if(inputPos + blockSize > input.size()) inputPos = 0 - (input.size() - inputPos);

    while(input.size() >= inputPos + blockSize) {
      inputPos += blockSize;
      auto view = round == 0 ? input.first(inputPos) : input;
      auto result = bpf.process(view, inputPos, blockSize, peakThreshold);
      if(result.iPeak != view.end()) {
        const auto peakPos = size_t(result.iPeak - view.begin());
        const auto peakPos2 = inputPos >= result.distanceToEndOfCurrentBlock
                                  ? inputPos - result.distanceToEndOfCurrentBlock
                                  : input.size() + inputPos - result.distanceToEndOfCurrentBlock;
        CHECK(peakPos == peakPos2);
        peaks.emplace_back(peakPos);
      }
    }
  }

  if(auto remainder = input.size() - inputPos; remainder > 0) {
    inputPos += remainder;
    auto result = bpf.process(input, inputPos, remainder, peakThreshold);
  }

  return peaks;
}


void checkPeakPositionsMatch(const ArrayView<int> expected, const std::vector<size_t>& actual) {
  V1_ASSERT(expected.size() == actual.size());
  for(size_t i = 0; i < expected.size(); ++i) V1_ASSERT(size_t(expected[i]) == actual[i]);
}

TEST_CASE("findPeak") {
  auto view = make_array_view(sMultiPeakSequence);

  CHECK(findPeak(view.begin() + 0x0, view.begin() + 0x10) == view.begin() + 0xB);  // global max
  CHECK(findPeak(view.begin() + 0x8, view.begin() + 0x0A) == view.begin() + 0x9);  // tiny max
  CHECK(findPeak(view.begin() + 0x9, view.begin() + 0x0B) == view.begin() + 0x9);  // tiny max
  CHECK(findPeak(view.begin() + 0x9, view.begin() + 0x0C) == view.begin() + 0xB);  // tiny max
  CHECK(findPeak(view.begin() + 0x9, view.begin() + 0x0E) == view.begin() + 0xB);  // tiny max
  CHECK(findPeak(view.begin() + 0x1, view.begin() + 0x04) == view.begin() + 0x4);  // no max
  CHECK(findPeak(view.begin() + 0x3, view.begin() + 0x09) == view.begin() + 0x3);  // first
  CHECK(findPeak(view.begin() + 0x0, view.begin() + 0x04) == view.begin() + 0x2);  // plateau
  CHECK(findPeak(view.begin() + 0x1, view.begin() + 0x05) == view.begin() + 0x2);  // plateau
  CHECK(findPeak(view.begin() + 0x0, view.begin() + 0x05) == view.begin() + 0x2);  // plateau
  CHECK(findPeak(view.begin() + 0x7, view.begin() + 0x0C) == view.begin() + 0xB);  // last
  CHECK(findPeak(view.begin() + 0xE, view.begin() + 0x10) == view.begin() + 0xF);  // last
}

TEST_CASE("findPeak-longSlope") {
  auto slope = {0.1f, 0.2f, 0.3f, 0.4f, 0.8f, 0.85f, 0.8f, 0.7f, 0.6f, 0.5f, 0.1f};
  auto view = make_array_view(slope);
  CHECK(*findPeak(view.begin(), view.end()) == 0.85f);
}


TEST_CASE("findPeakWithLockout") {
  auto view = make_array_view(sMultiPeakSequence);

  CHECK(findPeakWithLockout(view.begin() + 0x8, view.begin() + 0x0D, 2)
        == view.begin() + 0xD);  // outside
  CHECK(findPeakWithLockout(view.begin() + 0x8, view.begin() + 0x0E, 3)
        == view.begin() + 0xE);  // just outside
  CHECK(findPeakWithLockout(view.begin() + 0x9, view.begin() + 0x0F, 3)
        == view.begin() + 0xB);  // just inside
  CHECK(findPeakWithLockout(view.begin() + 0x0, view.begin() + 0x09, 4)
        == view.begin() + 0x2);  // inside
}

TEST_CASE("bufferedPeakFinder-basic") {
  auto view = make_array_view(sMultiPeakSequence);

  BufferedPeakFinder pf{6};
  CHECK(pf.process(view, 0x3, 3, 0.f).iPeak == view.end());          // no peak
  CHECK(pf.process(view, 0x6, 3, 0.f).iPeak == view.begin() + 0x2);  // plateau
  CHECK(pf.process(view, 0x9, 3, 0.f).iPeak == view.end());          // no peak, too close to end
  CHECK(pf.process(view, 0xC, 3, 0.f).iPeak == view.end());          // no peak, too close to end
  CHECK(pf.process(view, 0xF, 3, 0.f).iPeak == view.begin() + 0xB);  // peak

  CHECK(pf.process(view, 0x2, 3, 0.f).iPeak == view.end());          // 0xD blocked by previous peak
  CHECK(pf.process(view, 0x5, 3, 0.f).iPeak == view.begin() + 0xF);  // greedy: 1st peak
  CHECK(pf.process(view, 0x8, 3, 0.f).iPeak == view.begin() + 0x2);  // plateau, size 2
  CHECK(pf.process(view, 0xB, 3, 0.f).iPeak == view.end());          // blocked by previous peak
  CHECK(pf.process(view, 0xE, 3, 0.f).iPeak == view.end());          // blocked by next peak
}

TEST_CASE("bufferedPeakFinder-threshold") {
  auto view = make_array_view(sMultiPeakSequence);

  BufferedPeakFinder pf{6};
  CHECK(pf.process(view, 0x3, 3, 0.f).iPeak == view.end());          // no peak
  CHECK(pf.process(view, 0x6, 3, 1.1f).iPeak == view.end());         // peak too low
  CHECK(pf.process(view, 0x9, 3, 2.f).iPeak == view.end());          // no peak, too close to end
  CHECK(pf.process(view, 0xC, 3, 2.f).iPeak == view.end());          // no peak, too close to end
  CHECK(pf.process(view, 0xF, 3, 3.f).iPeak == view.begin() + 0xB);  // peak

  CHECK(pf.process(view, 0x2, 3, 0.f).iPeak == view.end());          // 0xD blocked by previous peak
  CHECK(pf.process(view, 0x5, 3, 0.f).iPeak == view.begin() + 0xF);  // greedy: 1st peak
  CHECK(pf.process(view, 0x8, 3, 1.1f).iPeak == view.end());         // peak too low
  CHECK(pf.process(view, 0xB, 3, 0.f).iPeak == view.end());          // blocked by next peak
  CHECK(pf.process(view, 0xE, 3, 0.f).iPeak == view.end());          // blocked by next peak
}

TEST_CASE("bufferedPeakFinder-longSequence") {
  auto peaks = runPeakFinderOn(make_array_view(sMultiPeakSequence), 8, 1, 0.f, 2);
  checkPeakPositionsMatch(make_array_view({
                              0xB,
                          }),
      peaks);
}


}}}  // namespace v1util::dsp::test
