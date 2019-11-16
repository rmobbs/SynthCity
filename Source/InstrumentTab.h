#pragma once

#include "View.h"
#include "Instrument.h"
#include "InstrumentInstance.h"

#include <set>

class Dialog;
class InstrumentTab : public View {
protected:
  std::set<Instrument*> newInstruments;
  std::set<Instrument*> openInstruments;
  std::map<Instrument*, InstrumentInstance*> instrumentInstances;

  template<typename T> class InstrumentTrackData {
  public:
    Instrument* instrument = nullptr;
    uint32 trackId = kInvalidUint32;
    T data = 0;

    inline bool operator==(const InstrumentTrackData<T>& that) const {
      return instrument == that.instrument && trackId == that.trackId && data == that.data;
    }
    inline bool operator!=(const InstrumentTrackData<T>& that) const {
      return !operator==(that);
    }
  };

  InstrumentTrackData<float> pendingVolumeTrack;
  InstrumentTrackData<int32> pendingPlayTrack;
  InstrumentTrackData<int32> pendingCloneTrack;
  InstrumentTrackData<int32> pendingRemoveTrack;

  bool pendingNewInstrument = false;
  bool pendingLoadInstrument = false;
  Instrument* pendingSaveInstrument = nullptr;

  Dialog* pendingDialog = nullptr;
  Dialog* activeDialog = nullptr;
  Dialog* finishDialog = nullptr;

  bool localGuiDisabled = false;

  void ConditionalEnableBegin(bool condition);
  void ConditionalEnableEnd();

  bool SaveInstrument(Instrument* instrument);
  std::string GetUniqueInstrumentName(std::string instrumentNameBase);
  std::string GetUniqueTrackName(Instrument* instrument, std::string trackNameBase);
public:
  InstrumentTab();
  ~InstrumentTab();

  void Show() override;
  void Hide() override;
  void DoMainMenuBar() override;
  void Render(ImVec2 canvasSize) override;
  void DoLockedActions() override;
};