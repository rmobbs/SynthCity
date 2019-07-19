#pragma once

#include "BaseTypes.h"
#include <string>
#include <map>
#include <vector>

class WavData {
public:
  std::string fileName;
  uint32 channels = 0;
  std::vector<uint8> data;

};

class WavBank {
public:
protected:
  static WavBank* singleton;

  std::map<std::string, WavData*> wavDataMap;

  WavBank() = default;

public:
  ~WavBank();

  WavData* GetWav(std::string const& fileName);
  WavData* LoadWav(std::string const& fileName);

  static bool InitSingleton();
  static bool TermSingleton();

  static WavBank& Get() {
    return *singleton;
  }
};