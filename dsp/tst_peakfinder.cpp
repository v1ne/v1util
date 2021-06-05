#include "peakfinder.hpp"

#include "v1util/base/math.hpp"
#include "v1util/container/array_view.hpp"

#include "doctest/doctest.h"

// #define V1_PEAKFINDER_FUZZING 1
#ifdef V1_PEAKFINDER_FUZZING
#  include <random>
#endif

#include <sstream>
#include <vector>

namespace v1util {
namespace dsp {
inline bool operator==(const PeakFinderValueAtPos<int>& a, const PeakFinderValueAtPos<int>& b) {
  return a.streamPos == b.streamPos && a.value == b.value;
}

template <typename T>
inline std::ostream& operator<<(std::ostream& oss, const PeakFinderValueAtPos<T>& value) {
  oss << "[@" << value.streamPos << ": " << value.value << ']';
  return oss;
}

inline doctest::String toString(const PeakFinderValueAtPos<int>& x) {
  std::ostringstream oss;
  oss << "[@" << x.streamPos << ": " << x.value << ']';
  return oss.str().c_str();
}
}  // namespace dsp


template <typename T>
inline doctest::String toString(const v1util::ArrayView<T>& view) {
  std::ostringstream oss;
  oss << '[';
  if(!view.empty()) {
    oss << view.front();
    for(const auto element : view.skip(1)) oss << ", " << element;
  }
  oss << ']';
  return oss.str().c_str();
}
}  // namespace v1util


namespace v1util::dsp::test {


/** Slow reference implementation of a streaming peak finder
 *
 * Greedily finds peaks that dominate all other peaks within +/- patternSize/2 samples around it.
 * A peak is either a local maximum, i.e. an element surrounded by a smaller element on both
 * sides, or the middle element of a plateau, i.e. a sequence of elements of the same value
 * surrounded by a smaller element on both sides.
 *
 * A peak P dominates another peak P' if the value of P is bigger than that of P'. P also
 * dominates P' if both have the same value and P comes first.
 */
template <typename Value>
class ReferenceStreamingPeakFinder {
 public:
  ReferenceStreamingPeakFinder() = default;
  ReferenceStreamingPeakFinder(size_t patternSize) : mLockoutDistance(patternSize / 2) {}

  V1_DEFAULT_CP_MV(ReferenceStreamingPeakFinder);

  void reconfigure(size_t patternSize) { *this = ReferenceStreamingPeakFinder(patternSize); }

  //! The reference peak finder has a different signature since it needs to look at older data.
  template <typename View, typename Invokable>
  void process(const View& data,
      size_t blockSizeU,
      size_t offsetAfterCurrentBlock,
      Value peakThreshold,
      Invokable&& peakHandler) {
    V1_ASSERT(mLockoutDistance);
    V1_ASSERT(data.size() >= offsetAfterCurrentBlock);  // need to look at past samples

    const auto blockSize = int64_t(blockSizeU);
    const auto samplesRead = mNumSamplesRead + blockSize;

    for(int64_t i = 0; i < blockSize; ++i) {
      if(++mNumSamplesRead <= mLockoutDistance) continue;

      /*
       * This contraption works by looking at the last element that is part of the lock-out window
       * on the right, locating the peak candidate and scanning the whole lock-out window for a
       * peak. If peak and candidate coincide, it's a proper peak.
       */

      const auto numSamplesLookBack = std::min(2 * mLockoutDistance + 1, mNumSamplesRead);

      auto iRealEnd = RingIterator(data.begin(), data.end()) + offsetAfterCurrentBlock;
      auto iEnd = iRealEnd - (blockSize - 1 - i);
      auto iPeakCandidate = iEnd - (mLockoutDistance + 1);
      auto iBegin = iEnd - numSamplesLookBack;
      V1_ASSERT(iRealEnd - iBegin <= samplesRead);
      V1_ASSERT(iEnd - iBegin <= mNumSamplesRead);

      auto iPeak = findPeak(iBegin, iEnd);
      if(iPeak != iPeakCandidate) continue;
      if(isPlateauTooLong(iPeak, iBegin, iEnd, 2 * mLockoutDistance - 1)) continue;
      if(*iPeak < peakThreshold) continue;

      peakHandler(
          PeakFinderValueAtPos<Value>{offsetAfterCurrentBlock - size_t(iRealEnd - iPeak), *iPeak});
    }
  }

 private:
  int64_t patternLength() const { return 2 * mLockoutDistance + 1; }

  // const:
  int64_t mLockoutDistance = 0;

