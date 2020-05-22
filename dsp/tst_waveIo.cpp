#include "waveIo.hpp"

#include "audioBuffer.hpp"

#include "v1util/container/range.hpp"
#include "v1util/stl-plus/filesystem.hpp"

#include "doctest/doctest.h"

namespace v1util::dsp::io::test {

TEST_CASE("WaveReader-1channel") {
  auto samples = {0.f, -0.2f, -0.5f, -0.3f, 0.3f, 1.f, 0.f, -1.f};

  for(auto filename : {"wave-1ch-PCM16.wav", "wave-1ch-PCM32.wav", "wave-1ch-float32.wav"}) {
    auto reader = WaveReader(testFilesPath() / "dsp" / filename);
    AudioBuffer buf;
    buf.resize(1, 8);
    CHECK(reader.read(buf.audioBlock()) == 8);
    CHECK(reader.empty());
    CHECK(pmrange::approxEqual(
        buf.channel(0), samples, reader.format().bitsPerSample == 16 ? 4e-5f : 1e-9f));
  }
}

TEST_CASE("WaveReader-3channel") {
  auto samples1 = {0.f, -0.2f, -0.5f, -0.3f, 0.3f, 1.f, 0.f, -1.f};
  auto samples2 = {1.f, -1.f, 0.5f, -0.5f, 0.3f, -0.3f, 0.1f, -0.1f};
  auto samples3 = {0.2f, -0.2f, 0.2f, -0.2f, 0.2f, -0.2f, 0.2f, -.2f};

  auto reader = WaveReader(testFilesPath() / "dsp/wave-3ch-float32.wav");
  AudioBuffer buf;
  buf.resize(3, 8);
  CHECK(reader.read(buf.audioBlock()) == 8);
  CHECK(reader.empty());
  CHECK(pmrange::approxEqual(buf.channel(0), samples1, 1e-9f));
  CHECK(pmrange::approxEqual(buf.channel(1), samples2, 1e-9f));
  CHECK(pmrange::approxEqual(buf.channel(2), samples3, 1e-9f));
}

TEST_CASE("WaveReader-channelRedution") {
  auto samples1 = {0.f, -0.2f, -0.5f, -0.3f, 0.3f, 1.f, 0.f, -1.f};

  auto reader = WaveReader(testFilesPath() / "dsp/wave-3ch-float32.wav");
  AudioBuffer buf;
  buf.resize(1, 8);
  CHECK(reader.read(buf.audioBlock()) == 8);
  CHECK(reader.empty());
  CHECK(pmrange::approxEqual(buf.channel(0), samples1, 1e-9f));
}

TEST_CASE("Wave-roundtrip") {
  auto samples1 = {0.f, -0.2f, -0.5f, -0.3f, 0.3f, 1.f, 0.f, -1.f};
  auto samples2 = {1.f, -1.f, 0.5f, -0.5f, 0.3f, -0.3f, 0.1f, -0.1f};
  auto samples3 = {0.2f, -0.2f, 0.2f, -0.2f, 0.2f, -0.2f, 0.2f, -.2f};

  WaveInfo format;
  format.sampleRate = 8;
  format.numChannels = 3;
  format.isFloatingPoint = false;
  format.bitsPerSample = 16;

  AudioBuffer buf(3, 8);
  pmrange::copy(samples1, buf.channel(0));
  pmrange::copy(samples2, buf.channel(1));
  pmrange::copy(samples3, buf.channel(2));

  auto writer = WaveWriter(std::tmpfile(), format);
  CHECK(writer.write(buf.constAudioBlock()));

  auto pFile = writer.release();
  ::fseek(pFile, 0, SEEK_SET);

  auto reader = WaveReader(pFile);
  CHECK(reader.read(buf.audioBlock()) == 8);
  CHECK(reader.empty());

  CHECK(pmrange::approxEqual(buf.channel(0), samples1, 4e-5f));
  CHECK(pmrange::approxEqual(buf.channel(1), samples2, 4e-5f));
  CHECK(pmrange::approxEqual(buf.channel(2), samples3, 4e-5f));
}

}  // namespace v1util::dsp::io::test
