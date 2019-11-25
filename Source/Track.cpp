#include "Track.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include "Instrument.h"
#include "SerializeImpl.h"
#include "AudioGlobals.h"
#include "Logging.h"
#include "Patch.h"
#include "imgui.h"

#include "ProcessDecay.h"
#include "WavSound.h"

static constexpr const char* kNameTag("name");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");
static constexpr const char* kTrackIdTag("id");

Track::Track(Instrument* instrument)
  : instrument(instrument) {

}

Track::Track(const Track& that)
  : name(that.name)
  , colorScheme(that.colorScheme)
  , volume(that.volume) {
  patch = new Patch(*that.patch);
}

Track::Track(const std::string& name)
  : name(name) {
  patch = new Patch;
}

Track::Track(const ReadSerializer& serializer) {
  if (!SerializeRead(serializer)) {
    throw std::runtime_error("Unable to serialize track");
  }
}

Track::~Track() {
  delete patch;
  patch = nullptr;
}

void Track::SetPatch(Patch* newPatch) {
  delete patch;
  patch = newPatch;
}

bool Track::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // Name tag:string
  w.Key(kNameTag);
  w.String(name.c_str());

  // Track ID
  assert(uniqueId != kInvalidUint32);
  w.Key(kTrackIdTag);
  w.Uint(uniqueId);

  // Color scheme tag:string
  if (colorScheme.length()) {
    w.Key(kColorSchemeTag);
    w.String(colorScheme.c_str());
  }

  patch->SerializeWrite(serializer);

  w.EndObject();

  return true;
}

bool Track::SerializeRead(const ReadSerializer& serializer) {
  auto& d = serializer.d;

  if (!d.HasMember(kNameTag) || !d[kNameTag].IsString()) {
    MCLOG(Error, "Invalid track in tracks array!");
    return false;
  }
  name = d[kNameTag].GetString();

  if (d.HasMember(kTrackIdTag)) {
    if (!d[kTrackIdTag].IsUint()) {
      MCLOG(Error, "Invalid track ID in tracks array");
      return false;
    }
    uniqueId = d[kTrackIdTag].GetUint();
    assert(uniqueId != kInvalidUint32);
  }

  if (d.HasMember(kColorSchemeTag) && d[kColorSchemeTag].IsString()) {
    colorScheme = d[kColorSchemeTag].GetString();

    if (instrument->GetTrackPalette().find(colorScheme) == instrument->GetTrackPalette().end()) {
      colorScheme.clear();
    }
  }

  try {
    patch = new Patch({ serializer });
  }
  catch (...) {
    return false;
  }

  return true;
}

