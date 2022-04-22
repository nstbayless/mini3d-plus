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

// Ignored if textures are disabled.
// Can take multiple values:
// 0: always use affine texture mapping
// 1: always use perspective-correct texture mapping
// 2: only use affine mapping on polygons whose zmin/zmax > TEXTURE_PROJECTIVE_RATIO_THRESHOLD
// 3: only use affine mapping on polygons whose zmin/zmax > TEXTURE_PROJECTIVE_AREA_FACTOR / area
// 4: only use affine mapping on polygons whose zmin/zmax > TEXTURE_PROJECTIVE_LENGTH_FACTOR / manhattan-circumference
// This is quite expensive. Disable it if at all possible
// (e.g. if textures are always far from the camera, or always orthogonal to it e.g. imposters)
#ifndef TEXTURE_PERSPECTIVE_MAPPING
    #define TEXTURE_PERSPECTIVE_MAPPING 4
#endif

// Ignored if TEXTURE_PERSPECTIVE_MAPPING <= 1
// Experimental.
// Allows polygons to be partially rendered affine, partially rendered perspective-correct.
#ifndef TEXTURE_PERSPECTIVE_MAPPING_SPLIT
    #define TEXTURE_PERSPECTIVE_MAPPING_SPLIT 0
#endif

// requires TEXTURE_PERSPECTIVE_MAPPING >= 1
// skips a division step by using a large lookup table
// you may need to tweak these constants in render.c to achieve good results: PROJECTION_FIDELITY, PROJECTION_FIDELITY_B, UV_SHIFT, W_SHIFT
// It's been found that this does not improve performance, so it's unadvisable to set this to 1.
#ifndef PRECOMPUTE_PROJECTION
    #define PRECOMPUTE_PROJECTION 0
#endif

// Only applies if TEXTURE_PERSPECTIVE_MAPPING == 2.
// If this value is too high, performance may drop.
// If this value is too low, textures may "jump" slightly as they approach the camera.
// Comment this definition out entirely to disable this check.
#ifndef TEXTURE_PROJECTIVE_RATIO_THRESHOLD
    //#define TEXTURE_PROJECTIVE_RATIO_THRESHOLD 0.85f
#endif

// only applies if TEXTURE_PERSPECTIVE_MAPPING == 3
// Increase this value to improve performance at the cost of warped textures
#ifndef TEXTURE_PROJECTIVE_AREA_FACTOR
    #define TEXTURE_PROJECTIVE_AREA_FACTOR (1 << 10)
#endif

// only applies if TEXTURE_PERSPECTIVE_MAPPING == 4
// Increase this value to improve performance at the cost of warped textures
#ifndef TEXTURE_PROJECTIVE_LENGTH_FACTOR
    #define TEXTURE_PROJECTIVE_LENGTH_FACTOR (1 << 8)
#endif

// Allows shapes to optionally define a 'scanlining' effect which
// causes even (or odd) rows to look different.
// This may have a performance benefit.
// Currently, this is only implemented for textured surfaces.
// (Untextured surfaces can achieve a similar effect with custom dither patterns)
#ifndef ENABLE_POLYGON_SCANLINING
    #define ENABLE_POLYGON_SCANLINING 1
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
    #define CLIP_EPSILON 0.75f
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
    #define ZBUF8
#endif

// distance fog to smoothly fade out objects in the distance to a particular tone.
// requires Z-based rendering.
#ifndef ENABLE_DISTANCE_FOG
    #define ENABLE_DISTANCE_FOG 0
#endif

// avoid unnecessary calculations for pixels that are obscured by the z buffer
#ifndef ENABLE_ZRENDERSKIP
    #define ENABLE_ZRENDERSKIP 1
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
extern PlaydateAPI* pd;

#ifdef MINI3D_AS_LIBRARY
typedef PlaydateAPI PlaydateAPI;
int mini3d_eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg);
#endif

#ifndef FORCEINLINE
    #ifdef __MSVC__
    #define FORCEINLINE __forceinline
    #else
    #define FORCEINLINE __attribute__((always_inline))
    #endif
#endif

#if !defined(MIN)
#define MIN(a, b) (((a)<(b))?(a):(b))
#endif

#if !defined(MAX)
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif

#if !defined(CLAMP)
#define CLAMP(a, b, x) (MAX(a, MIN(b, x)))
#endif

#endif /* mini3d_h */
