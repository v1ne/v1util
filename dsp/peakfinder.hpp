#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/math.hpp"
#include "v1util/container/range.hpp"
#include "v1util/container/ringbuffer.hpp"

#include <algorithm>
#include <limits>

namespace v1util::dsp {

/*! Find the biggest peak in the given range (@p iBegin, @p iEnd]
 *
 * A peak is the first largest element in the sequence or the middle element of the first
 * plateau of largest elements in the sequence.
 * A peak is only a peak if it or its plateau has a larger value than its neighbours.
 * Thus, it is never one of the boundary elements of the range.
 *
 * @returns the iterator pointing to the peak or @p iEnd if no peak is found.
 */
template <typename Iter>
Iter findPeak(Iter iBegin, Iter iEnd) {
  auto iMax = std::max_element(iBegin, iEnd);
  if(iMax == iEnd) return iEnd;

  const auto& maxValue = *iMax;

  auto iLeftEdge = iMax;
  while(iLeftEdge != iBegin && *iLeftEdge == maxValue) --iLeftEdge;
  if(*iLeftEdge != maxValue) ++iLeftEdge;

  auto iRightEdge = iMax + 1;
  while(iRightEdge != iEnd && *iRightEdge == maxValue) ++iRightEdge;
  if(iRightEdge == iEnd || *iRightEdge != maxValue) --iRightEdge;

  if(iLeftEdge == iBegin || iRightEdge + 1 == iEnd) return iEnd;  // no peak, all same values
  return iLeftEdge + (iRightEdge - iLeftEdge) / 2;
}


/*! Return whether a plateau exceeds @p maxPlateauSize
 *
 * One criterion for a peak is that it is surrounded by smaller elements. The same must hold for a
 * peak. If a plateau consists of more samples than @p maxPlateauSize, it is assumed to be larger
 * than the sliding window, which means that it cannot be discerned from a flat line inside that
 * window. Thus, it is an invalid plateau.
 */
template <typename I>
bool isPlateauTooLong(I iPeak, I iBegin, I iEnd, size_t maxPlateauSize) {
  auto peakValue = *iPeak;
  auto iPlateauBegin = iPeak;
  while(iPlateauBegin != iBegin && *iPlateauBegin == peakValue) --iPlateauBegin;
  if(*iPlateauBegin != peakValue) ++iPlateauBegin;

  auto iPlateauEnd = iPeak + 1;
  while(iPlateauEnd != iEnd && *iPlateauEnd == peakValue) ++iPlateauEnd;

  return size_t(iPlateauEnd - iPlateauBegin) > maxPlateauSize;
}


/*! Pair of absolute stream position and value.
 *
 * All stream positions are valid, in-band signalling can only happen via tag values
 * in @p value.
 */
template <typename Value>
struct PeakFinderValueAtPos {
  size_t streamPos;
  Value value;
};


enum class PeakType : int { kPeak, kRising, kFalling };
template <typename Value>
struct PeakFinderRawPeak : public PeakFinderValueAtPos<Value> {
  PeakFinderRawPeak() = default;
  PeakFinderRawPeak(size_t streamPos_, Value value_, PeakType type_)
      : PeakFinderValueAtPos<Value>{streamPos_, value_}, type(type_) {}
  V1_DEFAULT_CP_MV(PeakFinderRawPeak);

  PeakType type;
};


/*! Find raw peaks and plateaus in a stream of data, latency
 *
 * A raw peak is a local maximum or the middle element of a plateau consisting of local maxima.
 * A raw peak or plateau must be surrounded by elements of strictly smaller value.
 * A plateau must consist of at most maxPlateauSize samples. If it exceeds this limit, it is
 * ignored. No raw peak/plateau will be detected after such a condition until the signal changes its
 * value and the aforementioned rules apply again.
 *
 * All peaks and plateaus must reach at least peakThreshold in order to be accepted.
 *
 * The detector has a base latency of 1. It increases when a plateau is encountered to the length of
 * the plateau + 1.
 */
template <typename Value>
class StreamingPeakDetector {
  static constexpr const auto kFirstSamplePlateauSizeMarker = ~size_t(0);

 public:
  StreamingPeakDetector() = default;
  StreamingPeakDetector(size_t maxPlateauSize)
      : mTooBigPlateauSize(std::max(maxPlateauSize, maxPlateauSize + 1)) {
    V1_ASSERT(maxPlateauSize > 0U);
  }
  V1_DEFAULT_CP_MV(StreamingPeakDetector);

