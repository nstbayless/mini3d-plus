//
//  shape.c
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "mini3d.h"
#include "imposter.h"
#include "pattern.h"

void Imposter3D_init(Imposter3D* imposter)
{	
	imposter->retainCount = 0;
	imposter->center.x = 0;
	imposter->center.y = 0;
	imposter->center.z = 0;
	imposter->x1 = -1;
	imposter->x2 = 1;
	imposter->y1 = -1;
	imposter->y2 = 1;
	imposter->z1 = 0;
	imposter->z2 = 0;
	imposter->z3 = 0;
	imposter->z4 = 0;
	#if ENABLE_TEXTURES
	imposter->bitmap = NULL;
	
	#if ENABLE_TEXTURES_GREYSCALE
	imposter->lighting = 0;
	#endif
	#endif
	
	#if ENABLE_CUSTOM_PATTERNS
	imposter->pattern = &patterns;
	#endif
}

Imposter3D* Imposter3D_retain(Imposter3D* imposter)
{
	++imposter->retainCount;
	return imposter;
}

void Imposter3D_release(Imposter3D* imposter)
{
	if ( --imposter->retainCount > 0 )
		return;
	
	m3d_free(imposter);
	
	#if ENABLE_TEXTURES
	if (imposter->bitmap)
		Texture_unref(imposter->bitmap);
	#endif
	
	#if ENABLE_CUSTOM_PATTERNS
	Pattern_unref(imposter->pattern);
	#endif
}

void Imposter3D_setPosition(Imposter3D* imposter, Point3D* position)
{
	imposter->center = *position;
}

void Imposter3D_setRectangle(Imposter3D* imposter, float x1, float y1, float x2, float y2)
{
	imposter->x1 = x1;
	imposter->y1 = y1;
	imposter->x2 = x2;
	imposter->y2 = y2;
}

void Imposter3D_setZOffsets(Imposter3D* imposter, float z1, float z2, float z3, float z4)
{
	imposter->z1 = z1;
	imposter->z2 = z2;
	imposter->z3 = z3;
	imposter->z4 = z4;
}

#if ENABLE_TEXTURES
void Imposter3D_setBitmap(Imposter3D* imposter, Texture* bitmap)
{
	if (bitmap) Texture_ref(bitmap);
	if (imposter->bitmap) Texture_unref(imposter->bitmap);
	imposter->bitmap = bitmap;
}

#if ENABLE_TEXTURES_GREYSCALE
void Imposter3D_setLighting(
	Imposter3D* imposter, float lighting
)
{
	imposter->lighting = lighting;
}
#endif
#endif

#if ENABLE_CUSTOM_PATTERNS
// pattern must either be NULL,
// or else it must be a refcounted pattern table created via Pattern_new().
void Imposter3D_setPattern(Imposter3D* imposter, PatternTable* pattern)
{
	if (pattern == NULL) pattern = &patterns;
	Pattern_ref(pattern);
	Pattern_unref(imposter->pattern);
	imposter->pattern = pattern;
}
#endif