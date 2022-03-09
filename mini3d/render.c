//
//  render.c
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize ("O3")
#endif

#include <stdint.h>
#include <string.h>
#include "render.h"
#include <assert.h>

#define LCD_ROWS 240
#define LCD_COLUMNS 400

#if !defined(MIN)
#define MIN(a, b) (((a)<(b))?(a):(b))
#endif

#if !defined(MAX)
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif

#if !defined (CLAMP)
#define CLAMP(a, b, x) (MAX(a, MIN(b, x)))
#endif

#if ENABLE_TEXTURES_PROJECTIVE
	typedef int32_t uvw_int_t;
	typedef int64_t uvw_int2_t;
	#define UVW_SHIFT 28
	#define UVW_SLOPE slope64
#else
	typedef int16_t uvw_int_t;
	typedef int32_t uvw_int2_t;
	#define UVW_SHIFT 16
	#define UVW_SLOPE slope
#endif

Pattern patterns[LIGHTING_PATTERN_COUNT] =
{
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00 },
	{ 0x88, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00 },
	{ 0x88, 0x00, 0x20, 0x00, 0x88, 0x00, 0x02, 0x00 },
	{ 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 },
	{ 0xa8, 0x00, 0x22, 0x00, 0x8a, 0x00, 0x22, 0x00 },
	{ 0xaa, 0x00, 0x22, 0x00, 0xaa, 0x00, 0x22, 0x00 },
	{ 0xaa, 0x00, 0xa2, 0x00, 0xaa, 0x00, 0x2a, 0x00 },
	{ 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00 },
	{ 0xaa, 0x40, 0xaa, 0x00, 0xaa, 0x04, 0xaa, 0x00 },
	{ 0xaa, 0x44, 0xaa, 0x00, 0xaa, 0x44, 0xaa, 0x00 },
	{ 0xaa, 0x44, 0xaa, 0x10, 0xaa, 0x44, 0xaa, 0x01 },
	{ 0xaa, 0x44, 0xaa, 0x11, 0xaa, 0x44, 0xaa, 0x11 },
	{ 0xaa, 0x54, 0xaa, 0x11, 0xaa, 0x45, 0xaa, 0x11 },
	{ 0xaa, 0x55, 0xaa, 0x11, 0xaa, 0x55, 0xaa, 0x11 },
	{ 0xaa, 0x55, 0xaa, 0x51, 0xaa, 0x55, 0xaa, 0x15 },
	{ 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55 },
	{ 0xba, 0x55, 0xaa, 0x55, 0xab, 0x55, 0xaa, 0x55 },
	{ 0xbb, 0x55, 0xaa, 0x55, 0xbb, 0x55, 0xaa, 0x55 },
	{ 0xbb, 0x55, 0xea, 0x55, 0xbb, 0x55, 0xae, 0x55 },
	{ 0xbb, 0x55, 0xee, 0x55, 0xbb, 0x55, 0xee, 0x55 },
	{ 0xfb, 0x55, 0xee, 0x55, 0xbf, 0x55, 0xee, 0x55 },
	{ 0xff, 0x55, 0xee, 0x55, 0xff, 0x55, 0xee, 0x55 },
	{ 0xff, 0x55, 0xfe, 0x55, 0xff, 0x55, 0xef, 0x55 },
	{ 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55 },
	{ 0xff, 0x55, 0xff, 0xd5, 0xff, 0x55, 0xff, 0x5d },
	{ 0xff, 0x55, 0xff, 0xdd, 0xff, 0x55, 0xff, 0xdd },
	{ 0xff, 0x75, 0xff, 0xdd, 0xff, 0x57, 0xff, 0xdd },
	{ 0xff, 0x77, 0xff, 0xdd, 0xff, 0x77, 0xff, 0xdd },
	{ 0xff, 0x77, 0xff, 0xfd, 0xff, 0x77, 0xff, 0xdf },
	{ 0xff, 0x77, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff },
	{ 0xff, 0xf7, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff },
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
};

#if ENABLE_Z_BUFFER
static uint16_t zbuf[LCD_COLUMNS*LCD_ROWS];
static float zscale;

#define Z_BIAS 0

