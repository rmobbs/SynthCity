#pragma once

#include "Process.h"

// Most basic process is decay
class ProcessDecay : public Process {
protected:
  float decay = 0.0f;
public:
  ProcessDecay();
  ProcessDecay(const ProcessDecay& that);
  ProcessDecay(float decay);
  ProcessDecay(const ReadSerializer& serializer);

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;

  ProcessInstance* CreateInstance(float patchDuration) override;

  void RenderDialog() override;

  Process* Clone() override;

  inline float GetDecay() const {
    return decay;
  }
};
