#pragma once

#include "audioBlock.hpp"
#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"

#include <cstring>


namespace v1util { namespace dsp {
//! An owning, non-interleaved chunk of audio data
class AudioBuffer {
 public:
  AudioBuffer() = default;
  AudioBuffer(int numChannels, size_t numSamples) { resize(numChannels, numSamples); }
  ~AudioBuffer() { destroy(); }

  AudioBuffer(const AudioBuffer& src) { copyFrom(src); }
  AudioBuffer(AudioBuffer&& src) noexcept
      : mppChannels(src.mppChannels)
      , mpBuffer(src.mpBuffer)
      , mNumChannels(src.mNumChannels)
      , mNumSamples(src.mNumSamples) {
    src.mNumSamples = 0;
#ifdef V1_DEBUG
    src.mppChannels = nullptr;
    src.mpBuffer = nullptr;
    src.mNumChannels = 0;
#endif
  }
  AudioBuffer& operator=(const AudioBuffer& src) {
    copyFrom(src);
    return *this;
  }
  AudioBuffer& operator=(AudioBuffer&& src) noexcept {
    destroy();

    mppChannels = src.mppChannels;
    mpBuffer = src.mpBuffer;
    mNumChannels = src.mNumChannels;
    mNumSamples = src.mNumSamples;
    src.mNumSamples = 0;
#ifdef V1_DEBUG
    src.mppChannels = nullptr;
    src.mpBuffer = nullptr;
    src.mNumChannels = 0;
#endif

    return *this;
  }


  // @{ getters
  auto constAudioBlock() const { return ConstAudioBlock{mppChannels, mNumChannels, mNumSamples}; }
  ConstAudioBlock audioBlock() const { return {mppChannels, mNumChannels, mNumSamples}; }
  AudioBlock audioBlock() { return {mppChannels, mNumChannels, mNumSamples}; }

  auto constChannel(int chan) const { return constAudioBlock().channel(chan); }
  auto channel(int chan) const { return audioBlock().channel(chan); }
  auto channel(int chan) { return audioBlock().channel(chan); }

  size_t numSamples() const { return mNumSamples; }
  int numChannels() const { return mNumChannels; }

  // @{ setters
  void resize(int numChannels, size_t numSamples) {
    destroy();

    mppChannels = new float*[numChannels]();
    mpBuffer = new float[size_t(numChannels) * numSamples]();
    mNumChannels = numChannels;
    mNumSamples = numSamples;

    for(int i = 0; i < numChannels; ++i) mppChannels[i] = mpBuffer + (i * numSamples);
  }

  void destroy() {
    if(mNumSamples == 0) return;
    V1_ASSERT(mppChannels);

    delete mppChannels;
    delete mpBuffer;

    mNumSamples = 0;
#ifdef V1_DEBUG
    mNumChannels = 0;
    mpBuffer = nullptr;
    mppChannels = nullptr;
#endif
  }

  void clear() { audioBlock().clear(); }

  // @}

 private:
  void copyFrom(const AudioBuffer& other) {
    if(mNumSamples && other.mNumSamples && mpBuffer == other.mpBuffer) return;

    resize(other.mNumChannels, other.mNumSamples);
    std::memcpy(mpBuffer, other.mpBuffer, sizeof(float) * size_t(mNumChannels * mNumSamples));
    for(int i = 0; i < mNumChannels; ++i) mppChannels[i] = mpBuffer + (i * mNumSamples);
  }


  float** mppChannels = nullptr;
  float* mpBuffer = nullptr;
  int mNumChannels = 0;
  size_t mNumSamples = 0;
};

}}  // namespace v1util::dsp