void resetZBuffer(float zmin)
{
	memset(zbuf, 0, sizeof(zbuf));
	zscale = 0xffff * (zmin + Z_BIAS);
}
#endif

// swap big-endian / little-endian
static inline uint32_t swap(uint32_t n)
{
#if TARGET_PLAYDATE
	//return __REV(n);
	uint32_t result;
	
	__asm volatile ("rev %0, %1" : "=l" (result) : "l" (n));
	return(result);
#else
	return ((n & 0xff000000) >> 24) | ((n & 0xff0000) >> 8) | ((n & 0xff00) << 8) | (n << 24);
#endif
}

static inline void
_drawMaskPattern(uint32_t* p, uint32_t mask, uint32_t color)
{
	if ( mask == 0xffffffff )
		*p = color;
	else
		*p = (*p & ~mask) | (color & mask);
}

static void
drawFragment(uint32_t* row, int x1, int x2, uint32_t color)
{
	if ( x2 < 0 || x1 >= LCD_COLUMNS )
		return;
	
	if ( x1 < 0 )
		x1 = 0;
	
	if ( x2 > LCD_COLUMNS )
		x2 = LCD_COLUMNS;
	
	if ( x1 > x2 )
		return;
	
	// Operate on 32 bits at a time
	
	int startbit = x1 % 32;
	uint32_t startmask = swap((1 << (32 - startbit)) - 1);
	int endbit = x2 % 32;
	uint32_t endmask = swap(((1 << endbit) - 1) << (32 - endbit));
	
	int col = x1 / 32;
	uint32_t* p = row + col;

	if ( col == x2 / 32 )
	{
		uint32_t mask = 0;
		
		if ( startbit > 0 && endbit > 0 )
			mask = startmask & endmask;
		else if ( startbit > 0 )
			mask = startmask;
		else if ( endbit > 0 )
			mask = endmask;
		
		_drawMaskPattern(p, mask, color);
	}
	else
	{
		int x = x1;
		
		if ( startbit > 0 )
		{
			_drawMaskPattern(p++, startmask, color);
			x += (32 - startbit);
		}
		
		while ( x + 32 <= x2 )
		{
			_drawMaskPattern(p++, 0xffffffff, color);
			x += 32;
		}
		
		if ( endbit > 0 )
			_drawMaskPattern(p, endmask, color);
	}
}

#if ENABLE_TEXTURES_PROJECTIVE
	#if ENABLE_TEXTURES_MASK
		#define RENDER_T
		#define RENDER_A
		#define RENDER_P
		#include "render.inc"
	#endif

	#if ENABLE_TEXTURES >= 1
		#define RENDER_T
		#define RENDER_P
		#include "render.inc"
	#endif

	#if ENABLE_Z_BUFFER
		#if ENABLE_TEXTURES_GREYSCALE
			#if ENABLE_TEXTURES_MASK
				#define RENDER_Z
				#define RENDER_T
				#define RENDER_A
				#define RENDER_G
				#define RENDER_P
				#include "render.inc"
			#endif
			
			#define RENDER_Z
			#define RENDER_T
			#define RENDER_G
			#define RENDER_P
			#include "render.inc"
		#endif
		
		#if ENABLE_TEXTURES_MASK
			#define RENDER_Z
			#define RENDER_T
			#define RENDER_A
			#define RENDER_P
			#include "render.inc"
		#endif

		#if ENABLE_TEXTURES
			#define RENDER_Z
			#define RENDER_T
			#define RENDER_P
			#include "render.inc"
		#endif
	#endif
#endif

#if ENABLE_TEXTURES_MASK
	#define RENDER_T
	#define RENDER_A
	#include "render.inc"
#endif

#if ENABLE_TEXTURES >= 1
	#define RENDER_T
	#include "render.inc"
#endif

#if ENABLE_Z_BUFFER
	#define RENDER_Z
	#include "render.inc"
	
	#if ENABLE_TEXTURES_GREYSCALE
		#if ENABLE_TEXTURES_MASK
			#define RENDER_Z
			#define RENDER_T
			#define RENDER_A
			#define RENDER_G
			#include "render.inc"
		#endif
		
		#define RENDER_Z
		#define RENDER_T
		#define RENDER_G
		#include "render.inc"
	#endif
	
	#if ENABLE_TEXTURES_MASK
		#define RENDER_Z
		#define RENDER_T
		#define RENDER_A
		#include "render.inc"
	#endif

	#if ENABLE_TEXTURES
		#define RENDER_Z
		#define RENDER_T
		#include "render.inc"
	#endif
