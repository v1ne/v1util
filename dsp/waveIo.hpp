#pragma once

#include "audioBlock.hpp"

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/stl-plus/filesystem-fwd.hpp"

#include <cstdio>


namespace v1util::dsp::io {

struct WaveInfo {
  unsigned int numChannels = 0U;
  unsigned int numSamples = 0U;
  double sampleRate = 0.;
  bool isFloatingPoint = false;
  uint8_t bitsPerSample = 0;

  inline bool isValid() const { return numChannels > 0 && sampleRate > 0.; }
};

/*! Blocking RIFF Wave ("WAV") reader
 *
 * Caveat: This is just a bare-bones implementation.
 */
class WaveReader {
 public:
  WaveReader(const stdfs::path& filename);
  WaveReader(FILE* pFile);

  WaveReader() = default;
  WaveReader(const WaveReader&) = delete;
  WaveReader(WaveReader&& other) noexcept { *this = std::move(other); }
  WaveReader& operator=(const WaveReader&) = delete;
  WaveReader& operator=(WaveReader&&) noexcept;
  ~WaveReader();

  //! Returns what opening @p filename as a Wave file would yield.
  static WaveInfo taste(const stdfs::path& filename);

  inline bool isOpen() const { return mpFile; }
  inline const WaveInfo& format() const { return mInfo; }

  //! Returns whether all input has been read
  inline bool empty() const { return mpFile && mSamplePos >= mInfo.numSamples; }
  inline unsigned int samplePos() const { return mSamplePos; }

  //! Reads samples into @p target, returning how many samples were read
  decltype(AudioBlock::numSamples) read(AudioBlock target);

  //! Relinquishes access to the current file handle, returning it.
  FILE* release();

 private:
  FILE* mpFile = nullptr;
  WaveInfo mInfo;
  unsigned int mSamplePos = 0U;
};

/*! Blocking RIFF Wave ("WAV") writer
 *
 * Caveat: This is just a bare-bones implementation.
 */
class WaveWriter {
 public:
  WaveWriter(
      const stdfs::path& filename, const WaveInfo& format, bool overwriteExistingFile = false);
  WaveWriter(FILE* pFile, const WaveInfo& format);

  WaveWriter() = default;
  WaveWriter(const WaveWriter&) = delete;
  WaveWriter(WaveWriter&& other) noexcept { *this = std::move(other); }
  WaveWriter& operator=(const WaveWriter&) = delete;
  WaveWriter& operator=(WaveWriter&&) noexcept;
  ~WaveWriter();

  inline bool isOpen() const { return mpFile; }
  inline const WaveInfo& format() const { return mInfo; }

  //! Returns whether writing all samples was successful
  bool write(ConstAudioBlock source);

  //! Relinquishes access to the current file handle, returning it.
  FILE* release();

 private:
  FILE* mpFile = nullptr;
  WaveInfo mInfo;
};


}  // namespace v1util::dsp::io
