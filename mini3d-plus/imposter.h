#ifndef imposter_h
#define imposter_h

#include "mini3d.h"
#include "3dmath.h"
#include "texture.h"
#include "scanline.h"

typedef LCDBitmap LCDBitmap;

typedef struct
{
    int retainCount;
    Point3D center;
    float x1, x2, y1, y2;
    float z1, z2, z3, z4;
    
    #if ENABLE_POLYGON_SCANLINING
	ScanlineFill scanline;
    #endif
    
    #if ENABLE_TEXTURES
    Texture* bitmap; // FIXME: rename to 'texture'
    #if ENABLE_TEXTURES_GREYSCALE
    float lighting;
    #endif
    #endif
    
    #if ENABLE_CUSTOM_PATTERNS
    PatternTable* pattern;
    #endif
} Imposter3D;

void Imposter3D_init(Imposter3D* imposter);
Imposter3D* Imposter3D_retain(Imposter3D* imposter);
void Imposter3D_release(Imposter3D* imposter);
void Imposter3D_setPosition(Imposter3D* imposter, Point3D* position);
void Imposter3D_setRectangle(Imposter3D* imposter, float x1, float y1, float x2, float y2);
void Imposter3D_setZOffsets(Imposter3D* imposter, float z1, float z2, float z3, float z4);

#if ENABLE_TEXTURES
// FIXME: rename to ..._setTexture
void Imposter3D_setBitmap(Imposter3D* imposter, Texture* bitmap);

#if ENABLE_TEXTURES_GREYSCALE
void Imposter3D_setLighting(
	Imposter3D* imposter, float lighting
);
#endif
#endif

#if ENABLE_CUSTOM_PATTERNS
// pattern must either be NULL,
// or else it must be a refcounted pattern table created via Pattern_new().
void Imposter3D_setPattern(Imposter3D* imposter, PatternTable* pattern);
#endif

#endif