#include "ProcessAttackSustainDecay.h"

#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Dialog.h"
#include "Patch.h"
#include "imgui.h"

#include <stdexcept>

// Decay process
REGISTER_PROCESS(ProcessAttackSustainDecay, "Attack/sustain/decay envelope");

static constexpr const char* kAttackTag = "attack";
static constexpr const char* kSustainTag = "sustain";
static constexpr const char* kDecayTag = "decay";

class ProcessInstanceAttackSustainDecay : public ProcessInstance {
public:
  enum class State {
    Attack,
    Sustain,
    Decay,
  };
  State state = State::Attack;

  using ProcessInstance::ProcessInstance;


};

ProcessAttackSustainDecay::ProcessAttackSustainDecay()
  : Process("ProcessAttackSustainDecay") {

}

ProcessAttackSustainDecay::ProcessAttackSustainDecay(float attack, float sustain, float decay)
: Process("ProcessAttackSustainDecay")
, attack(attack)
, sustain(sustain)
, decay(decay) {

}

ProcessAttackSustainDecay::ProcessAttackSustainDecay(const ProcessAttackSustainDecay& that)
  : Process(that)
  , attack(that.attack)
  , sustain(that.sustain)
  , decay(that.decay) {

}

ProcessAttackSustainDecay::ProcessAttackSustainDecay(const ReadSerializer& serializer)
  : Process("ProcessAttackSustainDecay") {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize process");
  }
}

bool ProcessAttackSustainDecay::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.Key(kAttackTag);
  w.Double(attack);

  w.Key(kSustainTag);
  w.Double(sustain);

  w.Key(kDecayTag);
  w.Double(decay);

  return true;
}

bool ProcessAttackSustainDecay::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  if (!d.HasMember(kAttackTag) || !d[kAttackTag].IsDouble()) {
    MCLOG(Error, "Missing or invalid attack in process");
    return false;
  }

  attack = static_cast<float>(d[kAttackTag].GetDouble());

  if (!d.HasMember(kSustainTag) || !d[kSustainTag].IsDouble()) {
    MCLOG(Error, "Missing or invalid sustain in process");
    return false;
  }

  sustain = static_cast<float>(d[kSustainTag].GetDouble());

  if (!d.HasMember(kDecayTag) || !d[kDecayTag].IsDouble()) {
    MCLOG(Error, "Missing or invalid decay in process");
    return false;
  }

  decay = static_cast<float>(d[kDecayTag].GetDouble());

  return true;
}

Process* ProcessAttackSustainDecay::Clone() {
  return new ProcessAttackSustainDecay(*this);
}

bool ProcessAttackSustainDecay::ProcessSamples(float* samples, uint32 numSamples, uint32 frame, ProcessInstance* genericInstance) {
  ProcessInstanceAttackSustainDecay* instance = static_cast<ProcessInstanceAttackSustainDecay*>(genericInstance);

  // TODO: Would be great to figure this out somewhere else ...
  uint32 lastFrame = static_cast<uint32>(instance->soundDuration * 44100.0f);
  uint32 sustainFrame =  static_cast<uint32>(attack * lastFrame);
  uint32 decayFrame = static_cast<uint32>(decay * lastFrame);

  switch (instance->state) {
    case ProcessInstanceAttackSustainDecay::State::Attack: {
      if (frame < sustainFrame) {
        instance->volume = sustain * static_cast<float>(frame) / static_cast<float>(sustainFrame);
        break;
      }

      instance->state = ProcessInstanceAttackSustainDecay::State::Sustain;

      // Intentional fall-through
    }
    case ProcessInstanceAttackSustainDecay::State::Sustain: {
      if (frame < decayFrame) {
        instance->volume = sustain;
        break;
      }

      instance->state = ProcessInstanceAttackSustainDecay::State::Decay;

      // Intentional fall-through
    }
    case ProcessInstanceAttackSustainDecay::State::Decay: {
      if (frame < lastFrame) {
        float pct = 1.0f - std::min(1.0f, static_cast<float>(frame - decayFrame) / static_cast<float>(lastFrame - decayFrame));
        instance->volume = sustain * pct;
        break;
      }

      return false;
    }
  }

  for (uint32 s = 0; s < numSamples; ++s) {
    samples[s] *= instance->volume;
  }

  return true;
}

void ProcessAttackSustainDecay::RenderDialog() {
  ImGui::PushID(&attack);
  if (ImGui::SliderFloat("Attack", &attack, 0.0f, 1.0f)) {
    if (attack > decay) {
      decay = std::min(1.0f, attack + 0.01f);
    }
  }
  ImGui::PopID();

  ImGui::PushID(&sustain);
  ImGui::SliderFloat("Sustain", &sustain, 0.0f, 1.0f);
  ImGui::PopID();

  ImGui::PushID(&decay);
  if (ImGui::SliderFloat("Decay", &decay, 0.0f, 1.0f)) {
    if (decay < attack) {
      attack = std::min(1.0f, decay - 0.01f);
    }
  }
  ImGui::PopID();
}

ProcessInstance* ProcessAttackSustainDecay::CreateInstance() {
  return new ProcessInstanceAttackSustainDecay(this);
}

