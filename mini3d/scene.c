//
//  scene.c
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include "3dmath.h"
#include "mini3d.h"
#include "scene.h"
#include "shape.h"
#include "render.h"

#include <pd_api.h>

#define WIDTH 400
#define HEIGHT 240

void
Scene3DNode_init(Scene3DNode* node)
{
	node->transform = identityMatrix;
	node->parentNode = NULL;
	node->childNodes = NULL;
	node->nChildren = 0;
	node->shapes = NULL;
	node->colorBias = 0;
	node->renderStyle = kRenderInheritStyle;
	node->isVisible = 1;
	node->needsUpdate = 1;
#if ENABLE_Z_BUFFER
	node->useZBuffer = 1;
#endif
}

void
Scene3DNode_deinit(Scene3DNode* node)
{
	ShapeInstance* shape = node->shapes;
	
	while ( shape != NULL )
	{
		ShapeInstance* next = shape->next;
		Shape3D_release(shape->prototype);
		m3d_free(shape->points);
		m3d_free(shape->faces);
		m3d_free(shape);
		shape = next;
	}
	
	if ( node->shapes != NULL )
		m3d_free(node->shapes);
	
	int i;
	
	for ( i = 0; i < node->nChildren; ++i )
	{
		Scene3DNode_deinit(node->childNodes[i]);
		m3d_free(node->childNodes[i]);
	}
	
	if ( node->childNodes != NULL )
		m3d_free(node->childNodes);

	node->childNodes = NULL;
}

void
Scene3DNode_setTransform(Scene3DNode* node, Matrix3D* xform)
{
	node->transform = *xform;
	
	// mark this branch of the tree for updating
	
	while ( node != NULL )
	{
		node->needsUpdate = 1;
		node = node->parentNode;
	}
}

void
Scene3DNode_addTransform(Scene3DNode* node, Matrix3D* xform)
{
	Matrix3D m = Matrix3D_multiply(node->transform, *xform);
	Scene3DNode_setTransform(node, &m);
}

void
Scene3DNode_setColorBias(Scene3DNode* node, float bias)
{
	node->colorBias = bias;
	node->needsUpdate = 1;
}

void
Scene3DNode_setRenderStyle(Scene3DNode* node, RenderStyle style)
{
	node->renderStyle = style;
	node->needsUpdate = 1;
}

RenderStyle
Scene3DNode_getRenderStyle(Scene3DNode* node)
{
	while ( node != NULL )
	{
		if ( node->renderStyle != kRenderInheritStyle )
			return node->renderStyle;
		
		node = node->parentNode;
	}
	
	return kRenderFilled;
}

void
Scene3DNode_setVisible(Scene3DNode* node, int visible)
{
	node->isVisible = visible;
	node->needsUpdate = 1;
}

#if ENABLE_Z_BUFFER
void Scene3DNode_setUsesZBuffer(Scene3DNode* node, int flag)
{
	node->useZBuffer = flag;
	node->needsUpdate = 1;
}
#endif

void
Scene3DNode_addShapeWithTransform(Scene3DNode* node, Shape3D* shape, Matrix3D transform)
{
	ShapeInstance* nodeshape = m3d_malloc(sizeof(ShapeInstance));
	int i;
	
	nodeshape->renderStyle = kRenderInheritStyle;
	nodeshape->prototype = Shape3D_retain(shape);
	nodeshape->nPoints = shape->nPoints;
	nodeshape->points = m3d_malloc(sizeof(Point3D) * shape->nPoints);

	// not strictly necessary, since we set these again in Scene3D_updateShapeInstance:
	//for ( i = 0; i < shape->nPoints; ++i )
	//	nodeshape->points[i] = shape->points[i];
	
	nodeshape->nFaces = shape->nFaces;
	nodeshape->faces = m3d_malloc(sizeof(FaceInstance) * shape->nFaces);
	
	for ( i = 0; i < shape->nFaces; ++i )
	{
		// point face vertices at copy's points array
		FaceInstance* face = &nodeshape->faces[i];

		face->p1 = &nodeshape->points[shape->faces[i].p1];
		face->p2 = &nodeshape->points[shape->faces[i].p2];
		face->p3 = &nodeshape->points[shape->faces[i].p3];
		
		if ( shape->faces[i].p4 != 0xffff )
			face->p4 = &nodeshape->points[shape->faces[i].p4];
		else
			face->p4 = NULL;

        face->colorBias = shape->faces[i].colorBias;

		// also not necessary
		face->normal = normal(face->p1, face->p2, face->p3);
	}
	
	nodeshape->transform = transform;
	nodeshape->center = Matrix3D_apply(transform, shape->center);
	nodeshape->colorBias = 0;
	
#if ENABLE_ORDERING_TABLE
	nodeshape->orderTableSize = 0;
	nodeshape->orderTable = NULL;
#endif
	
	nodeshape->next = node->shapes;
	node->shapes = nodeshape;
	++node->shapeCount;
}

