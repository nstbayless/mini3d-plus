#ifndef texture_h
#define texture_h

#include "mini3d.h"
#include <pd_api.h>

#if ENABLE_TEXTURES

// a Texture* is a tagged pointer
// if ends in 0, then points to an LCDBitmap*.
// if ends in 1, then points to a GreyBitmap.
// in both cases, subtract 4 bytes to get a refcounter.
typedef void Texture;

#if ENABLE_TEXTURES_GREYSCALE

typedef struct
{
    // 1 byte per pixel
    // bit 0x80 is 0 if transparent
    // bits 0x3F describe the intensity (0-33)
    // bit 0x4 unused
    
    uint16_t width;
    uint16_t height;
    uint8_t transparency:1;
    // (data follows)
} GreyBitmap;

static inline int
Texture_isLCDBitmap(Texture* t)
{
    return ((uintptr_t)t & 1) == 0;
}

static inline int
Texture_isGreyBitmap(Texture* t)
{
    return ((uintptr_t)t & 1) == 1;
}

static inline LCDBitmap*
Texture_getLCDBitmap(Texture* t)
{
    // (~1 is actually not necessary)
    return *(LCDBitmap**)((uintptr_t)t & ~1);
}

static inline GreyBitmap*
Texture_getGreyBitmap(Texture* t)
{
    return (void*)((uintptr_t)t & ~1);
}

// returns fmt=0 if lcd, fmt=1 if greyscale
static inline void
Texture_getData(Texture* t, int* width, int* height, int* rowbytes, int* hasmask, int* fmt, uint8_t** data)
{
    if (Texture_isLCDBitmap(t))
    {
        pd->graphics->getBitmapData(Texture_getLCDBitmap(t), width, height, rowbytes, (uint8_t**)hasmask, data);
        if (fmt) *fmt = 0;
    }
    else
    {
        GreyBitmap* g = Texture_getGreyBitmap(t);
        if (width) *width = g->width;
        if (height) *height = g->height;
        if (rowbytes) *rowbytes = g->width;
        if (hasmask) *hasmask = g->transparency;
        if (fmt) *fmt = 1;
        if (data) *data = (void*)(g + 1);
    }
}
#else
static inline int
Texture_isLCDBitmap(Texture* t)
{
    return 1;
}

static inline int
Texture_isGreyBitmap(Texture* t)
{
    return 0;
}

static inline LCDBitmap*
Texture_getLCDBitmap(Texture* t)
{
    return *(LCDBitmap**)t;
}

static inline void
Texture_getData(Texture* t, int* width, int* height, int* rowbytes, int* hasmask, int* fmt, uint8_t** data)
{
    pd->graphics->getBitmapData(Texture_getLCDBitmap(t), width, height, rowbytes, hasmask, data);
    if (fmt) *fmt = 0;
}
#endif

// creates a texture with a refcount of 1.
Texture* Texture_loadFromPath(const char* path, int greyscale, const char** outerr);

// takes ownership of the LCDBitmap
// returns a texture with a refcount of 1.
Texture* Texture_fromLCDBitmap(LCDBitmap* l);

static inline
uint32_t* Texture_refCount(Texture* t)
{
    return (uint32_t*)((uintptr_t)t & ~1) - 1;
}

Texture* Texture_ref(Texture* t);
void Texture_unref(Texture* t);

#endif
#endif