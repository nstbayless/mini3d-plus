//
//  pd3d.h
//  Playdate Simulator
//
//  Created by Dave Hayden on 8/25/15.
//  Copyright (c) 2015 Panic, Inc. All rights reserved.
//

#ifndef __Playdate_Simulator__pd3d__
#define __Playdate_Simulator__pd3d__

#include <stdio.h>
#include <stdint.h>
#include <math.h>

static inline float fisr(float n)
{
	// Fast inverse square root: http://en.wikipedia.org/wiki/Fast_inverse_square_root
	
	union {
		float f;
		uint32_t i;
	} conv;
	
	float x2;
	const float threehalfs = 1.5F;
	
	x2 = n * 0.5F;
	conv.f  = n;
	conv.i  = 0x5f3759df - ( conv.i >> 1 );
	conv.f  = conv.f * ( threehalfs - ( x2 * conv.f * conv.f ) );
	conv.f  = conv.f * ( threehalfs - ( x2 * conv.f * conv.f ) );
	return conv.f;
}

typedef struct
{
	float x;
	float y;
	float z;
} Point3D;

static inline Point3D Point3DMake(float x, float y, float z)
{
	return (Point3D){ .x = x, .y = y, .z = z };
}

static inline int Point3D_equals(Point3D a, Point3D b)
{
	return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

typedef struct
{
	float dx;
	float dy;
	float dz;
} Vector3D;

static inline Vector3D Vector3DMake(float dx, float dy, float dz)
{
	return (Vector3D){ .dx = dx, .dy = dy, .dz = dz };
}

static inline Vector3D Vector3DCross(Vector3D a, Vector3D b)
{
	return (Vector3D){ .dx = a.dy * b.dz - a.dz * b.dy, .dy = a.dz * b.dx - a.dx * b.dz, .dz = a.dx * b.dy - a.dy * b.dx };
}

static inline float Vector3DDot(Vector3D a, Vector3D b)
{
	return a.dx * b.dx + a.dy * b.dy + a.dz * b.dz;
}

Vector3D Vector3D_normalize(Vector3D v);
Vector3D normal(Point3D* p1, Point3D* p2, Point3D* p3);

static inline float Vector3D_lengthSquared(Vector3D* v)
{
	return v->dx * v->dx + v->dy * v->dy + v->dz * v->dz;
}

static inline float Vector3D_length(Vector3D* v)
{
	return sqrtf(Vector3D_lengthSquared(v));
}

static inline Point3D Point3D_addVector(Point3D a, Vector3D v)
{
	return Point3DMake(a.x + v.dx, a.y + v.dy, a.z + v.dz);
}

static inline Vector3D Point3D_difference(Point3D* a, Point3D* b)
{
	return (Vector3D){ .dx = b->x - a->x, .dy = b->y - a->y, .dz = b->z - a->z };
}

static inline int Point3D_equal(Point3D* a, Point3D* b)
{
	if (a->x != b->x) return 0;
	if (a->y != b->y) return 0;
	if (a->z != b->z) return 0;
	return 1;
}

// XXX - which side are we multiplying on?

typedef struct
{
	int isIdentity : 1;
	int inverting : 1;
	float m[3][3];
	float dx;
	float dy;
	float dz;
} Matrix3D;

extern Matrix3D identityMatrix;

Matrix3D Matrix3DMake(float m11, float m12, float m13, float m21, float m22, float m23, float m31, float m32, float m33, int inverting);
Matrix3D Matrix3DMakeTranslate(float dx, float dy, float dz);
Matrix3D Matrix3D_multiply(Matrix3D l, Matrix3D r);
Point3D Matrix3D_apply(Matrix3D m, Point3D p);

float Matrix3D_getDeterminant(Matrix3D* m);

#endif
