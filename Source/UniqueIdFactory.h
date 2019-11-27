#pragma once

#include "BaseTypes.h"

template<typename OwnerClass, typename IdType> class UniqueIdFactory{
protected:
  static inline IdType nextId = 0;
public:
  static IdType NextId() {
    return nextId++;
  }
  static void Reset() {
    nextId = 0;
  }
};

