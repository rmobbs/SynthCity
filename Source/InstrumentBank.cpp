#include "InstrumentBank.h"
#include "Instrument.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Globals.h"

#include <fstream>
#include <string>
#include <map>
#include <list>
#include <functional>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#include <commdlg.h>
#include <atlbase.h>
#include <algorithm>

/* static */
InstrumentBank* InstrumentBank::singleton = nullptr;

bool InstrumentBank::InitSingleton() {
  if (!singleton) {
    singleton = new InstrumentBank;
    return true;
  }
  return false;
}

bool InstrumentBank::TermSingleton() {
  delete singleton;
  return true;
}

InstrumentBank::~InstrumentBank() {
  for (auto instrument : instrumentsByPath) {
    delete instrument.second;
  }
  instrumentsByPath.clear();
}

Instrument* InstrumentBank::LoadInstrumentName(std::string requiredInstrument, bool uniqueOnly) {
  WCHAR szFile[FILENAME_MAX] = { 0 };
  OPENFILENAME ofn = { 0 };

  USES_CONVERSION;
  ofn.lStructSize = sizeof(ofn);

  std::string windowTitle("Load Instrument");
  if (!requiredInstrument.empty()) {
    windowTitle += " " + requiredInstrument;
  }
  ofn.lpstrTitle = A2W(windowTitle.c_str());
  ofn.hwndOwner = reinterpret_cast<HWND>(Globals::mainWindowHandle);
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
  ofn.lpstrFilter = _TEXT("JSON\0*.json\0");
  ofn.nFilterIndex = 0;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  Instrument* instrument = nullptr;

  while (instrument == nullptr) {
    if (GetOpenFileName(&ofn)) {
      instrument = InstrumentBank::Get().LoadInstrumentFile(std::string(W2A(szFile)), uniqueOnly);
      if (instrument) {
        if (!requiredInstrument.empty() && instrument->GetName() != requiredInstrument) {
          delete instrument;
          instrument = nullptr;

          // Hit cancel on error message
          if (IDCANCEL == MessageBox(reinterpret_cast<HWND>(Globals::mainWindowHandle),
            _TEXT("Song requires a different instrument"),
            _TEXT("Error"), MB_OKCANCEL)) {
            break;
          }
          // Try again
        }
        // Correct instrument successfully loaded
      }
      // Instrument failed to load
      else {
        // Hit cancel on error message
        if (IDCANCEL == MessageBox(reinterpret_cast<HWND>(Globals::mainWindowHandle),
          _TEXT("Selected instrument failed to load"),
          _TEXT("Error"), MB_OKCANCEL)) {
          break;
        }
        // Try again
      }
    }
    // Hit cancel on load file dialog
    else {
      break;
    }
  }

  return instrument;
}

Instrument* InstrumentBank::LoadInstrumentFile(std::string fileName, bool uniqueOnly) {
  std::string absoluteFileName = std::filesystem::absolute(fileName).generic_string();
  std::replace(absoluteFileName.begin(), absoluteFileName.end(), '/', '\\');

  // See if it's already loaded
  auto loadedInstrument = instrumentsByPath.find(absoluteFileName);
  if (loadedInstrument != instrumentsByPath.end()) {
    if (uniqueOnly) {
      return nullptr;
    }
    return loadedInstrument->second;
  }

  MCLOG(Info, "Loading instrument from file \'%s\'", fileName.c_str());

  std::ifstream ifs(fileName);

  if (ifs.bad()) {
    MCLOG(Error, "Unable to load instrument from file %s", fileName.c_str());
    return nullptr;
  }

  std::string fileData((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  if (!fileData.length()) {
    MCLOG(Error, "Unable to load instrument from file %s", fileName.c_str());
    return nullptr;
  }

  // Create JSON parser
  rapidjson::Document document;
  document.Parse(fileData.c_str());

  if (!document.IsObject()) {
    MCLOG(Error, "Failure parsing JSON in file %s", fileName.c_str());
    return nullptr;
  }

  auto curpath = std::filesystem::current_path();
  Instrument* newInstrument = nullptr;

  // Path needs to be relative to the instrument to load its WAV files
  std::filesystem::current_path(std::filesystem::absolute(fileName).parent_path());
  try {
    newInstrument = new Instrument({ document, fileName });
    instrumentsByPath.insert({ absoluteFileName, newInstrument });
  }
  catch (std::runtime_error& rte) {
    MCLOG(Error, "Failed to serialize instrument: %s", rte.what());
  }
  std::filesystem::current_path(curpath);

  return newInstrument;
}

