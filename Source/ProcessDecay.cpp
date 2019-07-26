#include "ProcessDecay.h"
#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Dialog.h"
#include "Patch.h"
#include "imgui.h"

#include <stdexcept>

class ProcessInstanceDecay : public ProcessInstance {
public:
  using ProcessInstance::ProcessInstance;

  bool ProcessSamples(float* samples, uint32 numSamples, uint32 frame) override {
    // Decay is the percentage of the total length of the sound that should be used for a linear fade-out
    auto derived = static_cast<ProcessDecay*>(process);

    // Decay of ~0.0 would fade out sound immediately
    if (derived->GetDecay() <= std::numeric_limits<float>::epsilon()) {
      return false;
    }

    volume = std::max(0.0f, 1.0f -
      static_cast<float>(frame) / (patchDuration * derived->GetDecay() * 44100.0f));

    // Effectively faded out
    if (volume < std::numeric_limits<float>::epsilon()) {
      return false;
    }

    for (uint32 s = 0; s < numSamples; ++s) {
      samples[s] *= volume;
    }

    return true;
  }
};

static constexpr uint32 kProcessDecayInstancePoolSize = 128;
REGISTER_PROCESS_INSTANCE(ProcessInstanceDecay, ProcessDecay, kProcessDecayInstancePoolSize);

// Decay process
REGISTER_PROCESS(ProcessDecay, "Simple linear decay");

static constexpr const char* kDecayTag = "decay";

ProcessDecay::ProcessDecay()
  : Process("ProcessDecay") {

}

ProcessDecay::ProcessDecay(float decay)
  : Process("ProcessDecay")
  , decay(decay) {

}

ProcessDecay::ProcessDecay(const ProcessDecay& that)
  : Process(that)
  , decay(that.decay) {

}

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

Process* ProcessDecay::Clone() {
  return new ProcessDecay(*this);
}

void ProcessDecay::RenderDialog() {
  ImGui::PushID(&decay);
  ImGui::SliderFloat("Decay", &decay, 0.0f, 1.0f);
  ImGui::PopID();
}
