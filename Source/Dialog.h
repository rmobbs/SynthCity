#pragma once

#include "BaseTypes.h"
#include "SerializeFwd.h"

class Dialog {
protected:
  bool exitedOk = false;
public:
  Dialog() = default;
  virtual ~Dialog() = default;

  virtual void Open();
  virtual bool Render();
  virtual void Close() {

  }

  inline bool ExitedOk() const {
    return exitedOk;
  }
};

