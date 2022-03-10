//
//  pd3d.c
//  Playdate Simulator
//
//  Created by Dave Hayden on 8/25/15.
//  Copyright (c) 2015 Panic, Inc. All rights reserved.
//

#include <string.h>
#include "3dmath.h"

Matrix3D identityMatrix = { .isIdentity = 1, .inverting = 0, .m = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, .dx = 0, .dy = 0, .dz = 0 };

Matrix3D Matrix3DMake(float m11, float m12, float m13, float m21, float m22, float m23, float m31, float m32, float m33, int inverting)
{
	return (Matrix3D){ .isIdentity = 0, .inverting = inverting, .m = {{m11, m12, m13}, {m21, m22, m23}, {m31, m32, m33}}, .dx = 0, .dy = 0, .dz = 0 };
}

Matrix3D Matrix3DMakeTranslate(float dx, float dy, float dz)
{
	return (Matrix3D){ .isIdentity = 1, .inverting = 0, .m = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, .dx = dx, .dy = dy, .dz = dz };
}

Matrix3D Matrix3D_multiply(Matrix3D l, Matrix3D r)
{
	Matrix3D m = { .isIdentity = 0, .inverting = l.inverting ^ r.inverting };
	
	if ( l.isIdentity )
	{
		if ( r.isIdentity )
			m = identityMatrix;
		else
			memcpy(&m.m, &r.m, sizeof(r.m));
		
		m.dx = l.dx + r.dx;
		m.dy = l.dy + r.dy;
		m.dz = l.dz + r.dz;
	}
	else
	{
		if ( !r.isIdentity )
		{
			m.m[0][0] = l.m[0][0] * r.m[0][0] + l.m[1][0] * r.m[0][1] + l.m[2][0] * r.m[0][2];
			m.m[1][0] = l.m[0][0] * r.m[1][0] + l.m[1][0] * r.m[1][1] + l.m[2][0] * r.m[1][2];
			m.m[2][0] = l.m[0][0] * r.m[2][0] + l.m[1][0] * r.m[2][1] + l.m[2][0] * r.m[2][2];

			m.m[0][1] = l.m[0][1] * r.m[0][0] + l.m[1][1] * r.m[0][1] + l.m[2][1] * r.m[0][2];
			m.m[1][1] = l.m[0][1] * r.m[1][0] + l.m[1][1] * r.m[1][1] + l.m[2][1] * r.m[1][2];
			m.m[2][1] = l.m[0][1] * r.m[2][0] + l.m[1][1] * r.m[2][1] + l.m[2][1] * r.m[2][2];

			m.m[0][2] = l.m[0][2] * r.m[0][0] + l.m[1][2] * r.m[0][1] + l.m[2][2] * r.m[0][2];
			m.m[1][2] = l.m[0][2] * r.m[1][0] + l.m[1][2] * r.m[1][1] + l.m[2][2] * r.m[1][2];
			m.m[2][2] = l.m[0][2] * r.m[2][0] + l.m[1][2] * r.m[2][1] + l.m[2][2] * r.m[2][2];
		}
		else
			memcpy(&m.m, &l.m, sizeof(l.m));

		m.dx = l.dx * r.m[0][0] + l.dy * r.m[0][1] + l.dz * r.m[0][2] + r.dx;
		m.dy = l.dx * r.m[1][0] + l.dy * r.m[1][1] + l.dz * r.m[1][2] + r.dy;
		m.dz = l.dx * r.m[2][0] + l.dy * r.m[2][1] + l.dz * r.m[2][2] + r.dz;
	}
	
	return m;
}

Point3D Matrix3D_apply(Matrix3D m, Point3D p)
{
	if ( m.isIdentity )
		return Point3DMake(p.x + m.dx, p.y + m.dy, p.z + m.dz);

	float x = p.x * m.m[0][0] + p.y * m.m[0][1] + p.z * m.m[0][2] + m.dx;
	float y = p.x * m.m[1][0] + p.y * m.m[1][1] + p.z * m.m[1][2] + m.dy;
	float z = p.x * m.m[2][0] + p.y * m.m[2][1] + p.z * m.m[2][2] + m.dz;
	
	return Point3DMake(x, y, z);
}

Vector3D Vector3D_normalize(Vector3D v)
{
	float d = fisr(v.dx * v.dx + v.dy * v.dy + v.dz * v.dz);
	
	return Vector3DMake(v.dx * d, v.dy * d, v.dz * d);
}

Vector3D normal(Point3D* p1, Point3D* p2, Point3D* p3)
{
	Vector3D v = Vector3DCross(Vector3DMake(p2->x - p1->x, p2->y - p1->y, p2->z - p1->z),
							   Vector3DMake(p3->x - p1->x, p3->y - p1->y, p3->z - p1->z));
	
	return Vector3D_normalize(v);
}

float Matrix3D_getDeterminant(Matrix3D* m)
{
	return m->m[0][0] * m->m[1][1] * m->m[2][2]
		 + m->m[0][1] * m->m[1][2] * m->m[2][0]
		 + m->m[0][2] * m->m[1][0] * m->m[2][1]
		 - m->m[2][0] * m->m[1][1] * m->m[0][2]
		 - m->m[1][0] * m->m[0][1] * m->m[2][2]
		 - m->m[0][0] * m->m[2][1] * m->m[1][2];
}

Vector3D Point3D_line_difference(Point3D* a, Point3D* b, Point3D* p)
{
	Vector3D line = Point3D_difference(a, b);
	line = Vector3D_normalize(line);
	Vector3D diff = Point3D_difference(a, p);
	float dot = Vector3DDot(diff, line);
	line.dx *= dot;
	line.dy *= dot;
	line.dz *= dot;
	diff.dx -= line.dx;
	diff.dy -= line.dx;
	diff.dz -= line.dx;
	return diff;
}