void
Scene3DNode_addShapeWithOffset(Scene3DNode* node, Shape3D* shape, Vector3D offset)
{
	Scene3DNode_addShapeWithTransform(node, shape, Matrix3DMakeTranslate(offset.dx, offset.dy, offset.dz));
}

void
Scene3DNode_addShape(Scene3DNode* node, Shape3D* shape)
{
	Scene3DNode_addShapeWithTransform(node, shape, identityMatrix);
}

Scene3DNode*
Scene3DNode_newChild(Scene3DNode* node)
{
	node->childNodes = m3d_realloc(node->childNodes, sizeof(Scene3DNode*) * (node->nChildren + 1));

	Scene3DNode* child = m3d_malloc(sizeof(Scene3DNode));
	Scene3DNode_init(child);

	node->childNodes[node->nChildren++] = child;
	child->parentNode = node;

	return child;
}

static void
Scene3D_updateShapeInstance(Scene3D* scene, ShapeInstance* shape, Matrix3D xform, float colorBias, RenderStyle style)
{
	Shape3D* proto = shape->prototype;
	int i;
	
	// transform points
	
	for ( i = 0; i < shape->nPoints; ++i )
	{
		shape->points[i] = Matrix3D_apply(xform, Matrix3D_apply(shape->transform, proto->points[i]));
	}

	shape->center = Matrix3D_apply(xform, Matrix3D_apply(shape->transform, proto->center));
	shape->colorBias = proto->colorBias + colorBias;
	shape->renderStyle = style;
	shape->inverted = xform.inverting;
	
#if ENABLE_ORDERING_TABLE
	float zmin = 1e23;
	float zmax = 0;
	int ordersize = shape->prototype->orderTableSize;
	
	if ( ordersize != shape->orderTableSize )
	{
		shape->orderTableSize = ordersize;
		shape->orderTable = m3d_realloc(shape->orderTable, ordersize * sizeof(FaceInstance*));
	}
	
	memset(shape->orderTable, 0, ordersize * sizeof(FaceInstance*));
#endif

	// recompute face normals

	for ( i = 0; i < shape->nFaces; ++i )
	{
		FaceInstance* face = &shape->faces[i];
		face->normal = normal(face->p1, face->p2, face->p3);
		
#if ENABLE_ORDERING_TABLE
		if ( ordersize > 0 )
		{
			float z = face->p1->z + face->p2->z + face->p3->z;
			
			if ( z < zmin ) zmin = z;
			if ( z > zmax ) zmax = z;
		}
#endif
	}
	
	// apply perspective, scale to display
	
	for ( i = 0; i < shape->nPoints; ++i )
	{
		Point3D* p = &shape->points[i];
		
		if ( p->z > 0 )
		{
			if ( scene->hasPerspective )
			{
				p->x = scene->scale * (p->x / p->z + 1.6666666 * scene->centerx);
				p->y = scene->scale * (p->y / p->z + scene->centery);
			}
			else
			{
				p->x = scene->scale * (p->x + 1.6666666 * scene->centerx);
				p->y = scene->scale * (p->y + scene->centery);
			}
		}
		
#if ENABLE_Z_BUFFER
		if ( p->z < scene->zmin )
			scene->zmin = p->z;
#endif
	}
	
#if ENABLE_ORDERING_TABLE
	// Put faces in bins separated by average z value
	
	if ( ordersize > 0 )
	{
		float d = zmax - zmin + 0.0001;
		
		for ( i = 0; i < shape->nFaces; ++i )
		{
			FaceInstance* face = &shape->faces[i];
			
			int idx = ordersize * (face->p1->z + face->p2->z + face->p3->z - zmin) / d;
			face->next = shape->orderTable[idx];
			shape->orderTable[idx] = face;
		}
	}
#endif
}

