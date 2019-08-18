#include "View.h"

/* static */
std::map<uint32, View*> View::viewsByClassHash;
/* static */
uint32 View::currentViewHash;