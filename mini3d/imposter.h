#ifndef imposter_h
#define imposter_h

#include "mini3d.h"
#include "3dmath.h"

typedef struct
{
    int retainCount;
    Point3D center;
    float x1, x2, y1, y2;
} Imposter3D;

void Imposter3D_init(Imposter3D* imposter);
Imposter3D* Imposter3D_retain(Imposter3D* imposter);
void Imposter3D_release(Imposter3D* imposter);

#endif