#endif


static inline int32_t slope(float x1, float y1, float x2, float y2)
{
	float dx = x2-x1;
	float dy = y2-y1;
	
	if ( dy < 1 )
		return dx * (1<<16);
	else
		return dx / dy * (1<<16);
}

static inline int64_t slope64(float x1, float y1, float x2, float y2)
{
	float dx = x2-x1;
	float dy = y2-y1;
	
	if ( dy < 1 )
		return dx * ((int64_t)(1)<<28);
	else
		return dx / dy * ((int64_t)(1)<<28);
}

#if ENABLE_Z_BUFFER
LCDRowRange
drawLine_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, int thick, uint8_t pattern[8])
{
	if ( p1->y > p2->y )
	{
		Point3D* tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	int y = p1->y;
	int endy = p2->y;
	
	if ( y >= LCD_ROWS || endy < 0 || MIN(p1->x, p2->x) >= LCD_COLUMNS || MAX(p1->x, p2->x) < 0 )
		return (LCDRowRange){ 0, 0 };
	
	int32_t x = p1->x * (1<<16);
	int32_t dx = slope(p1->x, p1->y, p2->x, p2->y + 1);
	
	// move lines a bit forward so they don't get buried in solid geometry
	float z1 = zscale / (p1->z + Z_BIAS) + 256;
	float z2 = zscale / (p2->z + Z_BIAS) + 256;

	if ( z1 > 65535 ) z1 = 65535;
	if ( z2 > 65535 ) z2 = 65535;

	int32_t dzdy = slope(z1, p1->y, z2, p2->y + 1);
	int32_t dzdx = slope(z1, p1->x, z2, p2->x);

	uint32_t z = z1 * (1<<16);

	if ( y < 0 )
	{
		x += -y * dx;
		z += -y * dzdy;
		y = 0;
	}
	
	while ( y <= endy )
	{
		uint8_t p = pattern[y%8];
		uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
		int32_t x1 = (y < endy) ? x+dx : p2->x * (1<<16);

		if ( dx < 0 )
		{
			z += dzdy;
			drawFragment_z((uint32_t*)&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], x1>>16, (x>>16) + thick, z, dzdx, color);
		}
		else
		{
			drawFragment_z((uint32_t*)&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], x>>16, (x1>>16) + thick, z, dzdx, color);
			z += dzdy;
		}

		if ( ++y == LCD_ROWS )
			break;
		
		x = x1;
	}
	
	return (LCDRowRange){ MAX(0, p1->y), MIN(LCD_ROWS, p2->y) };
}
#endif

