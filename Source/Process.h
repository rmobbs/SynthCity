#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

class ProcessInstance {
public:
  float volume = 1.0f;
};

// A process is an entity that takes sound data and does something with it; could
// pitch-shift it, could fade it in/out, could run ADSR, etc.
class Process {
public:
  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;

  virtual ProcessInstance* CreateInstance() const = 0;
  virtual bool ProcessSamples(float* samples, uint32 numSamples, uint32 frame, ProcessInstance* instance) = 0;
};

// Most basic process is decay
class ProcessDecay : public Process {
protected:
  static constexpr float kVolumeEpsilon = 0.01f;

  float decay = 0.0f;
public:
  ProcessDecay() = default;
  ProcessDecay(float decay)
    : decay(decay) {

  }
  ProcessDecay(const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;

  ProcessInstance* CreateInstance() const override;
  bool ProcessSamples(float* samples, uint32 numSamples, uint32 frame, ProcessInstance* instance) override;
};
