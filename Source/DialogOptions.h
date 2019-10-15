#pragma once

#include "Dialog.h"

class DialogOptions : public Dialog {
public:
  void Open() override;
  bool Render() override;
};