LCDRowRange
drawLine(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, int thick, uint8_t pattern[8])
{
	if ( p1->y > p2->y )
	{
		Point3D* tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	int y = p1->y;
	int endy = p2->y;
	
	if ( y >= LCD_ROWS || endy < 0 || MIN(p1->x, p2->x) >= LCD_COLUMNS || MAX(p1->x, p2->x) < 0 )
		return (LCDRowRange){ 0, 0 };
	
	int32_t x = p1->x * (1<<16);
	int32_t dx = slope(p1->x, p1->y, p2->x, p2->y);
	
	if ( y < 0 )
	{
		x += -y * dx;
		y = 0;
	}

	while ( y <= endy )
	{
		uint8_t p = pattern[y%8];
		uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
		int32_t x1 = (y < endy) ? x+dx : p2->x * (1<<16);
		
		if ( dx < 0 )
			drawFragment((uint32_t*)&bitmap[y*rowstride], x1>>16, (x>>16) + thick, color);
		else
			drawFragment((uint32_t*)&bitmap[y*rowstride], x>>16, (x1>>16) + thick, color);
		
		if ( ++y == LCD_ROWS )
			break;

		x = x1;
	}
	
	return (LCDRowRange){ MAX(0, p1->y), MIN(LCD_ROWS, p2->y) };
}

static void fillRange(uint8_t* bitmap, int rowstride, int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2, uint8_t pattern[8])
{
	int32_t x1 = *x1p, x2 = *x2p;
	
	if ( endy < 0 )
	{
		int dy = endy - y;
		*x1p = x1 + dy * dx1;
		*x2p = x2 + dy * dx2;
		return;
	}
	
	if ( y < 0 )
	{
		x1 += -y * dx1;
		x2 += -y * dx2;
		y = 0;
	}
	
	while ( y < endy )
	{
		uint8_t p = pattern[y%8];
		uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
		
		drawFragment((uint32_t*)&bitmap[y*rowstride], (x1>>16), (x2>>16)+1, color);
		
		x1 += dx1;
		x2 += dx2;
		++y;
	}
	
	*x1p = x1;
	*x2p = x2;
}

#if ENABLE_TEXTURES
static inline void sortTri_t(
	Point3D** p1, Point3D** p2, Point3D** p3,
	Point2D* t1, Point2D* t2, Point2D* t3
)
{
	float y1 = (*p1)->y, y2 = (*p2)->y, y3 = (*p3)->y;
	
	if ( y1 <= y2 && y1 < y3 )
	{
		if ( y3 < y2 ) // 1,3,2
		{
			Point3D* tmp = *p2;
			Point2D tmpt = *t2;
			*p2 = *p3; *t2 = *t3;
			*p3 = tmp; *t3 = tmpt;
		}
	}
	else if ( y2 < y1 && y2 < y3 )
	{
		Point3D* tmp = *p1;
		Point2D tmpt = *t1;
		*p1 = *p2; *t1 = *t2;

		if ( y3 < y1 ) // 2,3,1
		{
			*p2 = *p3; *t2 = *t3;
			*p3 = tmp; *t3 = tmpt;
		}
		else // 2,1,3
		{
			*p2 = tmp; *t2 = tmpt;
		}
	}
	else
	{
		Point3D* tmp = *p1;
		Point2D tmpt = *t1;
		*p1 = *p3; *t1 = *t3;
		
		if ( y1 < y2 ) // 3,1,2
		{
			*p3 = *p2; *t3 = *t2;
			*p2 = tmp; *t2 = tmpt;
		}
		else // 3,2,1
		{
			*p3 = tmp; *t3 = tmpt;
		}
	}
}
#endif

static inline void sortTri(
	Point3D** p1, Point3D** p2, Point3D** p3
)
{
	float y1 = (*p1)->y, y2 = (*p2)->y, y3 = (*p3)->y;
	
	if ( y1 <= y2 && y1 < y3 )
	{
		if ( y3 < y2 ) // 1,3,2
		{
			Point3D* tmp = *p2;
			*p2 = *p3;
			*p3 = tmp;
		}
	}
	else if ( y2 < y1 && y2 < y3 )
	{
		Point3D* tmp = *p1;
		*p1 = *p2;

		if ( y3 < y1 ) // 2,3,1
		{
			*p2 = *p3;
			*p3 = tmp;
		}
		else // 2,1,3
			*p2 = tmp;
	}
	else
	{
		Point3D* tmp = *p1;
		*p1 = *p3;
		
		if ( y1 < y2 ) // 3,1,2
		{
			*p3 = *p2;
			*p2 = tmp;
		}
		else // 3,2,1
			*p3 = tmp;
	}
}

LCDRowRange fillTriangle(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t pattern[8])
{
	// sort by y coord
	
	sortTri(&p1, &p2, &p3);
	
	int endy = MIN(LCD_ROWS, p3->y);
	
	if ( p1->y > LCD_ROWS || endy < 0 )
		return (LCDRowRange){ 0, 0 };

	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;
	
	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y);

	int32_t dx1 = MIN(sb, sc);
	int32_t dx2 = MAX(sb, sc);
	
	fillRange(bitmap, rowstride, p1->y, MIN(LCD_ROWS, p2->y), &x1, dx1, &x2, dx2, pattern);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y);
	
	if ( sb < sc )
	{
		x1 = p2->x * (1<<16);
		fillRange(bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2, pattern);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange(bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, pattern);
	}
	
	return (LCDRowRange){ MAX(0, p1->y), endy };
}

