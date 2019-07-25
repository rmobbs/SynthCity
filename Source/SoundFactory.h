#pragma once

#include "Factory.h"
#include "FreeList.h"

using SoundFactory = Factory<class Sound>;
#define REGISTER_SOUND(SoundClass, SoundDesc) FACTORY_REGISTER(SoundFactory, SoundClass, SoundDesc)

using SoundInstanceFreeList = FreeListMapped<class SoundInstance, class Sound*>;
#define REGISTER_SOUND_INSTANCE(SoundInstanceClass, SoundClass, InitialSize) \
  SoundInstanceFreeList::Information FreeList##SoundInstanceClass(#SoundClass, InitialSize, \
    []() { \
      return new SoundInstanceClass; \
    }, \
    [](void* memory, Sound* sound) { \
      return new (memory) SoundInstanceClass(sound); \
    });


