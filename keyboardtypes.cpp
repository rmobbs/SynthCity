#include "keyboardtypes.h"

struct KeyboardKeyData {
  KeyboardKey key;
  std::string label;
  bool black;
};

std::vector<KeyboardKeyData> KeyboardKeyNames = {
  { A0,  "A0", false },
  { Bb0, "Bb0", true },
  { B0,  "B0", false },

  { C1,  "C1", false },
  { Db1, "Db1", true },
  { D1,  "D1", false },
  { Eb1, "Eb1", true },
  { E1,  "E1", false },
  { F1,  "F1", false },
  { Gb1, "Gb1", true },
  { G1,  "G1", false },
  { Ab1, "Ab1", true },
  { A1,  "A1", false },
  { Bb1, "Bb1", true },
  { B1,  "B1", false },

  { C2,  "C2", false },
  { Db2, "Db2", true },
  { D2,  "D2", false },
  { Eb2, "Eb2", true },
  { E2,  "E2", false },
  { F2,  "F2", false },
  { Gb2, "Gb2", true },
  { G2,  "G2", false },
  { Ab2, "Ab2", true },
  { A2,  "A2", false },
  { Bb2, "Bb2", true },
  { B2,  "B2", false },

  { C3,  "C3", false },
  { Db3, "Db3", true },
  { D3,  "D3", false },
  { Eb3, "Eb3", true },
  { E3,  "E3", false },
  { F3,  "F3", false },
  { Gb3, "Gb3", true },
  { G3,  "G3", false },
  { Ab3, "Ab3", true },
  { A3,  "A3", false },
  { Bb3, "Bb3", true },
  { B3,  "B3", false },

  { C4,  "C4", false },
  { Db4, "Db4", true },
  { D4,  "D4", false },
  { Eb4, "Eb4", true },
  { E4,  "E4", false },
  { F4,  "F4", false },
  { Gb4, "Gb4", true },
  { G4,  "G4", false },
  { Ab4, "Ab4", true },
  { A4,  "A4", false },
  { Bb4, "Bb4", true },
  { B4,  "B4", false },

  { C5,  "C5", false },
  { Db5, "Db5", true },
  { D5,  "D5", false },
  { Eb5, "Eb5", true },
  { E5,  "E5", false },
  { F5,  "F5", false },
  { Gb5, "Gb5", true },
  { G5,  "G5", false },
  { Ab5, "Ab5", true },
  { A5,  "A5", false },
  { Bb5, "Bb5", true },
  { B5,  "B5", false },

  { C6,  "C6", false },
  { Db6, "Db6", true },
  { D6,  "D6", false },
  { Eb6, "Eb6", true },
  { E6,  "E6", false },
  { F6,  "F6", false },
  { Gb6, "Gb6", true },
  { G6,  "G6", false },
  { Ab6, "Ab6", true },
  { A6,  "A6", false },
  { Bb6, "Bb6", true },
  { B6,  "B6", false },

  { C7,  "C7", false },
  { Db7, "Db7", true },
  { D7,  "D7", false },
  { Eb7, "Eb7", true },
  { E7,  "E7", false },
  { F7,  "F7", false },
  { Gb7, "Gb7", true },
  { G7,  "G7", false },
  { Ab7, "Ab7", true },
  { A7,  "A7", false },
  { Bb7, "Bb7", true },
  { B7,  "B7", false },

  { C8,  "C8", false },
};

