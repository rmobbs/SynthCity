#pragma once

#include "Process.h"

class ProcessAttackSustainDecay : public Process {
protected:
  float attack = 0.0f;
  float decay = 1.0f;
  float sustain = 1.0f;
public:
  ProcessAttackSustainDecay();
  ProcessAttackSustainDecay(const ProcessAttackSustainDecay& that);
  ProcessAttackSustainDecay(float attack, float sustain, float decay);
  ProcessAttackSustainDecay(const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;

  void RenderDialog() override;

  Process* Clone() override;

  inline float GetAttack() const {
    return attack;
  }
  inline float GetDecay() const {
    return decay;
  }
  inline float GetSustain() const {
    return sustain;
  }
};
