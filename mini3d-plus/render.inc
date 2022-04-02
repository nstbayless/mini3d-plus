// Z: zbuffer
// T: texture
// A: alpha / semitransparent / masked textures
// G: greyscale textures (not strictly black/white) and/or lit textures
// L: greyscale textures (requires G)
// P: projective texture mapping

#define _GLUE(a, b) a##b
#define GLUE(a, b) _GLUE(a, b)

#ifdef RENDER
    #if (RENDER) & RZ
        #define RENDER_Z
    #endif
    #if (RENDER) & RT
        #define RENDER_T
    #endif
    #if (RENDER) & RA
        #define RENDER_A
    #endif
    #if (RENDER) & RG
        #define RENDER_G
    #endif
    #if (RENDER) & RP
        #define RENDER_P
    #endif
    #if (RENDER) & RL
        #define RENDER_L
    #endif
#endif

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

#ifdef RENDER_G
    #define SYM_ZTAG GLUE(SYM_ZTA, g)
#else
    #define SYM_ZTAG SYM_ZTA
#endif

#ifdef RENDER_P
    #define SYM_ZTAGP GLUE(SYM_ZTAG, p)
#else
    #define SYM_ZTAGP SYM_ZTAG
#endif

#ifdef RENDER_L
    #define SYM_ZTAGPL GLUE(SYM_ZTAGP, l)
#else
    #define SYM_ZTAGPL SYM_ZTAGP
#endif

#define SYM(x) GLUE(x, SYM_ZTAGPL)

#if defined(__GCC__) || defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize("fif-conversion")
#pragma GCC optimize("fif-conversion")
__attribute__ ((hot))
#endif