void
Scene3D_updateNode(Scene3D* scene, Scene3DNode* node, Matrix3D xform, float colorBias, RenderStyle style, int update)
{
	if ( !node->isVisible )
		return;
	
	if ( node->needsUpdate )
	{
		update = 1;
		node->needsUpdate = 0;
	}
	
	if ( update )
	{
		xform = Matrix3D_multiply(node->transform, xform);
		colorBias += node->colorBias;
		
		if ( node->renderStyle != kRenderInheritStyle )
			style = node->renderStyle;
		
		ShapeInstance* shape = node->shapes;
		
		while ( shape != NULL )
		{
			Scene3D_updateShapeInstance(scene, shape, xform, colorBias, style);
#if ENABLE_Z_BUFFER
			shape->useZBuffer = node->useZBuffer;
#endif
			shape = shape->next;
		}
		
		int i;
		
		for ( i = 0; i < node->nChildren; ++i )
			Scene3D_updateNode(scene, node->childNodes[i], xform, colorBias, style, update);
	}
}


void
Scene3D_init(Scene3D* scene)
{
	scene->hasPerspective = 1;

	Scene3D_setCamera(scene, (Point3D){ 0, 0, 0 }, (Point3D){ 0, 0, 1 }, 1.0, (Vector3D){ 0, 1, 0 });
	Scene3D_setGlobalLight(scene, (Vector3D){ 0, -1, 0 });
	
	Scene3D_setCenter(scene, 0.5, 0.5);

	Scene3DNode_init(&scene->root);
	
	scene->shapelist = NULL;
	scene->shapelistsize = 0;
}

void
Scene3D_deinit(Scene3D* scene)
{
	if ( scene->shapelist != NULL )
		m3d_free(scene->shapelist);
	
	Scene3DNode_deinit(&scene->root);
}

void
Scene3D_setGlobalLight(Scene3D* scene, Vector3D light)
{
	scene->light = light;
}

void
Scene3D_setCenter(Scene3D* scene, float x, float y)
{
	scene->centerx = x;
	scene->centery = y;
}

Scene3DNode*
Scene3D_getRootNode(Scene3D* scene)
{
	return &scene->root;
}

static int
getShapesAtNode(Scene3D* scene, Scene3DNode* node, int count)
{
	if ( !node->isVisible )
		return count;
	
	ShapeInstance* shape = node->shapes;
	ShapeInstance** shapes = scene->shapelist;
	
	while ( shape != NULL )
	{
		// check if shape is outside camera view: apply camera transform to shape center
		
		if ( count + 1 > scene->shapelistsize )
		{
#define SHAPELIST_INCREMENT 16
			scene->shapelist = m3d_realloc(scene->shapelist, (scene->shapelistsize + SHAPELIST_INCREMENT) * sizeof(ShapeInstance*));
			shapes = scene->shapelist;
			
			scene->shapelistsize += SHAPELIST_INCREMENT;
		}
		
		shapes[count++] = shape;
		shape = shape->next;
	}
	
	int i;
	
	for ( i = 0; i < node->nChildren; ++i )
		count = getShapesAtNode(scene, node->childNodes[i], count);

	return count;
}

#define Z_IS_UP

#ifdef Z_IS_UP
static void swap(float* a, float* b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}
#endif