  /*! Process a block of data
   *
   * @p peakHandler(PeakFinderValueAtPos<Value>) is called for every peak that satisfied the
   * aforementioned criteria.
   */
  template <typename View, typename Invokable>
  void process(
      const View& data, size_t streamPosAtStart, Value peakThreshold, Invokable&& peakHandler) {
    V1_ASSERT(mTooBigPlateauSize > 0U);
    if(data.size() <= 0) return;  // kidding, eh?

    auto lastValue = mLastValue;
    auto plateauSize = mCurrentPlateauSize;
    auto streamPos = streamPosAtStart;

    for(Value sample : data) {
      V1_ASSERT(plateauSize <= mTooBigPlateauSize || plateauSize == kFirstSamplePlateauSizeMarker);

      if(lastValue < sample) {
        plateauSize = 1;

        /*
         * Notify of rising edge. Peak isolator needs this to lock out previous maxima, even though
         * the rising slope didn't turn into a peak yet. If it turnes into a peak, that peak might
         * be too late, making the peak isolator accept non-dominant peaks.
         */
        if(sample >= peakThreshold)
          peakHandler(PeakFinderRawPeak<Value>{streamPos, sample, PeakType::kRising});

      } else if(lastValue == sample) {
        if(plateauSize > 0U && plateauSize < mTooBigPlateauSize) ++plateauSize;
      } else /* lastValue > sample */ {
        if(plateauSize > 0U && plateauSize < mTooBigPlateauSize && lastValue >= peakThreshold) {
          auto rightHalfSize = plateauSize / 2;
          peakHandler(
              PeakFinderRawPeak<Value>{streamPos - rightHalfSize - 1, lastValue, PeakType::kPeak});

          /*
           * Add pseudo peak at the end of a plateau (not dominant because peakInfo has the same
           * value), since lockout distance is counted from the end of a plateau, not the middle:
           */
          if(rightHalfSize > 0)
            peakHandler(PeakFinderRawPeak<Value>{streamPos - 1, lastValue, PeakType::kFalling});

        } else if(plateauSize != kFirstSamplePlateauSizeMarker && lastValue >= peakThreshold) {
          peakHandler(PeakFinderRawPeak<Value>{streamPos - 1, lastValue, PeakType::kFalling});
        }

        plateauSize = 0;
      }

      lastValue = sample;
      ++streamPos;
    }

    mCurrentPlateauSize = plateauSize;
    mLastValue = lastValue;
  }

  size_t currentPlateauSize() const {
    return mCurrentPlateauSize >= mTooBigPlateauSize ? 0U : mCurrentPlateauSize;
  }

  Value currentPlateauValue() const {
    return currentPlateauSize() > 0U ? mLastValue : std::numeric_limits<Value>::min();
  }

 private:
  // const:
  size_t mTooBigPlateauSize = 0U;

  // non-const:
  Value mLastValue = std::numeric_limits<Value>::max();
  size_t mCurrentPlateauSize = kFirstSamplePlateauSizeMarker;
};


/*! Transform an input sequence to a sequence of windowed maxima. */
template <typename Value>
class SlidingWindowLocalMaximaFinder {
 public:
  SlidingWindowLocalMaximaFinder() = default;
  SlidingWindowLocalMaximaFinder(size_t windowSize) : mWindowSize(windowSize) {
    V1_ASSERT(windowSize > 0U);
    mMaximumCandidates.setCapacity(windowSize);
  }
  V1_DEFAULT_CP_MV(SlidingWindowLocalMaximaFinder);

  /*! Add a new sample.
   *
   * @p value: The value of the sample
   * @return the last maximum (delayed) or the smallest value if the maximum isn't available yet.
   */
  Value add(Value value) {
    Value retval = std::numeric_limits<Value>::min();

    if(mInitialElementsRead >= mWindowSize) {
      retval = mMaximumCandidates.front().value;

      while(!mMaximumCandidates.empty()
            && mCurrentSamplePos - mMaximumCandidates.front().samplePos >= mWindowSize)
        mMaximumCandidates.pop_front();
    } else
      ++mInitialElementsRead;

    while(!mMaximumCandidates.empty() && mMaximumCandidates.back().value <= value)
      mMaximumCandidates.pop_back();

    mMaximumCandidates.push_back(MaximumCandidate{mCurrentSamplePos, value});
    mCurrentSamplePos++;

    return retval;
  }