  // non-const:
  int64_t mNumSamplesRead = 0;
};


const auto sMultiPeakSequence = std::initializer_list<const int>{//
    /* 00 */ 0,
    /* 01 */ 10,
    /* 02 */ 10,  // proper plateau
    /* 03 */ 10,
    /* 04 */ 8,
    /* 05 */ 7,
    /* 06 */ 5,
    /* 07 */ 5,
    /* 08 */ 5,
    /* 09 */ 30,  // dominated by 0x0B
    /* 0A */ 0,
    /* 0B */ 40,  // proper peak
    /* 0C */ 0,
    /* 0D */ 20,  // dominated by 0x0B
    /* 0E */ 5,
    /* 0F */ 10,  // dominated by 0x0D
    /* 10 */ 0,
    /* 11 */ 10,  // dominated by 0x0F
    /* 12 */ 10,  // dominated by 0x11
    /* 13 */ 10,  // dominated by 0x12
    /* 14 */ 8,
    /* 15 */ 7,
    /* 16 */ 5,
    /* 17 */ 5,
    /* 18 */ 5,
    /* 19 */ 30,  // dominated by 0x1B
    /* 1A */ 0,
    /* 1B */ 40,  // proper peak
    /* 1C */ 0,
    /* 1D */ 20,  // dominated by 0x1B
    /* 1E */ 5,
    /* 1F */ 10,  // dominated by 0y1D
    /* 20 */ 0};


void verifyPeaks(ArrayView<int> input, int lockoutDistance, const std::vector<int>& peakPositions) {
  for(int peakPos : peakPositions) {
    const auto iPeak = RingIterator(input.begin(), input.end()) + peakPos;
    const auto iLockoutBegin = iPeak - int64_t(std::min(peakPos, lockoutDistance));
    const auto iLockoutEnd = iPeak + int64_t(lockoutDistance) + 1;

    V1_ASSERT(int(iLockoutEnd - iLockoutBegin)
              == std::min(peakPos, lockoutDistance) + lockoutDistance + 1);
    const auto foundPeak = findPeak(iLockoutBegin, iLockoutEnd);
    CHECK(foundPeak.base() == iPeak.base());
  }
}

template <typename PeakFinderImpl>
std::vector<int> runPeakFinderOn(
    ArrayView<int> input, int lockoutDistance, int blockSize, int peakThreshold = 0) {
  std::vector<int> peaks;

  PeakFinderImpl pf(2 * size_t(lockoutDistance) + 1);
  size_t streamPos = 0;

  while(input.size() >= streamPos + size_t(blockSize)) {
    auto view = input.subview(streamPos, blockSize);

    auto handlePeak = [&](const PeakFinderValueAtPos<int>& peak) {
      CHECK(*(input.begin() + peak.streamPos) == peak.value);
      CHECK(peak.value >= peakThreshold);
      peaks.emplace_back(int(peak.streamPos));
    };

    pf.process(view, streamPos, peakThreshold, handlePeak);
    streamPos += blockSize;
  }

  CHECK(input.size() == streamPos);  // no fractional buffers allowed
  return peaks;
}

template <typename PeakFinderImpl>
std::vector<int> runReferencePeakFinderOn(
    ArrayView<int> input, int lockoutDistance, int blockSize, int peakThreshold = 0) {
  std::vector<int> peaks;

  PeakFinderImpl pf(2 * size_t(lockoutDistance) + 1);
  size_t inputPos = 0;
  if(inputPos + blockSize > input.size()) inputPos = 0 - (input.size() - inputPos);

  while(input.size() >= inputPos + blockSize) {
    inputPos += blockSize;
    auto view = input.first(inputPos);

    auto handlePeak = [&](const PeakFinderValueAtPos<int>& peak) {
      CHECK(peak.streamPos < inputPos);
      CHECK(*(input.begin() + peak.streamPos) == peak.value);
      CHECK(peak.value >= peakThreshold);
      peaks.emplace_back(int(peak.streamPos));
    };

    pf.process(view, blockSize, inputPos, peakThreshold, handlePeak);
  }

  CHECK(input.size() == inputPos);  // no fractional buffers allowed
  return peaks;
}

void runPeakFinderAndCheckPeaks(std::initializer_list<const int> input,
    int lockoutDistance,
    int blockSize,
    std::initializer_list<const int>
        expectedPeaks,
    int peakThreshold = 0) {
  auto inputView = make_array_view(input);
  auto expectedPeaksView = make_array_view(expectedPeaks);

  if(0)
    printfToDebugger("Checking PeakFinders on %s with LOD %i, blockSize %i, thresh %i",
        toString(inputView).c_str(),
        lockoutDistance,
        blockSize,
        peakThreshold);

  // generate and check reference peaks:
  auto referencePeaks = runReferencePeakFinderOn<ReferenceStreamingPeakFinder<int>>(
      inputView, lockoutDistance, blockSize, peakThreshold);
  auto referencePeaksView = make_array_view(referencePeaks);
  if(!pmrange::equal(referencePeaksView, expectedPeaksView))
    CHECK(referencePeaksView == expectedPeaksView);

  verifyPeaks(inputView, lockoutDistance, referencePeaks);

  {
    // generate peaks normally and check aainst reference:
    auto peaks = runPeakFinderOn<StreamingPeakFinder<int>>(
        inputView, lockoutDistance, blockSize, peakThreshold);
    if(!pmrange::equal(referencePeaks, peaks))
      CHECK(make_array_view(referencePeaks) == make_array_view(peaks));
  }

  // make a second run with a huge buffer size:
  if(size_t(blockSize) < input.size()) {
    auto peaksWithHugeBlockSize = runPeakFinderOn<StreamingPeakFinder<int>>(
        inputView, lockoutDistance, int(inputView.size()), peakThreshold);
    if(!pmrange::equal(referencePeaks, peaksWithHugeBlockSize))
      CHECK(make_array_view(referencePeaks) == make_array_view(peaksWithHugeBlockSize));
  }
}

TEST_CASE("findPeak") {
  auto view = make_array_view(sMultiPeakSequence);

  CHECK(findPeak(view.begin() + 0x0, view.begin() + 0x10) == view.begin() + 0xB);  // global max
  CHECK(findPeak(view.begin() + 0x8, view.begin() + 0x0B) == view.begin() + 0x9);  // local max
  CHECK(findPeak(view.begin() + 0x9, view.begin() + 0x0B) == view.begin() + 0xB);  // no max
  CHECK(findPeak(view.begin() + 0x9, view.begin() + 0x0C) == view.begin() + 0xC);  // no max
  CHECK(findPeak(view.begin() + 0x9, view.begin() + 0x0E) == view.begin() + 0xB);  // local max
  CHECK(findPeak(view.begin() + 0x1, view.begin() + 0x04) == view.begin() + 0x4);  // no max
  CHECK(findPeak(view.begin() + 0x3, view.begin() + 0x09) == view.begin() + 0x9);  // no max
  CHECK(findPeak(view.begin() + 0x0, view.begin() + 0x04) == view.begin() + 0x4);  // no max
  CHECK(findPeak(view.begin() + 0x1, view.begin() + 0x05) == view.begin() + 0x5);  // no max
  CHECK(findPeak(view.begin() + 0x0, view.begin() + 0x05) == view.begin() + 0x2);  // plateau
  CHECK(findPeak(view.begin() + 0x7, view.begin() + 0x0D) == view.begin() + 0xB);  // last
}

TEST_CASE("findPeak-longSlope") {
  auto slope = {0.1f, 0.2f, 0.3f, 0.4f, 0.8f, 0.85f, 0.8f, 0.7f, 0.6f, 0.5f, 0.1f};
  auto view = make_array_view(slope);
  CHECK(*findPeak(view.begin(), view.end()) == 0.85f);
}

TEST_CASE("findPeak-monotonic") {
  auto flat = {0.1f, 0.1f, 0.1f, 0.1f};
  auto flatView = make_array_view(flat);
  CHECK(findPeak(flatView.begin(), flatView.end()) == flatView.end());

  auto increasing = {0.1f, 0.2f, 0.3f, 0.4f};
  auto increasingView = make_array_view(increasing);
  CHECK(findPeak(increasingView.begin(), increasingView.end()) == increasingView.end());

  auto decreasing = {0.4f, 0.3f, 0.2f, 0.1f};
  auto decreasingView = make_array_view(decreasing);
  CHECK(findPeak(decreasingView.begin(), decreasingView.end()) == decreasingView.end());
}

TEST_CASE("StreamingPeakDetector") {
  auto runOnSequenceAndCompareResult = [](std::initializer_list<const int> sequence,
                                           int blockSize,
                                           int maxPlateauSize,
                                           std::initializer_list<const int>
                                               expectedOffsets,
                                           int threshold = 0) {
    auto blockSizeU = size_t(blockSize);
    CHECK(sequence.size() % blockSizeU == 0);
    auto sequenceView = make_array_view(sequence);

    std::vector<int> peaks;
    StreamingPeakDetector<int> peakFinder(maxPlateauSize);

    for(size_t offset = 0; offset < sequence.size(); offset += blockSizeU) {
      auto block = sequenceView.subview(offset, blockSizeU);
      peakFinder.process(block, offset, threshold, [&](const PeakFinderRawPeak<int>& rawPeak) {
        if(rawPeak.type != PeakType::kPeak) return;

        CHECK(rawPeak.value == sequenceView[rawPeak.streamPos]);
        peaks.emplace_back(int(rawPeak.streamPos));
      });
    }

    auto expectedView = make_array_view(expectedOffsets);
    auto actualView = make_array_view(peaks);
    if(!pmrange::equal(actualView, expectedView))
      CHECK(actualView == expectedView);  // pointers never match; doctest prints the arrays
  };

  SUBCASE("peak") {
    // no peak (not surrounded by smaller samples):
    runOnSequenceAndCompareResult({1}, 1, 1, {});
    runOnSequenceAndCompareResult({1, 0}, 1, 1, {});
    runOnSequenceAndCompareResult({0, 1}, 1, 1, {});

    // proper peak:
    runOnSequenceAndCompareResult({0, 1, 0}, 1, 1, {1});
    runOnSequenceAndCompareResult({0, 1, 0, 0}, 2, 1, {1});
    runOnSequenceAndCompareResult({0, 1, 0}, 3, 1, {1});

    // proper peak, blocked by threshold:
    runOnSequenceAndCompareResult({0, 1, 0}, 1, 1, {}, 2);
    runOnSequenceAndCompareResult({0, 1, 0, 0}, 2, 1, {}, 2);
    runOnSequenceAndCompareResult({0, 1, 0}, 3, 1, {}, 2);

    // proper peak, passes threshold:
    runOnSequenceAndCompareResult({0, 2, 0}, 1, 1, {1}, 2);
    runOnSequenceAndCompareResult({0, 2, 0, 0}, 2, 1, {1}, 2);
    runOnSequenceAndCompareResult({0, 2, 0}, 3, 1, {1}, 2);

    for(int blockSize : {1, 2, 3, 6}) {
      // Peak finder actually finds peaks in partially strictly monotonic sequences:
      runOnSequenceAndCompareResult({0, 1, 2, 3, 4, 5}, blockSize, 1, {});
      runOnSequenceAndCompareResult({0, 1, 2, 3, 4, 3}, blockSize, 1, {4});
      runOnSequenceAndCompareResult({0, 1, 3, 2, 3, 2}, blockSize, 1, {2, 4});
      runOnSequenceAndCompareResult({0, 4, 3, 2, 1, 0}, blockSize, 1, {1});
      runOnSequenceAndCompareResult({5, 4, 3, 2, 1, 0}, blockSize, 1, {});
    }
  }

  SUBCASE("plateau") {
    // no plateaus (not surrounded by smaller samples):
    runOnSequenceAndCompareResult({1, 1}, 1, 2, {});
    runOnSequenceAndCompareResult({1, 1}, 2, 2, {});
    runOnSequenceAndCompareResult({1, 1, 0}, 1, 2, {});
    runOnSequenceAndCompareResult({1, 1, 0}, 3, 2, {});
    runOnSequenceAndCompareResult({0, 1, 1}, 1, 2, {});
    runOnSequenceAndCompareResult({0, 1, 1}, 3, 2, {});
    runOnSequenceAndCompareResult({2, 1, 1, 0}, 1, 2, {});
    runOnSequenceAndCompareResult({0, 1, 1, 2}, 1, 2, {});

    // proper plateaus:
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 1, 2, {1});
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 2, 2, {1});
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 4, 2, {1});
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 1, 65535, {1});
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 4, 65535, {1});
    runOnSequenceAndCompareResult({0, 1, 1, 1, 1, 1, 0}, 1, 5, {3});
    runOnSequenceAndCompareResult({0, 1, 1, 1, 1, 1, 0}, 7, 5, {3});

    // no plateaus (too long):
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 1, 1, {});
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 2, 1, {});
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 4, 1, {});

    // thresholds:
    runOnSequenceAndCompareResult({0, 1, 1, 0}, 4, 2, {}, 2);
    runOnSequenceAndCompareResult({0, 2, 2, 0}, 4, 2, {1}, 2);
    runOnSequenceAndCompareResult({0, 2, 2, 3, 3, 3, 0, 0}, 8, 3, {4}, 3);

    for(int blockSize : {1, 2, 8}) {
      runOnSequenceAndCompareResult({1, 1, 0, 1, 1, 0, 0, 0}, blockSize, 2, {3});
      runOnSequenceAndCompareResult({0, 1, 1, 0, 1, 1, 0, 0}, blockSize, 2, {1, 4});
      runOnSequenceAndCompareResult({0, 0, 1, 1, 0, 1, 1, 0}, blockSize, 2, {2, 5});
      runOnSequenceAndCompareResult({0, 0, 0, 1, 1, 0, 1, 1}, blockSize, 2, {3});

      runOnSequenceAndCompareResult({0, 1, 1, 1, 0, 1, 1, 0}, blockSize, 2, {5});
      runOnSequenceAndCompareResult({0, 1, 1, 0, 1, 1, 1, 0}, blockSize, 2, {1});

      runOnSequenceAndCompareResult({0, 2, 2, 2, 1, 1, 1, 0}, blockSize, 3, {2});
      runOnSequenceAndCompareResult({0, 1, 2, 2, 2, 1, 1, 0}, blockSize, 3, {3});
      runOnSequenceAndCompareResult({0, 1, 1, 2, 2, 2, 1, 0}, blockSize, 3, {4});
      runOnSequenceAndCompareResult({0, 1, 1, 1, 2, 2, 2, 1}, blockSize, 3, {5});
      runOnSequenceAndCompareResult({0, 0, 1, 1, 1, 2, 2, 2}, blockSize, 3, {});

      runOnSequenceAndCompareResult({0, 3, 3, 3, 2, 2, 1, 1}, blockSize, 3, {2});
      runOnSequenceAndCompareResult({0, 1, 1, 2, 2, 3, 3, 0}, blockSize, 3, {5});

      runOnSequenceAndCompareResult({0, 1, 1, 0, 3, 3, 3, 0}, blockSize, 3, {1, 5});
    }
  }

  SUBCASE("mixed") {
    for(int blockSize : {1, 2, 8}) {
      runOnSequenceAndCompareResult({0, 1, 1, 2, 2, 6, 2, 0}, blockSize, 3, {5});
      runOnSequenceAndCompareResult({0, 6, 5, 5, 4, 4, 3, 0}, blockSize, 3, {1});
      runOnSequenceAndCompareResult({0, 1, 1, 2, 3, 4, 4, 0}, blockSize, 3, {5});
    }
  }
}