#if ENABLE_Z_BUFFER
LCDRowRange fillTriangle_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t pattern[8])
{
	sortTri(&p1, &p2, &p3);
	
	int endy = MIN(LCD_ROWS, p3->y);
	int det = (p3->x - p1->x) * (p2->y - p1->y) - (p2->x - p1->x) * (p3->y - p1->y);
	
	if ( p1->y > LCD_ROWS || endy < 0 || det == 0 )
		return (LCDRowRange){ 0, 0 };

	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;

	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y);

	int32_t dx1 = MIN(sb,sc);
	int32_t dx2 = MAX(sb,sc);
	
	float z1 = zscale / (p1->z + Z_BIAS);
	float z2 = zscale / (p2->z + Z_BIAS);
	float z3 = zscale / (p3->z + Z_BIAS);

	float mx = p1->x + (p2->y-p1->y) * (p3->x-p1->x) / (p3->y-p1->y);
	float mz = z1 + (p2->y-p1->y) * (z3-z1) / (p3->y-p1->y);

	int32_t dzdx, dzdy;

	if ( sc < sb )
	{
		dzdx = slope(mz, mx, z2, p2->x);
		dzdy = slope(z1, p1->y, z3, p3->y);
	}
	else
	{
		dzdx = slope(z2, p2->x, mz, mx);
		dzdy = slope(z1, p1->y, z2, p2->y);
	}
	
	uint32_t z = z1 * (1<<16);

	fillRange_z(bitmap, rowstride, p1->y, MIN(LCD_ROWS, p2->y), &x1, dx1, &x2, dx2, &z, dzdy, dzdx, pattern);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y);

	if ( sb < sc )
	{
		dzdy = slope(z2, p2->y, z3, p3->y);
		x1 = p2->x * (1<<16);
		z = z2 * (1<<16);
		fillRange_z(bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2, &z, dzdy, dzdx, pattern);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange_z(bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, &z, dzdy, dzdx, pattern);
	}
	
	return (LCDRowRange){ MAX(0, p1->y), endy };
}
#endif

