//
//  shape.c
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "mini3d.h"
#include "shape.h"

void Shape3D_init(Shape3D* shape)
{
	shape->retainCount = 0;
	shape->nPoints = 0;
	shape->points = NULL;
	shape->nFaces = 0;
	shape->faces = NULL;
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
	
	m3d_free(shape);
}

int Shape3D_addPoint(Shape3D* shape, Point3D* p)
{
	int i;
	
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

void Shape3D_addFace(Shape3D* shape, Point3D* a, Point3D* b, Point3D* c, Point3D* d, float colorBias)
{
	shape->faces = m3d_realloc(shape->faces, (shape->nFaces + 1) * sizeof(Face3D));
	
	Face3D* face = &shape->faces[shape->nFaces];
	face->p1 = Shape3D_addPoint(shape, a);
	face->p2 = Shape3D_addPoint(shape, b);
	face->p3 = Shape3D_addPoint(shape, c);
	face->p4 = (d != NULL) ? Shape3D_addPoint(shape, d) : 0xffff;
	face->colorBias = colorBias;
	
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
}

void Shape3D_setClosed(Shape3D* shape, int flag)
{
	shape->isClosed = flag;
}

#if ENABLE_ORDERING_TABLE
void Shape3D_setOrderTableSize(Shape3D* shape, int size)
{
	shape->orderTableSize = size;
}
#endif