TEST_CASE("SlidingWindowLocalMaximaFinder") {
  auto runOnSequenceAndCompareResult = [&](std::initializer_list<const int> sequence,
                                           int windowSize,
                                           std::initializer_list<const int>
                                               expectedMaxima) {
    std::vector<int> localMaxima;
    SlidingWindowLocalMaximaFinder<int> finder{size_t(windowSize)};

    for(int value : sequence) {
      auto maxVal = finder.add(value);
      if(maxVal > std::numeric_limits<int>::min()) localMaxima.emplace_back(maxVal);
    }

    auto maxVal = finder.finalize();
    if(maxVal > std::numeric_limits<int>::min()) localMaxima.emplace_back(maxVal);

    const auto actualView = make_array_view(localMaxima);
    const auto expectedView = make_array_view(expectedMaxima);
    if(!pmrange::equal(actualView, expectedView))
      CHECK(expectedView == actualView);  // pointers never match; doctest prints the arrays
  };

  SUBCASE("window size 1") {
    runOnSequenceAndCompareResult({}, 1, {});
    runOnSequenceAndCompareResult({1}, 1, {1});
    runOnSequenceAndCompareResult({1, 0}, 1, {1, 0});
    runOnSequenceAndCompareResult({1, 1}, 1, {1, 1});
    runOnSequenceAndCompareResult({1, 2}, 1, {1, 2});
    runOnSequenceAndCompareResult({2, 1}, 1, {2, 1});
    runOnSequenceAndCompareResult({1, 2, 3}, 1, {1, 2, 3});
    runOnSequenceAndCompareResult({1, 2, 3, 0}, 1, {1, 2, 3, 0});
    runOnSequenceAndCompareResult({3, 2, 1}, 1, {3, 2, 1});
  }

  SUBCASE("window size 2") {
    /*
     * Outputting the local maximum is delayed until <window size> samples have been read in
     * addition.
     */
    runOnSequenceAndCompareResult({1, 1}, 2, {1});
    runOnSequenceAndCompareResult({1, 2, 0}, 2, {2, 2});
    runOnSequenceAndCompareResult({1, 2, 1}, 2, {2, 2});
    runOnSequenceAndCompareResult({1, 2, 2}, 2, {2, 2});
    runOnSequenceAndCompareResult({1, 2, 0, 0}, 2, {2, 2, 0});
    runOnSequenceAndCompareResult({1, 2, 2, 2}, 2, {2, 2, 2});
    runOnSequenceAndCompareResult({1, 2, 2, 2, 1}, 2, {2, 2, 2, 2});
    runOnSequenceAndCompareResult({1, 2, 3, 4, 5}, 2, {2, 3, 4, 5});
  }

  SUBCASE("left edge case") {
    // Window only starts when all elements from the start have been read:
    runOnSequenceAndCompareResult({1, 2, 3, 4, 0, 0, 0}, 3, {3, 4, 4, 4, 0});
  }

  SUBCASE("ramps") {
    runOnSequenceAndCompareResult({9, 8, 7, 0, 0, 0}, 3, {9, 8, 7, 0});
    runOnSequenceAndCompareResult({6, 7, 8, 9, 0, 0, 0}, 3, {8, 9, 9, 9, 0});
    runOnSequenceAndCompareResult({7, 8, 9, 0, 0, 0}, 3, {9, 9, 9, 0});
    runOnSequenceAndCompareResult({7, 8, 9, 8, 7, 0, 0, 0}, 3, {9, 9, 9, 8, 7, 0});
    runOnSequenceAndCompareResult(
        {7, 8, 9, 7, 7, 6, 6, 6, 0, 0, 0}, 3, {9, 9, 9, 7, 7, 6, 6, 6, 0});
  }
}

