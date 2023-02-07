//
//  render.c
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#if defined(__ARMCOMPILER_VERSION)
#pragma OMax
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize ("O3")
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize ("-fsingle-precision-constant")
#pragma GCC optimize ("-ffast-math")
//#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <stdint.h>
#include <string.h>
#include "render.h"

// indicates the type has been optimized
// to be a full word because it's faster.
#define OPTU32(x) x

typedef int16_t uvw_int_t;
typedef int32_t uvw_int2_t;
#define UV_SHIFT 21
#define W_SHIFT 28
#define UV_SLOPE slope
#define W_SLOPE slope

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

#if ENABLE_RENDER_DISTANCE_MAX
float render_distance_max = 100000000000000.0f;
void setRenderDistanceMax(float f)
{
	render_distance_max = f;
}
float getRenderDistanceMax()
{
	return render_distance_max;
}
static inline int
render_distance_bounds(Point3D* p1, Point3D* p2, Point3D* p3)
{
	return p1->z > render_distance_max && p2->z > render_distance_max && p3->z > render_distance_max;
}
#endif

#if ENABLE_TEXTURES && TEXTURE_PERSPECTIVE_MAPPING && PRECOMPUTE_PROJECTION


// decreasing this value will cause ripples in projected textures
// increasing will double the size of the lookup table
#define PROJECTION_FIDELITY 11

// decreasing this value too much will cause ripples
// increasing too much may cause overflow errors
#define PROJECTION_FIDELITY_B 18


#define DIVISION_TABLE_C (2 << PROJECTION_FIDELITY)
static int projectionTablePrecomputed = 0;
static uint16_t projection_table[DIVISION_TABLE_C];

#if defined(__GNUC__) || defined(__clang__)
void __attribute__((constructor)) precomputeProjectionTable(void);
__attribute__((constructor))
#endif
void precomputeProjectionTable(void)
{
	if (projectionTablePrecomputed != 0) return;
	projectionTablePrecomputed = 1;
	for (size_t i = 0; i < DIVISION_TABLE_C; ++i)
	{
		projection_table[i] = (1 << PROJECTION_FIDELITY_B) / MAX(i, 1);
	}
	projection_table[0] *= 2;
}

static inline uint32_t getProjectionMult(uint32_t divisor)
{
	divisor >>= (W_SHIFT - PROJECTION_FIDELITY);
	divisor = MIN(divisor, DIVISION_TABLE_C-1);
	return projection_table[divisor];
}

#endif

#if ENABLE_INTERLACE
// if bit 1 is set, then interface is DISABLED
// bit 0 controls the line to render (even / odd)
static int interlace_frame = 64;
void setInterlace(int i)
{
	#if INTERLACE_INTERVAL <= 2
		int set = !i;
		interlace_frame = (interlace_frame & ~1) | set;
	#else
		interlace_frame = (interlace_frame & ~63) | (i & 63);
	#endif
}

int getInterlace(void)
{
	#if INTERLACE_INTERVAL <= 2
	return !(interlace_frame & 1);
	#else
	return interlace_frame & 63;
	#endif
}

void setInterlaceEnabled(int i)
{
	int set = (!i) << 6;
	interlace_frame = (interlace_frame & ~64) | (set);
}

int getInterlaceEnabled(void)
{
	return !(interlace_frame & 64);
}

static inline int
interlacePermitsRow(int y)
{
	#if INTERLACE_INTERVAL <= 2
	return ((y >> INTERLACE_ROW_LGC) % 2 != interlace_frame);
	#else
	return !getInterlaceEnabled() || ((y >> INTERLACE_ROW_LGC) % INTERLACE_INTERVAL == (interlace_frame & 63));
	#endif
}
#else
#define interlacePermitsRow(x) 1
#endif

#if ENABLE_POLYGON_SCANLINING
static inline int
scanlinePermitsRow(int y, ScanlineFill* scanline)
{
	return (y % 2) != (scanline->select);
}
#endif

#ifdef ZBUF32
	// this doesn't seem to work at all.
	// not sure why.
	typedef uint32_t zbuf_t;
	#define ZSCALE_MULT 0xffffffff
	#define ZSHIFT 0
#elif defined(ZBUF16)
	typedef uint16_t zbuf_t;
	#define ZSCALE_MULT 0xffff
	#define ZSHIFT 16
