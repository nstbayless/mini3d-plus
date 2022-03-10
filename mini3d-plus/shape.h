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

#if ENABLE_TEXTURES
typedef LCDBitmap LCDBitmap;

typedef struct
{
	int texture_enabled:1;
	Point2D t1;
	Point2D t2;
	Point2D t3;
	Point2D t4;
#if ENABLE_TEXTURES_GREYSCALE
	// Range from 0 to 1.
	// If 0, then texture will 100% determined by texture and
	// lighting will be ignored.
	// If 1, face will be 100% determined by lighting and texture
	// will be ignored.
	float lighting;
#endif
} FaceTexture;
#endif

typedef struct
{
	int retainCount;
	int nPoints;
	Point3D* points;
	int nFaces;
	Face3D* faces;
#if ENABLE_TEXTURES
	// iff NULL then this shape is not textured.
	LCDBitmap* texture;
	FaceTexture* texmap;
#endif
	Point3D center; // used for z-sorting entire shapes at a time, and for collision detection
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

size_t Shape3D_addFace(Shape3D* shape, Point3D* a, Point3D* b, Point3D* c, Point3D* d, float colorBias);

void Shape3D_setClosed(Shape3D* shape, int flag);

#if ENABLE_TEXTURES
// t4 can be set to anything if not used
void Shape3D_setFaceTextureMap(
	Shape3D* shape, size_t face_idx,
	Point2D t1, Point2D t2, Point2D t3, Point2D t4
);

#if ENABLE_TEXTURES_GREYSCALE
void Shape3D_setFaceLighting(
	Shape3D* shape, size_t face_idx, float lighting
);
#endif

// Note: shape gains ownership of this bitmap.
void Shape3D_setTexture(Shape3D* shape, LCDBitmap* texture);
#endif

#if ENABLE_ORDERING_TABLE
void Shape3D_setOrderTableSize(Shape3D* shape, int size);
#endif

#endif /* shape_h */
