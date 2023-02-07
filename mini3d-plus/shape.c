//
//  shape.c
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "mini3d.h"
#include "shape.h"
#include "pattern.h"

void Shape3D_init(Shape3D* shape)
{
	shape->retainCount = 0;
	shape->nPoints = 0;
	shape->points = NULL;
	shape->nFaces = 0;
	shape->faces = NULL;
#if ENABLE_TEXTURES
	shape->texture = NULL;
	shape->texmap = NULL;
#endif
	#if ENABLE_POLYGON_SCANLINING
	shape->scanline.select = kScanlineAll;
	shape->scanline.fill = 0xAAAAAAAA;
	#endif
#if ENABLE_CUSTOM_PATTERNS
	shape->pattern = &patterns;
#endif
	shape->center.x = 0;
	shape->center.y = 0;
	shape->center.z = 0;
	shape->colorBias = 0;
	shape->isClosed = 0;
#if ENABLE_ORDERING_TABLE
	shape->orderTableSize = 0;
#endif
}

Shape3D* Shape3D_retain(Shape3D* shape)
{
	++shape->retainCount;
	return shape;
}

void Shape3D_release(Shape3D* shape)
{
	if ( --shape->retainCount > 0 )
		return;
	
	if ( shape->points != NULL )
		m3d_free(shape->points);
	
	if ( shape->faces != NULL )
		m3d_free(shape->faces);
		
	#if ENABLE_TEXTURES
	if ( shape->texture != NULL )
		Texture_unref(shape->texture);

	if ( shape->texmap != NULL )
		m3d_free(shape->texmap);
	#endif
	
	#if ENABLE_CUSTOM_PATTERNS
	Pattern_unref(shape->pattern);
	#endif
	
	m3d_free(shape);
}

int Shape3D_addPoint(Shape3D* shape, Point3D* p)
{
	int i;
	
	// check if this point is already there -- return that index instead.
	for ( i = 0; i < shape->nPoints; ++i )
	{
		if ( Point3D_equals(*p, shape->points[i]) )
			return i;
	}
	
	shape->points = m3d_realloc(shape->points, (shape->nPoints + 1) * sizeof(Point3D));
	
	Point3D* point = &shape->points[shape->nPoints];
	*point = *p;
	
	return shape->nPoints++;
}

#if ENABLE_TEXTURES
// makes the texmap buffer as long as the face buffer
static void
Shape3D_resize_texmap_buffer(Shape3D* shape)
{
	int was_null = shape->texmap == NULL;
	shape->texmap = m3d_realloc(shape->texmap, shape->nFaces * sizeof(FaceTexture));
	if (was_null)
	{
		memset(shape->texmap, 0, sizeof(FaceTexture) * shape->nFaces);
	}
	else
	{
		memset(shape->texmap + (shape->nFaces - 1), 0, sizeof(FaceTexture));
	}
}
#endif

size_t Shape3D_addFace(Shape3D* shape, Point3D* a, Point3D* b, Point3D* c, Point3D* d, float colorBias)
{
	shape->faces = m3d_realloc(shape->faces, (shape->nFaces + 1) * sizeof(Face3D));
	
	Face3D* face = &shape->faces[shape->nFaces];
	face->p1 = Shape3D_addPoint(shape, a);
	face->p2 = Shape3D_addPoint(shape, b);
	face->p3 = Shape3D_addPoint(shape, c);
	face->p4 = (d != NULL) ? Shape3D_addPoint(shape, d) : 0xffff;
	face->colorBias = colorBias;
	face->isDoubleSided = 0;
	
	++shape->nFaces;
	
	if ( d != NULL )
	{
		shape->center.x += (a->x + b->x + c->x + d->x) / 4 / shape->nFaces;
		shape->center.y += (a->y + b->y + c->y + d->y) / 4 / shape->nFaces;
		shape->center.z += (a->z + b->z + c->z + d->z) / 4 / shape->nFaces;
	}
	else
	{
		shape->center.x += (a->x + b->x + c->x) / 3 / shape->nFaces;
		shape->center.y += (a->y + b->y + c->y) / 3 / shape->nFaces;
		shape->center.z += (a->z + b->z + c->z) / 3 / shape->nFaces;
	}
	
	#if ENABLE_TEXTURES
	if (shape->texmap)
		Shape3D_resize_texmap_buffer(shape);
	#endif
	
	return shape->nFaces - 1;
}

void Shape3D_setClosed(Shape3D* shape, int flag)
{
	shape->isClosed = flag;
}

void Shape3D_setFaceDoubleSided(Shape3D* shape, size_t face_idx, int flag)
{
	if (face_idx < shape->nFaces)
		shape->faces[face_idx].isDoubleSided = flag;
}

#if ENABLE_TEXTURES
void Shape3D_setFaceTextureMap(
	Shape3D* shape, size_t face_idx,
	Point2D t1, Point2D t2, Point2D t3, Point2D t4
)
{
	if (!shape->texmap)
		Shape3D_resize_texmap_buffer(shape);
	if (face_idx < shape->nFaces)
	{
		FaceTexture* ft = &shape->texmap[face_idx];
		ft->texture_enabled = 1;
		ft->t1 = t1;
		ft->t2 = t2;
		ft->t3 = t3;
		ft->t4 = t4;
	}
}

#if ENABLE_TEXTURES_GREYSCALE
void Shape3D_setFaceLighting(
	Shape3D* shape, size_t face_idx, float lighting
)
{
	if (!shape->texmap)
		Shape3D_resize_texmap_buffer(shape);
	if (face_idx < shape->nFaces)
	{
		FaceTexture* ft = &shape->texmap[face_idx];
		ft->texture_enabled = (lighting < 1);
		ft->lighting = lighting;
	}
}
#endif

// Note: shape gains ownership of this bitmap.
void Shape3D_setTexture(Shape3D* shape, Texture* texture)
{
	if (texture) Texture_ref(texture);
	if (shape->texture) Texture_unref(shape->texture);
	shape->texture = texture;
}
#endif

#if ENABLE_CUSTOM_PATTERNS
void Shape3D_setPattern(Shape3D* shape, PatternTable* pattern)
{
	if (pattern == NULL) pattern = &patterns;
	Pattern_ref(pattern);
	Pattern_unref(shape->pattern);
	shape->pattern = pattern;
}
#endif

#if ENABLE_ORDERING_TABLE
void Shape3D_setOrderTableSize(Shape3D* shape, size_t size)
{
	shape->orderTableSize = size;
}
#endif

#if ENABLE_POLYGON_SCANLINING
void Shape3D_setScanlining(Shape3D* shape, ScanlineFill scanlineFill)
{
	shape->scanline = scanlineFill;
}
#endif