#else
	typedef uint8_t zbuf_t;
	#define ZSCALE_MULT 0xff
	#define ZSHIFT 16
#endif

#define ZCOORD_INT

#ifdef ZCOORD_INT
typedef uint32_t zcoord_t;
#define slopez(a, b, c, d, e) slope(a, b, c, d, e)
#else
// although it would seem integers are faster, we actually can exploit that
// there are additional registers in the FPU which we wouldn't otherwise be using.
typedef float zcoord_t;
#define slopez(a, b, c, d, X) slopef(a, b, c, d)
#undef ZSHIFT
#define ZSHIFT 0
#endif

static float zscale;
#define Z_BIAS 0

#if ENABLE_Z_BUFFER

#if Z_BUFFER_FRAME_PARITY
	static int zbuff_parity = 0;
	#define ZBUFF_PARITY_MULT 2
#else
	#define ZBUFF_PARITY_MULT 1
#endif

static zbuf_t zbuf[VIEWPORT_WIDTH*VIEWPORT_HEIGHT*ZBUFF_PARITY_MULT];

#define ZBUF_IDX(x, y) (&zbuf[0] + ((((y) - VIEWPORT_TOP) * VIEWPORT_WIDTH + (x) - VIEWPORT_LEFT) * ZBUFF_PARITY_MULT))

void prefetch_zbuf(void)
{
	#if defined(__GNUC__) || defined(__clang__)
	__builtin_prefetch(&zbuf[0]);
	#endif
}

void resetZBuffer(void)
{
	#if Z_BUFFER_FRAME_PARITY == 0
	memset(zbuf, 0, sizeof(zbuf));
	#else
	zbuff_parity = !zbuff_parity;
	#endif
}
#endif

void resetZScale(float zmin)
{
	zscale = ZSCALE_MULT * (zmin + Z_BIAS);
}

#if ENABLE_DISTANCE_FOG
static uint8_t render_fog_color = 0;

#define FOG_SCALE 0x1000000

// projective variations
static uint32_t render_fog_endz_p = 0;
static uint32_t render_fog_startz_p = 0;
static uint32_t render_fog_slope_p = 0;

// z: 0 is at camera, 1 is far away.
void fog_set(uint8_t color, float startz, float endz)
{
	render_fog_color = color;
	render_fog_startz_p = FOG_SCALE * (zscale / startz);
	render_fog_endz_p = FOG_SCALE * (zscale / endz);
	if (render_fog_endz_p < render_fog_startz_p)
	{
		render_fog_slope_p = (float)FOG_SCALE / (render_fog_startz_p - render_fog_endz_p);
	}
	pd->system->logToConsole("%f, %f | %x, %x : %x",
		startz, endz,
		render_fog_startz_p, render_fog_endz_p, render_fog_slope_p
	);
}

// z ranges from 0 to 0x10000, where 0 means far.
// return value from 0 to 0x10000, where 0 means FULL fog.
FORCEINLINE uint32_t
fog_transform_projective(uint32_t z)
{
	if (z >= render_fog_startz_p) return FOG_SCALE;
	if (z <= render_fog_endz_p) return 0x0;
	return (z - render_fog_endz_p) * render_fog_slope_p;
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
	*p = (*p & ~mask) | (color & mask);
}