void
Scene3D_setCamera(Scene3D* scene, Point3D origin, Point3D lookAt, float scale, Vector3D up)
{
	#ifdef Z_IS_UP
		// swap z and y
		Matrix3D camera = Matrix3DMake(
			1, 0, 0,
			0, 0, 1,
			0, 1, 0,
		0);
		swap(&origin.y, &origin.z);
		swap(&lookAt.y, &lookAt.z);
		swap(&up.dy, &up.dz);
	#else
		Matrix3D camera = identityMatrix;
	#endif
	camera.isIdentity = 0;

	camera.dx = -origin.x;
	camera.dy = -origin.y;
	camera.dz = -origin.z;
	
	Vector3D dir = Vector3DMake(lookAt.x - origin.x, lookAt.y - origin.y, lookAt.z - origin.z);
	
	float linv = fisr(Vector3D_lengthSquared(&dir));
	
	dir.dx *= linv;
	dir.dy *= linv;
	dir.dz *= linv;
	
	scene->scale = 240 * scale;

	// first yaw around the y axis
	
	float h = 0;
	
	if ( dir.dx != 0 || dir.dz != 0 )
	{
		linv = fisr(dir.dx * dir.dx + dir.dz * dir.dz);
		
		Matrix3D yaw = Matrix3DMake(dir.dz * linv, 0, -dir.dx * linv, 0, 1, 0, dir.dx * linv, 0, dir.dz * linv, 0);
		camera = Matrix3D_multiply(camera, yaw);
		
		h = 1 / linv;
	}
	
	// then pitch up/down to y elevation
	
	Matrix3D pitch = Matrix3DMake(1, 0, 0, 0, h, -dir.dy, 0, dir.dy, h, 0);
	camera = Matrix3D_multiply(camera, pitch);
	
	// and roll to position the up vector
	
	if ( up.dx != 0 || up.dy != 0 )
	{
		linv = fisr(up.dx * up.dx + up.dy * up.dy);
		Matrix3D roll = Matrix3DMake(up.dy * linv, up.dx * linv, 0, -up.dx * linv, up.dy * linv, 0, 0, 0, 1, 0);
	
		scene->camera = Matrix3D_multiply(camera, roll);
	}
	else
		scene->camera = camera;
	
	#ifdef Z_IS_UP
	{
		Matrix3D _swap = Matrix3DMake(
			1, 0, 0,
			0, 0, 1,
			0, 1, 0,
		0);
		camera = Matrix3D_multiply(camera, _swap);
	}
	#endif
	
	scene->root.needsUpdate = 1;
}

typedef uint8_t Pattern[8];

static Pattern patterns[] =
{
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00 },
	{ 0x88, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00 },
	{ 0x88, 0x00, 0x20, 0x00, 0x88, 0x00, 0x02, 0x00 },
	{ 0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00 },
	{ 0xa8, 0x00, 0x22, 0x00, 0x8a, 0x00, 0x22, 0x00 },
	{ 0xaa, 0x00, 0x22, 0x00, 0xaa, 0x00, 0x22, 0x00 },
	{ 0xaa, 0x00, 0xa2, 0x00, 0xaa, 0x00, 0x2a, 0x00 },
	{ 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00 },
	{ 0xaa, 0x40, 0xaa, 0x00, 0xaa, 0x04, 0xaa, 0x00 },
	{ 0xaa, 0x44, 0xaa, 0x00, 0xaa, 0x44, 0xaa, 0x00 },
	{ 0xaa, 0x44, 0xaa, 0x10, 0xaa, 0x44, 0xaa, 0x01 },
	{ 0xaa, 0x44, 0xaa, 0x11, 0xaa, 0x44, 0xaa, 0x11 },
	{ 0xaa, 0x54, 0xaa, 0x11, 0xaa, 0x45, 0xaa, 0x11 },
	{ 0xaa, 0x55, 0xaa, 0x11, 0xaa, 0x55, 0xaa, 0x11 },
	{ 0xaa, 0x55, 0xaa, 0x51, 0xaa, 0x55, 0xaa, 0x15 },
	{ 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55 },
	{ 0xba, 0x55, 0xaa, 0x55, 0xab, 0x55, 0xaa, 0x55 },
	{ 0xbb, 0x55, 0xaa, 0x55, 0xbb, 0x55, 0xaa, 0x55 },
	{ 0xbb, 0x55, 0xea, 0x55, 0xbb, 0x55, 0xae, 0x55 },
	{ 0xbb, 0x55, 0xee, 0x55, 0xbb, 0x55, 0xee, 0x55 },
	{ 0xfb, 0x55, 0xee, 0x55, 0xbf, 0x55, 0xee, 0x55 },
	{ 0xff, 0x55, 0xee, 0x55, 0xff, 0x55, 0xee, 0x55 },
	{ 0xff, 0x55, 0xfe, 0x55, 0xff, 0x55, 0xef, 0x55 },
	{ 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55 },
	{ 0xff, 0x55, 0xff, 0xd5, 0xff, 0x55, 0xff, 0x5d },
	{ 0xff, 0x55, 0xff, 0xdd, 0xff, 0x55, 0xff, 0xdd },
	{ 0xff, 0x75, 0xff, 0xdd, 0xff, 0x57, 0xff, 0xdd },
	{ 0xff, 0x77, 0xff, 0xdd, 0xff, 0x77, 0xff, 0xdd },
	{ 0xff, 0x77, 0xff, 0xfd, 0xff, 0x77, 0xff, 0xdf },
	{ 0xff, 0x77, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff },
	{ 0xff, 0xf7, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xff },
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
};