TEST_CASE("SlidingWindowDominantPeakIsolator") {
  constexpr const auto kAdvance = -1;

  auto runSimple = [&](std::initializer_list<int> sequence,
                       int windowSize,  // also called: lockoutDistance
                       std::initializer_list<PeakFinderValueAtPos<int>>
                           expectedDominantPeaks) {
    std::vector<PeakFinderValueAtPos<int>> dominantPeaks;
    SlidingWindowDominantPeakIsolator<int> isolator{size_t(windowSize)};

    size_t now = 0;
    for(int value : sequence) {
      auto dominantPeakHandler = [&](const PeakFinderValueAtPos<int>& dominantPeak) {
        CHECK(now - dominantPeak.streamPos >= windowSize);
        dominantPeaks.emplace_back(dominantPeak);
      };

      if(value == kAdvance)
        V1_CODEMISSING();
      else
        isolator.onRawPeakEvent({now, value, PeakType::kPeak}, dominantPeakHandler);

      now++;
    }

    const auto actual = make_array_view(dominantPeaks);
    const auto expected = make_array_view(expectedDominantPeaks);
    if(!pmrange::equal(actual, expected)) CHECK(actual == expected);
  };


  auto run = [&](std::initializer_list<PeakFinderValueAtPos<int>> sequence,
                 int windowSize,  // also called: lockoutDistance
                 std::initializer_list<PeakFinderValueAtPos<int>>
                     expectedDominantPeaks) {
    std::vector<PeakFinderValueAtPos<int>> dominantPeaks;
    SlidingWindowDominantPeakIsolator<int> isolator{size_t(windowSize)};

    for(const auto& rawPeak : sequence) {
      auto now = rawPeak.streamPos;

      auto dominantPeakHandler = [&](const PeakFinderValueAtPos<int>& dominantPeak) {
        CHECK(now - dominantPeak.streamPos >= windowSize);
        dominantPeaks.emplace_back(dominantPeak);
      };

      if(rawPeak.value == kAdvance) {
        isolator.purgeUpUntil(now, dominantPeakHandler);
      } else
        isolator.onRawPeakEvent(
            PeakFinderRawPeak<int>(rawPeak.streamPos, rawPeak.value, PeakType::kPeak),
            dominantPeakHandler);
    }

    const auto actual = make_array_view(dominantPeaks);
    const auto expected = make_array_view(expectedDominantPeaks);
    if(!pmrange::equal(actual, expected)) CHECK(actual == expected);
  };

  SUBCASE("lock-out distance 1") {
    runSimple({}, 1, {});
    runSimple({1}, 1, {});
    runSimple({1, 0}, 1, {{0, 1}});
    runSimple({1, 1}, 1, {{0, 1}});
    runSimple({1, 2}, 1, {});
    runSimple({2, 1}, 1, {{0, 2}});
    runSimple({1, 2, 3}, 1, {});
    runSimple({1, 2, 3, 0}, 1, {{2, 3}});
    runSimple({3, 2, 1}, 1, {{0, 3}});

    // clarify that the lock-out distance is 1:
    runSimple({1, 5, 1, 4, 1, 6, 1, 0}, 1, {{1, 5}, {3, 4}, {5, 6}});
  }

  SUBCASE("lock-out distance 2") {
    runSimple({1, 1}, 2, {});
    runSimple({1, 2, 0}, 2, {});
    runSimple({1, 2, 0, 0}, 2, {{1, 2}});
    runSimple({1, 2, 1, 0}, 2, {{1, 2}});
    runSimple({1, 2, 2, 0}, 2, {{1, 2}});
    runSimple({1, 2, 2, 0}, 2, {{1, 2}});
    runSimple({1, 2, 0, 0}, 2, {{1, 2}});
    runSimple({1, 2, 2, 2}, 2, {{1, 2}});
    runSimple({1, 2, 2, 2, 1}, 2, {{1, 2}});
    runSimple({1, 2, 3, 4, 5}, 2, {});
    runSimple({1, 2, 3, 4, 5, 0}, 2, {});
    runSimple({1, 2, 3, 4, 5, 0, 0}, 2, {{4, 5}});
    runSimple({5, 4, 3, 2, 1, 0, 0}, 2, {{0, 5}});
    runSimple({0, 5, 4, 3, 2, 1, 0}, 2, {{1, 5}});
    runSimple({0, 0, 5, 4, 3, 2, 1}, 2, {{2, 5}});

    // clarify that the lock-out distance is 2:
    runSimple({1, 5, 1, 4, 1, 6, 1, 0}, 2, {{1, 5}, {5, 6}});
    runSimple({1, 5, 1, 5, 0, 0}, 2, {{1, 5}});
    runSimple({1, 5, 1, 1, 5, 0, 0}, 2, {{1, 5}, {4, 5}});
  }

  SUBCASE("lock-out distance 3") {
    runSimple({1, 1, 1}, 3, {});
    runSimple({1, 1, 1, 1}, 3, {{0, 1}});
    runSimple({1, 1, 1, 2}, 3, {});
    runSimple({1, 2, 3, 4, 3, 2, 1, 0}, 3, {{3, 4}});
    runSimple({1, 2, 3, 4, 5, 5, 5, 0}, 3, {{4, 5}});
    runSimple({1, 2, 3, 4, 5, 5, 6, 0}, 3, {});
    runSimple({1, 1, 2, 2, 3, 3, 3, 0}, 3, {{4, 3}});
    runSimple({1, 6, 1, 1, 5, 1, 1, 0}, 3, {{1, 6}});
    runSimple({1, 6, 1, 1, 1, 5, 0, 0, 0}, 3, {{1, 6}, {5, 5}});
  }

  SUBCASE("with gaps") {
    // check that right dominance detection for ({1, 4}) can deal with gaps:
    run({{1, 4}, {4, 5}, {9, 0}}, 3, {{4, 5}});
    run({{1, 4}, {5, 5}, {9, 0}}, 3, {{1, 4}, {5, 5}});
    // check that left dominance detection for ({4/5, 4}) can deal with gaps:
    run({{1, 5}, {4, 4}, {9, 0}}, 3, {{1, 5}});
    run({{1, 5}, {5, 4}, {9, 0}}, 3, {{1, 5}, {5, 4}});
    run({{1, 5}, {3, 2}, {4, 4}, {9, 0}}, 3, {{1, 5}});
    run({{1, 5}, {3, 2}, {5, 4}, {9, 0}}, 3, {{1, 5}, {5, 4}});
  }

  SUBCASE("purgeUpUntil") {
    // purgeUpUntil doesn't influence the output negatively:
    run({{0, 1}, {1, 2}, {2, 0}, {2, kAdvance}, {2, kAdvance}}, 2, {});
    run({{0, 1}, {1, 2}, {2, 0}, {2, kAdvance}, {2, kAdvance}, {3, 0}}, 2, {{1, 2}});
    // purgeUpUntil flushes out results:
    run({{0, 1}, {1, 2}, {2, 0}, {3, kAdvance}}, 2, {{1, 2}});

    run({{1, 4}, {4, 5}}, 3, {});
    run({{1, 4}, {4, 5}, {6, kAdvance}}, 3, {});
    run({{1, 4}, {4, 5}, {7, kAdvance}}, 3, {{4, 5}});
    run({{1, 4}, {4, 5}, {9001, kAdvance}}, 3, {{4, 5}});
    run({{1, 5}, {4, 4}, {6, kAdvance}}, 3, {{1, 5}});

    // purgeUpUntil with streamPos in the past within lockoutDistance(=2) from now(2) is a no-op
    run({{0, 1}, {1, 2}, {2, 0}, {2, kAdvance}, {1, kAdvance}}, 2, {});
  }
}