static void
drawFragment(uint32_t* row, int x1, int x2, uint32_t color)
{
	if ( x2 < VIEWPORT_LEFT || x1 >= VIEWPORT_RIGHT )
		return;
	
	if ( x1 < VIEWPORT_LEFT )
		x1 = VIEWPORT_LEFT;
	
	if ( x2 > VIEWPORT_RIGHT )
		x2 = VIEWPORT_RIGHT;
	
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

#define RZ 1
#define RT 2
#define RA 4
#define RG 8
#define RP 16
#define RL 32

#if ENABLE_Z_BUFFER
	#define RENDER RZ
	#include "render_range.inc"
#endif

#if TEXTURE_PERSPECTIVE_MAPPING
	#if ENABLE_TEXTURES && ENABLE_TEXTURES_GREYSCALE
			#if ENABLE_TEXTURES_MASK
				#define RENDER RT | RA | RG | RL | RP
				#include "render_range.inc"
				
				#define RENDER RT | RA | RG | RP
				#include "render_range.inc"
			#endif
			
			#define RENDER RT | RG | RL | RP
			#include "render_range.inc"
			
			#define RENDER RT | RG | RP
			#include "render_range.inc"
		#endif

	#if ENABLE_TEXTURES && ENABLE_TEXTURES_MASK
		#define RENDER RT | RA | RP
		#include "render_range.inc"
	#endif

	#if ENABLE_TEXTURES
		#define RENDER RT | RP
		#include "render_range.inc"
	#endif

	#if ENABLE_Z_BUFFER
		#if ENABLE_TEXTURES && ENABLE_TEXTURES_GREYSCALE
			#if ENABLE_TEXTURES_MASK
				#define RENDER RZ | RT | RA | RG | RL | RP
				#include "render_range.inc"
				
				#define RENDER RZ | RT | RA | RG | RP
				#include "render_range.inc"
			#endif
			
			#define RENDER RZ | RT | RG | RL | RP
			#include "render_range.inc"
			
			#define RENDER RZ | RT | RG | RP
			#include "render_range.inc"
		#endif
		
		#if ENABLE_TEXTURES && ENABLE_TEXTURES_MASK
			#define RENDER RZ | RT | RA | RP
			#include "render_range.inc"
		#endif

		#if ENABLE_TEXTURES
			#define RENDER RZ | RT | RP
			#include "render_range.inc"
		#endif
	#endif
#endif

#if ENABLE_TEXTURES && ENABLE_TEXTURES_GREYSCALE
	#if ENABLE_TEXTURES_MASK
		#define RENDER RT | RA | RG | RL
		#include "render_range.inc"
		
		#define RENDER RT | RA | RG
		#include "render_range.inc"
	#endif
	
	#define RENDER RT | RG | RL
	#include "render_range.inc"
	
	#define RENDER RT | RG
	#include "render_range.inc"
#endif

#if ENABLE_TEXTURES && ENABLE_TEXTURES_MASK
	#define RENDER RT | RA
	#include "render_range.inc"
#endif

#if ENABLE_TEXTURES
	#define RENDER RT
	#include "render_range.inc"
#endif

#if ENABLE_Z_BUFFER
	#if ENABLE_TEXTURES && ENABLE_TEXTURES_GREYSCALE
		#if ENABLE_TEXTURES_MASK
			#define RENDER RZ | RT | RA | RG | RL
			#include "render_range.inc"
			
			#define RENDER RZ | RT | RA | RG
			#include "render_range.inc"
		#endif
		
		#define RENDER RZ | RT | RG | RL
		#include "render_range.inc"
		
		#define RENDER RZ | RT | RG
		#include "render_range.inc"
	#endif
	
	#if ENABLE_TEXTURES && ENABLE_TEXTURES_MASK
		#define RENDER RZ | RT | RA
		#include "render_range.inc"
	#endif

	#if ENABLE_TEXTURES
		#define RENDER RZ | RT
		#include "render_range.inc"
	#endif
#endif

static inline int32_t slope(float x1, float y1, float x2, float y2, const int shift)
{
	float dx = x2-x1;
	float dy = y2-y1;
	
	if ( dy < 1 )
		return dx * (1<<shift);
	else
		return dx / dy * (1<<shift);
}

static inline float slopef(float x1, float y1, float x2, float y2)
{
	float dx = x2 - x1;
	float dy = y2 - y1;
	if (dy == 0)
	{
		return dx * 1000.0f;
	}
	else
	{
		return dx / dy;
	}
}

static inline int64_t slope64(float x1, float y1, float x2, float y2, const int shift)
{
	float dx = x2-x1;
	float dy = y2-y1;
	
	if ( dy < 1 )
		return dx * ((int64_t)(1)<<shift);
	else
		return dx / dy * ((int64_t)(1)<<shift);
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
	
	if ( y >= VIEWPORT_HEIGHT || y < VIEWPORT_TOP || MIN(p1->x, p2->x) >= VIEWPORT_RIGHT || MAX(p1->x, p2->x) < VIEWPORT_LEFT )
		return (LCDRowRange){ 0, 0 };
	
	int32_t x = p1->x * (1<<16);
	int32_t dx = slope(p1->x, p1->y, p2->x, p2->y + 1, 16);
	
	// move lines a bit forward so they don't get buried in solid geometry
	float z1 = zscale / (p1->z + Z_BIAS) + 256;
	float z2 = zscale / (p2->z + Z_BIAS) + 256;

	if ( z1 > zscale ) z1 = zscale;
	if ( z2 > zscale ) z2 = zscale;
	zcoord_t z = z1 * (1<<ZSHIFT);

	zcoord_t dzdy = slopez(z1, p1->y, z2, p2->y + 1, ZSHIFT);
	zcoord_t dzdx = slopez(z1, p1->x, z2, p2->x, ZSHIFT);

	if ( y < VIEWPORT_TOP )
	{
		x += (VIEWPORT_TOP-y) * dx;
		z += (VIEWPORT_TOP-y) * dzdy;
		y = VIEWPORT_TOP;
	}
	
	while ( y <= endy )
	{
		uint8_t p = pattern[y%8];
		uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
		int32_t x1 = (y < endy) ? x+dx : p2->x * (1<<16);

		if ( dx < 0 )
		{
			z += dzdy;
			drawFragment_z((uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), x1>>16, (x>>16) + thick, z, dzdx, color);
		}
		else
		{
			drawFragment_z((uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), x>>16, (x1>>16) + thick, z, dzdx, color);
			z += dzdy;
		}

		if ( ++y == VIEWPORT_BOTTOM )
			break;
		
		x = x1;
	}
	
	return (LCDRowRange){ MAX(VIEWPORT_TOP, p1->y), MIN(VIEWPORT_BOTTOM, p2->y) };
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
	
	if ( y >= VIEWPORT_BOTTOM || endy < VIEWPORT_TOP || MIN(p1->x, p2->x) >= VIEWPORT_RIGHT || MAX(p1->x, p2->x) < VIEWPORT_LEFT )
		return (LCDRowRange){ 0, 0 };
	
	int32_t x = p1->x * (1<<16);
	int32_t dx = slope(p1->x, p1->y, p2->x, p2->y, 16);
	
	if ( y < VIEWPORT_TOP )
	{
		x += (VIEWPORT_TOP-y) * dx;
		y = VIEWPORT_TOP;
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
		
		if ( ++y == VIEWPORT_BOTTOM )
			break;

		x = x1;
	}
	
	return (LCDRowRange){ MAX(VIEWPORT_TOP, p1->y), MIN(VIEWPORT_BOTTOM, p2->y) };
}

static void fillRange(uint8_t* bitmap, int rowstride, int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2, uint8_t pattern[8])
{
	int32_t x1 = *x1p, x2 = *x2p;
	
	if ( endy < VIEWPORT_TOP )
	// early-out
	{
		int dy = endy - y;
		*x1p = x1 + dy * dx1;
		*x2p = x2 + dy * dx2;
		return;
	}
	
	if ( y < VIEWPORT_TOP )
	{
		x1 += (VIEWPORT_TOP-y) * dx1;
		x2 += (VIEWPORT_TOP-y) * dx2;
		y = VIEWPORT_TOP;
	}
	
	while ( y < endy )
	{
		if (interlacePermitsRow(y) || ENABLE_INTERLACE == 2)
		{
			uint8_t p = pattern[y%8];
			uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
			
			drawFragment((uint32_t*)&bitmap[y*rowstride], (x1>>16), (x2>>16)+1, color);
			
			x1 += dx1;
			x2 += dx2;
			++y;
		}
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
	
	#if ENABLE_RENDER_DISTANCE_MAX == 1
	if (render_distance_bounds(p1, p2, p3)) return (LCDRowRange){ 0, 0 };;
	#endif
	
	int endy = MIN(VIEWPORT_BOTTOM, p3->y);
	
	if ( p1->y > VIEWPORT_BOTTOM || endy < VIEWPORT_TOP )
		return (LCDRowRange){ 0, 0 };

	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;
	
	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y, 16);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y, 16);

	int32_t dx1 = MIN(sb, sc);
	int32_t dx2 = MAX(sb, sc);
	
	fillRange(bitmap, rowstride, p1->y, MIN(VIEWPORT_BOTTOM, p2->y), &x1, dx1, &x2, dx2, pattern);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y, 16);
	
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
	
	return (LCDRowRange){ MAX(VIEWPORT_TOP, p1->y), endy };
}

