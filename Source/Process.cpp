#include "Process.h"
#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "Logging.h"

#include <stdexcept>

// Decay process
REGISTER_PROCESS(ProcessDecay, "Simple linear decay", DialogProcessDecay);

static constexpr const char* kDecayTag = "decay";

ProcessDecay::ProcessDecay(const ReadSerializer& serializer) {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize process");
  }
}

bool ProcessDecay::SerializeWrite(const WriteSerializer& serializer) {
  return true;
}

bool ProcessDecay::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  if (!d.HasMember(kDecayTag) || !d[kDecayTag].IsDouble()) {
    MCLOG(Error, "Missing or invalid decay in patch");
    return false;
  }

  decay = static_cast<float>(d[kDecayTag].GetDouble());

  return true;
}

ProcessInstance* ProcessDecay::CreateInstance() const {
  return new ProcessInstance;
}

bool ProcessDecay::ProcessSamples(float* samples, uint32 numSamples, uint32 frame, ProcessInstance* instance) {
  if (instance->volume < kVolumeEpsilon) {
    return false;
  }

  for (uint32 s = 0; s < numSamples; ++s) {
    samples[s] *= instance->volume;
  }

  instance->volume -= instance->volume * decay;
  
  return true;
}
