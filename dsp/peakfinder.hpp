#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/math.hpp"
#include "v1util/container/range.hpp"
#include "v1util/container/ringbuffer.hpp"

#include <algorithm>

namespace v1util { namespace dsp {

/*! Find the biggest peak in the given range (@p iBegin, @p iEnd]
 *
 * A peak is the first largest element in the sequence or the middle element of the first
 * plateau of largest elements in the sequence.
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

  if(iLeftEdge == iBegin && iRightEdge + 1 == iEnd) return iEnd;  // no peak, all same values
  return iLeftEdge + (iRightEdge - iLeftEdge) / 2;
}


/*! Find a peak in the given range, if it's the biggest peak within @p lockoutDistance
 *
 * The same as findPeak, but only accepts a peak if it occurs within the first lockoutDistance
 * elements.
 *
 * This is useful to greedily search for peaks within +/- @p lockoutDistance. It is assumed that
 * the last peak is at iBegin - 1 or earlier, so further than @p lockoutDistance away.
 *
 * @returns the iterator pointing at the peak or @p iEnd, if none is found/usable.
 */
template <typename Iter>
Iter findPeakWithLockout(Iter iBegin, Iter iEnd, size_t lockoutDistance) {
  V1_ASSERT(size_t(iEnd - iBegin) >= 2 * lockoutDistance);
  auto iMax = findPeak(iBegin, iBegin + 2 * lockoutDistance);
  if(size_t(iMax - iBegin) >= lockoutDistance) return iEnd;
  return iMax;
};

/*! Finds peaks while being fed with blocks of data
 *
 * |-- lockout distance -|--- lockout distance --|- lockout distance -|
 * +---------------------+-----------------------+--------------------+
 * | already scanned data| valid peak positions  |   too recent data  | time
 * |----=====^===------------------===============^===========--------|  ----->
 * +---------------------+-----------------------+--------------------+
 *           ^- last peak position                ^- next peak (too recent)
 *
 * - 2x lockout distance = pattern size (peaks locks out left and right half of pattern)
 * - Peaks must be at least lockoutDistance apart to be recognised as peaks.
 * - If a peak is within lockoutDistance to the end of the current block (i.e. "now"),
 *   it is discarded, too, since new data might contain a bigger peak. Also, the block will
 *   move to the valid peak position once it has left the too recent range.
 *
 *
 */
class BufferedPeakFinder {
 public:
  BufferedPeakFinder() = default;
  BufferedPeakFinder(size_t patternSize) : mLockoutDistance((1 + patternSize) / 2) {}

  V1_DEFAULT_CP_MV(BufferedPeakFinder);

  void reconfigure(size_t patternSize) { *this = BufferedPeakFinder(patternSize); }

  //! Minimal buffer size to use for process(), next even pattern size
  size_t minBufferSize() const { return 2 * mLockoutDistance; }

  template <typename Iter>
  struct IterAndOffset {
    Iter iPeak = {};
    size_t distanceToEndOfCurrentBlock = 0;
  };

  /*! process new data
   *
   * Data is written into the ring buffer @p data and process() is called afterwards.
   *
   * @param data  Ring buffer of old and new data. New data ends before @p offsetAfterCurrentBlock
   * @param offsetAfterCurrentBlock  offset in @p data of the first element after the current block
   * @param blockSize  how many elements have been added to @p data after the last call to this method
   * @param peakThreshold  minimum value for a peak to be accepted as a peak
   */
  template <typename View>
  IterAndOffset<typename View::const_iterator> process(const View& data,
      uint64_t offsetAfterCurrentBlock, uint64_t blockSize,
      typename View::value_type peakThreshold) {
    V1_ASSERT(mLockoutDistance);
    V1_ASSERT(blockSize <= 2 * mLockoutDistance);

    mLastPeakPos = std::max(mLastPeakPos - int64_t(blockSize), -int64_t(mLockoutDistance));
    mSamplesToScan += blockSize;

    // Not enough data to find peak reliably, greedily
    if(mSamplesToScan < 2 * mLockoutDistance) return {data.end(), 0};

    // Peak locked out by last peak
    if(mLastPeakPos > -int64_t(mLockoutDistance)) {
      mSamplesToScan -= blockSize;
      return {data.end(), 0};
    }

    mSamplesToScan -= mLockoutDistance;

    // Only check these after enough data has been read (in tests, a read-only buffer is used that
    // grows with the input)
    V1_ASSERT(data.size() >= 2 * mLockoutDistance);
    V1_ASSERT(data.size() >= offsetAfterCurrentBlock);

    auto iEnd = RingIterator(data.begin(), data.end()) + offsetAfterCurrentBlock;
    auto iBegin = iEnd - 2 * mLockoutDistance;
    auto iPeak = findPeakWithLockout(iBegin, iEnd, mLockoutDistance);
    if(iPeak == iEnd || *iPeak < peakThreshold) return {data.end(), 0};

    mLastPeakPos = iPeak - iBegin;  // nonnegative

#ifdef V1_DEBUG
    const auto peakOff = size_t(iPeak.base() - data.begin());
    const auto offToACB = peakOff > offsetAfterCurrentBlock
                              ? offsetAfterCurrentBlock + data.size() - peakOff
                              : offsetAfterCurrentBlock - peakOff;
    // TODO: How can this assertion be violated? It happens! (which one?)
    V1_ASSERT(offToACB <= 2 * mLockoutDistance);
    V1_ASSERT(offToACB >= mLockoutDistance);
#endif

    return {iPeak.base(), size_t(iEnd - iPeak)};
  }

 private:
  size_t mLockoutDistance = 0;
  size_t mSamplesToScan = 0;
  int64_t mLastPeakPos = 0;  // no last peak
};

}}  // namespace v1util::dsp
