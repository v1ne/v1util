#include "waveIo.hpp"

#include "v1util/base/alloca.hpp"
#include "v1util/base/debug.hpp"
#include "v1util/base/endianness.hpp"
#include "v1util/base/platform.hpp"
#include "v1util/stl-plus/filesystem.hpp"

#include <algorithm>
#include <cstdio>


namespace v1util::dsp::io {
namespace {
inline FILE* toFile(void* pFile) {
  return (FILE*)pFile;
}

FILE* openWave(const std::filesystem::path& path, bool forWriting, bool overwrite) {
  FILE* pFile = nullptr;
#if defined(V1_OS_WIN)
  auto ret =
      ::_wfopen_s(&pFile, path.native().c_str(), forWriting ? (overwrite ? L"wb" : L"wxb") : L"rb");
  if(ret) pFile = nullptr;
#else
  pFile = ::fopen(path.native().c_str(), forWriting ? (overwrite ? "wb" : "wxb") : "rb");
#endif
  return pFile;
}

#pragma pack(push, 1)
/* RIFF headers are little-endian, except for IDs, which are BE */

struct RiffChunkHeader {
  uint32_t chunkId;  // BE
  uint32_t chunkSize;
};
static_assert(sizeof(RiffChunkHeader) == 8);

struct WaveFormatSubchunkHeader {
  uint16_t audioFormat;  // LE!?
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;       // B/s
  uint16_t blockAlign;     // B/sample frame (i.e. all channels of 1 sample)
  uint16_t bitsPerSample;  // sample: individual sample, one channel
};
static_assert(sizeof(WaveFormatSubchunkHeader) == 16);
#pragma pack(pop)

constexpr const uint16_t kWaveFormatPcm = 1;
constexpr const uint16_t kWaveFormatIeeeFloat = 3;

bool seekToChunk(FILE* pFile, RiffChunkHeader* pHeader, uint32_t chunkId) {
  uint32_t skip = 0;
  do {
    if(::fseek(pFile, long(skip), SEEK_CUR)) return !K(found);
    if(::fread(pHeader, 1, sizeof(*pHeader), pFile) != sizeof(*pHeader)) return !K(found);
    skip = le2nat(pHeader->chunkSize);
  } while(be2nat(pHeader->chunkId) != chunkId);
  return K(found);
}

v1util::dsp::io::WaveInfo readInfo(FILE* pFile) {
  v1util::dsp::io::WaveInfo result = {};
  RiffChunkHeader chunkHeader;
  if(::fread(&chunkHeader, 1, sizeof(chunkHeader), pFile) != sizeof(chunkHeader)) return {};
  if(be2nat(chunkHeader.chunkId) != 'RIFF') return {};

  uint32_t riffFormat;
  if(::fread(&riffFormat, 1, sizeof(riffFormat), pFile) != sizeof(riffFormat)) return {};
  if(be2nat(riffFormat) != 'WAVE') return {};


  if(!seekToChunk(pFile, &chunkHeader, 'fmt ')) return {};
  WaveFormatSubchunkHeader waveFormat;
  if(::fread(&waveFormat, 1, sizeof(waveFormat), pFile) != sizeof(waveFormat)) return {};
  if(::fseek(pFile, chunkHeader.chunkSize - sizeof(waveFormat), SEEK_CUR)) return {};
  switch(waveFormat.audioFormat) {
  case kWaveFormatPcm: result.isFloatingPoint = false; break;
  case kWaveFormatIeeeFloat: result.isFloatingPoint = true; break;
  default: V1_CODEMISSING(); return {};
  }
  V1_ASSERT(le2nat(waveFormat.blockAlign)
            == le2nat(waveFormat.numChannels) * le2nat(waveFormat.bitsPerSample) / 8);


  if(!seekToChunk(pFile, &chunkHeader, 'data')) return {};
  result.numChannels = le2nat(waveFormat.numChannels);
  result.sampleRate = le2nat(waveFormat.sampleRate);
  result.numSamples = le2nat(chunkHeader.chunkSize) / le2nat(waveFormat.blockAlign);
  result.bitsPerSample = uint8_t(le2nat(waveFormat.bitsPerSample));

  return result;
}


bool writeWaveInfo(FILE* pFile, const v1util::dsp::io::WaveInfo& info) {
  V1_ASSERT(pFile);

  uint32_t subchunksSize = 0;
  // data chunk:
  auto sampleBytes = info.numChannels * info.numSamples * (info.bitsPerSample / 8);
  subchunksSize += sizeof(RiffChunkHeader) + ((sampleBytes + 1) / 2 * 2);

  // fact chunk:
  subchunksSize += 12;

  // fmt chunk:
  auto needsFactChunkAndFmtExtension = info.bitsPerSample > 16 || info.isFloatingPoint;
  auto waveChunkSize = 16 + (info.bitsPerSample > 16 ? 2 /* extension size */ : 0);
  subchunksSize += waveChunkSize;

  if(::fseek(pFile, 0, SEEK_SET)) return !K(OK);

  // RIFF
  RiffChunkHeader chunkHeader;
  chunkHeader.chunkId = nat2be('RIFF');
  chunkHeader.chunkSize = nat2le(subchunksSize);
  if(::fwrite(&chunkHeader, sizeof(chunkHeader), 1, pFile) != 1) return !K(OK);
  auto waveFormat = nat2be('WAVE');
  if(::fwrite(&waveFormat, sizeof(waveFormat), 1, pFile) != 1) return !K(OK);

  // FMT
  WaveFormatSubchunkHeader waveHeader = {};
  waveHeader.audioFormat = nat2le(info.isFloatingPoint ? kWaveFormatIeeeFloat : kWaveFormatPcm);
  waveHeader.numChannels = nat2le(uint16_t(info.numChannels));
  waveHeader.sampleRate = nat2le(uint32_t(info.sampleRate));
  waveHeader.blockAlign = nat2le(uint16_t(info.bitsPerSample * info.numChannels / 8));
  waveHeader.byteRate = nat2le(le2nat(waveHeader.blockAlign) * uint32_t(info.sampleRate));
  waveHeader.bitsPerSample = nat2le(uint16_t(info.bitsPerSample));
  chunkHeader.chunkId = nat2be('fmt ');
  chunkHeader.chunkSize = nat2le(waveChunkSize);
  if(::fwrite(&chunkHeader, sizeof(chunkHeader), 1, pFile) != 1) return !K(OK);
  if(::fwrite(&waveHeader, sizeof(waveHeader), 1, pFile) != 1) return !K(OK);
  if(info.bitsPerSample > 16) {
    uint16_t extraSize = 0;
    if(::fwrite(&extraSize, sizeof(extraSize), 1, pFile) != 1) return !K(OK);
  }

  // FACT
  if(needsFactChunkAndFmtExtension) {
    chunkHeader.chunkId = nat2be('fact');
    chunkHeader.chunkSize = nat2le(4);
    uint32_t numSamples = nat2le(info.numSamples);
    if(::fwrite(&chunkHeader, sizeof(chunkHeader), 1, pFile) != 1) return !K(OK);
    if(::fwrite(&numSamples, sizeof(numSamples), 1, pFile) != 1) return !K(OK);
  }

  // DATA
  chunkHeader.chunkId = nat2be('data');
  chunkHeader.chunkSize = nat2le(sampleBytes);
  if(::fwrite(&chunkHeader, sizeof(chunkHeader), 1, pFile) != 1) return !K(OK);

  return K(OK);
}

template <typename T, typename SignedT, uint32_t FullScaleValue>
void deinterleaveAndConvertAudio(ArrayView<T> interleavedSource,
    unsigned int numSourceChannels,
    AudioBlock deinterleavedTarget,
    size_t targetOffset) {
  V1_ASSUME(deinterleavedTarget.numChannels > 0);
  V1_ASSERT(interleavedSource.size() <= numSourceChannels * deinterleavedTarget.numSamples);
  V1_ASSERT(interleavedSource.size() % size_t(deinterleavedTarget.numChannels) == 0);
  auto numSamples = interleavedSource.size() / size_t(numSourceChannels);

  /*
   * Scatter samples by channel, since we assume numChannels < numSamples and
   * thus a smaller stride when iterating over all samples of one channel.
   * Also, we don't assume that channel pointers in deinterleavedTarget have
   * the same stride.
   */
  auto iSourceBegin = interleavedSource.begin();
  for(int chan = 0; chan < deinterleavedTarget.numChannels; ++chan) {
    auto iSource = iSourceBegin + chan;
    auto iTarget = deinterleavedTarget.channel(chan).begin() + targetOffset;
    for(size_t i = 0; i < numSamples; ++i)
      if constexpr(FullScaleValue == 0) {
        static_assert(V1_ENDIANNESS == V1_ENDIANNESS_LITTLE);
        *(iTarget + i) = *(iSource + i * numSourceChannels);
      } else {
        auto nativeSample = le2nat(*(iSource + i * numSourceChannels));
        if constexpr(sizeof(T) < 4)
          *(iTarget + i) = float(SignedT(nativeSample)) / float(FullScaleValue);
        else
          *(iTarget + i) = float(double(SignedT(nativeSample)) / double(FullScaleValue));
      }
  }
}

template <typename T, typename SignedT, uint32_t FullScaleValue>
void convertAndInterleaveAudio(
    ConstAudioBlock deinterleavedSource, size_t sourceOffset, Span<T> interleavedTarget) {
  V1_ASSUME(deinterleavedSource.numChannels > 0);
  V1_ASSERT(
      interleavedTarget.size() <= deinterleavedSource.numChannels * deinterleavedSource.numSamples);
  V1_ASSERT(interleavedTarget.size() % size_t(deinterleavedSource.numChannels) == 0);
  auto numSamples = interleavedTarget.size() / size_t(deinterleavedSource.numChannels);

  /* Gather samples by channel, see deinterleaveAndConvertAudio. */
  auto iTargetBegin = interleavedTarget.begin();
  for(int chan = 0; chan < deinterleavedSource.numChannels; ++chan) {
    auto iTarget = iTargetBegin + chan;
    auto iSource = deinterleavedSource.channel(chan).begin() + sourceOffset;
    for(size_t i = 0; i < numSamples; ++i)
      if constexpr(FullScaleValue == 0) {
        static_assert(V1_ENDIANNESS == V1_ENDIANNESS_LITTLE);
        *(iTarget + i * deinterleavedSource.numChannels) = *(iSource + i);
      } else {
        T nativeSample;
        if constexpr(sizeof(T) < 4)
          nativeSample = T(SignedT(*(iSource + i) * float(FullScaleValue)));
        else
          nativeSample = T(SignedT(double(*(iSource + i)) * double(FullScaleValue)));
        *(iTarget + i * deinterleavedSource.numChannels) = nat2le(nativeSample);
      }
  }
}


template <typename T, typename SignedT, uint32_t FullScaleValue>
size_t readDeinterleaveConvert(FILE* pSource,
    Span<T>
        buffer,
    size_t numSamples,
    unsigned int numSourceChannels,
    AudioBlock target) {
  size_t samplesRead = 0;
  while(samplesRead < numSamples) {
    auto bytesReadIntoBuffer = ::fread(buffer.data(), 1, buffer.size() * sizeof(T), pSource);
    auto samplesReadIntoBuffer = bytesReadIntoBuffer / (sizeof(T) * size_t(numSourceChannels));
    if(!samplesReadIntoBuffer) return samplesRead;

    deinterleaveAndConvertAudio<T, SignedT, FullScaleValue>(
        buffer.view().first(samplesReadIntoBuffer * numSourceChannels),
        numSourceChannels,
        target,
        samplesRead);

    samplesRead += samplesReadIntoBuffer;
  }

  return samplesRead;
}

template <typename T, typename SignedT, uint32_t FullScaleValue>
bool convertInterleaveWrite(ConstAudioBlock source, Span<T> buffer, FILE* pTarget) {
  auto numSamples = size_t(source.numSamples);
  size_t samplesWritten = 0;
  while(samplesWritten < numSamples) {
    auto valuesToWrite =
        std::min(source.numChannels * (numSamples - samplesWritten), buffer.size());
    convertAndInterleaveAudio<T, SignedT, FullScaleValue>(
        source, samplesWritten, buffer.first(valuesToWrite));

    auto valuesWritten = ::fwrite(buffer.data(), sizeof(T), valuesToWrite, pTarget);
    if(valuesWritten != valuesToWrite) return !K(OK);


    samplesWritten += valuesToWrite / source.numChannels;
  }

  return K(OK);
}


}  // namespace


WaveReader::WaveReader(const std::filesystem::path& filename)
    : WaveReader(openWave(filename, !K(forWriting), !K(overwriteExistingFile))) {}

WaveReader::WaveReader(FILE* pFile) {
  if(!pFile) return;

  mInfo = readInfo(pFile);
  if(!mInfo.isValid()) return;
  mpFile = pFile;
}

WaveReader::~WaveReader() {
  if(mpFile) ::fclose(toFile(mpFile));
}

WaveInfo WaveReader::taste(const std::filesystem::path& filename) {
  auto pFile = openWave(filename, !K(forWriting), !K(overwriteExistingFile));
  if(!pFile) return {};

  auto info = readInfo(pFile);
  ::fclose(pFile);
  return info;
}

WaveReader& WaveReader::operator=(WaveReader&& other) noexcept {
  if(mpFile) ::fclose(toFile(mpFile));

  mpFile = other.mpFile;
  mInfo = other.mInfo;
  mSamplePos = other.mSamplePos;

  other.mpFile = nullptr;

  return *this;
}


decltype(AudioBlock::numSamples) WaveReader::read(AudioBlock target) {
  if(!mpFile) {
    V1_INVALID();
    return 0;
  }

  V1_ASSERT(target.numChannels >= 0 && ((unsigned int)(target.numChannels)) <= mInfo.numChannels);

  auto numSamplesToRead = std::min(size_t(mInfo.numSamples - mSamplePos), target.numSamples);

  constexpr const auto kMaxBufferSize = 4096 * 16;
  auto bytesPerValue = mInfo.bitsPerSample / 8;
  auto maxSamplesPerBuffer = size_t(kMaxBufferSize / bytesPerValue / mInfo.numChannels);
  auto bufferSizeB = std::max(std::min(maxSamplesPerBuffer, numSamplesToRead), size_t(1))
                     * bytesPerValue * mInfo.numChannels;

  auto pBuffer = (void*)V1_ALLOCA(bufferSizeB);

  size_t samplesRead = 0;
  switch(mInfo.bitsPerSample) {
  case 8:
    V1_CODEMISSING();
    mpFile = nullptr;
    break;

  case 16:
    samplesRead = readDeinterleaveConvert<uint16_t, int16_t, 0x7FFFU>(toFile(mpFile),
        Span<uint16_t>((uint16_t*)pBuffer, bufferSizeB / bytesPerValue), numSamplesToRead,
        mInfo.numChannels, target);
    break;

  case 32:
    if(mInfo.isFloatingPoint)
      samplesRead = readDeinterleaveConvert<float, float, 0U>(toFile(mpFile),
          Span<float>((float*)pBuffer, bufferSizeB / bytesPerValue), numSamplesToRead,
          mInfo.numChannels, target);
    else
      samplesRead = readDeinterleaveConvert<uint32_t, int32_t, 0x7FFF'FFFFUL>(toFile(mpFile),
          Span<uint32_t>((uint32_t*)pBuffer, bufferSizeB / bytesPerValue), numSamplesToRead,
          mInfo.numChannels, target);
    break;

  default: V1_INVALID();
  }

