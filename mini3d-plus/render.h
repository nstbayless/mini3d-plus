//
//  render.h
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#ifndef render_h
#define render_h

#include "3dmath.h"
#include "mini3d.h"
#include "texture.h"
#include "scanline.h"

typedef struct
{
	int16_t start;
	int16_t end;
} LCDRowRange;

LCDRowRange drawLine(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, int thick, uint8_t pattern[8]);
LCDRowRange fillTriangle(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t pattern[8]);
LCDRowRange fillQuad(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4, uint8_t pattern[8]);

#if ENABLE_Z_BUFFER
void resetZBuffer(float zmin);
// intended for debugging.
void render_zbuff(uint8_t* out, int rowstride);

LCDRowRange drawLine_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, int thick, uint8_t pattern[8]);
LCDRowRange fillTriangle_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t pattern[8]);
LCDRowRange fillQuad_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4, uint8_t pattern[8]);
#endif

#if ENABLE_TEXTURES && ENABLE_Z_BUFFER

LCDRowRange fillTriangle_zt(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3,
	Texture* texture, Point2D t1, Point2D t2, Point2D t3
	#if ENABLE_CUSTOM_PATTERNS
	, PatternTable* pattern
	#endif
	#if ENABLE_POLYGON_SCANLINING
	, ScanlineFill* scanline
	#endif
	#if ENABLE_TEXTURES_GREYSCALE
	// if lighting_weight 0, then ignore light and only use texture
	// if lighting weight 1, then ignore texture and only use light
	// (in which case, please call the non-texture version of this function instead!)
	, float lighting, float lighting_weight
	#endif
	#if ENABLE_TEXTURES_PROJECTIVE
	, int projective
	#endif
);
LCDRowRange fillQuad_zt(
	uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4,
	Texture* texture, Point2D t1, Point2D t2, Point2D t3, Point2D t4
	#if ENABLE_CUSTOM_PATTERNS
	, PatternTable* pattern
	#endif
	#if ENABLE_POLYGON_SCANLINING
	, ScanlineFill* scanline
	#endif
	#if ENABLE_TEXTURES_GREYSCALE
	, float lighting, float lighting_weight
	#endif
	#if ENABLE_TEXTURES_PROJECTIVE
	, int projective
	#endif
);

#endif

#if ENABLE_INTERLACE
void setInterlaceEnabled(int e);
void setInterlace(int i);
int getInterlaceEnabled(void);
int getInterlace(void);
#endif

#if ENABLE_RENDER_DISTANCE_MAX
void setRenderDistanceMax(float f);
float getRenderDistanceMax();
#endif

#if ENABLE_TEXTURES && ENABLE_TEXTURES_PROJECTIVE && PRECOMPUTE_PROJECTION
void precomputeProjectionTable(void);
#endif

#endif /* render_h */
