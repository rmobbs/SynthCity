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

bool ProcessDecay::ProcessSamples(float* samples, uint32 numSamples, uint32 frame, Patch* patch, ProcessInstance* instance) {
  // Decay of effectively 0.0 is an unending process
  if (decay <= std::numeric_limits<float>::epsilon()) {
    return true;
  }

  // Decay of effectively 1.0 is immediate termination
  if (decay >= 1.0f - std::numeric_limits<float>::epsilon()) {
    return false;
  }

  // Effectively faded out
  if (instance->volume < std::numeric_limits<float>::epsilon()) {
    return false;
  }

  for (uint32 s = 0; s < numSamples; ++s) {
    samples[s] *= instance->volume;
  }

  float pct = static_cast<float>(frame) /
    (patch->GetSoundDuration() * 44100.0f * (1.0f - decay));
  instance->volume = std::max(0.0f, 1.0f - pct);

  return true;
}

void ProcessDecay::RenderDialog() {
  ImGui::PushID(&decay);
  ImGui::SliderFloat("Decay", &decay, 0.0f, 1.0f);
  ImGui::PopID();
}