  /*! Return the last local maximum, if available */
  Value finalize() {
    return mMaximumCandidates.empty() ? std::numeric_limits<Value>::min()
                                      : mMaximumCandidates.front().value;
  }

 private:
  struct MaximumCandidate {
    size_t samplePos;
    Value value;
  };

  // const:
  size_t mWindowSize = 0U;

  // non-const:
  size_t mInitialElementsRead = 0U;
  size_t mCurrentSamplePos = 0U;
  FixedSizeDeque<MaximumCandidate> mMaximumCandidates;
};


/*! Fed with a sequence of raw peaks, filter dominant peaks.
 *
 * This is very similar to finding local maxima, but still different:
 * - The same peak may only be returned at most once.
 * - Stream positions must be attributed perfectly to the peaks.
 * - Events should be delayed always by the same amount.
 *
 * In this context, a peak is considered dominant if it is either
 * the first or the biggest within +/- lockoutDistance number of samples around it.
 */
template <typename Value>
class SlidingWindowDominantPeakIsolator {
 public:
  SlidingWindowDominantPeakIsolator() = default;
  SlidingWindowDominantPeakIsolator(size_t lockoutDistance) : mWindowSize(lockoutDistance) {
    V1_ASSERT(lockoutDistance > 0U);
    mRightDominantPeaks.setCapacity(patternSize());
    mLeftDominanceMemory.setCapacity(patternSize());
  }
  V1_DEFAULT_CP_MV(SlidingWindowDominantPeakIsolator);

  /*! Add a new raw peak.
   *
   * The timeline is taken from @p sample.streamPos, which is considered to be
   * a steady clock.
   *
   * If a dominant peak is found, @p dominantPeakHandler is called with it.
   */
  template <typename Invokable>
  void onRawPeakEvent(PeakFinderRawPeak<Value> rawPeak, Invokable&& dominantPeakHandler) {
    const auto now = rawPeak.streamPos;
    // Time must progress:
    V1_ASSERT(mRightDominantPeaks.empty() || now != mRightDominantPeaks.back().streamPos
              || (rawPeak.type == PeakType::kPeak
                  && mRightDominantPeaks.back().type == PeakType::kRising));
    V1_ASSERT(ringGreaterEq(rawPeak.streamPos, mLastPurgeStreamPos)
              || rawPeak.value <= youngestRawPeakValue()
              || rawPeak.value <= youngestLeftDominanceMemoryValue());

    if(!mRightDominantPeaks.empty() && now == mRightDominantPeaks.back().streamPos) {
      // Promotion of a non-peak to a peak is allowed:
      V1_ASSERT(rawPeak.value == mRightDominantPeaks.back().value);
      mRightDominantPeaks.back().type = rawPeak.type;
      return;
    }

    // If this fired, the stream position probably jumped backwards:
    V1_ASSERT(
        mRightDominantPeaks.empty() || ringGreaterEq(now, mRightDominantPeaks.back().streamPos));

    while(!mRightDominantPeaks.empty() && mRightDominantPeaks.back().value < rawPeak.value
          && now - mRightDominantPeaks.back().streamPos <= mWindowSize)
      mRightDominantPeaks.pop_back();

    if(rawPeak.type != PeakType::kRising) mRightDominantPeaks.push_back(rawPeak);

    // Contrary to the minimum finding, cleaning is done at the end here. Why?
    cleanOldRightDominantPeaks(now, dominantPeakHandler);
  }