static inline void
drawShapeFace(Scene3D* scene, ShapeInstance* shape, uint8_t* bitmap, int rowstride, FaceInstance* face)
{
	
	// If any vertex is behind the camera, skip the whole face
	if ( face->p1->z <= 0 || face->p2->z <= 0 || face->p3->z <= 0
		|| (face->p4 && face->p4->z <= 0))
			return;
	
	float x1 = face->p1->x;
	float y1 = face->p1->y;
	float x2 = face->p2->x;
	float y2 = face->p2->y;
	float x3 = face->p3->x;
	float y3 = face->p3->y;
	
	// quick bounds check
	
	if ( (x1 < 0 && x2 < 0 && x3 < 0 && (face->p4 == NULL || face->p4->x < 0)) ||
		 (x1 >= WIDTH && x2 >= WIDTH && x3 >= WIDTH && (face->p4 == NULL || face->p4->x >= WIDTH)) ||
		 (y1 < 0 && y2 < 0 && y3 < 0 && (face->p4 == NULL || face->p4->y < 0)) ||
		 (y1 >= HEIGHT && y2 >= HEIGHT && y3 >= HEIGHT && (face->p4 == NULL || face->p4->y >= HEIGHT)) )
		return;

	if ( shape->prototype->isClosed )
	{
		// only render front side of faces

		float d;
		
		if ( scene->hasPerspective ) // use winding order
			d = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
		else // use direction of normal
			d = face->normal.dz;
		
		if ( (d >= 0) ^ (shape->inverted ? 1 : 0) )
			return;
	}
	
	// lighting
	
	float c = face->colorBias + shape->colorBias;
	float v;
	
	if ( c <= -1 )
		v = 0;
	else if ( c >= 1 )
		v = 1;
	else
	{
		if ( shape->inverted )
			v = (1.0 + Vector3DDot(face->normal, scene->light)) / 2;
		else
			v = (1.0 - Vector3DDot(face->normal, scene->light)) / 2;

		if ( c > 0 )
			v = c + (1-c) * v; // map [0,1] to [c,1]
		else if ( c < 0 )
			v *= 1 + c; // map [0,1] to [0, 1+c]
	}
	
	// cheap gamma adjust
	// v = v * v;

	int vi = (int)(32.99 * v);

	if ( vi > 32 )
		vi = 32;
	else if ( vi < 0 )
		vi = 0;

	uint8_t* pattern = (uint8_t*)&patterns[vi];

	if ( face->p4 != NULL )
	{
#if ENABLE_Z_BUFFER
		if ( shape->useZBuffer )
			fillQuad_zbuf(bitmap, rowstride, face->p1, face->p2, face->p3, face->p4, pattern);
		else
#endif
			fillQuad(bitmap, rowstride, face->p1, face->p2, face->p3, face->p4, pattern);
	}
	else
	{
#if ENABLE_Z_BUFFER
		if ( shape->useZBuffer )
			fillTriangle_zbuf(bitmap, rowstride, face->p1, face->p2, face->p3, pattern);
		else
#endif
			fillTriangle(bitmap, rowstride, face->p1, face->p2, face->p3, pattern);
	}
}
static inline void
drawFilledShape(Scene3D* scene, ShapeInstance* shape, uint8_t* bitmap, int rowstride)
{
#if ENABLE_ORDERING_TABLE
	if ( shape->orderTableSize > 0 )
	{
		for ( int i = shape->orderTableSize - 1; i >= 0; --i )
		{
			FaceInstance* face = shape->orderTable[i];
			
			while ( face != NULL )
			{
				drawShapeFace(scene, shape, bitmap, rowstride, face);
				face = face->next;
			}
		}
	}
	else
#endif
	for ( int f = 0; f < shape->nFaces; ++f )
		drawShapeFace(scene, shape, bitmap, rowstride, &shape->faces[f]);
}

