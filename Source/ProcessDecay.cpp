#include "ProcessDecay.h"
#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Dialog.h"
#include "imgui.h"

#include <stdexcept>

// Decay process
REGISTER_PROCESS(ProcessDecay, "Simple linear decay");

static constexpr const char* kDecayTag = "decay";

ProcessDecay::ProcessDecay(const ReadSerializer& serializer)
  : Process("ProcessDecay") {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize process");
  }
}

bool ProcessDecay::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.Key(kDecayTag);
  w.Double(decay);

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
  if (decay > 0.0f) {
    if (instance->volume < kVolumeEpsilon) {
      return false;
    }

    for (uint32 s = 0; s < numSamples; ++s) {
      samples[s] *= instance->volume;
    }

    instance->volume -= instance->volume * decay * 0.0001f;
  }
  return true;
}

void ProcessDecay::RenderDialog() {
  ImGui::SliderFloat("Decay", &decay, 0.0f, 1.0f);
}