#if ENABLE_Z_BUFFER
LCDRowRange fillTriangle_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t pattern[8])
{
	sortTri(&p1, &p2, &p3);
	
	#if ENABLE_RENDER_DISTANCE_MAX == 1
	if (render_distance_bounds(p1, p2, p3)) return (LCDRowRange){ 0, 0 };
	#endif
	
	int endy = MIN(VIEWPORT_BOTTOM, p3->y);
	int det = (p3->x - p1->x) * (p2->y - p1->y) - (p2->x - p1->x) * (p3->y - p1->y);
	
	if ( p1->y > VIEWPORT_BOTTOM || endy < VIEWPORT_TOP || det == 0 )
		return (LCDRowRange){ 0, 0 };

	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;

	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y, 16);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y, 16);

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
		dzdx = slope(mz, mx, z2, p2->x, 16);
		dzdy = slope(z1, p1->y, z3, p3->y, 16);
	}
	else
	{
		dzdx = slope(z2, p2->x, mz, mx, 16);
		dzdy = slope(z1, p1->y, z2, p2->y, 16);
	}
	
	#ifdef ZCOORD_INT
	zcoord_t z = z1 * (1 << ZSHIFT);
	#else
	zcoord_t z = z1;
	#endif

	fillRange_z(bitmap, rowstride, p1->y, MIN(VIEWPORT_BOTTOM, p2->y), &x1, dx1, &x2, dx2, &z, dzdy, dzdx, pattern);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y, 16);

	if ( sb < sc )
	{
		dzdy = slope(z2, p2->y, z3, p3->y, 16);
		x1 = p2->x * (1<<16);
		z = z2 * (1<<ZSHIFT);
		fillRange_z(bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2, &z, dzdy, dzdx, pattern);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange_z(bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, &z, dzdy, dzdx, pattern);
	}
	
	return (LCDRowRange){ MAX(VIEWPORT_TOP, p1->y), endy };
}
#endif