  /*! Process queued peaks, assuming no further peaks have arrived up until @p streamPos
   *
   * @p streamPos may be in the past up to mWindowSize samples.
   */
  template <typename Invokable>
  void purgeUpUntil(size_t streamPos, Invokable&& dominantPeakHandler) {
#ifdef V1_DEBUG
    V1_ASSERT(ringGreaterEq(streamPos, mLastPurgeStreamPos - mWindowSize));
    mLastPurgeStreamPos = streamPos;
#endif

    auto youngestRightDominantPeak =
        mRightDominantPeaks.empty() ? streamPos : mRightDominantPeaks.back().streamPos;
    if(ringGreaterEq(youngestRightDominantPeak, streamPos))
      return;  // the last addRawPeak was after streamPos, no purging necessary

    // If this assertion hit, you either purge not often enough or streamPos is too old.
    V1_ASSERT(mLeftDominanceMemory.empty()
              || ringGreaterEq(streamPos, mLeftDominanceMemory.front().streamPos));
    /*
     * Purge the left dominance memory explicitly. Otherwise, elements might linger there for so
     * long that streamPos rolls over and the time stamps lost their meaning.
     */
    while(!mLeftDominanceMemory.empty()
          && streamPos - mLeftDominanceMemory.front().streamPos > 3 * mWindowSize + 1)
      mLeftDominanceMemory.pop_front();

    cleanOldRightDominantPeaks(streamPos, dominantPeakHandler);
  }

  /*! Fix boundary condition to lock-out peaks after local max. at very first element
   *
   * The peak detector rightfully ignores it, but the isolator needs to know about it
   * to suppress subsequent raw peaks, since such a maximum locks out later peaks.
   */
  void addLockoutForInitialValue(const PeakFinderValueAtPos<Value>& initialValue) {
    mLeftDominanceMemory.push_back(initialValue);
  }

  Value youngestRawPeakValue() const {
    return mRightDominantPeaks.empty() ? std::numeric_limits<Value>::min()
                                       : mRightDominantPeaks.back().value;
  }

 private:
  template <typename Invokable>
  inline void cleanOldRightDominantPeaks(size_t now, Invokable&& dominantPeakHandler) {
    while(!mRightDominantPeaks.empty()
          && now - mRightDominantPeaks.front().streamPos >= mWindowSize) {
      checkLeftDominanceAndRememberRightDominantPeak(
          mRightDominantPeaks.front(), dominantPeakHandler);
      mRightDominantPeaks.pop_front();
    }
  }

  /*! Check that a given right-dominant peak is also left-dominant and only then accept it.
   *
   * A left-dominant peak must be bigger than all the peaks that came before in time (i.e. left of
   * it) within mWindowSize. Equal value is not enough on the left side because a proper peak is
   * bigger or the first of equal-value peaks within a window.
   */
  template <typename Invokable>
  void checkLeftDominanceAndRememberRightDominantPeak(
      const PeakFinderRawPeak<Value>& rightDominantPeak, Invokable dominantPeakHandler) {
    while(!mLeftDominanceMemory.empty()
          && rightDominantPeak.streamPos - mLeftDominanceMemory.front().streamPos > mWindowSize)
      mLeftDominanceMemory.pop_front();

    while(!mLeftDominanceMemory.empty()
          && mLeftDominanceMemory.back().value < rightDominantPeak.value)
      mLeftDominanceMemory.pop_back();

    /*
     * empty() implies:
     * All entries mLeftDominanceMemory after purging old entries were smaller than new
     * (right-dominant) peak value. Thus, rightDominantpeak is also left-dominant.
     * But only accept peaks, not falling slopes. Such a falling slope can be the end of a plateau
     * or after a plateau that is too long.
     */
    if(rightDominantPeak.type == PeakType::kPeak && mLeftDominanceMemory.empty())
      dominantPeakHandler(rightDominantPeak);

    mLeftDominanceMemory.push_back(rightDominantPeak);
  };

  Value youngestLeftDominanceMemoryValue() const {
    return mLeftDominanceMemory.empty() ? std::numeric_limits<Value>::min()
                                        : mLeftDominanceMemory.back().value;
  }

  size_t patternSize() const { return 2 * mWindowSize + 1; }

  // const:
  size_t mWindowSize = 0U;

  // non-const:

  /*! queue for peaks which are dominant on the right side
   *
   * i.e. they are bigger than or equal to raw peaks that come later in time within mWindowSize
   */
  FixedSizeDeque<PeakFinderRawPeak<Value>> mRightDominantPeaks;


