#pragma once

#include "v1util/base/cpppainrelief.hpp"
#include "v1util/base/time.hpp"

#include <cstdint>


namespace v1util { namespace dsp {

class MidiMsg {
 public:
  enum Types : uint8_t {
    kNoteOff = 0x80,
    kNoteOn = 0x90,
    kPolyphonicAftertouch = 0xA0,
    kControlChange = 0xB0,
    kProgramChange = 0xC0,
    kChannelAftertouch = 0xD0,
    kPitchWheel = 0xE0
  };

  MidiMsg() = default;
  V1_DEFAULT_CP_MV(MidiMsg);

  MidiMsg(uint8_t status, uint8_t b1, uint8_t b2, TscStamp timestamp)
      : mData{status, b1, b2}, mTimestamp(timestamp) {}

  MidiMsg(const uint8_t* pData, TscStamp timestamp)
      : mData{pData[0], pData[1], pData[2]}, mTimestamp(timestamp) {}

  inline uint8_t status() const { return mData[0]; }
  inline uint8_t data1() const { return mData[1]; }
  inline uint8_t data2() const { return mData[2]; }

  inline TscStamp timestamp() const { return mTimestamp; }
  inline void setTimestamp(TscStamp timestamp) { mTimestamp = timestamp; }

  inline const uint8_t* msg() const { return mData; }
  inline uint8_t* msg() { return mData; }


  inline uint8_t type() const { return mData[0] & 0b11110000; }
  inline bool isNoteOn() const { return type() == kNoteOn; }
  inline bool isNoteOff() const { return type() == kNoteOff; }
  inline bool isNote() const {
    auto t = type();
    return t == kNoteOff || t == kNoteOn;
  }

  struct Note {
    bool isOn = false;
    uint8_t number = 0;
    uint8_t velocity = 0;
  };
  Note note() const { return {isNoteOn(), mData[1], mData[2]}; }
  static MidiMsg makeNote(Note note) {
    return {note.isOn ? kNoteOn : kNoteOff, note.number, note.velocity, {}};
  }

 private:
  uint8_t mData[3] = {};
  TscStamp mTimestamp;
};


inline float velocityToFloat(uint8_t midiVelocity) {
  return float(midiVelocity) / 127.f;
}

inline uint8_t velocityToMidi(float velocity) {
  return uint8_t(0.5f + velocity * 127.f);
}

}}  // namespace v1util::dsp
