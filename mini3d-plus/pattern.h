#ifndef pattern_h
#define pattern_h

#include "mini3d.h"

#if ENABLE_CUSTOM_PATTERNS
PatternTable* Pattern_new(void);
PatternTable* Pattern_ref(PatternTable* p);
void Pattern_unref(PatternTable* p);
#endif
#endif