#if ENABLE_Z_BUFFER && ENABLE_TEXTURES
LCDRowRange fillTriangle_zt(
	uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3,
	LCDBitmap* texture, Point2D t1, Point2D t2, Point2D t3
	#if ENABLE_TEXTURES_GREYSCALE
	, float lighting, float lighting_weight
	#endif
)
{
	sortTri_t(&p1, &p2, &p3, &t1, &t2, &t3);
	int endy = MIN(LCD_ROWS, p3->y);
	int det = (p3->x - p1->x) * (p2->y - p1->y) - (p2->x - p1->x) * (p3->y - p1->y);
	if ( p1->y > LCD_ROWS || endy < 0 || det == 0 )
		return (LCDRowRange){ 0, 0 };
	
	// scale points to texture size
	int width, height;
	pd->graphics->getBitmapData(texture, &width, &height, NULL, NULL, NULL);
	t1.x *= width; t1.y *= height;
	t2.x *= width; t2.y *= height;
	t3.x *= width; t3.y *= height;
	
	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;

	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y);

	int32_t dx1 = MIN(sb,sc);
	int32_t dx2 = MAX(sb,sc);
	
	float z1 = zscale / (p1->z + Z_BIAS);
	float z2 = zscale / (p2->z + Z_BIAS);
	float z3 = zscale / (p3->z + Z_BIAS);
	
	#if ENABLE_TEXTURES_PROJECTIVE
	float w1;
	float w2;
	float w3;
	#ifndef TEXTURE_PROJECTIVE_RATIO_THRESHOLD
	w1 = 1 / p1->z;
	w2 = 1 / p2->z;
	w3 = 1 / p3->z;
	const int projective_texture_mapping = 1;
	#else
	float zmin = MIN(p1->z, MIN(p2->z, p3->z));
	float zmax = MAX(p1->z, MAX(p2->z, p3->z));
	int projective_texture_mapping;
	if (zmin / zmax < TEXTURE_PROJECTIVE_RATIO_THRESHOLD)
	{
		projective_texture_mapping = 1;
		w1 = 1 / p1->z;
		w2 = 1 / p2->z;
		w3 = 1 / p3->z;
	}
	else
	{
		projective_texture_mapping = 0;
		w1 = 1;
		w2 = 1;
		w3 = 1;
	}
	#endif
	
	#else
	const int projective_texture_mapping = 0;
	const float w1 = 1;
	const float w2 = 1;
	const float w3 = 1;
	#endif
	float u1 = t1.x * w1;
	float v1 = t1.y * w1;
	float u2 = t2.x * w2;
	float v2 = t2.y * w2;
	float u3 = t3.x * w3;
	float v3 = t3.y * w3;
	
	float mx = p1->x + (p2->y-p1->y) * (p3->x-p1->x) / (p3->y-p1->y);
	float mz = z1 + (p2->y-p1->y) * (z3-z1) / (p3->y-p1->y);
	float mu = u1 + (p2->y-p1->y) * (u3-u1) / (p3->y-p1->y);
	float mv = v1 + (p2->y-p1->y) * (v3-v1) / (p3->y-p1->y);
	#if ENABLE_TEXTURES_PROJECTIVE
	float mw;
	if (projective_texture_mapping)
	{
		mw = w1 + (p2->y-p1->y) * (w3-w1) / (p3->y-p1->y);
	}
	uvw_int2_t dwdx, dwdy;
	#endif

	int32_t dzdx, dzdy;
	uvw_int2_t dudx, dudy, dvdx, dvdy;

	if ( sc < sb )
	{
		dzdx = slope(mz, mx, z2, p2->x);
		dzdy = slope(z1, p1->y, z3, p3->y);
		dudx = UVW_SLOPE(mu, mx, u2, p2->x);
		dudy = UVW_SLOPE(u1, p1->y, u3, p3->y);
		dvdx = UVW_SLOPE(mv, mx, v2, p2->x);
		dvdy = UVW_SLOPE(v1, p1->y, v3, p3->y);
		#if ENABLE_TEXTURES_PROJECTIVE
		if (projective_texture_mapping)
		{
			dwdx = UVW_SLOPE(mw, mx, w2, p2->x);
			dwdy = UVW_SLOPE(w1, p1->y, w3, p3->y);
		}
		#endif
	}
	else
	{
		dzdx = slope(z2, p2->x, mz, mx);
		dzdy = slope(z1, p1->y, z2, p2->y);
		dudx = UVW_SLOPE(u2, p2->x, mu, mx);
		dudy = UVW_SLOPE(u1, p1->y, u2, p2->y);
		dvdx = UVW_SLOPE(v2, p2->x, mv, mx);
		dvdy = UVW_SLOPE(v1, p1->y, v2, p2->y);
		#if ENABLE_TEXTURES_PROJECTIVE
		if (projective_texture_mapping)
		{
			dwdx = UVW_SLOPE(w2, p2->x, mw, mx);
			dwdy = UVW_SLOPE(w1, p1->y, w2, p2->y);
		}
		#endif
	}
	
	uint32_t z = z1 * (1<<16);
	uvw_int2_t u = u1 * ((uvw_int2_t)(1)<<UVW_SHIFT);
	uvw_int2_t v = v1 * ((uvw_int2_t)(1)<<UVW_SHIFT);
	#if ENABLE_TEXTURES_PROJECTIVE
	uvw_int2_t w = w1 * ((uvw_int2_t)(1)<<UVW_SHIFT);
	#endif
	
	#if ENABLE_TEXTURES_GREYSCALE
		// map lighting to range 0-255
		uint8_t u8lightp = CLAMP(0.0f, 1.0f, lighting_weight) * 255.99;
		uint8_t u8light = CLAMP(0.0f, 1.0f, lighting) * (LIGHTING_PATTERN_COUNT - 0.001f);
		// precompute
		u8light = (((uint16_t)u8light * u8lightp) + 0x80) >> 8;
		
		#define fillRange_zt_or_ztg(fname, fname_g, ...) \
			if (u8lightp == 0) \
			{ \
				fname(__VA_ARGS__); \
			} \
			else \
			{ \
				fname_g(__VA_ARGS__, u8light, 0xff - u8lightp); \
			}
	#else
		#define fillRange_zt_or_ztg(fname, fname_g, ...) fname(__VA_ARGS__)
	#endif
	
	#if ENABLE_TEXTURES_PROJECTIVE
		#define fillRange_zt_or_ztp(...) \
		if (projective_texture_mapping) \
		{ \
			fillRange_zt_or_ztg(fillRange_ztp, fillRange_ztgp, __VA_ARGS__, &w, dwdy, dwdx); \
		} \
		else \
		{ \
			fillRange_zt_or_ztg(fillRange_zt, fillRange_ztg, __VA_ARGS__); \
		}
	#else
		#define fillRange_zt_or_ztp fillRange_ztg
	#endif


	fillRange_zt_or_ztp(
		bitmap, rowstride, p1->y, MIN(LCD_ROWS, p2->y), &x1, dx1, &x2, dx2, &z, dzdy, dzdx, &u, dudy, dudx, &v, dvdy, dvdx, texture
	);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y);

	if ( sb < sc )
	{
		dzdy = slope(z2, p2->y, z3, p3->y);
		dudy = UVW_SLOPE(u2, p2->y, u3, p3->y);
		dvdy = UVW_SLOPE(v2, p2->y, v3, p3->y);
		x1 = p2->x * (1<<16);
		z = z2 * (1<<16);
		u = u2 * ((uvw_int2_t)(1)<<UVW_SHIFT);
		v = v2 * ((uvw_int2_t)(1)<<UVW_SHIFT);
		#if ENABLE_TEXTURES_PROJECTIVE
		if (projective_texture_mapping)
		{
			dwdy = UVW_SLOPE(w2, p2->y, w3, p3->y);
			w = w2 * ((uvw_int2_t)(1)<<UVW_SHIFT);
		}
		#endif
		fillRange_zt_or_ztp(
			bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2, &z, dzdy, dzdx, &u, dudy, dudx, &v, dvdy, dvdx, texture
		);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange_zt_or_ztp(
			bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, &z, dzdy, dzdx, &u, dudy, dudx, &v, dvdy, dvdx, texture
		);
	}
		
	return (LCDRowRange){ MAX(0, p1->y), endy };
}
#endif

