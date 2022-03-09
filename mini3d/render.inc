// Z: zbuffer
// T: texture
// A: semitransparent textures

#define _GLUE(a, b) a##b
#define GLUE(a, b) _GLUE(a, b)

#ifdef RENDER_Z
    #define SYM_Z _z
#else
    #define SYM_Z _
#endif

#ifdef RENDER_T
    #define SYM_ZT GLUE(SYM_Z, t)
#else
    #define SYM_ZT SYM_Z
#endif

#ifdef RENDER_A
    #define SYM_ZTA GLUE(SYM_ZT, a)
#else
    #define SYM_ZTA SYM_ZT
#endif

#define SYM(x) GLUE(x, SYM_ZTA)

static void
SYM(drawFragment)(
	uint32_t* row, uint16_t* zbrow, int x, int endx
#ifdef RENDER_Z
	, uint32_t z, int32_t dzdx
#endif
#ifdef RENDER_T
	, uvw_int2_t u, uvw_int2_t dudx,
	uvw_int2_t v, uvw_int2_t dvdx,
	uint8_t* texdata, uint16_t texrowbytes, uint16_t texw, uint16_t texh
#if ENABLE_TEXTURES_PROJECTIVE
    , uvw_int2_t w, uvw_int2_t dwdx
#endif
#else
    , uint32_t color
#endif
)
{
	if ( endx < 0 || x >= LCD_COLUMNS )
		return;
        
    if ( endx > LCD_COLUMNS )
		endx = LCD_COLUMNS;
	
	if ( x < 0 )
	{
        #ifdef RENDER_Z
		z += -x * dzdx;
        #endif
        
        #ifdef RENDER_T
            u += -x * dudx;
            v += -x * dvdx;
            #if ENABLE_TEXTURES_PROJECTIVE
                w += -x * dwdx;
            #endif
        #endif
        
		x = 0;
	}

	uint32_t mask = 0;
	uint32_t* p = row + x/32;
    #ifdef RENDER_T
	uint32_t color = 0;
    #endif
    #ifdef RENDER_A
    uint8_t* maskdata = texdata + (texh * texrowbytes);
    #endif

	while ( x < endx )
	{
		// read texture
        #ifdef RENDER_T
        #if ENABLE_TEXTURES_PROJECTIVE
        // |1 to prevent floating point division error
        uint16_t ui = (u / (w|1)) % texw;
		uint16_t vi = (v / (w|1)) % texh;
        #else
		uint16_t ui = (u >> UVW_SHIFT) % texw;
		uint16_t vi = (v >> UVW_SHIFT) % texh;
        #endif
		uint16_t ti = (vi * texrowbytes) + ui / 8;
		uint32_t texpix = (texdata[ti] << (ui % 8)) & 0x80; // either 0x80 or 0.
        #endif
        
        #ifdef RENDER_T
        color |= (texpix << 24) >> (x%32);
        #endif
        
        #ifdef RENDER_A
        uint32_t alpha = (maskdata[ti] << (ui % 8)) & 0x80; // either 0x80 or 0.
        #else
        const uint8_t alpha = 1;
        #endif
        
        #ifdef RENDER_Z
        uint16_t zi = z >> 16;
        if (zi > zbrow[x] && alpha)
        {
            mask |= 0x80000000u >> (x%32);
            zbrow[x] = zi;
        }
        #elif defined(RENDER_T)
        mask |= (alpha << 24) >> (x%32);
        #else
            #pragma error "render.inc requires at least one of RENDER_Z and RENDER_T defined."
        #endif
		
		++x;
        
        #ifdef RENDER_Z
		z += dzdx;
        #endif
        
        #ifdef RENDER_T
            u += dudx;
            v += dvdx;
            #if ENABLE_TEXTURES_PROJECTIVE
                w += dwdx;
            #endif
        #endif
		
		// TODO: consider using Duff's Device to avoid this branch?
		if ( x % 32 == 0 )
		{
            #ifdef RENDER_T
            // no need to swap if this is not a texture,
            // because we assume that all bytes are identical (8-periodic)
            // if not a texture.
            color = swap(color);
            #endif
            
			_drawMaskPattern(p++, swap(mask), color);
			mask = 0;
            
            #ifdef RENDER_T
			color = 0;
            #endif
		}
	}
	
    // If x % 32 == 0, then this does nothing anyway.
    // It may be preferable not to branch at all.
    // #prematureoptimization
    #if 0
	if ( x % 32 != 0 )
    #endif
    {
        #ifdef RENDER_T
        // see above
        color = swap(color);
        #endif
		_drawMaskPattern(p, swap(mask), color);
    }
}