std::vector<std::array<std::string, KeyVelocity::Count>> KeyboardKeyAudioFiles = {
  { "Piano.pp.A0.wav",  "Piano.mf.A0.wav",  "Piano.ff.A0.wav",  },
  { "Piano.pp.Bb0.wav", "Piano.mf.Bb0.wav", "Piano.ff.Bb0.wav", },
  { "Piano.pp.B0.wav",  "Piano.mf.B0.wav",  "Piano.ff.B0.wav",  },

  { "Piano.pp.C1.wav",  "Piano.mf.C1.wav",  "Piano.ff.C1.wav",  },
  { "Piano.pp.Db1.wav", "Piano.mf.Db1.wav", "Piano.ff.Db1.wav", },
  { "Piano.pp.D1.wav",  "Piano.mf.D1.wav",  "Piano.ff.D1.wav",  },
  { "Piano.pp.Eb1.wav", "Piano.mf.Eb1.wav", "Piano.ff.Eb1.wav", },
  { "Piano.pp.E1.wav",  "Piano.mf.E1.wav",  "Piano.ff.E1.wav",  },
  { "Piano.pp.F1.wav",  "Piano.mf.F1.wav",  "Piano.ff.F1.wav",  },
  { "Piano.pp.Gb1.wav", "Piano.mf.Gb1.wav", "Piano.ff.Gb1.wav", },
  { "Piano.pp.G1.wav",  "Piano.mf.G1.wav",  "Piano.ff.G1.wav",  },
  { "Piano.pp.Ab1.wav", "Piano.mf.Ab1.wav", "Piano.ff.Ab1.wav", },
  { "Piano.pp.A1.wav",  "Piano.mf.A1.wav",  "Piano.ff.A1.wav",  },
  { "Piano.pp.Bb1.wav", "Piano.mf.Bb1.wav", "Piano.ff.Bb1.wav", },
  { "Piano.pp.B1.wav",  "Piano.mf.B1.wav",  "Piano.ff.B1.wav",  },

  { "Piano.pp.C2.wav",  "Piano.mf.C2.wav",  "Piano.ff.C2.wav",  },
  { "Piano.pp.Db2.wav", "Piano.mf.Db2.wav", "Piano.ff.Db2.wav", },
  { "Piano.pp.D2.wav",  "Piano.mf.D2.wav",  "Piano.ff.D2.wav",  },
  { "Piano.pp.Eb2.wav", "Piano.mf.Eb2.wav", "Piano.ff.Eb2.wav", },
  { "Piano.pp.E2.wav",  "Piano.mf.E2.wav",  "Piano.ff.E2.wav",  },
  { "Piano.pp.F2.wav",  "Piano.mf.F2.wav",  "Piano.ff.F2.wav",  },
  { "Piano.pp.Gb2.wav", "Piano.mf.Gb2.wav", "Piano.ff.Gb2.wav", },
  { "Piano.pp.G2.wav",  "Piano.mf.G2.wav",  "Piano.ff.G2.wav",  },
  { "Piano.pp.Ab2.wav", "Piano.mf.Ab2.wav", "Piano.ff.Ab2.wav", },
  { "Piano.pp.A2.wav",  "Piano.mf.A2.wav",  "Piano.ff.A2.wav",  },
  { "Piano.pp.Bb2.wav", "Piano.mf.Bb2.wav", "Piano.ff.Bb2.wav", },
  { "Piano.pp.B2.wav",  "Piano.mf.B2.wav",  "Piano.ff.B2.wav",  },

  { "Piano.pp.C3.wav",  "Piano.mf.C3.wav",  "Piano.ff.C3.wav",  },
  { "Piano.pp.Db3.wav", "Piano.mf.Db3.wav", "Piano.ff.Db3.wav", },
  { "Piano.pp.D3.wav",  "Piano.mf.D3.wav",  "Piano.ff.D3.wav",  },
  { "Piano.pp.Eb3.wav", "Piano.mf.Eb3.wav", "Piano.ff.Eb3.wav", },
  { "Piano.pp.E3.wav",  "Piano.mf.E3.wav",  "Piano.ff.E3.wav",  },
  { "Piano.pp.F3.wav",  "Piano.mf.F3.wav",  "Piano.ff.F3.wav",  },
  { "Piano.pp.Gb3.wav", "Piano.mf.Gb3.wav", "Piano.ff.Gb3.wav", },
  { "Piano.pp.G3.wav",  "Piano.mf.G3.wav",  "Piano.ff.G3.wav",  },
  { "Piano.pp.Ab3.wav", "Piano.mf.Ab3.wav", "Piano.ff.Ab3.wav", },
  { "Piano.pp.A3.wav",  "Piano.mf.A3.wav",  "Piano.ff.A3.wav",  },
  { "Piano.pp.Bb3.wav", "Piano.mf.Bb3.wav", "Piano.ff.Bb3.wav", },
  { "Piano.pp.B3.wav",  "Piano.mf.B3.wav",  "Piano.ff.B3.wav",  },

  { "Piano.pp.C4.wav",  "Piano.mf.C4.wav",  "Piano.ff.C4.wav",  },
  { "Piano.pp.Db4.wav", "Piano.mf.Db4.wav", "Piano.ff.Db4.wav", },
  { "Piano.pp.D4.wav",  "Piano.mf.D4.wav",  "Piano.ff.D4.wav",  },
  { "Piano.pp.Eb4.wav", "Piano.mf.Eb4.wav", "Piano.ff.Eb4.wav", },
  { "Piano.pp.E4.wav",  "Piano.mf.E4.wav",  "Piano.ff.E4.wav",  },
  { "Piano.pp.F4.wav",  "Piano.mf.F4.wav",  "Piano.ff.F4.wav",  },
  { "Piano.pp.Gb4.wav", "Piano.mf.Gb4.wav", "Piano.ff.Gb4.wav", },
  { "Piano.pp.G4.wav",  "Piano.mf.G4.wav",  "Piano.ff.G4.wav",  },
  { "Piano.pp.Ab4.wav", "Piano.mf.Ab4.wav", "Piano.ff.Ab4.wav", },
  { "Piano.pp.A4.wav",  "Piano.mf.A4.wav",  "Piano.ff.A4.wav",  },
  { "Piano.pp.Bb4.wav", "Piano.mf.Bb4.wav", "Piano.ff.Bb4.wav", },
  { "Piano.pp.B4.wav",  "Piano.mf.B4.wav",  "Piano.ff.B4.wav",  },

  { "Piano.pp.C5.wav",  "Piano.mf.C5.wav",  "Piano.ff.C5.wav",  },
  { "Piano.pp.Db5.wav", "Piano.mf.Db5.wav", "Piano.ff.Db5.wav", },
  { "Piano.pp.D5.wav",  "Piano.mf.D5.wav",  "Piano.ff.D5.wav",  },
  { "Piano.pp.Eb5.wav", "Piano.mf.Eb5.wav", "Piano.ff.Eb5.wav", },
  { "Piano.pp.E5.wav",  "Piano.mf.E5.wav",  "Piano.ff.E5.wav",  },
  { "Piano.pp.F5.wav",  "Piano.mf.F5.wav",  "Piano.ff.F5.wav",  },
  { "Piano.pp.Gb5.wav", "Piano.mf.Gb5.wav", "Piano.ff.Gb5.wav", },
  { "Piano.pp.G5.wav",  "Piano.mf.G5.wav",  "Piano.ff.G5.wav",  },
  { "Piano.pp.Ab5.wav", "Piano.mf.Ab5.wav", "Piano.ff.Ab5.wav", },
  { "Piano.pp.A5.wav",  "Piano.mf.A5.wav",  "Piano.ff.A5.wav",  },
  { "Piano.pp.Bb5.wav", "Piano.mf.Bb5.wav", "Piano.ff.Bb5.wav", },
  { "Piano.pp.B5.wav",  "Piano.mf.B5.wav",  "Piano.ff.B5.wav",  },

  { "Piano.pp.C6.wav",  "Piano.mf.C6.wav",  "Piano.ff.C6.wav",  },
  { "Piano.pp.Db6.wav", "Piano.mf.Db6.wav", "Piano.ff.Db6.wav", },
  { "Piano.pp.D6.wav",  "Piano.mf.D6.wav",  "Piano.ff.D6.wav",  },
  { "Piano.pp.Eb6.wav", "Piano.mf.Eb6.wav", "Piano.ff.Eb6.wav", },
  { "Piano.pp.E6.wav",  "Piano.mf.E6.wav",  "Piano.ff.E6.wav",  },
  { "Piano.pp.F6.wav",  "Piano.mf.F6.wav",  "Piano.ff.F6.wav",  },
  { "Piano.pp.Gb6.wav", "Piano.mf.Gb6.wav", "Piano.ff.Gb6.wav", },
  { "Piano.pp.G6.wav",  "Piano.mf.G6.wav",  "Piano.ff.G6.wav",  },
  { "Piano.pp.Ab6.wav", "Piano.mf.Ab6.wav", "Piano.ff.Ab6.wav", },
  { "Piano.pp.A6.wav",  "Piano.mf.A6.wav",  "Piano.ff.A6.wav",  },
  { "Piano.pp.Bb6.wav", "Piano.mf.Bb6.wav", "Piano.ff.Bb6.wav", },
  { "Piano.pp.B6.wav",  "Piano.mf.B6.wav",  "Piano.ff.B6.wav",  },

  { "Piano.pp.C7.wav",  "Piano.mf.C7.wav",  "Piano.ff.C7.wav",  },
  { "Piano.pp.Db7.wav", "Piano.mf.Db7.wav", "Piano.ff.Db7.wav", },
  { "Piano.pp.D7.wav",  "Piano.mf.D7.wav",  "Piano.ff.D7.wav",  },
  { "Piano.pp.Eb7.wav", "Piano.mf.Eb7.wav", "Piano.ff.Eb7.wav", },
  { "Piano.pp.E7.wav",  "Piano.mf.E7.wav",  "Piano.ff.E7.wav",  },
  { "Piano.pp.F7.wav",  "Piano.mf.F7.wav",  "Piano.ff.F7.wav",  },
  { "Piano.pp.Gb7.wav", "Piano.mf.Gb7.wav", "Piano.ff.Gb7.wav", },
  { "Piano.pp.G7.wav",  "Piano.mf.G7.wav",  "Piano.ff.G7.wav",  },
  { "Piano.pp.Ab7.wav", "Piano.mf.Ab7.wav", "Piano.ff.Ab7.wav", },
  { "Piano.pp.A7.wav",  "Piano.mf.A7.wav",  "Piano.ff.A7.wav",  },
  { "Piano.pp.Bb7.wav", "Piano.mf.Bb7.wav", "Piano.ff.Bb7.wav", },
  { "Piano.pp.B7.wav",  "Piano.mf.B7.wav",  "Piano.ff.B7.wav",  },

  { "Piano.pp.C8.wav",  "Piano.mf.C8.wav",  "Piano.ff.C8.wav",  },
};

/* static */
const std::array<std::string, KeyVelocity::Count> &GetAudioFilesForKey(KeyboardKey keyboardKey) {
  return KeyboardKeyAudioFiles[keyboardKey];
}


/* static */
const std::string& GetKeyboardKeyName(KeyboardKey keyboardKey) {
  return KeyboardKeyNames[keyboardKey].label;
}

/* static */
bool IsBlackKey(KeyboardKey keyboardKey) {
  return  KeyboardKeyNames[keyboardKey].black;
}