LCDRowRange fillQuad(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4, uint8_t pattern[8])
{
	// XXX - implement with 3 fillrange_z() calls
	fillTriangle(bitmap, rowstride, p1, p2, p3, pattern);
	return fillTriangle(bitmap, rowstride, p1, p3, p4, pattern);
}

#if ENABLE_Z_BUFFER
LCDRowRange fillQuad_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4, uint8_t pattern[8])
{
	// XXX - implement with 3 fillrange_z() calls
	fillTriangle_zbuf(bitmap, rowstride, p1, p2, p3, pattern);
	return fillTriangle_zbuf(bitmap, rowstride, p1, p3, p4, pattern);
}
#endif

#if ENABLE_Z_BUFFER && ENABLE_TEXTURES
LCDRowRange fillQuad_zt(
	uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4,
	LCDBitmap* texture, Point2D t1, Point2D t2, Point2D t3, Point2D t4
	#if ENABLE_TEXTURES_GREYSCALE
	, float lighting, float lighting_weight
	#endif
)
{
	// XXX - implement with 3 fillrange_z() calls
	fillTriangle_zt(
		bitmap, rowstride, p1, p2, p3, texture, t1, t2, t3
		#if ENABLE_TEXTURES_GREYSCALE
		, lighting, lighting_weight
		#endif
	);
	return fillTriangle_zt(
		bitmap, rowstride, p1, p3, p4, texture, t1, t3, t4
		#if ENABLE_TEXTURES_GREYSCALE
		, lighting, lighting_weight
		#endif
	);
}
#endif

#if ENABLE_Z_BUFFER
#include <stdio.h>
#include <stdlib.h>

void render_zbuff(uint8_t* out, int rowstride)
{
	for (uint16_t x = 0; x < LCD_COLUMNS; ++x)
	{
		for (uint16_t y = 0; y < LCD_ROWS; ++y)
		{
			uint16_t pixi = (y * rowstride) + (x/8);
			uint8_t mask = 0x80 >> (x % 8);
			out[pixi] &= ~mask;
			if ((rand() & 0xffff) < zbuf[y * LCD_COLUMNS + x])
			{
				out[pixi] |= mask;
			}
		}
	}
}
#endif