  /*! Memory of past (right- and total) dominant peaks
   *
   * Used to check right-dominant peaks against to ensure that they are also left-dominant.
   */
  FixedSizeDeque<PeakFinderValueAtPos<Value>> mLeftDominanceMemory;

#ifdef V1_DEBUG
  size_t mLastPurgeStreamPos = size_t(0ULL - 1ULL);
#endif
};


/*! Find peaks: Samples that are the largest within a certain window in a stream of data
 *
 * Greedily finds peaks that dominate all other peaks within +/- lockoutDistance :=
 * floor(patternSize/2) samples around it. A peak is either a local maximum, i.e. an element
 * surrounded by a smaller element on both sides, or the middle element of a plateau, i.e. a
 * sequence of elements of the same value surrounded by a smaller element on both sides.
 *
 * A peak P dominates another peak P' if the value of P is bigger than that of P'. P also
 * dominates P' if both have the same value and P comes first.
 *
 * Plateaus are recognised up to a size of patternSize - 2 (patternSize is always made an odd
 * number). Larger plateaus are ignored.
 *
 * The peak finder has a latency of lockoutDistance samples, but it increases if a plateau is
 * encountered.
 *
 * Internally, peaks and falling slopes within the last patternSize are remembered:
 *
 * |------            patternSize          -------|
 * |-  lockoutDistance  -| |-  lockoutDistance   -|
 * +---------------------+ +----------------------+  samples
 * |O<-   old peaks    <-|P|<- peak candidates  <-|<--------
 * +^--------------------+^+----------------------+
 *  |                     |                           time
 *  |                     |        ------------------------->
 *  |                     |
 *  |                     `- peak candidate P, lockoutDistance away from new data
 *  `- largest sample left of P within P's lock-out distance
 *
 * The idea is very similar to the problem of finding a windowed local maximum.
 * The queues shown above are sorted in descending order, i.e. the largest element is on the left.
 * As data moves to the left, it is filtered against newer data so that when an element is
 * lockoutDistance samples old and has reached the position P without being removed, no newer data
 * dominates it. To check that no past data dominates it, too, it's checked against the oldest
 * element in the left queue, which is up to islockoutDistance samples oder than P. If P > O, P is a
 * dominant peak and returned. In any case, P is moved into the left queue.
 */
template <typename Value>
class StreamingPeakFinder {
 public:
  StreamingPeakFinder() = default;
  StreamingPeakFinder(size_t patternSize)
      : mLockoutDistance(patternSize / 2)
      , mPeakDetector(maxPlateauLength())
      , mDominantPeakIsolator(mLockoutDistance) {}
  V1_DEFAULT_CP_MV(StreamingPeakFinder);

  void reconfigure(size_t patternSize) { *this = StreamingPeakFinder(patternSize); }

  template <typename View, typename Invokable>
  void process(const View& data, size_t streamPosAtStartOfData, Value peakThreshold,
      Invokable&& handlePeak) {
    auto newStreamPos = streamPosAtStartOfData + data.size();

    if(!mIsSubsequentBlock) {
      mIsSubsequentBlock = true;
      mDominantPeakIsolator.addLockoutForInitialValue({streamPosAtStartOfData, data.front()});
    }

    mPeakDetector.process(
        data, streamPosAtStartOfData, peakThreshold, [&](const PeakFinderRawPeak<Value>& rawPeak) {
          // Check for maximum peak detector delay + blockSize because peaks don't align with blocks
          V1_ASSERT(newStreamPos - rawPeak.streamPos <= ((patternSize() - 2) + 1) + data.size());
          mDominantPeakIsolator.onRawPeakEvent(rawPeak, handlePeak);
        });

    /*
     * for purging, take different delays into account:
     * - StreamingPeakDetector has a delay of 1 sample, anyway
     * - In addition, for a plateau, StreamingPeakDetector has to wait until the plateau (at most
     *   patternSize()-2 samples long) ends before it can return it
     */
    auto peakDetectorDelay =
        (mPeakDetector.currentPlateauValue() > mDominantPeakIsolator.youngestRawPeakValue()
                ? mPeakDetector.currentPlateauSize()
                : 0)
        + 1;
    mDominantPeakIsolator.purgeUpUntil(newStreamPos - peakDetectorDelay, handlePeak);
  }

 private:
  size_t patternSize() const { return 2 * mLockoutDistance + 1; }
  size_t maxPlateauLength() const { return patternSize() - 2; }

  // const:
  size_t mLockoutDistance = 0;

  // non-const:
  bool mIsSubsequentBlock = false;
  StreamingPeakDetector<Value> mPeakDetector;
  SlidingWindowDominantPeakIsolator<Value> mDominantPeakIsolator;
  size_t mStreamPos = 0U;
};

}  // namespace v1util::dsp
