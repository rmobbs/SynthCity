#include "Sound.h"
#include <stdexcept>

SoundInstance::SoundInstance(Sound* sound)
  : sound(sound)
  , soundHash(sound->GetClassHash()) {

}

Sound::Sound(const Sound& that)
  : className(that.className)
  , classHash(that.classHash) {

}

Sound::Sound(const std::string& className)
  : className(className)
  , classHash(std::hash<std::string>{}(className)) {
}
