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
#define ENABLE_Z_BUFFER 1

// The ordering table is faster but can produce glitches if the table is too small or there's
// intersecting geometry in the scene.
// This is not as well-supported as the z buffer. Some glitches may occur when using it.
#define ENABLE_ORDERING_TABLE 0

// ------

// this OPTIONALLY enables interlace mode.
// if true, only update odd rows on some frames and even rows on others.
// you must also call lib3d.interlace.enable(1) to set this from lua. (!)
// set this to 2 to only interlace textures
#define ENABLE_INTERLACE 1

// interlace row width is 2 to the power of this number
#define INTERLACE_ROW_LGC 0

// must be at least 2. draw only 1 in every INTERLACE_INTERVAL rows.
// Pixel-write portion of rendering time should decrease as the reciprocal of this.
#define INTERLACE_INTERVAL 2

// allow shapes to have custom dither patterns (per-shape)
#define ENABLE_CUSTOM_PATTERNS 1

// allow textured faces
#define ENABLE_TEXTURES 1

// allow textures to have non-opaque pixels
#define ENABLE_TEXTURES_MASK 1

// allow textures to have intermediate colours, and lighting if ENABLE_TEXTURES_LIGHTING is 1.
// all will be dithered on final render.
#define ENABLE_TEXTURES_GREYSCALE 1

// allow textures to have lighting (requires ENABLE_TEXTURES_GREYSCALE)
#define ENABLE_TEXTURES_LIGHTING 1

// if this is true, then texture dimensions must always be a power of 2.
// However, this enables a performance improvement.
// Undefined behaviour occurs if the texture has dimensions which are not powers of 2.
#define TEXTURES_ALWAYS_POWER_OF_2 1

// ignored if textures are disabled
// if 1, textures are mapped with homogenous coordinates.
// if 0, textures will be significantly warped as they approach
// the camera.
// This is quite expensive. Disable it if at all possible
// (e.g. if textures are always far from the camera, or always orthogonal to it e.g. imposters)
#define ENABLE_TEXTURES_PROJECTIVE 1

// requires ENABLE_TEXTURES_PROJECTIVE
// skips a division step by using a large lookup table
#define PRECOMPUTE_PROJECTION 0

// Only applies if if ENABLE_TEXTURES_PROJECTIVE.
// Value in the range (0, 1).
// Only faces whose total z ratio (from closest to furthest point)
// is less than this will have projective textures.
// If this value is too high, performance may drop.
// If this value is too low, textures may "jump" slightly as they approach the camera.
// Comment this definition out entirely to disable this check.
#define TEXTURE_PROJECTIVE_RATIO_THRESHOLD 0.94

// clip faces which are partly behind the camera.
// This allows rendering faces which are partly behind the camera.
#define FACE_CLIPPING 1

// clip faces that are closer than this
#define CLIP_EPSILON 0.5f

// do not render anything beyond a distance specified
// by lua: mini3d.renderer.setRenderDistance()
// set this to 2 to enable only for textured surfaces
#define ENABLE_RENDER_DISTANCE_MAX 0

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern void* (*m3d_realloc)(void* ptr, size_t size);
void* m3d_malloc(size_t size);
void* m3d_calloc(size_t items, size_t size);
void m3d_free(void* ptr);

void mini3d_setRealloc(void* (*realloc)(void* ptr, size_t size));

#define LIGHTING_PATTERN_COUNT 33
typedef uint8_t Pattern[8];
typedef Pattern PatternTable[LIGHTING_PATTERN_COUNT];

// TODO:
extern PatternTable patterns;

typedef struct PlaydateAPI PlaydateAPI;
typedef struct LCDBitmap LCDBitmap;
PlaydateAPI* pd;

#endif /* mini3d_h */
