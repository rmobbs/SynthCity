#pragma once

#include "Dialog.h"
#include "BaseTypes.h"
#include "Instrument.h"
#include <string>
#include <array>
#include <vector>

class DialogInstrument : public Dialog {
protected:
  std::string title;
  Instrument* instrument = nullptr;
  bool wasPlaying = false;
  std::vector<std::pair<std::string, std::array<uint32, Instrument::kColorPaletteSize>>> trackPalette;

  std::string GetUniqueColorKeyName(std::string nameBase);
public:
  DialogInstrument(std::string title, Instrument* instrument);
  ~DialogInstrument();

  void Open() override;
  bool Render() override;
  void Close() override;
};

