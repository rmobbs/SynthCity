#include "Track.h"
#include "SoundFactory.h"
#include "Sequencer.h"
#include "Instrument.h"
#include "SerializeImpl.h"
#include "Logging.h"
#include "Patch.h"
#include "imgui.h"

static constexpr const char* kNameTag("name");
static constexpr const char* kColorSchemeTag("colorscheme");
static constexpr const char* kSoundsTag("sounds");

Track::Track() {

}

Track::Track(const std::string& name)
  : name(name) {

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

void Track::AddNotes(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  notes.resize(notes.size() + noteCount, noteValue);
  SDL_UnlockAudio();
}

void Track::SetNoteCount(uint32 noteCount, uint8 noteValue) {
  SDL_LockAudio();
  notes.resize(noteCount, noteValue);
  SDL_UnlockAudio();
}

void Track::SetNote(uint32 noteIndex, uint8 noteValue) {
  if (noteIndex >= notes.size()) {
    notes.resize(noteIndex + 1, 0);
  }
  notes[noteIndex] = noteValue;
}

void Track::ClearNotes() {
  SDL_LockAudio();
  std::fill(notes.begin(), notes.end(), 0);
  SDL_UnlockAudio();
}

void Track::SetPatch(Patch* newPatch) {
  SDL_LockAudio();
  delete patch;
  patch = newPatch;
  SDL_UnlockAudio();
}

bool Track::SerializeWrite(const WriteSerializer& serializer) {
  auto& w = serializer.w;

  w.StartObject();

  // Name tag:string
  w.Key(kNameTag);
  w.String(name.c_str());

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

  if (d.HasMember(kColorSchemeTag) && d[kColorSchemeTag].IsString()) {
    colorScheme = d[kColorSchemeTag].GetString();
  }

  try {
    patch = new Patch({ serializer });
  }
  catch (...) {
    return false;
  }

  return true;
}