TEST_CASE("StreamingPeakFinder-basic") {
  runPeakFinderAndCheckPeaks(sMultiPeakSequence, 3, 1, {0x2, 0xB, 0x1B});
  runPeakFinderAndCheckPeaks(sMultiPeakSequence, 3, 3, {0x2, 0xB, 0x1B});

  runPeakFinderAndCheckPeaks(sMultiPeakSequence, 3, 3, {0xB, 0x1B}, 40);
}

TEST_CASE("StreamingPeakFinder-longSequence") {
  runPeakFinderAndCheckPeaks(sMultiPeakSequence, 8, 1, {0x0B});
}

TEST_CASE("StreamingPeakFinder-literalEdgeCases") {
  SUBCASE("boundaries") {
    /*
     * For the first peak, the lock-out is ignored.
     * But a proper peak, surrounded by non-peak values, has to be present nonetheless.
     *
     * The last peak must be at least lockoutDistance elements away from the end.
     */
    runPeakFinderAndCheckPeaks({1, 0, 0, 0, 0, 0, 0}, 3, 1, {});
    runPeakFinderAndCheckPeaks({0, 1, 0, 0, 0, 0, 0}, 3, 1, {1});
    runPeakFinderAndCheckPeaks({0, 0, 1, 0, 0, 0, 0}, 3, 1, {2});
    runPeakFinderAndCheckPeaks({0, 0, 0, 1, 0, 0, 0}, 3, 1, {3});
    runPeakFinderAndCheckPeaks({0, 0, 0, 0, 1, 0, 0}, 3, 1, {});
    runPeakFinderAndCheckPeaks({0, 0, 0, 0, 0, 1, 0}, 3, 1, {});
    runPeakFinderAndCheckPeaks({0, 0, 0, 0, 0, 0, 1}, 3, 1, {});

    // the same with two peaks:
    runPeakFinderAndCheckPeaks({0, 0, 1, 0, 0, 1, 0, 0}, 2, 1, {2, 5});
    runPeakFinderAndCheckPeaks({0, 1, 0, 0, 1, 0, 0}, 2, 1, {1, 4});
    runPeakFinderAndCheckPeaks({1, 0, 0, 1, 0, 0}, 2, 1, {3});
    runPeakFinderAndCheckPeaks({0, 1, 0, 0, 1, 0}, 2, 1, {1});
    runPeakFinderAndCheckPeaks({0, 1, 0, 0, 1}, 2, 1, {1});
  }

  SUBCASE("plateaus") {
    /*
     * The maximum length of a plateau is (pattern_size) - 2, i.e. 2*lockoutDistance - 1
     * A longer plateau overflows the window used for peak detection.
     */
    runPeakFinderAndCheckPeaks({0, 1, 0, 0}, 2, 1, {1});
    runPeakFinderAndCheckPeaks({0, 1, 1, 0, 0}, 2, 1, {1});
    runPeakFinderAndCheckPeaks({0, 1, 1, 1, 0, 0}, 2, 1, {2});
    runPeakFinderAndCheckPeaks({0, 1, 1, 1, 1, 0, 0}, 2, 1, {});
    runPeakFinderAndCheckPeaks({0, 1, 1, 1, 1, 1, 0, 0}, 2, 1, {});

    runPeakFinderAndCheckPeaks({0, 2, 2, 0, 0}, 1, 1, {});
  }
}

