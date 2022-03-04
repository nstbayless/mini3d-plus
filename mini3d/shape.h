//
//  shape.h
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#ifndef shape_h
#define shape_h

#include "mini3d.h"
#include "3dmath.h"

typedef struct
{
	uint16_t p1;
	uint16_t p2;
	uint16_t p3;
	uint16_t p4; // 0xffff if it's a tri
	float colorBias;
} Face3D;

typedef struct
{
	int retainCount;
	int nPoints;
	Point3D* points;
	int nFaces;
	Face3D* faces;
	Point3D center; // used for z-sorting entire shapes at a time
	float colorBias;
	int isClosed : 1;
#if ENABLE_ORDERING_TABLE
	int orderTableSize;
#endif
} Shape3D;

void Shape3D_init(Shape3D* shape);
void Shape3D_initWithPrototype(Shape3D* shape, Shape3D* proto);

Shape3D* Shape3D_retain(Shape3D* shape);
void Shape3D_release(Shape3D* shape);

void Shape3D_addFace(Shape3D* shape, Point3D* a, Point3D* b, Point3D* c, Point3D* d, float colorBias);

void Shape3D_setClosed(Shape3D* shape, int flag);

#if ENABLE_ORDERING_TABLE
void Shape3D_setOrderTableSize(Shape3D* shape, int size);
#endif

#endif /* shape_h */