  mSamplePos += (unsigned int)samplesRead;
  return (unsigned int)samplesRead;
}

FILE* WaveReader::release() {
  auto pFile = mpFile;

  mpFile = nullptr;
  *this = WaveReader();

  return pFile;
}


/******************************************************************************/

WaveWriter::WaveWriter(
    const std::filesystem::path& filename, const WaveInfo& info, bool overwriteExistingFile)
    : WaveWriter(openWave(filename, K(forWriting), overwriteExistingFile), info) {}


WaveWriter::WaveWriter(FILE* pFile, const WaveInfo& info) {
  V1_ASSERT(pFile);
  if(!pFile) return;

  mInfo = info;
  mInfo.numSamples = 0;
  if(!writeWaveInfo(pFile, mInfo)) {
    mInfo = {};
    return;
  }

  mpFile = pFile;
}


WaveWriter::~WaveWriter() {
  if(mpFile) {
    // Overwrite the previous header, now with the right sample count:
    writeWaveInfo(toFile(mpFile), mInfo);
    ::fclose(toFile(mpFile));
  }
}


WaveWriter& WaveWriter::operator=(WaveWriter&& other) noexcept {
  if(mpFile) {
    // Overwrite the previous header, now with the right sample count:
    writeWaveInfo(toFile(mpFile), mInfo);
    ::fclose(toFile(mpFile));
  }

  mpFile = other.mpFile;
  mInfo = other.mInfo;

  other.mpFile = nullptr;

  return *this;
}