#if TEXTURE_PERSPECTIVE_MAPPING

#if TEXTURE_PERSPECTIVE_MAPPING_SPLIT
// ranges from 0-1
typedef float projective_ratio_test_result_t;
#else
// boolean
typedef int projective_ratio_test_result_t;
#endif

static inline projective_ratio_test_result_t
projective_ratio_test(Point3D* p1, Point3D* p2, Point3D* p3)
{
	float zmin = MIN(p1->z, MIN(p2->z, p3->z));
	float zmax = MAX(p1->z, MAX(p2->z, p3->z));
	
	#if TEXTURE_PERSPECTIVE_MAPPING == 1
		return 1; // always.
	#elif TEXTURE_PERSPECTIVE_MAPPING == 2
		return zmax * TEXTURE_PROJECTIVE_RATIO_THRESHOLD > zmin;
	#elif TEXTURE_PERSPECTIVE_MAPPING == 3
		// twice the area of the triangle
		// https://keisan.casio.com/exec/system/1223520411
		// OPTIMIZE: try using integers instead
		float area2 = (p1->x * p2->y + p2->x * p3->y + p3->x * p1->y
					-p1->y * p2->x - p2->y * p3->x - p3->y * p1->x);
		area2 = fabsf(area2);
		
		return (area2 * zmax > zmin * TEXTURE_PROJECTIVE_AREA_FACTOR);
	#elif TEXTURE_PERSPECTIVE_MAPPING == 4
	
		// manhattan-circumference of triangle
		float circumference =
			fabsf(p1->x - p2->x) + fabsf(p3->x - p2->x) + fabsf(p3->x - p1->x)
			+ fabsf(p1->y - p2->y) + fabsf(p3->y - p2->y) + fabsf(p3->y - p1->y);
		
		#if TEXTURE_PERSPECTIVE_MAPPING_SPLIT
			return ((circumference / TEXTURE_PROJECTIVE_LENGTH_FACTOR) * zmax - zmin) / (1 << 6);
		#else
			return (circumference * zmax > zmin * TEXTURE_PROJECTIVE_LENGTH_FACTOR);
		#endif
	#else
	// safety
	return 0;
	#endif
}

#if TEXTURE_PERSPECTIVE_MAPPING_SPLIT
// affine triangle
static Point3D p1a, p2a, p3a;
static Point2D t1a, t2a, t3a;

// perspective-correct triangle
static Point3D p1b, p2b, p3b;
static Point2D t1b, t2b, t3b;