static void
SYM(drawFragment)(
	uint32_t* row, zbuf_t* zbrow, int x, int endx
#ifdef RENDER_Z
	, uint32_t z, int32_t dzdx
#endif
#ifdef RENDER_T
	, uvw_int2_t u, uvw_int2_t dudx,
	uvw_int2_t v, uvw_int2_t dvdx,
	uint8_t* texdata, OPTU32(uint16_t) texrowbytes, OPTU32(uint16_t) texw, OPTU32(uint16_t) texh
    #if ENABLE_CUSTOM_PATTERNS
    , PatternTable* pattern
    #endif
#ifdef RENDER_P
    , uvw_int2_t w, uvw_int2_t dwdx
#endif
#ifdef RENDER_G
    #if ENABLE_TEXTURES_LIGHTING
    , OPTU32(uint8_t) light, OPTU32(uint8_t) texp
    #endif
    , OPTU32(uint8_t) pattern_y
    #endif
#else
    , uint32_t color
#endif
)
{
	if ( endx < VIEWPORT_LEFT || x >= VIEWPORT_RIGHT )
		return;
        
    if ( endx > VIEWPORT_RIGHT )
		endx = VIEWPORT_RIGHT;
	
	if ( x < VIEWPORT_LEFT )
	{
        #ifdef RENDER_Z
		z += (VIEWPORT_LEFT-x) * dzdx;
        #endif
        
        #ifdef RENDER_T
            u += (VIEWPORT_LEFT-x) * dudx;
            v += (VIEWPORT_LEFT-x) * dvdx;
            #ifdef RENDER_P
                w += (VIEWPORT_LEFT-x) * dwdx;
            #endif
        #endif
        
		x = VIEWPORT_LEFT;
	}

	uint32_t mask = 0;
	uint32_t* p = row + x/32;
    #ifdef RENDER_T
	uint32_t color = 0;
    #endif
    #if defined(RENDER_A) && !defined(RENDER_L)
    uint8_t* maskdata = texdata + (texh * texrowbytes);
    #endif
    
    #if defined(RENDER_T) && TEXTURES_ALWAYS_POWER_OF_2
    // allows quicker modulo later.
    texw--;
    texh--;
    #endif

	while ( (unsigned)x < (unsigned)endx )
	{
		// read texture
        #ifdef RENDER_T
            #ifdef RENDER_P
                // |1 to prevent floating point division error
                #if !PRECOMPUTE_PROJECTION
                OPTU32(uvw_int2_t) divisor = (w >> MAX(0, W_SHIFT - UV_SHIFT))|1;
                OPTU32(uint16_t) ui = (u / divisor) >> MAX(0, UV_SHIFT - W_SHIFT);
                OPTU32(uint16_t) vi = (v / divisor) >> MAX(0, UV_SHIFT - W_SHIFT);
                #else
                uint32_t projection_mult = getProjectionMult(w);
                OPTU32(uint16_t) ui = ((u >> (UV_SHIFT - PROJECTION_FIDELITY)) * projection_mult) >> PROJECTION_FIDELITY_B;
                OPTU32(uint16_t) vi = ((v >> (UV_SHIFT - PROJECTION_FIDELITY)) * projection_mult) >> PROJECTION_FIDELITY_B;
                #endif
            #else
                // OPTIMIZE: try other operations here. (This is just paranoid bounds anyway.)
                OPTU32(uint16_t) ui = (u >> UV_SHIFT);
                OPTU32(uint16_t) vi = (v >> UV_SHIFT);
            #endif
            #if TEXTURES_ALWAYS_POWER_OF_2
                ui &= texw;
                vi &= texh;
            #else
                ui %= texw; // OPTIMIZE: try other operations here. (This is just paranoid bounds anyway.)
                vi %= texh; // OPTIMIZE: try other operations here. (This is just paranoid bounds anyway.)
            #endif
            #ifdef RENDER_L
            OPTU32(uint16_t) ti = (vi * texrowbytes) + ui;
            uint32_t texpix = texdata[ti] & ~0x80; // mask out alpha bit
            #else
            OPTU32(uint16_t) ti = (vi * texrowbytes) + ui / 8;
            uint32_t texpix = (texdata[ti] << (ui % 8)) & 0x80; // either 0x80 or 0.
            #endif
        #endif
        
        #ifdef RENDER_A
            #ifdef RENDER_L
                uint32_t alpha = (texdata[ti]) & 0x80;
            #else
                uint32_t alpha = (maskdata[ti] << (ui % 8)) & 0x80; // either 0x80 or 0.
            #endif
        #else
            const OPTU32(uint8_t) alpha = 1;
        #endif
        
        #ifdef RENDER_Z
            zbuf_t zi = z >> ZSHIFT;
            int zx = x;
            #if Z_BUFFER_FRAME_PARITY
            zx *= 2;
            if (zbrow[zx + 1] != zbuff_parity)
            {
                zbrow[zx + 0] = 0;
            }
            zbrow[zx + 1] = zbuff_parity;
            #endif
            if (zi > zbrow[zx] && alpha)
            {
                mask |= 0x80000000u >> (x%32);
                zbrow[zx] = zi;
            }
        #elif defined(RENDER_T)
            mask |= (alpha << 24) >> (x%32);
        #else
            #pragma error "render.inc requires at least one of RENDER_Z and RENDER_T defined."
        #endif
        
        #ifdef RENDER_T
            #ifdef RENDER_G
                #ifdef RENDER_L
                    OPTU32(uint8_t) tex = texpix;
                #else
                    OPTU32(uint8_t) tex = texpix ? (LIGHTING_PATTERN_COUNT - 1) : 0;
                #endif
                OPTU32(uint8_t) combined;
                #if ENABLE_TEXTURES_LIGHTING
                // OPTIMIZE: make light != 0 a different function.
                if (light != 0)
                {
                    combined = ((OPTU32(uint16_t))tex * texp + 0x80) >> 8;
                    combined += light;
                }
                else
                #endif
                {
                    combined = tex;
                }
                OPTU32(uint8_t) pattern_byte = 
                #if ENABLE_CUSTOM_PATTERNS
                (*pattern)
                #else
                patterns
                #endif
                [combined][pattern_y];
                texpix = (pattern_byte << (x % 8)) & 0x80;
            #endif
            
            color |= (texpix << 24) >> (x%32);
        #endif
		
		++x;
        
        #ifdef RENDER_Z
		z += dzdx;
        #endif
        
        #ifdef RENDER_T
            u += dudx;
            v += dvdx;
            #ifdef RENDER_P
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

#if defined(__GCC__) || defined(__clang__)
#pragma GCC pop_options
#endif

#if !defined(RENDER_A) && !defined(RENDER_L)
static void 
SYM(fillRange)(
	uint8_t* bitmap, int rowstride, int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2
#ifdef RENDER_Z
	, uint32_t* zp, int32_t dzdy, int32_t dzdx
#endif
#ifdef RENDER_T
	, uvw_int2_t* up, uvw_int2_t dudy, uvw_int2_t dudx,
	uvw_int2_t* vp, uvw_int2_t dvdy, uvw_int2_t dvdx,
	Texture* texture
    #if ENABLE_CUSTOM_PATTERNS
    , PatternTable* pattern
    #endif
    #if defined(RENDER_T) && ENABLE_POLYGON_SCANLINING
    , ScanlineFill* scanline
    #endif
    #ifdef RENDER_P
    , uvw_int2_t* wp, uvw_int2_t dwdy, uvw_int2_t dwdx
    #endif
    #if defined(RENDER_G)
    , uint8_t light, uint8_t texp
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
        #ifdef RENDER_P
            uvw_int2_t w = *wp;
        #endif
    #endif
	int count = 0;
	
	if ( endy < VIEWPORT_TOP )
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
            #ifdef RENDER_P
                *wp = w + dy * dwdy;
            #endif
        #endif
		return;
	}

	if ( y < VIEWPORT_TOP )
	{
		x1 += (VIEWPORT_TOP-y) * dx1;
		x2 += (VIEWPORT_TOP-y) * dx2;
        #ifdef RENDER_Z
		z += (VIEWPORT_TOP-y) * dzdy;
        #endif
        #ifdef RENDER_T
		    u += (VIEWPORT_TOP-y) * dudy;
    		v += (VIEWPORT_TOP-y) * dvdy;
            #ifdef RENDER_P
                w += (VIEWPORT_TOP-y) * dwdy;
            #endif
        #endif
		y = VIEWPORT_TOP;
	}
    
    #ifdef RENDER_T
        // read texture bitmap
        uint8_t* bmdata;
        int width, height;
        int rowbytes;
        int hasmask;
        int fmt;
        Texture_getData(texture, &width, &height, &rowbytes, &hasmask, &fmt, &bmdata);
    #endif

	while ( y < endy )
	{	
        #if defined(RENDER_P)
            #define drawFragment_ztag_OPT_p drawFragment_ztagp
            #define drawFragment_ztg_OPT_p drawFragment_ztgp
            #define drawFragment_tag_OPT_p drawFragment_tagp
            #define drawFragment_tg_OPT_p drawFragment_tgp
            #define drawFragment_ztagl_OPT_p drawFragment_ztagpl
            #define drawFragment_ztgl_OPT_p drawFragment_ztgpl
            #define drawFragment_tagl_OPT_p drawFragment_tagpl
            #define drawFragment_tgl_OPT_p drawFragment_tgpl
            #define drawFragment_zta_OPT_p drawFragment_ztap
            #define drawFragment_zt_OPT_p drawFragment_ztp
            #define drawFragment_ta_OPT_p drawFragment_tap
            #define drawFragment_t_OPT_p drawFragment_tp
        #else
            #define drawFragment_ztag_OPT_p drawFragment_ztag
            #define drawFragment_ztg_OPT_p drawFragment_ztg
            #define drawFragment_tag_OPT_p drawFragment_tag
            #define drawFragment_tg_OPT_p drawFragment_tg
            #define drawFragment_ztagl_OPT_p drawFragment_ztagl
            #define drawFragment_ztgl_OPT_p drawFragment_ztgl
            #define drawFragment_tagl_OPT_p drawFragment_tagl
            #define drawFragment_tgl_OPT_p drawFragment_tgl
            #define drawFragment_zta_OPT_p drawFragment_zta
            #define drawFragment_zt_OPT_p drawFragment_zt
            #define drawFragment_ta_OPT_p drawFragment_ta
            #define drawFragment_t_OPT_p drawFragment_t
        #endif
        
        #if defined(RENDER_T) && ENABLE_POLYGON_SCANLINING
        const int scanline_permits = scanlinePermitsRow(y, scanline);
        #else
        const int scanline_permits = 1;
        #endif
        
        if (interlacePermitsRow(y) && scanline_permits)
        {
            #if defined(RENDER_T) && defined(RENDER_Z) && defined(RENDER_G)
                #if ENABLE_TEXTURES_MASK
                (hasmask
                    ? (!fmt ? drawFragment_ztag_OPT_p : drawFragment_ztagl_OPT_p)
                    : (!fmt ? drawFragment_ztg_OPT_p : drawFragment_ztgl_OPT_p)
                )(
                #else
                (!fmt ? drawFragment_ztg_OPT_p : drawFragment_ztgl_OPT_p)(
                #endif
                    (uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), (x1>>16), (x2>>16)+1,
                    z, dzdx,
                    u, dudx, v, dvdx,
                    bmdata, rowbytes, width, height
                    #if ENABLE_CUSTOM_PATTERNS
                    , pattern
                    #endif
                    #ifdef RENDER_P
                        , w, dwdx
                    #endif
                    #if ENABLE_TEXTURES_LIGHTING
                    , light, texp
                    #endif
                    , y % 8
                );
            #elif defined(RENDER_T) && !defined(RENDER_Z) && defined(RENDER_G)
                #if ENABLE_TEXTURES_MASK
                (hasmask
                    ? (!fmt ? drawFragment_tag_OPT_p : drawFragment_tagl_OPT_p)
                    : (!fmt ? drawFragment_tg_OPT_p : drawFragment_tgl_OPT_p)
                )(
                #else
                (!fmt ? drawFragment_tg_OPT_p : drawFragment_tgl_OPT_p)(
                #endif
                    (uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), (x1>>16), (x2>>16)+1,
                    u, dudx, v, dvdx,
                    bmdata, rowbytes, width, height
                    #if ENABLE_CUSTOM_PATTERNS
                    , pattern
                    #endif
                    #ifdef RENDER_P
                        , w, dwdx
                    #endif
                    #if ENABLE_TEXTURES_LIGHTING
                    , light, texp
                    #endif
                    , y%8
                );
            #elif defined(RENDER_T) && defined(RENDER_Z)
                #if ENABLE_TEXTURES_MASK
                (hasmask ? drawFragment_zta_OPT_p : drawFragment_zt_OPT_p)(
                #else
                drawFragment_zt_OPT_p(
                #endif
                    (uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), (x1>>16), (x2>>16)+1,
                    z, dzdx,
                    u, dudx, v, dvdx,
                    bmdata, rowbytes, width, height
                    #if ENABLE_CUSTOM_PATTERNS
                    , pattern
                    #endif
                    #ifdef RENDER_P
                        , w, dwdx
                    #endif
                );
            #elif defined(RENDER_T) && !defined(RENDER_Z)
                #if ENABLE_TEXTURES_MASK
                (hasmask ? drawFragment_ta_OPT_p : drawFragment_t_OPT_p)(
                #else
                drawFragment_t_OPT_p(
                #endif
                    (uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), (x1>>16), (x2>>16)+1,
                    u, dudx, v, dvdx,
                    bmdata, rowbytes, width, height
                    #if ENABLE_CUSTOM_PATTERNS
                    , pattern
                    #endif
                    #ifdef RENDER_P
                        , w, dwdx
                    #endif
                );
            #elif defined(RENDER_Z) && !defined(RENDER_T)
                // no texture
                uint8_t p = pattern[y%8];
                uint32_t color = (p<<24) | (p<<16) | (p<<8) | p;
                drawFragment_z(
                    (uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), (x1>>16), (x2>>16)+1,
                    z, dzdx, color
                );
            #endif
        }
        #if defined(RENDER_T) && ENABLE_POLYGON_SCANLINING
        else if (interlacePermitsRow(y) && !scanline_permits)
        {
            #if defined (RENDER_Z)
            drawFragment_z(
                (uint32_t*)&bitmap[y*rowstride], ZBUF_IDX(0, y), (x1>>16), (x2>>16)+1,
                z, dzdx, scanline->fill
            );
            #else
            drawFragment(
                (uint32_t*)&bitmap[y*rowstride], (x1>>16), (x2>>16)+1, scanline->fill
            );
            #endif
        }
        #endif
		
		x1 += dx1;
		x2 += dx2;
        #ifdef RENDER_Z
		z += dzdy;
        #endif
        #ifdef RENDER_T
		u += dudy;
		v += dvdy;
        #ifdef RENDER_P
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
    #ifdef RENDER_P
        *wp = w;
    #endif
    #endif
}

#undef drawFragment_ztagl_OPT_p
#undef drawFragment_ztgl_OPT_p
#undef drawFragment_tagl_OPT_p
#undef drawFragment_tgl_OPT_p
#undef drawFragment_ztag_OPT_p
#undef drawFragment_ztg_OPT_p
#undef drawFragment_tag_OPT_p
#undef drawFragment_tg_OPT_p
#undef drawFragment_zta_OPT_p
#undef drawFragment_zt_OPT_p
#undef drawFragment_ta_OPT_p
#undef drawFragment_t_OPT_p

#endif

#undef SYM
#undef SYM_Z
#undef SYM_ZT
#undef SYM_ZTA
#undef SYM_ZTAG
#undef SYM_ZTAGP
#undef SYM_ZTAGPL
#undef _GLUE
#undef GLUE

#ifdef RENDER_Z
    #undef RENDER_Z
#endif
#ifdef RENDER_T
    #undef RENDER_T
#endif
#ifdef RENDER_A
    #undef RENDER_A
#endif
#ifdef RENDER_G
    #undef RENDER_G
#endif
#ifdef RENDER_P
    #undef RENDER_P
#endif
#ifdef RENDER_L
    #undef RENDER_L
#endif

#undef RENDER