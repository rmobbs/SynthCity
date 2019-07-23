#include "ProcessDecay.h"
#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Dialog.h"
#include "Patch.h"
#include "imgui.h"

#include <stdexcept>

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

ProcessInstance* ProcessDecay::CreateInstance() {
  return new ProcessInstance(this);
}

Process* ProcessDecay::Clone() {
  return new ProcessDecay(*this);
}

bool ProcessDecay::ProcessSamples(float* samples, uint32 numSamples, uint32 frame, ProcessInstance* instance) {
  // Decay is the percentage of the total length of the sound that should be used for a linear fade-out

  // Decay of ~0.0 would fade out sound immediately
  if (decay <= std::numeric_limits<float>::epsilon()) {
    return false;
  }

  instance->volume = std::max(0.0f, 1.0f -
    static_cast<float>(frame) / (instance->soundDuration * 44100.0f * decay));

  // Effectively faded out
  if (instance->volume < std::numeric_limits<float>::epsilon()) {
    return false;
  }

  for (uint32 s = 0; s < numSamples; ++s) {
    samples[s] *= instance->volume;
  }

  return true;
}

void ProcessDecay::RenderDialog() {
  ImGui::PushID(&decay);
  ImGui::SliderFloat("Decay", &decay, 0.0f, 1.0f);
  ImGui::PopID();
}