TEST_CASE("StreamingPeakFinder-monotonic") {
  // Monotonic sequences are not a peak, thus no peak is detected.

  // constant:
  runPeakFinderAndCheckPeaks({0, 0, 0, 0}, 1, 1, {});
  runPeakFinderAndCheckPeaks({1, 1, 1, 1}, 2, 2, {});

  // strictly monotonically increasing:
  runPeakFinderAndCheckPeaks({0, 1, 2, 3, 4}, 1, 1, {});
  runPeakFinderAndCheckPeaks({0, 1, 2, 3, 4, 5}, 2, 1, {});
  runPeakFinderAndCheckPeaks({0, 1, 2, 3, 4, 5}, 2, 2, {});

  // strictly monotonically decreasing:
  runPeakFinderAndCheckPeaks({4, 3, 2, 1, 0}, 1, 1, {});
  runPeakFinderAndCheckPeaks({5, 4, 3, 2, 1, 0}, 2, 1, {});
  runPeakFinderAndCheckPeaks({5, 4, 3, 2, 1, 0}, 2, 2, {});

  // monotonically increasing:
  runPeakFinderAndCheckPeaks({0, 1, 1, 1, 1}, 1, 1, {});
  runPeakFinderAndCheckPeaks({0, 0, 1, 1, 1}, 1, 1, {});
  runPeakFinderAndCheckPeaks({0, 0, 0, 1, 1}, 1, 1, {});
  runPeakFinderAndCheckPeaks({0, 0, 0, 0, 1}, 1, 1, {});

  runPeakFinderAndCheckPeaks({0, 1, 1, 1, 1, 1}, 2, 2, {});
  runPeakFinderAndCheckPeaks({0, 0, 1, 1, 1, 1}, 2, 2, {});
  runPeakFinderAndCheckPeaks({0, 0, 0, 1, 1, 1}, 2, 2, {});
  runPeakFinderAndCheckPeaks({0, 0, 0, 0, 1, 1}, 2, 2, {});
  runPeakFinderAndCheckPeaks({0, 0, 0, 0, 0, 1}, 2, 2, {});

  // monotonically decreasing:
  runPeakFinderAndCheckPeaks({1, 1, 1, 1, 0}, 1, 1, {});
  runPeakFinderAndCheckPeaks({1, 1, 1, 0, 0}, 1, 1, {});
  runPeakFinderAndCheckPeaks({1, 1, 0, 0, 0}, 1, 1, {});
  runPeakFinderAndCheckPeaks({1, 0, 0, 0, 0}, 1, 1, {});

  runPeakFinderAndCheckPeaks({1, 1, 1, 1, 1, 0}, 2, 2, {});
  runPeakFinderAndCheckPeaks({1, 1, 1, 1, 0, 0}, 2, 2, {});
  runPeakFinderAndCheckPeaks({1, 1, 1, 0, 0, 0}, 2, 2, {});
  runPeakFinderAndCheckPeaks({1, 1, 0, 0, 0, 0}, 2, 2, {});
  runPeakFinderAndCheckPeaks({1, 0, 0, 0, 0, 0}, 2, 2, {});
}