static inline void
drawWireframe(Scene3D* scene, ShapeInstance* shape, uint8_t* bitmap, int rowstride)
{
	int f;
	RenderStyle style = shape->renderStyle;
	uint8_t* color = patterns[32];

	for ( f = 0; f < shape->nFaces; ++f )
	{
		FaceInstance* face = &shape->faces[f];
		
		// If any vertex is behind the camera, skip it
		
		if ( face->p1->z <= 0 || face->p2->z <= 0 || face->p3->z <= 0 || (face->p4 != NULL && face->p4->x <= 0) )
			continue;
		
		float x1 = face->p1->x;
		float y1 = face->p1->y;
		float x2 = face->p2->x;
		float y2 = face->p2->y;
		float x3 = face->p3->x;
		float y3 = face->p3->y;
		float x4 = (face->p4 != NULL) ? face->p4->x : 0;
		float y4 = (face->p4 != NULL) ? face->p4->y : 0;

		// quick bounds check
		
		if ( (x1 < 0 && x2 < 0 && x3 < 0 && x4 < 0) || (x1 >= WIDTH && x2 >= WIDTH && x3 >= WIDTH && x4 >= WIDTH) ||
			 (y1 < 0 && y2 < 0 && y3 < 0 && x4 < 0) || (y1 >= HEIGHT && y2 >= HEIGHT && y3 >= HEIGHT && y4 >= HEIGHT) )
			continue;

		if ( (style & kRenderWireframeBack) == 0 )
		{
			// only render front side of faces

			float d;
			
			if ( scene->hasPerspective ) // use winding order
				d = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
			else // use direction of normal
				d = face->normal.dz;
			
			if ( (d > 0) ^ (shape->inverted ? 1 : 0) )
				continue;
		}

		// XXX - can avoid 1/2 the drawing if we're doing the entire shape (kRenderWireframeBack is set)
		// and the shape is closed: skip lines where y1 > y2
		
#if ENABLE_Z_BUFFER
		if ( shape->useZBuffer )
		{
			drawLine_zbuf(bitmap, rowstride, face->p1, face->p2, 1, color);
			drawLine_zbuf(bitmap, rowstride, face->p2, face->p3, 1, color);

			if ( face->p4 != NULL )
			{
				drawLine_zbuf(bitmap, rowstride, face->p3, face->p4, 1, color);
				drawLine_zbuf(bitmap, rowstride, face->p4, face->p1, 1, color);
			}
			else
				drawLine_zbuf(bitmap, rowstride, face->p3, face->p1, 1, color);
		}
		else
		{
#endif
			drawLine(bitmap, rowstride, face->p1, face->p2, 1, color);
			drawLine(bitmap, rowstride, face->p2, face->p3, 1, color);
			
			if ( face->p4 != NULL )
			{
				drawLine(bitmap, rowstride, face->p3, face->p4, 1, color);
				drawLine(bitmap, rowstride, face->p4, face->p1, 1, color);
			}
			else
				drawLine(bitmap, rowstride, face->p3, face->p1, 1, color);
#if ENABLE_Z_BUFFER
		}
#endif
	}
}

static int compareZ(const void* a, const void* b)
{
	ShapeInstance* shapea = *(ShapeInstance**)a;
	ShapeInstance* shapeb = *(ShapeInstance**)b;
	
	return shapea->center.z < shapeb->center.z;
}

void
Scene3D_drawNode(Scene3D* scene, Scene3DNode* node, uint8_t* bitmap, int rowstride)
{
	// order shapes by z
	
	int count = getShapesAtNode(scene, node, 0);
	
	// and draw back to front
	
	if ( count > 1 )
		qsort(scene->shapelist, count, sizeof(ShapeInstance*), compareZ);
	
	int i;
	
	for ( i = 0; i < count; ++i )
	{
		ShapeInstance* shape = scene->shapelist[i];
		RenderStyle style = shape->renderStyle;
		
		if ( style & kRenderFilled )
			drawFilledShape(scene, shape, bitmap, rowstride);
		
		if ( style & kRenderWireframe )
			drawWireframe(scene, shape, bitmap, rowstride);
	}
}

void
Scene3D_draw(Scene3D* scene, uint8_t* bitmap, int rowstride)
{
#if ENABLE_Z_BUFFER
	scene->zmin = 1e23;
#endif
	
	Scene3D_updateNode(scene, &scene->root, scene->camera, 0, kRenderFilled, 0);
	
#if ENABLE_Z_BUFFER
	resetZBuffer(4);
#endif
	
	Scene3D_drawNode(scene, &scene->root, bitmap, rowstride);
}