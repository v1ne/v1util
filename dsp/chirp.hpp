#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/container/span.hpp"

#include <cstddef>

namespace v1util { namespace dsp {

template <typename V>
class ChirpGen {
 public:
  ChirpGen(V sampleRate, V startFreqHz, V endFreqHz, V durationS, V fadeInOutS)
      : mSampleRate(sampleRate)
      , mStartFreqHz(startFreqHz)
      , mEndFreqHz(endFreqHz)
      , mDurationS(durationS)
      , mFadeInOutS(std::min(fadeInOutS, V(durationS / 3))) {
    mLengthSmpl = size_t(std::round(mDurationS * mSampleRate));
  }

  V1_DEFAULT_CP_MV(ChirpGen)

  size_t lengthSmpl() const { return mLengthSmpl; }

  template <typename V2>
  void fillBlock(Span<V2> span) {
    auto pBuffer = span.data();

    const auto dT = 1 / mSampleRate;
    const auto w1 = mStartFreqHz * 2 * 3.14;
    const auto w2 = mEndFreqHz * 2 * 3.14;
    auto t = 0.;
    for(size_t i = 0; i < mLengthSmpl; ++i, t += dT) {
      pBuffer[i] = V2(sin((w1 + (w2 - w1) * t / (2 * mDurationS)) * t));
    }

    const auto numFadeSamples = size_t(std::round(mFadeInOutS * mSampleRate));
    const auto decayFactor = -5. / numFadeSamples;
    auto pFadeIn = pBuffer + numFadeSamples - 1;
    auto pFadeOut = pBuffer + (mLengthSmpl - numFadeSamples);
    for(int i = 0; i < numFadeSamples; ++i) {
      const auto decay = V2(exp(decayFactor * i));
      *pFadeIn-- *= decay;
      *pFadeOut++ *= decay;
    }
  }

 private:
  V mSampleRate;
  V mStartFreqHz, mEndFreqHz;
  V mDurationS;
  V mFadeInOutS;

  size_t mLengthSmpl;
};

}}  // namespace v1util::dsp
