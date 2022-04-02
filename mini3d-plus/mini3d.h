//
//  mini3d.h
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#ifndef mini3d_h
#define mini3d_h

// Generally, you'd only need one of these, as they're two ways of solving the same problem:

// The Z buffer is slower but more accurate, and can handle intersecting geometry.
#ifndef ENABLE_Z_BUFFER
    #define ENABLE_Z_BUFFER 1
#endif

// The ordering table is faster but can produce glitches if the table is too small or there's
// intersecting geometry in the scene.
// This is not as well-supported as the z buffer. Some glitches may occur when using it.
#ifndef ENABLE_ORDERING_TABLE
    #define ENABLE_ORDERING_TABLE 0
#endif

// ------

// viewport bounds
#ifndef VIEWPORT_LEFT
    #define VIEWPORT_LEFT 0
#endif

#ifndef VIEWPORT_TOP
    #define VIEWPORT_TOP 0
#endif

#ifndef VIEWPORT_RIGHT
    #define VIEWPORT_RIGHT 400
#endif

#ifndef VIEWPORT_BOTTOM
    #define VIEWPORT_BOTTOM 240
#endif

// allow shapes to have custom dither patterns (per-shape)
#ifndef ENABLE_CUSTOM_PATTERNS
    #define ENABLE_CUSTOM_PATTERNS 1
#endif

// allow textured faces
#ifndef ENABLE_TEXTURES
    #define ENABLE_TEXTURES 1
#endif

// allow textures to have non-opaque pixels
#ifndef ENABLE_TEXTURES_MASK
    #define ENABLE_TEXTURES_MASK 1
#endif

// allow textures to have intermediate colours, and lighting if ENABLE_TEXTURES_LIGHTING is 1.
// all will be dithered on final render.
#ifndef ENABLE_TEXTURES_GREYSCALE
    #define ENABLE_TEXTURES_GREYSCALE 1
#endif

// allow textures to have lighting (requires ENABLE_TEXTURES_GREYSCALE)
#ifndef ENABLE_TEXTURES_LIGHTING
    #define ENABLE_TEXTURES_LIGHTING 0
#endif

// if this is true, then texture dimensions must always be a power of 2.
// However, this enables a performance improvement.
// Undefined behaviour occurs if the texture has dimensions which are not powers of 2.
#ifndef TEXTURES_ALWAYS_POWER_OF_2
    #define TEXTURES_ALWAYS_POWER_OF_2 1
#endif

// ignored if textures are disabled
// if 1, textures are mapped with homogenous coordinates.
// if 0, textures will be significantly warped as they approach
// the camera.
// This is quite expensive. Disable it if at all possible
// (e.g. if textures are always far from the camera, or always orthogonal to it e.g. imposters)
#ifndef ENABLE_TEXTURES_PROJECTIVE
    #define ENABLE_TEXTURES_PROJECTIVE 1
#endif

// requires ENABLE_TEXTURES_PROJECTIVE
// skips a division step by using a large lookup table
// you may need to tweak these constants in render.c to achieve good results: PROJECTION_FIDELITY, PROJECTION_FIDELITY_B, UV_SHIFT, W_SHIFT
#ifndef PRECOMPUTE_PROJECTION
    #define PRECOMPUTE_PROJECTION 1
#endif

// Only applies if if ENABLE_TEXTURES_PROJECTIVE.
// If this value is too high, performance may drop.
// If this value is too low, textures may "jump" slightly as they approach the camera.
// Comment this definition out entirely to disable this check.
#ifndef TEXTURE_PROJECTIVE_RATIO_THRESHOLD
    #define TEXTURE_PROJECTIVE_RATIO_THRESHOLD 0.9f
#endif

// Allows shapes to optionally define a 'scanlining' effect which
// causes even (or odd) rows to look different.
// This may have a performance benefit.
// Currently, this is only implemented for textured surfaces.
// (Untextured surfaces can achieve a similar effect with custom dither patterns)
#ifndef ENABLE_POLYGON_SCANLINING
    #define ENABLE_POLYGON_SCANLINING 0
#endif

// clip faces which are partly behind the camera.
// This allows rendering faces which are partly behind the camera.
#ifndef FACE_CLIPPING
    #define FACE_CLIPPING 1
#endif

// Clip faces that are closer than this.
// Also, when using the z buffer, increasing this could also increase 
// the maximum view distance.
#ifndef CLIP_EPSILON
    #define CLIP_EPSILON 0.5f
#endif  

// do not render anything beyond a distance specified
// by lua: mini3d.renderer.setRenderDistance()
// set this to 2 to enable only for textured surfaces
#ifndef ENABLE_RENDER_DISTANCE_MAX
    #define ENABLE_RENDER_DISTANCE_MAX 0
#endif

// this OPTIONALLY enables interlace mode.
// if true, only update odd rows on some frames and even rows on others.
// you must also call lib3d.interlace.enable(1) to set this from lua. (!)
// set this to 2 to only interlace textures
#ifndef ENABLE_INTERLACE
    #define ENABLE_INTERLACE 0
#endif

// interlace row width is 2 to the power of this number
#ifndef INTERLACE_ROW_LGC
    #define INTERLACE_ROW_LGC 0
#endif

// must be at least 2. draw only 1 in every INTERLACE_INTERVAL rows.
// Pixel-write portion of rendering time should decrease as the reciprocal of this.
#ifndef INTERLACE_INTERVAL
    #define INTERLACE_INTERVAL 2
#endif

// experimental alternative to clearing the z buffer to 0 each frame.
#ifndef Z_BUFFER_FRAME_PARITY
    #define Z_BUFFER_FRAME_PARITY 0
#endif

// comment this out to use an 8-bit z-buffer, which is faster but less accurate.
#if !defined(ZBU32) && !defined(ZBUF16) && !defined(ZBUF8)
    #define ZBUF16
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern void* (*m3d_realloc)(void* ptr, size_t size);
void* m3d_malloc(size_t size);
void* m3d_calloc(size_t items, size_t size);
void m3d_free(void* ptr);

void mini3d_setRealloc(void* (*realloc)(void* ptr, size_t size));

#define VIEWPORT_WIDTH (VIEWPORT_RIGHT - VIEWPORT_LEFT)
#define VIEWPORT_HEIGHT (VIEWPORT_BOTTOM - VIEWPORT_TOP)

#define LIGHTING_PATTERN_COUNT 33
typedef uint8_t Pattern[8];
typedef Pattern PatternTable[LIGHTING_PATTERN_COUNT];

// TODO:
extern PatternTable patterns;

typedef struct PlaydateAPI PlaydateAPI;
typedef struct LCDBitmap LCDBitmap;
PlaydateAPI* pd;

#endif /* mini3d_h */
