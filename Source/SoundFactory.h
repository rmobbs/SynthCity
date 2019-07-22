#pragma once

#include "Factory.h"

class Sound;
class SoundFactory : public Factory<Sound> {

};

#define REGISTER_SOUND(SoundClass, SoundDesc) FACTORY_REGISTER(SoundFactory, SoundClass, SoundDesc)
