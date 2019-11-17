#pragma once

#include "BaseTypes.h"
#include <map>

template<typename T> class HashedController {
private:
  std::map<uint32, T*> objectsByClassHash;
  uint32 currentClassHash = kInvalidUint32;
public:
  ~HashedController() {
    Term();
  }

  void Term() {
    for (auto& object : objectsByClassHash) {
      delete object.second;
    }
    objectsByClassHash.clear();
  }

  template<typename D> void Register(D* object) {
    objectsByClassHash.insert({ typeid(D).hash_code(), object });
  }

  template<typename D> D* Get() {
    auto objectByClassHash = objectsByClassHash.find(typeid(D).hash_code());
    if (objectByClassHash != objectsByClassHash.end()) {
      return static_cast<D*>(objectByClassHash->second);
    }
    return nullptr;
  }

  T* GetCurrent() {
    auto objectByClassHash = objectsByClassHash.find(currentClassHash);
    if (objectByClassHash != objectsByClassHash.end()) {
      return objectByClassHash->second;
    }
    return nullptr;
  }

  template<typename D> void SetCurrent() {
    auto objectByClassHash = objectsByClassHash.find(currentClassHash);
    if (objectByClassHash != objectsByClassHash.end()) {
      objectByClassHash->second->Hide();
    }

    currentClassHash = typeid(D).hash_code();

    objectByClassHash = objectsByClassHash.find(currentClassHash);
    if (objectByClassHash != objectsByClassHash.end()) {
      objectByClassHash->second->Show();
    }
  }

  void SetCurrent(uint32 classHash) {
    currentClassHash = classHash;
  }

  const std::map<uint32, T*>& GetAll() {
    return objectsByClassHash;
  }
};
