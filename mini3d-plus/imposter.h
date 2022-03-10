#ifndef imposter_h
#define imposter_h

#include "mini3d.h"
#include "3dmath.h"

typedef LCDBitmap LCDBitmap;

typedef struct
{
    int retainCount;
    Point3D center;
    float x1, x2, y1, y2;
    
    #if ENABLE_TEXTURES
    LCDBitmap* bitmap; // (owned)
    #endif
} Imposter3D;

void Imposter3D_init(Imposter3D* imposter);
Imposter3D* Imposter3D_retain(Imposter3D* imposter);
void Imposter3D_release(Imposter3D* imposter);
void Imposter3D_setPosition(Imposter3D* imposter, Point3D* position);
void Imposter3D_setRectangle(Imposter3D* imposter, float x1, float y1, float x2, float y2);

#if ENABLE_TEXTURES
// Note: imposter gains ownership of this bitmap.
void Imposter3D_setBitmap(Imposter3D* imposter, LCDBitmap* bitmap);
#endif

#endif