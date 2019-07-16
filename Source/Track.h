#pragma once

#include "Dialog.h"
#include <string>
#include <functional>
#include <map>

class Track {
public:
  std::string name;
  std::string colorScheme;
  std::vector<uint8> data;
  int	skip = 0;
  int	mute = 0;
  int soundIndex = -1;
  int voiceIndex = -1;
  float	decay = 0.0f;
  float	lvol = 1.0f;
  float	rvol = 1.0f;

  Track();
  Track(const ReadSerializer& serializer);
  ~Track();

  void AddNotes(uint32 noteCount, uint8 noteValue = 0);
  void SetNoteCount(uint32 noteCount, uint8 noteValue = 0);

  inline const std::string& GetColorScheme(void) const {
    return colorScheme;
  }
  inline const std::string& GetName(void) const {
    return name;
  }
  inline const std::vector<uint8>& GetNotes(void) const {
    return data;
  }

  bool SerializeWrite(const WriteSerializer& serializer);
  bool SerializeRead(const ReadSerializer& serializer);
};

class DialogTrack : public Dialog {
protected:
  std::string trackName;
  std::string soundName;
  Dialog* subDialog = nullptr;
public:
  DialogTrack() = default;
  ~DialogTrack();

  void Open() override;
  bool Render() override;

  bool SerializeWrite(const WriteSerializer& serializer) override;
  bool SerializeRead(const ReadSerializer& serializer) override;
};