TEST_CASE("StreamingPeakFinder-peakSequences") {
  SUBCASE("same peak size") {
    // Peaks are far enough apart, they don't dominate each other
    runPeakFinderAndCheckPeaks({0, 1, 0, 1, 0, 1, 0, 1, 0}, 1, 1, {1, 3, 5, 7});
    /*
     * Peaks are so close that every peak dominates the next one peak. Only the first one is
     * accepted, greedily.
     */
    runPeakFinderAndCheckPeaks({0, 1, 0, 1, 0, 1, 0, 1, 0}, 2, 1, {1});
  }
  SUBCASE("increasing size") {
    // Every peak is dominated by the next one
    runPeakFinderAndCheckPeaks({0, 1, 0, 2, 0, 3, 0, 4, 0}, 2, 1, {});
  }
  SUBCASE("decreasing size") {
    // Every peak is dominated by the previous one, except the first
    runPeakFinderAndCheckPeaks({0, 4, 0, 3, 0, 2, 0, 1, 0}, 2, 1, {1});
  }
}

TEST_CASE("StreamingPeakFinder-triplePeaks") {
  /* Simplified cases of real data with peaks and smaller sub-peaks in between. */

  // same-height peaks too close:
  runPeakFinderAndCheckPeaks({0, 2, 0, 0, 2, 0, 2, 0}, 4, 1, {1});
  // middle peak dominated by previous + next:
  runPeakFinderAndCheckPeaks({0, 2, 0, 0, 1, 0, 2, 0, 0, 0, 0}, 4, 1, {1, 6});
  // middle peak dominated by prev:
  runPeakFinderAndCheckPeaks({0, 2, 0, 1, 0, 0, 2, 0, 0}, 2, 1, {1, 6});
  // middle peak dominated by next:
  runPeakFinderAndCheckPeaks({0, 2, 0, 0, 1, 0, 2, 0, 0}, 2, 1, {1, 6});

  // Plateau lock-out also works in multi-peak sequences:
  runPeakFinderAndCheckPeaks({0, 4, 0, 2, 0, 1, 1, 1, 0, 0, 0, 0}, 3, 3, {1});
}

TEST_CASE("StreamingPeakFinder-plateauSequences") {
  // train of maximum-length plateaus:
  runPeakFinderAndCheckPeaks({0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0}, 2, 1, {2, 6, 10});

  /*
   * Lock-out starts at the end of a plateau and ends at the next peak.
   * It does not start at the plateau's peak and it does not end at the next plateau's start.
   * That's why the previous test works and that's why the peak in the middle is ignored.
   */
  runPeakFinderAndCheckPeaks({0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0}, 2, 1, {2, 8});

  /*
   * Lock-out is also calculated from the previous larger value to the start of a plateau, not its
   * middle. Fortunately, this is implicitly dealt with by other rules.
   */
  runPeakFinderAndCheckPeaks({0, 6, 0, 1, 1, 1, 0, 0}, 2, 1, {1, 4});
}