// precondition: p2's y is middle.
static inline int splitTriangleProjective(
	float f,
	Point3D* p1, Point3D* p2, Point3D* p3,
	Point2D* t1, Point2D* t2, Point2D* t3
)
{
	// p2 is middle y.
	// swap p1/p3 so that p1's z > p3's z.
	if (p1->z < p3->z)
	{
		Point3D* tmp = p1;
		p1 = p3;
		p3 = tmp;
		
		Point2D* tmpt = t1;
		t1 = t3;
		t3 = tmpt;
	}
	
	// find point between p1, p3
	Point3D p4 = {
		p1->x * f + p3->x * (1 - f),
		p1->y * f + p3->y * (1 - f),
		p1->z * f + p3->z * (1 - f),
	};
	
	Point2D t4 = {
		t1->x * f + t3->x * (1 - f),
		t1->y * f + t3->y * (1 - f),
	};
	
	// output: 
	// affine (z far)
	p1a = *p1; p2a = *p2; p3a = p4;
	t1a = *t1; t2a = *t2; t3a = t4;
	
	// perpsective-correct (z near)
	
	p1b = p4; p2b = *p2; p3b = *p3;
	t1b = t4; t2b = *t2; t3b = *t3;
	
	return 1;
}	
#endif

#endif

#if ENABLE_TEXTURES
#include "render_triangle.inc"
#endif

#if ENABLE_Z_BUFFER && ENABLE_TEXTURES
	#define RENDER_Z
	#include "render_triangle.inc"
	#undef RENDER_Z
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
	#if TEXTURE_PERSPECTIVE_MAPPING
	, int projective
	#endif
)
{
	// XXX - implement with 3 fillrange_z() calls
	fillTriangle_zt(
		bitmap, rowstride, p1, p2, p3, texture, t1, t2, t3
		#if ENABLE_CUSTOM_PATTERNS
		, pattern
		#endif
		#if ENABLE_POLYGON_SCANLINING
		, scanline
		#endif
		#if ENABLE_TEXTURES_GREYSCALE
		, lighting, lighting_weight
		#endif
		#if TEXTURE_PERSPECTIVE_MAPPING
		, projective
		#endif
	);
	return fillTriangle_zt(
		bitmap, rowstride, p1, p3, p4, texture, t1, t3, t4
		#if ENABLE_CUSTOM_PATTERNS
		, pattern
		#endif
		#if ENABLE_POLYGON_SCANLINING
		, scanline
		#endif
		#if ENABLE_TEXTURES_GREYSCALE
		, lighting, lighting_weight
		#endif
		#if TEXTURE_PERSPECTIVE_MAPPING
		, projective
		#endif
	);
}
#endif

#if ENABLE_TEXTURES
LCDRowRange fillQuad_t(
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
	#if TEXTURE_PERSPECTIVE_MAPPING
	, int projective
	#endif
)
{
	// XXX - implement with 3 fillrange() calls
	fillTriangle_t(
		bitmap, rowstride, p1, p2, p3, texture, t1, t2, t3
		#if ENABLE_CUSTOM_PATTERNS
		, pattern
		#endif
		#if ENABLE_POLYGON_SCANLINING
		, scanline
		#endif
		#if ENABLE_TEXTURES_GREYSCALE
		, lighting, lighting_weight
		#endif
		#if TEXTURE_PERSPECTIVE_MAPPING
		, projective
		#endif
	);
	return fillTriangle_t(
		bitmap, rowstride, p1, p3, p4, texture, t1, t3, t4
		#if ENABLE_CUSTOM_PATTERNS
		, pattern
		#endif
		#if ENABLE_POLYGON_SCANLINING
		, scanline
		#endif
		#if ENABLE_TEXTURES_GREYSCALE
		, lighting, lighting_weight
		#endif
		#if TEXTURE_PERSPECTIVE_MAPPING
		, projective
		#endif
	);
}
#endif

#if ENABLE_Z_BUFFER
#include <stdlib.h>

void render_zbuff(uint8_t* out, int rowstride)
{
	for (uint16_t x = VIEWPORT_LEFT; x < VIEWPORT_RIGHT; ++x)
	{
		for (uint16_t y = VIEWPORT_TOP; y < VIEWPORT_BOTTOM; ++y)
		{
			uint16_t pixi = (y * rowstride) + (x/8);
			uint8_t mask = 0x80 >> (x % 8);
			out[pixi] &= ~mask;
			if ((rand() & ZSCALE_MULT) < *ZBUF_IDX(x, y))
			{
				out[pixi] |= mask;
			}
		}
	}
}
#endif