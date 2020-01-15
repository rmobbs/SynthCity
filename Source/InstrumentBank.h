#pragma once

#include "BaseTypes.h"
#include <string>
#include <map>
#include <vector>

class Instrument;
class InstrumentBank {
public:
protected:
  static InstrumentBank* singleton;

  std::map<std::string, Instrument*> instrumentsByPath;

  InstrumentBank() = default;

public:
  ~InstrumentBank();

  Instrument* LoadInstrumentFile(std::string fileName, bool canInstance);
  Instrument* LoadInstrumentName(std::string instrumentName, bool canInstance);

  inline const std::map<std::string, Instrument*>& GetInstruments() const {
    return instrumentsByPath;
  }

  inline std::map<std::string, Instrument*>& GetInstruments() {
    return instrumentsByPath;
  }

  static bool InitSingleton();
  static bool TermSingleton();

  static InstrumentBank& Get() {
    return *singleton;
  }
};