bool WaveWriter::write(ConstAudioBlock source) {
  if(!mpFile) {
    V1_INVALID();
    return false;
  }

  V1_ASSERT(source.numChannels >= 0 && ((unsigned int)(source.numChannels)) == mInfo.numChannels);

  constexpr const auto kMaxBufferSize = 4096 * 16;
  auto bytesPerValue = mInfo.bitsPerSample / 8;
  auto maxSamplesPerBuffer = size_t(kMaxBufferSize / bytesPerValue / mInfo.numChannels);
  auto bufferSizeB = std::max(std::min(maxSamplesPerBuffer, size_t(source.numSamples)), size_t(1))
                     * bytesPerValue * mInfo.numChannels;

  auto pBuffer = (void*)V1_ALLOCA(bufferSizeB);

  bool ok = false;
  switch(mInfo.bitsPerSample) {
  case 8:
    V1_CODEMISSING();
    mpFile = nullptr;
    break;

  case 16:
    ok = convertInterleaveWrite<uint16_t, int16_t, 0x7FFFU>(
        source, Span<uint16_t>((uint16_t*)pBuffer, bufferSizeB / bytesPerValue), toFile(mpFile));
    break;

  case 32:
    if(mInfo.isFloatingPoint)
      ok = convertInterleaveWrite<float, float, 0U>(
          source, Span<float>((float*)pBuffer, bufferSizeB / bytesPerValue), toFile(mpFile));
    else
      ok = convertInterleaveWrite<uint32_t, int32_t, 0x7FFF'FFFFU>(
          source, Span<uint32_t>((uint32_t*)pBuffer, bufferSizeB / bytesPerValue), toFile(mpFile));
    break;

  default: V1_INVALID();
  }

  mInfo.numSamples += (unsigned int)source.numSamples;
  return ok;
}

FILE* WaveWriter::release() {
  if(mpFile) writeWaveInfo(mpFile, mInfo);

  auto pFile = mpFile;
  mpFile = nullptr;
  *this = WaveWriter();

  return pFile;
}


}  // namespace v1util::dsp::io
