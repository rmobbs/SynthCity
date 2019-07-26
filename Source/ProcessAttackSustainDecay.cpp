#include "ProcessAttackSustainDecay.h"

#include "ProcessFactory.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Dialog.h"
#include "Patch.h"
#include "imgui.h"

#include <stdexcept>

static constexpr const char* kAttackTag = "attack";
static constexpr const char* kSustainTag = "sustain";
static constexpr const char* kDecayTag = "decay";

class ProcessInstanceAttackSustainDecay : public ProcessInstance {
protected:
  enum class State {
    Attack,
    Sustain,
    Decay,
  };

  State state = State::Attack;

  uint32 frameSustain = 0;
  uint32 frameDecay = 0;
  uint32 frameEnd = 0;

public:
  using ProcessInstance::ProcessInstance;

  ProcessInstanceAttackSustainDecay(Process* process, float patchDuration)
    : ProcessInstance(process, patchDuration) {
    frameEnd = static_cast<uint32>(patchDuration * 44100.0f);

    frameSustain = static_cast<uint32>(static_cast<ProcessAttackSustainDecay*>(process)->GetAttack() * frameEnd);
    frameDecay = static_cast<uint32>(static_cast<ProcessAttackSustainDecay*>(process)->GetDecay() * frameEnd);
  }

  bool ProcessSamples(float* samples, uint32 numSamples, uint32 frame) override {
    auto derived = static_cast<ProcessAttackSustainDecay*>(process);

    switch (state) {
      case State::Attack: {
        if (frame < frameSustain) {
          volume = derived->GetSustain() *
            (static_cast<float>(frame) / static_cast<float>(frameSustain));
          break;
        }

        state = State::Sustain;
      }
      case State::Sustain: {
        if (frame < frameDecay) {
          volume = derived->GetSustain();
          break;
        }

        state = ProcessInstanceAttackSustainDecay::State::Decay;
      }
      case ProcessInstanceAttackSustainDecay::State::Decay: {
        if (frame < frameEnd) {
          volume = derived->GetSustain() * (1.0f - std::min(1.0f,
            static_cast<float>(frame - frameDecay) / static_cast<float>(frameEnd - frameDecay)));
          break;
        }

        return false;
      }
    }

    for (uint32 s = 0; s < numSamples; ++s) {
      samples[s] *= volume;
    }

    return true;
  }

};

static constexpr uint32 kProcessAttackSustainDecayInstancePoolSize = 128;
REGISTER_PROCESS_INSTANCE(ProcessInstanceAttackSustainDecay, ProcessAttackSustainDecay, kProcessAttackSustainDecayInstancePoolSize);

REGISTER_PROCESS(ProcessAttackSustainDecay, "Attack/sustain/decay envelope");
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

ProcessInstance* ProcessAttackSustainDecay::CreateInstance(float patchDuration) {
  return new ProcessInstanceAttackSustainDecay(this, patchDuration);
}