#ifndef RENDER_A
static void 
SYM(fillRange)(
	uint8_t* bitmap, int rowstride, int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2
#ifdef RENDER_Z
	, uint32_t* zp, int32_t dzdy, int32_t dzdx
#endif
#ifdef RENDER_T
	, uvw_int2_t* up, uvw_int2_t dudy, uvw_int2_t dudx,
	uvw_int2_t* vp, uvw_int2_t dvdy, uvw_int2_t dvdx,
	LCDBitmap* texture
    #if ENABLE_TEXTURES_PROJECTIVE
    , uvw_int2_t* wp, uvw_int2_t dwdy, uvw_int2_t dwdx
    #endif
#else
    , uint8_t pattern[8]
#endif
)
{
	int32_t x1 = *x1p, x2 = *x2p;
    #ifdef RENDER_Z
	    uint32_t z = *zp;
    #endif
    #ifdef RENDER_T
        uvw_int2_t u = *up, v = *vp;
        #if ENABLE_TEXTURES_PROJECTIVE
            uvw_int2_t w = *wp;
        #endif
    #endif
	int count = 0;
	
	if ( endy < 0 )
    // early-out
	{
		int dy = endy - y;
		*x1p = x1 + dy * dx1;
		*x2p = x2 + dy * dx2;
        #ifdef RENDER_Z
		*zp = z + dy * dzdy;
        #endif
        #ifdef RENDER_T
            *up = u + dy * dudy;
            *vp = v + dy * dvdy;
            #if ENABLE_TEXTURES_PROJECTIVE
                *wp = w + dy * dwdy;
            #endif
        #endif
		return;
	}

	if ( y < 0 )
	{
		x1 += -y * dx1;
		x2 += -y * dx2;
        #ifdef RENDER_Z
		z += -y * dzdy;
        #endif
        #ifdef RENDER_T
		    u += -y * dudy;
    		v += -y * dvdy;
            #if ENABLE_TEXTURES_PROJECTIVE
                w += -y * dwdy;
            #endif
        #endif
		y = 0;
	}
    
    #ifdef RENDER_T
        // read texture bitmap
        uint8_t* bmdata;
        uint8_t* maskdata;
        int width, height;
        int rowbytes;
        int hasmask;
        pd->graphics->getBitmapData(texture, &width, &height, &rowbytes, &hasmask, &bmdata);
        
        #if ENABLE_TEXTURES >= 2
        if (hasmask)
        {
            maskdata = bmdata + height * rowbytes;
        }
        #endif
    #endif

	while ( y < endy )
	{	
        #if defined(RENDER_T) && defined(RENDER_Z)
            #if ENABLE_TEXTURES >= 2
            (hasmask ? drawFragment_zta : drawFragment_zt)(
            #else
            drawFragment_zt(
            #endif
                (uint32_t*)&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], (x1>>16), (x2>>16)+1,
                z, dzdx,
                u, dudx, v, dvdx,
                bmdata, rowbytes, width, height
                #if ENABLE_TEXTURES_PROJECTIVE
                    , w, dwdx
                #endif
            );
        #elif defined(RENDER_T) && !defined(RENDER_Z)
            #if ENABLE_TEXTURES >= 2
            (hasmask ? drawFragment_ta : drawFragment_t)(
            #else
            drawFragment_t(
            #endif
                (uint32_t*)&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], (x1>>16), (x2>>16)+1,
                u, dudx, v, dvdx,
                bmdata, rowbytes, width, height
                #if ENABLE_TEXTURES_PROJECTIVE
                    , w, dwdx
                #endif
            );
        #elif defined(RENDER_Z) && !defined(RENDER_T)
            // no texture
            uint8_t p = pattern[y%8];
            uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
            drawFragment_z(
                (uint32_t*)&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], (x1>>16), (x2>>16)+1,
                z, dzdx, color
            );
        #endif
		
		x1 += dx1;
		x2 += dx2;
        #ifdef RENDER_Z
		z += dzdy;
        #endif
        #ifdef RENDER_T
		u += dudy;
		v += dvdy;
        #if ENABLE_TEXTURES_PROJECTIVE
            w += dwdy;
        #endif
        #endif
		++y;
	}
	
	*x1p = x1;
	*x2p = x2;
    #ifdef RENDER_Z
	*zp = z;
    #endif
    #ifdef RENDER_T
	*up = u;
	*vp = v;
    #if ENABLE_TEXTURES_PROJECTIVE
        *wp = w;
    #endif
    #endif
}
#endif

#undef SYM
#undef SYM_Z
#undef SYM_ZT
#undef SYM_ZTA
#undef _GLUE
#undef GLUE
#undef RENDER_Z

#ifdef RENDER_Z
    #undef RENDER_Z
#endif
#ifdef RENDER_T
    #undef RENDER_T
#endif
#ifdef RENDER_A
    #undef RENDER_A
#endif