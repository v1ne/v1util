#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/container/array_view.hpp"
#include "v1util/container/span.hpp"

namespace v1util { namespace dsp {

//! Uninterleaved, non-owning block of data
template <typename PChannel>
class AudioBlockBase {
 public:
  AudioBlockBase() = default;
  AudioBlockBase(PChannel* ppBuffer, int numChannels_, size_t numSamples_)
      : ppBuffer(ppBuffer), numChannels(numChannels_), numSamples(numSamples_) {
    V1_ASSERT(numChannels || (!numSamples && !ppBuffer));
    V1_ASSERT(!numSamples || ppBuffer);
  }

  V1_DEFAULT_CP_MV(AudioBlockBase)

  ArrayView<float> channel(int channel) const {
    V1_ASSERT(channel < numChannels);
    return {ppBuffer[channel], numSamples};
  }

  PChannel* ppBuffer = nullptr;
  int numChannels = 0;
  size_t numSamples = 0;
};

//! Non-owning reference to an immutable block of audio (multiple channels), non-interleaved
class ConstAudioBlock : public AudioBlockBase<float const* const> {
 public:
  ConstAudioBlock() = default;
  ConstAudioBlock(const float* const* ppBuffer, int numChannels, size_t numSamples)
      : AudioBlockBase(ppBuffer, numChannels, numSamples) {}

  V1_DEFAULT_CP_MV(ConstAudioBlock)
};

//! Non-owning reference to a block of audio (multiple channels), non-interleaved
class AudioBlock : public AudioBlockBase<float*> {
 public:
  AudioBlock() = default;
  AudioBlock(float** ppBuffer, int numChannels, size_t numSamples)
      : AudioBlockBase(ppBuffer, numChannels, numSamples) {}

  V1_DEFAULT_CP_MV(AudioBlock)

  // @{ getters
  operator ConstAudioBlock() { return {ppBuffer, numChannels, numSamples}; }

  Span<float> channel(int channel) {
    V1_ASSERT(channel < numChannels);
    return {ppBuffer[channel], numSamples};
  }
  // @}

  // @{ setters
  void clear() {
    for(int i = 0; i < numChannels; ++i) {
      auto pSamples = ppBuffer[i];
      for(size_t j = 0; j < numSamples; ++j) pSamples[j] = 0.;
    }
  }

  void fill(int chan, float value) {
    auto pSamples = ppBuffer[chan];
    for(size_t j = 0; j < numSamples; ++j) pSamples[j] = value;
  }

  void fill(float value) {
    for(int chan = 0; chan < numChannels; ++chan) fill(chan, value);
  }

  void applyGain(int chan, float gain) {
    auto pSamples = ppBuffer[chan];
    for(size_t j = 0; j < numSamples; ++j) pSamples[j] *= gain;
  }

  void applyGain(float gain) {
    for(int chan = 0; chan < numChannels; ++chan) applyGain(chan, gain);
  }
  // @}
};

}}  // namespace v1util::dsp
