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

  inline bool ExitedOk() const {
    return exitedOk;
  }

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;
};

