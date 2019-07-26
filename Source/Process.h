#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

#include <string>

class ProcessInstance {
protected:
  float volume = 1.0f;
  float patchDuration = 0.0f;
  uint32 processHash = 0;
public:
  class Process* process = nullptr;

  ProcessInstance() = default;
  ProcessInstance(Process* process, float patchDuration);

  virtual ~ProcessInstance() {

  }

  inline uint32 GetProcessHash() const {
    return processHash;
  }

  virtual bool ProcessSamples(float* samples, uint32 numSamples, uint32 frame) = 0;
};

// A process is an entity that takes sound data and does something with it; could
// pitch-shift it, could fade it in/out, could run ADSR, etc.
class Process {
protected:
  std::string className;
  uint32 classHash = 0;
public:
  Process(const Process& that);
  Process(const std::string& className);

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;

  virtual Process* Clone() = 0;

  inline const std::string& GetProcessClassName() const {
    return className;
  }

  inline uint32 GetClassHash() const {
    return classHash;
  }

  virtual void RenderDialog() = 0;

  virtual bool StopPatchOnEnd() {
    return false;
  }
};