TEST_CASE("StreamingPeakFinder-lockOut") {

  /*
   * They fulfill all criterions to be accepted. Lockout regions of different peaks may overlap.
   * Lockout regions may not cover other peaks, though.
   */
  runPeakFinderAndCheckPeaks({0, 1, 0, 2, 0, 3, 0, 4, 0, 5}, 1, 1, {1, 3, 5, 7});
  runPeakFinderAndCheckPeaks(
      {0, 0, 1, 0, 0, 2, 0, 0, 0, 3, 0, 0, 4, 0, 0, 0, 5, 0, 0, 0, 6}, 3, 1, {5, 12, 16});
  runPeakFinderAndCheckPeaks({0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0}, 5, 2, {2});
  runPeakFinderAndCheckPeaks({1, 0, 1, 0, 0, 0}, 2, 1, {});

  // In constrast, a maximum at the end is no problem:
  runPeakFinderAndCheckPeaks({0, 1, 5, 0, 6}, 2, 1, {});

  // At the end, a ramp starts that exceeds the previous maximum just after lock-out ends:
  runPeakFinderAndCheckPeaks(
      {0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 1, 2, 5}, 7, 1, {7});  // reduced
  runPeakFinderAndCheckPeaks(
      {0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 1, 2, 5}, 7, 4, {7});  // reduced

  // Rising slope (1-*4*-5) properly locks out dominated peak (@1: 2) in front of it:
  runPeakFinderAndCheckPeaks({0, 1, 0, 2, 3, 0, 0}, 2, 1, {4});
  runPeakFinderAndCheckPeaks({0, 1, 0, 2, 3, 0, 0, 0}, 2, 8, {4});

  // Falling slope (2-*3*-4) after peak (2) locks out a later maximum (5) ofthe same value
  runPeakFinderAndCheckPeaks({0, 0, 2, 1, 0, 1, 0, 0}, 2, 2, {2});

  // Interaction of lock-out and block site
  runPeakFinderAndCheckPeaks({0, 5, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 5, 5, {1});

  // Lock-out after plateau that starts at the beginning
  runPeakFinderAndCheckPeaks({2, 2, 0, 0, 1, 0, 0, 0}, 3, 2, {});

  // Lock-out of single element vs. plateau after ramp
  runPeakFinderAndCheckPeaks({0, 0, 0, 3, 1, 2, 3, 3}, 4, 2, {3});
}

TEST_CASE("StreamingPeakFinder-hugeBlockSize") {
  // Ensure that the processing logic does not implicitly depend on lockoutDistance <= blockSize
  runPeakFinderAndCheckPeaks({1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 2, 10, {});
  runPeakFinderAndCheckPeaks({0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, 2, 10, {1});
  runPeakFinderAndCheckPeaks({0, 1, 0, 2, 0, 3, 0, 4, 0, 0}, 2, 10, {7});
  runPeakFinderAndCheckPeaks({0, 1, 0, 0, 1, 0, 0, 1, 0, 0}, 2, 10, {1, 4, 7});
}

TEST_CASE("StreamingPeakFinder-specialCases") {
  /* All those cases were found when fuzzing. They are interesting patterns, so keep them. */
  // Recurring maxima in the initial lockout-free zone can be tricky:
  // Check that the reference algo lock-out works properly:
  runPeakFinderAndCheckPeaks(
      {27, 37, 6, 32, 23, 7, 6, 49, 40, 43, 9, 44, 11, 48, 29}, 5, 5, {1, 7});

  // Edge case for purging in context of a plateau
  runPeakFinderAndCheckPeaks({0, 3, 0, 4, 0, 1, 0, 1, 1, 1, 0}, 4, 1, {3});
  runPeakFinderAndCheckPeaks({0, 3, 0, 4, 0, 1, 0, 1, 1, 0}, 4, 5, {3});

  // This does not assert, all peaks are properly detected once:
  runPeakFinderAndCheckPeaks(
      {42, 1, 47, 38, 17, 5, 6, 45, 9, 18, 24, 15, 31, 46, 14, 22}, 2, 4, {2, 7, 13});

  // This does not assert due to moving elements from right-dominant to left-dominant storage:
  runPeakFinderAndCheckPeaks({9, 10, 7, 4, 9, 9, 8, 5}, 4, 2, {1});
}

#ifdef V1_PEAKFINDER_FUZZING
TEST_CASE("StreamingPeakFinder-fuzzing") {
  std::random_device rd;
  std::mt19937 gen(rd());

  while(true) {
    constexpr const auto kSequenceLength = 1024;
    auto sequence = std::vector<int>(kSequenceLength);
    std::uniform_int_distribution<> valueDistribution(0, 99);
    for(auto& val : sequence) val = int(valueDistribution(gen));

    auto lockoutDistance = std::uniform_int_distribution<>(1, kSequenceLength / 2)(gen);
    auto blockSize = std::uniform_int_distribution<>(1, lockoutDistance)(gen);

    auto sequenceView = make_array_view(sequence).shrink(sequence.size() % blockSize);
    printfToDebugger("LoD %d, BufSize %d", lockoutDistance, blockSize);

    auto referencePeaks = runReferencePeakFinderOn<ReferenceStreamingPeakFinder<int>>(
        sequenceView, lockoutDistance, blockSize, 5);
    verifyPeaks(sequenceView, lockoutDistance, referencePeaks);
    auto peaks =
        runPeakFinderOn<StreamingPeakFinder<int>>(sequenceView, lockoutDistance, blockSize, 5);

    if(!pmrange::equal(referencePeaks, peaks)) {
      printfToDebugger("Sequence: %s", toString(sequenceView).c_str());
      printfToDebugger("LoD %d, BufSize %d", lockoutDistance, blockSize);
      CHECK(make_array_view(referencePeaks) == make_array_view(peaks));
    }
  }
}
#endif

}  // namespace v1util::dsp::test
