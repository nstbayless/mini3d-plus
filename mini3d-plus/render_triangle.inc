#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

LCDRowRange

#ifdef RENDER_Z
fillTriangle_zt
#else
fillTriangle_t
#endif
(
	uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3,
	Texture* texture, Point2D t1, Point2D t2, Point2D t3
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
	sortTri_t(&p1, &p2, &p3, &t1, &t2, &t3);
	
	#if ENABLE_RENDER_DISTANCE_MAX
	if (render_distance_bounds(p1, p2, p3)) return (LCDRowRange){ 0, 0 };
	#endif
	
	int endy = MIN(VIEWPORT_BOTTOM, p3->y);
	int det = (p3->x - p1->x) * (p2->y - p1->y) - (p2->x - p1->x) * (p3->y - p1->y);
	if ( p1->y > VIEWPORT_BOTTOM || endy < VIEWPORT_TOP || det == 0 )
		return (LCDRowRange){ 0, 0 };
	
	#if TEXTURE_PERSPECTIVE_MAPPING
	projective_ratio_test_result_t prt_factor;
	int do_projective_split = 0;
	if (projective)
	{
		prt_factor = projective_ratio_test(p1, p2, p3);
		#if TEXTURE_PERSPECTIVE_MAPPING_SPLIT
		if (prt_factor <= 0)
		{
			projective = 0;
		}
		else if (prt_factor < 1)
		{
			// split triangle
			do_projective_split = splitTriangleProjective(
				prt_factor,
				p1, p2, p3, &t1, &t2, &t3
			);
			
			if (do_projective_split)
			{
				p1 = &p1b; p2 = &p2b; p3 = &p3b;
				t1 = t1b; t2 = t2b; t3 = t3b;
				
				sortTri_t(&p1, &p2, &p3, &t1, &t2, &t3);
			}
		}
		#else
		projective = prt_factor;
		#endif
	}
	#endif
	
	// scale points to texture size
	int width, height, fmt;
	Texture_getData(texture, &width, &height, NULL, NULL, &fmt, NULL);
	t1.x *= width; t1.y *= height;
	t2.x *= width; t2.y *= height;
	t3.x *= width; t3.y *= height;
	
	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;

	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y, 16);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y, 16);

	int32_t dx1 = MIN(sb,sc);
	int32_t dx2 = MAX(sb,sc);
	
	float z1 = zscale / (p1->z + Z_BIAS);
	float z2 = zscale / (p2->z + Z_BIAS);
	float z3 = zscale / (p3->z + Z_BIAS);
	
	#if TEXTURE_PERSPECTIVE_MAPPING
	float w1;
	float w2;
	float w3;
	if (projective)
	{
		w1 = 1 / p1->z;
		w2 = 1 / p2->z;
		w3 = 1 / p3->z;
	}
	else
	{
		w1 = 1;
		w2 = 1;
		w3 = 1;
	}
	#else
	const int projective = 0;
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
	#if TEXTURE_PERSPECTIVE_MAPPING
	float mw;
	if (projective)
	{
		mw = w1 + (p2->y - p1->y) * (w3-w1) / (p3->y - p1->y);
	}
	uvw_int2_t dwdx, dwdy;
	#endif

	int32_t dzdx, dzdy;
	uvw_int2_t dudx, dudy, dvdx, dvdy;

	if ( sc < sb )
	{
		dzdx = slope(mz, mx, z2, p2->x, 16);
		dzdy = slope(z1, p1->y, z3, p3->y, 16);
		dudx = UV_SLOPE(mu, mx, u2, p2->x, UV_SHIFT);
		dudy = UV_SLOPE(u1, p1->y, u3, p3->y, UV_SHIFT);
		dvdx = UV_SLOPE(mv, mx, v2, p2->x, UV_SHIFT);
		dvdy = UV_SLOPE(v1, p1->y, v3, p3->y, UV_SHIFT);
		#if TEXTURE_PERSPECTIVE_MAPPING
		if (projective)
		{
			dwdx = W_SLOPE(mw, mx, w2, p2->x, W_SHIFT);
			dwdy = W_SLOPE(w1, p1->y, w3, p3->y, W_SHIFT);
		}
		#endif
	}
	else
	{
		dzdx = slope(z2, p2->x, mz, mx, 16);
		dzdy = slope(z1, p1->y, z2, p2->y, 16);
		dudx = UV_SLOPE(u2, p2->x, mu, mx, UV_SHIFT);
		dudy = UV_SLOPE(u1, p1->y, u2, p2->y, UV_SHIFT);
		dvdx = UV_SLOPE(v2, p2->x, mv, mx, UV_SHIFT);
		dvdy = UV_SLOPE(v1, p1->y, v2, p2->y, UV_SHIFT);
		#if TEXTURE_PERSPECTIVE_MAPPING
		if (projective)
		{
			dwdx = W_SLOPE(w2, p2->x, mw, mx, W_SHIFT);
			dwdy = W_SLOPE(w1, p1->y, w2, p2->y, W_SHIFT);
		}
		#endif
	}
	
	zcoord_t z = z1 * (1<<ZSHIFT);
	uvw_int2_t u = u1 * ((uvw_int2_t)(1)<<UV_SHIFT);
	uvw_int2_t v = v1 * ((uvw_int2_t)(1)<<UV_SHIFT);
	#if TEXTURE_PERSPECTIVE_MAPPING
	uvw_int2_t w = w1 * ((uvw_int2_t)(1)<<W_SHIFT);
	#endif
	
	#if ENABLE_TEXTURES_GREYSCALE
		// map lighting to range 0-255
		uint8_t u8lightp = CLAMP(0.0f, 1.0f, lighting_weight) * 255.99f;
		uint8_t u8light = CLAMP(0.0f, 1.0f, lighting) * (LIGHTING_PATTERN_COUNT - 0.001f);
		// precompute
		u8light = (((uint16_t)u8light * u8lightp) + 0x80) >> 8;
		
		#define fillRange_zt_or_ztg(fname, fname_g, ...) \
			if (u8lightp == 0 && fmt == 0) \
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
	
    #ifdef RENDER_Z
        #if TEXTURE_PERSPECTIVE_MAPPING
            #define fillRange_zt_or_ztp(...) \
            if (projective) \
            { \
                fillRange_zt_or_ztg(fillRange_ztp, fillRange_ztgp, __VA_ARGS__, &w, dwdy, dwdx); \
            } \
            else \
            { \
                fillRange_zt_or_ztg(fillRange_zt, fillRange_ztg, __VA_ARGS__); \
            }
        #else
            #define fillRange_zt_or_ztp(...) fillRange_zt_or_ztg(fillRange_zt, fillRange_ztg, __VA_ARGS__)
        #endif
    #else
        #if TEXTURE_PERSPECTIVE_MAPPING
            #define fillRange_zt_or_ztp(...) \
            if (projective) \
            { \
                fillRange_zt_or_ztg(fillRange_tp, fillRange_tgp, __VA_ARGS__, &w, dwdy, dwdx); \
            } \
            else \
            { \
                fillRange_zt_or_ztg(fillRange_t, fillRange_tg, __VA_ARGS__); \
            }
        #else
            #define fillRange_zt_or_ztp(...) fillRange_zt_or_ztg(fillRange_zt, fillRange_ztg, __VA_ARGS__)
        #endif
    #endif

	fillRange_zt_or_ztp(
		bitmap, rowstride, p1->y, MIN(VIEWPORT_BOTTOM, p2->y), &x1, dx1, &x2, dx2,
            #ifdef RENDER_Z
            &z, dzdy, dzdx,
            #endif
            &u, dudy, dudx, &v, dvdy, dvdx, texture
		#if ENABLE_CUSTOM_PATTERNS
		, pattern
		#endif
		#if ENABLE_POLYGON_SCANLINING
		, scanline
		#endif
	);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y, 16);

	if ( sb < sc )
	{
		dzdy = slope(z2, p2->y, z3, p3->y, 16);
		dudy = UV_SLOPE(u2, p2->y, u3, p3->y, UV_SHIFT);
		dvdy = UV_SLOPE(v2, p2->y, v3, p3->y, UV_SHIFT);
		x1 = p2->x * (1<<16);
		z = z2 * (1<<ZSHIFT);
		u = u2 * ((uvw_int2_t)(1)<<UV_SHIFT);
		v = v2 * ((uvw_int2_t)(1)<<UV_SHIFT);
		#if TEXTURE_PERSPECTIVE_MAPPING
		if (projective)
		{
			dwdy = UV_SLOPE(w2, p2->y, w3, p3->y, W_SHIFT);
			w = w2 * ((uvw_int2_t)(1)<<W_SHIFT);
		}
		#endif
		fillRange_zt_or_ztp(
			bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2,
            #ifdef RENDER_Z
            &z, dzdy, dzdx,
            #endif
            &u, dudy, dudx, &v, dvdy, dvdx, texture
			#if ENABLE_CUSTOM_PATTERNS
			, pattern
			#endif
			#if ENABLE_POLYGON_SCANLINING
			, scanline
			#endif
		);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange_zt_or_ztp(
			bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, 
            #ifdef RENDER_Z
            &z, dzdy, dzdx,
            #endif
            &u, dudy, dudx, &v, dvdy, dvdx, texture
			#if ENABLE_CUSTOM_PATTERNS
			, pattern
			#endif
			#if ENABLE_POLYGON_SCANLINING
			, scanline
			#endif
		);
	}
	
	#if TEXTURE_PERSPECTIVE_MAPPING && TEXTURE_PERSPECTIVE_MAPPING_SPLIT
	if (do_projective_split)
	{
		// we have split this triangle, and drawn only part of it so far.
		// now draw the rest.
		
		fillTriangle_zt(
			bitmap, rowstride,
			&p1a, &p2a, &p3a,
			texture, t1a, t2a, t3a
			#if ENABLE_CUSTOM_PATTERNS
			, pattern
			#endif
			#if ENABLE_POLYGON_SCANLINING
			, scanline
			#endif
			#if ENABLE_TEXTURES_GREYSCALE
			, lighting, lighting_weight
			#endif
			, 0
		);
	}
	#endif
		
	return (LCDRowRange){ MAX(0, p1->y), endy };
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#undef fillRange_zt_or_ztp
#undef fillRange_zt_or_ztg