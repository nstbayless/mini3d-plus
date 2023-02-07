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
#include "qsort.h"

#include <pd_api.h>

static inline int face_clips_epsilon(FaceInstance* face)
{
	return (face->p1->z < CLIP_EPSILON || face->p2->z < CLIP_EPSILON || face->p3->z < CLIP_EPSILON || (face->p4 && face->p4->z < CLIP_EPSILON));
}

#if SORT_3D_FACES_BY_Z
static void Scene3D_add_face_to_sortlist(Scene3D* scene, SortedFace sface)
{
	int idx = scene->sortedfacelistc++;
	if (idx >= scene->sortedfacelistsize)
	{
		#define FACELIST_INCREMENT 0x50
		scene->sortedfacelistsize += FACELIST_INCREMENT;
		scene->sortedfacelist = m3d_realloc(scene->sortedfacelist, scene->sortedfacelistsize * sizeof(SortedFace));
	}
	scene->sortedfacelist[idx] = sface;
}
#endif

void
Scene3DNode_init(Scene3DNode* node)
{
	node->transform = identityMatrix;
	node->parentNode = NULL;
	node->childNodes = NULL;
	node->nChildren = 0;
	node->nInstance = 0;
	node->instances = NULL;
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
	InstanceHeader* instance = node->instances;
	
	while ( instance != NULL )
	{
		InstanceHeader* next = instance->next;
		switch(instance->type)
		{
		case kInstanceTypeShape: {
				ShapeInstance* shape = (ShapeInstance*)instance;
				Shape3D_release(shape->prototype);
				m3d_free(shape->points);
				m3d_free(shape->faces);
			}
			break;
		case kInstanceTypeImposter: {
				ImposterInstance* imposter = (ImposterInstance*)instance;
				Imposter3D_release(imposter->prototype);
			}
			break;
		}
		m3d_free(instance);
		instance = next;
	}
	
	for ( int i = 0; i < node->nChildren; ++i )
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
	
	nodeshape->header.type = kInstanceTypeShape;
	nodeshape->renderStyle = kRenderInheritStyle;
	nodeshape->prototype = Shape3D_retain(shape);
	nodeshape->nPoints = shape->nPoints;
	nodeshape->points = m3d_malloc(sizeof(Point3D) * shape->nPoints);

	// not strictly necessary, since we set these again in Scene3D_updateShapeInstance:
	//for ( i = 0; i < shape->nPoints; ++i )
	//	nodeshape->points[i] = shape->points[i];
	
	nodeshape->nFaces = shape->nFaces;
	nodeshape->faces = m3d_malloc(sizeof(FaceInstance) * shape->nFaces);
	
	nodeshape->clipCapacity = 0;
	nodeshape->nClip = 0;
	nodeshape->clip = NULL;
	
	for ( i = 0; i < shape->nFaces; ++i )
	{
		// point face vertices at copy's points array
		FaceInstance* face = &nodeshape->faces[i];
		
		#if ENABLE_TEXTURES
		// and record its index too.
		face->org_face = i;
		#endif

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
		face->isDoubleSided = shape->faces[i].isDoubleSided;
	}
	
	nodeshape->header.transform = transform;
	nodeshape->header.center = Matrix3D_apply(transform, shape->center);
	nodeshape->colorBias = 0;
	
#if ENABLE_ORDERING_TABLE
	nodeshape->orderTableSize = 0;
	nodeshape->orderTable = NULL;
#endif
	
	nodeshape->header.next = node->instances;
	node->instances = &nodeshape->header;
	++node->nInstance;
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

void Scene3DNode_addImposterWithTransform(Scene3DNode* node, Imposter3D* imposter, Matrix3D transform)
{
	ImposterInstance* nodeimp = m3d_malloc(sizeof(ImposterInstance));
	nodeimp->header.type = kInstanceTypeImposter;
	nodeimp->prototype = Imposter3D_retain(imposter);
	nodeimp->header.transform = transform;
	nodeimp->header.center = Matrix3D_apply(transform, imposter->center);
	
	nodeimp->header.next = node->instances;
	node->instances = &nodeimp->header;
	++node->nInstance;
}

void Scene3DNode_addImposter(Scene3DNode* node, Imposter3D* imposter)
{
	Scene3DNode_addImposterWithTransform(node, imposter, identityMatrix);
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

static void applyPerspectiveToPoint(Scene3D* scene, Point3D* p)
{
	if ( scene->hasPerspective )
	{
		if (p->z >= CLIP_EPSILON)
		{
			p->x = scene->scale * (p->x / p->z) + scene->centerx;
			p->y = scene->scale * (p->y / p->z) + scene->centery;
		}
	}
	else
	{
		p->x = scene->scale * (p->x) + scene->centerx;
		p->y = scene->scale * (p->y) + scene->centery;
	}
}

#if FACE_CLIPPING
Scene3D* clipScene; // stored globally for ease of access.
static ClippedFace3D* clipAllocate(ShapeInstance* shape, FaceInstance* face)
{
	int index = shape->nClip++;
	if (shape->nClip > shape->clipCapacity)
	{
		shape->clipCapacity += CLIP_RESIZE;
		shape->clip = m3d_realloc(shape->clip, shape->clipCapacity * sizeof(ClippedFace3D));
	}
	shape->clip[index].p1 = NULL;
	shape->clip[index].src = face;
	#if ENABLE_TEXTURES && ENABLE_TEXTURES_GREYSCALE
	if (shape->prototype->texmap)
	{
		// copy lighting value from source texmap
		shape->clip[index].tex.lighting = shape->prototype->texmap[face->org_face].lighting;
	}
	#endif
	return &shape->clip[index];
}

static float interpolatePointAtZClip(Point3D* out, Point3D* a, Point3D* b)
{
	float p = (CLIP_EPSILON - b->z) / (a->z - b->z);
	out->z = CLIP_EPSILON;
	out->x = p * a->x + (1 - p) * b->x;
	out->y = p * a->y + (1 - p) * b->y;
	return p;
}

#if ENABLE_TEXTURES
static void interpolatePoint2D(Point2D* out, Point2D* a, Point2D* b, float p)
{
	out->x = p * a->x + (1-p) * b->x;
	out->y = p * a->y + (1-p) * b->y;
}
#endif

#if ENABLE_TEXTURES
// cheap trick to check if the face is NULL given only the offset into its point
#define POINT_FT_NOT_NULL(point) ((uintptr_t)(point) > offsetof(FaceTexture, t4))
#endif

// one point in front of camera, two points behind.
static void calculateClipping_straddle12(
	ClippedFace3D* clip, Point3D* a, Point3D* b1, Point3D* b2
#if ENABLE_TEXTURES
	, Point2D* ta, Point2D* tb1, Point2D* tb2
#endif
)
{
	clip->p2 = *a;
	float pab1 = interpolatePointAtZClip(&clip->p3, a, b1);
	float pab2 = interpolatePointAtZClip(&clip->p4, a, b2);
	
	#if ENABLE_TEXTURES
	if (POINT_FT_NOT_NULL(ta))
	{
		clip->tex.t1 = *ta;
		interpolatePoint2D(&clip->tex.t2, ta, tb1, pab1);
		interpolatePoint2D(&clip->tex.t3, ta, tb2, pab2);
		clip->tex.texture_enabled = 1;
	}
	else
	{
		clip->tex.texture_enabled = 0;
	}
	#endif
	
	applyPerspectiveToPoint(clipScene, &clip->p2);
	applyPerspectiveToPoint(clipScene, &clip->p3);
	applyPerspectiveToPoint(clipScene, &clip->p4);
}

// two points in front of camera, one point behind.
static void calculateClipping_straddle21(
	ClippedFace3D* clip, Point3D* a1, Point3D* a2, Point3D* b
#if ENABLE_TEXTURES
	, Point2D* ta1, Point2D* ta2, Point2D* tb
#endif
)
{
	clip->p1 = a1;
	clip->p2 = *a2;
	float pa2b = interpolatePointAtZClip(&clip->p3, a2, b);
	float pa1b = interpolatePointAtZClip(&clip->p4, a1, b);
	
	#if ENABLE_TEXTURES
	if (POINT_FT_NOT_NULL(ta1))
	{
		clip->tex.t4 = *ta1;
		clip->tex.t1 = *ta2;
		interpolatePoint2D(&clip->tex.t2, ta2, tb, pa2b);
		interpolatePoint2D(&clip->tex.t3, ta1, tb, pa1b);
		clip->tex.texture_enabled = 1;
	}
	else
	{
		clip->tex.texture_enabled = 0;
	}
	#endif
	
	applyPerspectiveToPoint(clipScene, &clip->p2);
	applyPerspectiveToPoint(clipScene, &clip->p3);
	applyPerspectiveToPoint(clipScene, &clip->p4);
}

// three points in front of camera, zero behind.
static void calculateClipping_straddle30(
	ClippedFace3D* clip, Point3D* a1, Point3D* a2, Point3D* a3
#if ENABLE_TEXTURES
	, Point2D* ta1, Point2D* ta2, Point2D* ta3
#endif
)
{
	clip->p2 = *a1;
	clip->p3 = *a2;
	clip->p4 = *a3;
	
	#if ENABLE_TEXTURES
	if (POINT_FT_NOT_NULL(ta1))
	{
		clip->tex.t1 = *ta1;
		clip->tex.t2 = *ta2;
		clip->tex.t3 = *ta3;
		clip->tex.texture_enabled = 1;
	}
	else
	{
		clip->tex.texture_enabled = 0;
	}
	#endif
	
	applyPerspectiveToPoint(clipScene, &clip->p2);
	applyPerspectiveToPoint(clipScene, &clip->p3);
	applyPerspectiveToPoint(clipScene, &clip->p4);
}

// two points in front of camera, two points behind.
static void calculateClipping_straddle22(
	ClippedFace3D* clip, Point3D* a1, Point3D* a2, Point3D* b1, Point3D* b2
#if ENABLE_TEXTURES
	, Point2D* ta1, Point2D* ta2, Point2D* tb1, Point2D* tb2
#endif
)
{
	float pa2b1 = interpolatePointAtZClip(&clip->p2, a2, b1);
	float pa1b2 = interpolatePointAtZClip(&clip->p3, a1, b2);
	clip->p4 = *a1;
	clip->p1 = a2;
	
	#if ENABLE_TEXTURES
	if (POINT_FT_NOT_NULL(ta1))
	{
		interpolatePoint2D(&clip->tex.t1, ta2, tb1, pa2b1);
		interpolatePoint2D(&clip->tex.t2, ta1, tb2, pa1b2);
		clip->tex.t3 = *ta1;
		clip->tex.t4 = *ta2;
		clip->tex.texture_enabled = 1;
	}
	else
	{
		clip->tex.texture_enabled = 0;
	}
	#endif
	
	applyPerspectiveToPoint(clipScene, &clip->p2);
	applyPerspectiveToPoint(clipScene, &clip->p3);
	applyPerspectiveToPoint(clipScene, &clip->p4);
}

// three points in front of camera, one point behind.
// interpolate b toward a1
static void calculateClipping_straddle31A1(
	ClippedFace3D* clip, Point3D* a1, Point3D* a2, Point3D* a3, Point3D* b
#if ENABLE_TEXTURES
	, Point2D* ta1, Point2D* ta2, Point2D* ta3, Point2D* tb
#endif
)
{
	clip->p1 = a1;
	clip->p2 = *a2;
	clip->p3 = *a3;
	float pa1b = interpolatePointAtZClip(&clip->p4, a1, b);
	
	#if ENABLE_TEXTURES
	if (POINT_FT_NOT_NULL(ta1))
	{
		clip->tex.t4 = *ta1;
		clip->tex.t1 = *ta2;
		clip->tex.t2 = *ta3;
		interpolatePoint2D(&clip->tex.t3, ta1, tb, pa1b);
		clip->tex.texture_enabled = 1;
	}
	else
	{
		clip->tex.texture_enabled = 0;
	}
	#endif
	
	applyPerspectiveToPoint(clipScene, &clip->p2);
	applyPerspectiveToPoint(clipScene, &clip->p3);
	applyPerspectiveToPoint(clipScene, &clip->p4);
}

// three points in front of camera, one point behind.
// interpolate b toward a3
static void calculateClipping_straddle31A3(
	ClippedFace3D* clip, Point3D* a1, Point3D* a2, Point3D* a3, Point3D* b
#if ENABLE_TEXTURES
	, Point2D* ta1, Point2D* ta2, Point2D* ta3, Point2D* tb
#endif
)
{
	clip->p1 = a1;
	clip->p2 = *a2;
	clip->p3 = *a3;
	float pa3b = interpolatePointAtZClip(&clip->p4, a3, b);
	
	#if ENABLE_TEXTURES
	if (POINT_FT_NOT_NULL(ta1))
	{
		clip->tex.t4 = *ta1;
		clip->tex.t1 = *ta2;
		clip->tex.t2 = *ta3;
		interpolatePoint2D(&clip->tex.t3, ta3, tb, pa3b);
		clip->tex.texture_enabled = 1;
	}
	else
	{
		clip->tex.texture_enabled = 0;
	}
	#endif
	
	applyPerspectiveToPoint(clipScene, &clip->p2);
	applyPerspectiveToPoint(clipScene, &clip->p3);
	applyPerspectiveToPoint(clipScene, &clip->p4);
}

#if !ENABLE_TEXTURES
	#define CLIP3(AB, A, B, C) \
	{ \
		calculateClipping_straddle##AB( \
			clipAllocate(shape, face), \
			p##A, p##B, p##C \
		); \
	}
	#define CLIP4(AB, A, B, C, D) \
	{ \
		calculateClipping_straddle##AB( \
			clipAllocate(shape, face), \
			p##A, p##B, p##C, p##D \
		); \
	}
#else
	#define CLIP3(AB, A, B, C) \
	{ \
		if (shape->prototype->texmap && shape->prototype->texmap[face->org_face].texture_enabled) \
		{ \
			FaceTexture* ft = &shape->prototype->texmap[face->org_face]; \
			calculateClipping_straddle##AB( \
				clipAllocate(shape, face), \
				p##A, p##B, p##C, &ft->t##A, &ft->t##B, &ft->t##C \
			); \
		}\
		else \
		{\
			calculateClipping_straddle##AB( \
				clipAllocate(shape, face), \
				p##A, p##B, p##C, NULL, NULL, NULL \
			); \
		}\
	}
	#define CLIP4(AB, A, B, C, D) \
	{ \
		if (shape->prototype->texmap && shape->prototype->texmap[face->org_face].texture_enabled) \
		{ \
			FaceTexture* ft = &shape->prototype->texmap[face->org_face]; \
			calculateClipping_straddle##AB( \
				clipAllocate(shape, face), \
				p##A, p##B, p##C, p##D, &ft->t##A, &ft->t##B, &ft->t##C, &ft->t##D \
			); \
		}\
		else \
		{\
			calculateClipping_straddle##AB( \
				clipAllocate(shape, face), \
				p##A, p##B, p##C, p##D, NULL, NULL, NULL, NULL \
			); \
		}\
	}
#endif

static void calculateClipping_straddleDispatch(ShapeInstance* shape, FaceInstance* face)
{
	Point3D* p1 = face->p1;
	Point3D* p2 = face->p2;
	Point3D* p3 = face->p3;
	
	uint8_t q1 = p1->z >= CLIP_EPSILON;
	uint8_t q2 = p2->z >= CLIP_EPSILON;
	uint8_t q3 = p3->z >= CLIP_EPSILON;
	
	if (face->p4)
	{
		Point3D* p4 = face->p4;
		uint8_t q4 = p4->z >= CLIP_EPSILON;
		if (q4 && q3 && q2 && q1) return;
		else if (!q4 && !q3 && !q2 && !q1) return;
		else if (q1 && !q2 && !q3 && !q4)
		{
			CLIP3(12, 1, 2, 4);
		}
		else if (!q1 && q2 && !q3 && !q4)
		{
			CLIP3(12, 2, 3, 1);
		}
		else if (!q1 && !q2 && q3 && !q4)
		{
			CLIP3(12, 3, 4, 2);
		}
		else if (!q1 && !q2 && !q3 && q4)
		{
			CLIP3(12, 4, 1, 3);
		}
		else if (q1 && q2 && !q3 && !q4)
		{
			CLIP4(22, 1, 2, 3, 4);
		}
		else if (!q1 && q2 && q3 && !q4)
		{
			CLIP4(22, 2, 3, 4, 1);
		}
		else if (!q1 && !q2 && q3 && q4)
		{
			CLIP4(22, 3, 4, 1, 2);
		}
		else if (q1 && !q2 && !q3 && q4)
		{
			CLIP4(22, 4, 1, 2, 3);
		}
		else if (!q1 && q2 && q3 && q4)
		{
			if (p2->z > p4-> z)
			{
				CLIP4(31A1, 2, 3, 4, 1);
				CLIP3(21, 3, 4, 1);
			}
			else
			{
				CLIP4(31A3, 2, 3, 4, 1);
				CLIP3(21, 2, 3, 1);
			}
		}
		else if (q1 && !q2 && q3 && q4)
		{
			CLIP3(30, 3, 4, 1);
			CLIP3(21, 3, 1, 2);
		}
		else if (q1 && q2 && !q3 && q4)
		{
			if (p4->z > p2-> z)
			{
				CLIP4(31A1, 4, 1, 2, 3);
				CLIP3(21, 1, 2, 3);
			}
			else
			{
				CLIP4(31A3, 4, 1, 2, 3);
				CLIP3(21, 4, 1, 3);
			}
		}
		else if (q1 && q2 && q3 && !q4)
		{
			CLIP3(30, 1, 2, 3);
			CLIP3(21, 1, 3, 4);
		}
	}
	else
	{
		if (q1 && q2 && q3) return;
		else if (!q1 && !q2 && !q3) return;
		else if (q1 && !q2 && !q3)
		{
			CLIP3(12, 1, 2, 3);
		}
		else if (!q1 && q2 && !q3)
		{
			CLIP3(12, 2, 3, 1);
		}
		else if (!q1 && !q2 && q3)
		{
			CLIP3(12, 3, 1, 2);
		}
		else if (q1 && q2 && !q3)
		{
			CLIP3(21, 1, 2, 3);
		}
		else if (q1 && !q2 && q3)
		{
			CLIP3(21, 3, 1, 2);
		}
		else if (!q1 && q2 && q3)
		{
			CLIP3(21, 2, 3, 1);
		}
	}
}

// adds clipped faces for those faces which straddle Z=0
static void calculateClipping(ShapeInstance* shape)
{
	// reset clip buffer
	shape->nClip = 0;
	
	// determine which faces need clipping
	for (int i = 0; i < shape->nFaces; ++i)
	{
		FaceInstance* face = &shape->faces[i];
		calculateClipping_straddleDispatch(shape, face);
		
		if (shape->nClip >= MAXCLIP_CAPACITY - 2)
		{
			return;
		}
	}
	
	// free clip buffer if unused
	if (shape->nClip == 0 && shape->clipCapacity > 0)
	{
		shape->clipCapacity = 0;
		m3d_free(shape->clip);
		shape->clip = NULL;
	}
}
#endif

static void
Scene3D_updateShapeInstance(Scene3D* scene, ShapeInstance* shape, Matrix3D xform, float colorBias, RenderStyle style)
{
	Shape3D* proto = shape->prototype;
	int i;
	
	// transform points
	
	for ( i = 0; i < shape->nPoints; ++i )
	{
		shape->points[i] = Matrix3D_apply(xform, Matrix3D_apply(shape->header.transform, proto->points[i]));
	}

	shape->header.center = Matrix3D_apply(xform, Matrix3D_apply(shape->header.transform, proto->center));
	shape->colorBias = proto->colorBias + colorBias;
	shape->renderStyle = style;
	shape->inverted = xform.inverting;
	
#if ENABLE_ORDERING_TABLE
	float zmin = 1e23;
	float zmax = 0;
	size_t ordersize = shape->prototype->orderTableSize;
	
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
		face->isDoubleSided = shape->faces[i].isDoubleSided;
		
#if ENABLE_ORDERING_TABLE
		if ( ordersize > 0 )
		{
			float z = face->p1->z + face->p2->z + face->p3->z;
			
			if ( z < zmin ) zmin = z;
			if ( z > zmax ) zmax = z;
		}
#endif
	}
	
	#if FACE_CLIPPING
	clipScene = scene;
	calculateClipping(shape);
	#endif
	
	// apply perspective, scale to display
	
	for ( i = 0; i < shape->nPoints; ++i )
	{
		Point3D* p = &shape->points[i];
		applyPerspectiveToPoint(scene, p);
		
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
			
			// note the conversion float -> size_t
			size_t idx = (size_t)(ordersize * (face->p1->z + face->p2->z + face->p3->z - zmin) / d);
			
			face->next = shape->orderTable[idx];
			shape->orderTable[idx] = face;
		}
	}
#endif

#if SORT_3D_FACES_BY_Z
	// add non-clipped faces to the face sort list.
	for ( int i = 0; i < shape->nFaces; ++i )
	{
		FaceInstance* face = &shape->faces[i];
		// skip if face goes behind the camera at all.
		if (face_clips_epsilon(face))
		{
			continue;
		}
		
		float zcomp = MAX(MAX(face->p1->z, face->p2->z), face->p3->z);
		if (face->p4) zcomp = MAX(zcomp, face->p4->z);
		
		// add this face to the list.
		SortedFace sf = {
			.comparison = zcomp,
			.instance = &shape->header,
			.face = (uint32_t)i
		};
		Scene3D_add_face_to_sortlist(scene, sf);
	}
	
	// add clipped faces to the face sort list
	for (int i = 0; i < shape->nClip; ++i)
	{
		ClippedFace3D* face = &shape->clip[i];
		float zcomp = MAX(MAX(face->p2.z, face->p3.z), face->p4.z);
		if (face->p1) zcomp = MAX(zcomp, face->p1->z);
		
		SortedFace sf = {
			.comparison = zcomp,
			.instance = &shape->header,
			.face = 0x80000000 | (uint32_t)i
		};
		Scene3D_add_face_to_sortlist(scene, sf);
	}
#endif
}

static void
Scene3D_updateImposterInstance(Scene3D* scene, ImposterInstance* imposter, Matrix3D xform)
{
	Imposter3D* proto = imposter->prototype;
	imposter->header.center = Matrix3D_apply(xform, Matrix3D_apply(imposter->header.transform, proto->center));
	
	if (imposter->header.center.z < CLIP_EPSILON) return;
	
	#if SORT_3D_FACES_BY_Z
	SortedFace sf = {
		.comparison = imposter->header.center.z,
		.instance = &imposter->header,
	};
	Scene3D_add_face_to_sortlist(scene, sf);
	#endif
	
	imposter->tl = imposter->header.center;
	imposter->br = imposter->header.center;
	imposter->tl.x += proto->x1;
	imposter->tl.y += proto->y1;
	imposter->br.x += proto->x2;
	imposter->br.y += proto->y2;
	applyPerspectiveToPoint(scene, &imposter->tl);
	applyPerspectiveToPoint(scene, &imposter->br);
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
		
		// update instances
		InstanceHeader* instance = node->instances;
		
		while ( instance != NULL )
		{
			switch(instance->type)
			{
			case kInstanceTypeShape:
				Scene3D_updateShapeInstance(scene, (ShapeInstance*)instance, xform, colorBias, style);
				break;
			case kInstanceTypeImposter:
				Scene3D_updateImposterInstance(scene, (ImposterInstance*)instance, xform);
				break;
			}
#if ENABLE_Z_BUFFER
			instance->useZBuffer = node->useZBuffer;
#endif
			instance = instance->next;
		}
		
		// update children
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
	
	#if SORT_3D_INSTANCES_BY_Z
	scene->instancelist = NULL;
	scene->instancelistsize = 0;
	#endif
	
	#if SORT_3D_FACES_BY_Z
	scene->sortedfacelist = NULL;
	scene->sortedfacelistc = scene->sortedfacelistsize = 0;
	#endif
}

void
Scene3D_deinit(Scene3D* scene)
{
	#if SORT_3D_INSTANCES_BY_Z
	if ( scene->instancelist != NULL )
		m3d_free(scene->instancelist);
	#endif
	
	#if SORT_3D_FACES_BY_Z
	if ( scene->sortedfacelist != NULL )
		m3d_free(scene->sortedfacelist);
	#endif
	
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
	scene->centerx = x * scene->scale * (float)VIEWPORT_WIDTH / (float)VIEWPORT_HEIGHT + VIEWPORT_LEFT;
	scene->centery = y * scene->scale + VIEWPORT_TOP;
}

Scene3DNode*
Scene3D_getRootNode(Scene3D* scene)
{
	return &scene->root;
}

#if SORT_3D_INSTANCES_BY_Z
static int
getInstancesAtNode(Scene3D* scene, Scene3DNode* node, int count)
{
	if ( !node->isVisible )
		return count;
	
	InstanceHeader* instance = node->instances;
	InstanceHeader** instances = scene->instancelist;
	
	while ( instance != NULL )
	{
		// check if shape is outside camera view: apply camera transform to shape center
		
		if ( count + 1 > scene->instancelistsize )
		{
			#define SHAPELIST_INCREMENT 0x20
			scene->instancelistsize += SHAPELIST_INCREMENT;
			scene->instancelist = m3d_realloc(scene->instancelist, scene->instancelistsize * sizeof(ShapeInstance*));
			instances = scene->instancelist;
		}
		
		instances[count++] = instance;
		instance = instance->next;
	}
	
	for ( int i = 0; i < node->nChildren; ++i )
		count = getInstancesAtNode(scene, node->childNodes[i], count);

	return count;
}
#endif

#define Z_IS_UP

#ifdef Z_IS_UP
static void swap(float* a, float* b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}
#endif

// Good reading: https://www.3dgep.com/understanding-the-view-matrix/#Look_At_Camera
// (Elegant use of the "up" ak "zenith" vector in particular.)
void
Scene3D_setCamera(Scene3D* scene, Point3D origin, Point3D lookAt, float scale, Vector3D zenith)
{
	// normalize zenith
	if (zenith.dx == 0 && zenith.dy == 0 && zenith.dz == 0)
	{
		zenith.dz = 1;
	}
	
	// flips zenith to account for screen coordinates flipped (y is -)
	zenith.dx = -zenith.dx;
	zenith.dy = -zenith.dy;
	zenith.dz = -zenith.dz;
	
	Vector3D dTarget = Point3D_difference(&origin, &lookAt);
	dTarget = Vector3D_normalize(dTarget);
	Vector3D dRight = Vector3DCross(dTarget, zenith);
	dRight = Vector3D_normalize(dRight); // FIXME: what to do if dTarget colinear with zenith?
	Vector3D dSup = Vector3DCross(dRight, dTarget);// overhead pitch ("Superior")
	
	// mathematically speaking, dSup should already be normal, since it is the cross of two orthonormal vectors.
	// However, due to rounding errors, it might not quite be, so we could normalize again here.
	// dSup = Vector3D_normalize(dSup);
	
	// note that for orthonormal matrix, transpose is the same as invert.
	#define TRANSPOSE( m11, m12, m13, m21, m22, m23, m31, m32, m33 ) \
		m11, m21, m31, \
		m12, m22, m32, \
		m13, m23, m33
		
	
	Matrix3D orient = Matrix3DMake(
		TRANSPOSE(
			dRight.dx, dSup.dx, dTarget.dx,
			dRight.dy, dSup.dy, dTarget.dy,
			dRight.dz, dSup.dz, dTarget.dz
		),
		0
	);
	
	Matrix3D translate = Matrix3DMakeTranslate(
		-origin.x, -origin.y, -origin.z
	);
	
	// XXX corrects a bug in the Matrix3D_multiply function
	translate.isIdentity = 0;
	
	scene->camera = Matrix3D_multiply(translate, orient);
	scene->scale = VIEWPORT_HEIGHT * scale;
	
	scene->root.needsUpdate = 1;
}

// assumption: caller has already verified that no vertex falls below CLIP_EPSILON in z.
static FORCEINLINE inline void
drawShapeFace(Scene3D* scene, ShapeInstance* shape, FaceInstance* face, uint8_t* bitmap, int rowstride
	#if ENABLE_TEXTURES
	, FaceTexture* ft
	#else
	, void* UNUSED
	#endif
)
{
	float x1 = face->p1->x;
	float y1 = face->p1->y;
	float x2 = face->p2->x;
	float y2 = face->p2->y;
	float x3 = face->p3->x;
	float y3 = face->p3->y;
	
	// quick bounds check
	
	if ( (x1 < VIEWPORT_LEFT && x2 < VIEWPORT_LEFT && x3 < VIEWPORT_LEFT && (face->p4 == NULL || face->p4->x < VIEWPORT_LEFT)) ||
		 (x1 >= VIEWPORT_RIGHT && x2 >= VIEWPORT_RIGHT && x3 >= VIEWPORT_RIGHT && (face->p4 == NULL || face->p4->x >= VIEWPORT_RIGHT)) ||
		 (y1 < VIEWPORT_TOP && y2 < VIEWPORT_TOP && y3 < VIEWPORT_TOP && (face->p4 == NULL || face->p4->y < VIEWPORT_TOP)) ||
		 (y1 >= VIEWPORT_BOTTOM && y2 >= VIEWPORT_BOTTOM && y3 >= VIEWPORT_BOTTOM && (face->p4 == NULL || face->p4->y >= VIEWPORT_BOTTOM)) )
		return;

	if ( shape->prototype->isClosed && !face->isDoubleSided )
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
			v = (1.0f + Vector3DDot(face->normal, scene->light)) / 2;
		else
			v = (1.0f - Vector3DDot(face->normal, scene->light)) / 2;

		if ( c > 0 )
			v = c + (1-c) * v; // map [0,1] to [c,1]
		else if ( c < 0 )
			v *= 1 + c; // map [0,1] to [0, 1+c]
	}
	
	// cheap gamma adjust
	// v = v * v;

	int vi = (int)((LIGHTING_PATTERN_COUNT - 0.01f) * v);

	if ( vi > (LIGHTING_PATTERN_COUNT - 1) )
		vi = LIGHTING_PATTERN_COUNT - 1;
	else if ( vi < 0 )
		vi = 0;

	uint8_t* pattern = (uint8_t*)&
	#if ENABLE_CUSTOM_PATTERNS
	(*shape->prototype->pattern)
	#else
	patterns
	#endif
	[vi];
	
	#if ENABLE_TEXTURES
	if (!ft && shape->prototype->texmap && shape->prototype->texture)
	{
		ft = &shape->prototype->texmap[face->org_face];
	}
	#endif

	if ( face->p4 != NULL )
	{
#if ENABLE_Z_BUFFER
		if ( shape->header.useZBuffer )
		{
			#if ENABLE_TEXTURES
			if ( ft && ft->texture_enabled && shape->prototype->texture )
			{
				fillQuad_zt(bitmap, rowstride, face->p1, face->p2, face->p3, face->p4,
					shape->prototype->texture, ft->t1, ft->t2, ft->t3, ft->t4
					#if ENABLE_CUSTOM_PATTERNS
					, shape->prototype->pattern
					#endif
					#if ENABLE_POLYGON_SCANLINING
					, &shape->prototype->scanline
					#endif
					#if ENABLE_TEXTURES_GREYSCALE
					, v, ft->lighting
					#endif
					#if TEXTURE_PERSPECTIVE_MAPPING
					, 1
					#endif
				);
			}
			else
			#endif
			fillQuad_zbuf(bitmap, rowstride, face->p1, face->p2, face->p3, face->p4, pattern);
		}
		else
#endif
		{
			#if ENABLE_TEXTURES
			if ( ft && ft->texture_enabled && shape->prototype->texture )
			{
				fillQuad_t(bitmap, rowstride, face->p1, face->p2, face->p3, face->p4,
					shape->prototype->texture, ft->t1, ft->t2, ft->t3, ft->t4
					#if ENABLE_CUSTOM_PATTERNS
					, shape->prototype->pattern
					#endif
					#if ENABLE_POLYGON_SCANLINING
					, &shape->prototype->scanline
					#endif
					#if ENABLE_TEXTURES_GREYSCALE
					, v, ft->lighting
					#endif
					#if TEXTURE_PERSPECTIVE_MAPPING
					, 1
					#endif
				);
			}
			else
			#endif
			fillQuad(bitmap, rowstride, face->p1, face->p2, face->p3, face->p4, pattern);
		}
	}
	else
	{
#if ENABLE_Z_BUFFER
		if ( shape->header.useZBuffer )
		{
			#if ENABLE_TEXTURES
			if ( ft && ft->texture_enabled && shape->prototype->texture )
			{
				fillTriangle_zt(bitmap, rowstride, face->p1, face->p2, face->p3,
					shape->prototype->texture, ft->t1, ft->t2, ft->t3
					#if ENABLE_CUSTOM_PATTERNS
					, shape->prototype->pattern
					#endif
					#if ENABLE_POLYGON_SCANLINING
					, &shape->prototype->scanline
					#endif
					#if ENABLE_TEXTURES_GREYSCALE
					, v, ft->lighting
					#endif
					#if TEXTURE_PERSPECTIVE_MAPPING
					, 1
					#endif
				);
			}
			else
			#endif
			fillTriangle_zbuf(bitmap, rowstride, face->p1, face->p2, face->p3, pattern);
		}
		else
#endif
		{
			#if ENABLE_TEXTURES
			if ( ft && ft->texture_enabled && shape->prototype->texture )
			{
				fillTriangle_t(bitmap, rowstride, face->p1, face->p2, face->p3,
					shape->prototype->texture, ft->t1, ft->t2, ft->t3
					#if ENABLE_CUSTOM_PATTERNS
					, shape->prototype->pattern
					#endif
					#if ENABLE_POLYGON_SCANLINING
					, &shape->prototype->scanline
					#endif
					#if ENABLE_TEXTURES_GREYSCALE
					, v, ft->lighting
					#endif
					#if TEXTURE_PERSPECTIVE_MAPPING
					, 1
					#endif
				);
			}
			else
			#endif
			fillTriangle(bitmap, rowstride, face->p1, face->p2, face->p3, pattern);
		}
	}
}

#if FACE_CLIPPING
static inline void drawClippedFace(Scene3D* scene, ShapeInstance* shape, ClippedFace3D* clip, uint8_t* bitmap, int rowstride)
{
	// create a fictitious face instance for each clipped face, and render that.
	FaceInstance f;
	memcpy(&f, clip->src, sizeof(FaceInstance));
	f.p1 = &clip->p2;
	f.p2 = &clip->p3;
	f.p3 = &clip->p4;
	f.p4 = clip->p1;
	
	drawShapeFace(scene, shape, &f, bitmap, rowstride,
	#if ENABLE_TEXTURES
		&clip->tex
	#else
		NULL
	#endif
	);
}
#endif

static inline void
drawFilledShape(Scene3D* scene, ShapeInstance* shape, uint8_t* bitmap, int rowstride)
{
#if ENABLE_ORDERING_TABLE
	if ( shape->orderTableSize > 0 )
	{
		for ( size_t i = shape->orderTableSize; i --> 0; )
		{
			FaceInstance* face = shape->orderTable[i];
			
			while ( face != NULL )
			{
				if (!face_clips_epsilon(face))
				{
					drawShapeFace(scene, shape, face, bitmap, rowstride, NULL);
				}
				face = face->next;
			}
		}
	}
	else
#endif
	for ( int f = 0; f < shape->nFaces; ++f )
	{
		if (!face_clips_epsilon(&shape->faces[f]))
		{
			drawShapeFace(scene, shape, &shape->faces[f], bitmap, rowstride, NULL);
		}
	}
		
	// XXX (ORDERING_TABLE only)
	// NOTE (ORDERING_TABLE): we are incorrectly assuming that culled faces are ALWAYS closest to the camera.
	
	#if FACE_CLIPPING
	for ( int i = 0; i < shape->nClip; ++i)
	{
		ClippedFace3D* clip = &shape->clip[i];
		
		drawClippedFace(scene, shape, clip, bitmap, rowstride);
	}
	#endif
}

// assumption: callee has already ensured that no vertices of this face are behind the camera.
static FORCEINLINE inline void
drawWireframeFace(Scene3D* scene, ShapeInstance* shape, FaceInstance* face, uint8_t* bitmap, int rowstride)
{
	RenderStyle style = shape->renderStyle;
	uint8_t* color = patterns[32];
	
	float x1 = face->p1->x;
	float y1 = face->p1->y;
	float x2 = face->p2->x;
	float y2 = face->p2->y;
	float x3 = face->p3->x;
	float y3 = face->p3->y;
	float x4 = (face->p4 != NULL) ? face->p4->x : 0;
	float y4 = (face->p4 != NULL) ? face->p4->y : 0;

	// quick bounds check
	
	if ( (x1 < VIEWPORT_LEFT && x2 < VIEWPORT_LEFT && x3 < VIEWPORT_LEFT && x4 < VIEWPORT_LEFT)
		|| (x1 >= VIEWPORT_RIGHT && x2 >= VIEWPORT_RIGHT && x3 >= VIEWPORT_RIGHT && x4 >= VIEWPORT_RIGHT)
		|| (y1 < VIEWPORT_TOP && y2 < VIEWPORT_TOP && y3 < VIEWPORT_TOP && y4 < VIEWPORT_TOP)
		|| (y1 >= VIEWPORT_BOTTOM && y2 >= VIEWPORT_BOTTOM && y3 >= VIEWPORT_BOTTOM && y4 >= VIEWPORT_BOTTOM) )
		return;

	if ( (style & kRenderWireframeBack) == 0 )
	{
		// only render front side of faces

		float d;
		
		if ( scene->hasPerspective ) // use winding order
			d = (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
		else // use direction of normal
			d = face->normal.dz;
		
		if ( (d > 0) ^ (shape->inverted ? 1 : 0) )
			return;
	}

	// XXX - can avoid 1/2 the drawing if we're doing the entire shape (kRenderWireframeBack is set)
	// and the shape is closed: skip lines where y1 > y2
	
	// TODO: render clipped lines also.
		
#if ENABLE_Z_BUFFER
	if ( shape->header.useZBuffer )
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

static inline void
drawWireframe(Scene3D* scene, ShapeInstance* shape, uint8_t* bitmap, int rowstride)
{
	for ( int f = 0; f < shape->nFaces; ++f )
	{
		FaceInstance* face = &shape->faces[f];
		
		// If any vertex is behind the camera, skip it
		
		if ( face->p1->z <= 0 || face->p2->z <= 0 || face->p3->z <= 0 || (face->p4 != NULL && face->p4->x <= 0) )
			continue;
		
		drawWireframeFace(scene, shape, face, bitmap, rowstride);
	}
}

static void
drawImposter(Scene3D* scene, ImposterInstance* imposter, uint8_t* bitmap, int rowstride)
{
	#if ENABLE_CUSTOM_PATTERNS
	PatternTable* patterns = imposter->prototype->pattern;
	uint8_t* pattern = (uint8_t*)&(*patterns)
	#else
	uint8_t* pattern = patterns
	#endif
	[25];
	
	if (imposter->header.center.z < CLIP_EPSILON)
	{
		return;
	}
	
	Point3D tl = imposter->tl;
	Point3D br = imposter->br;
	Point3D tr = tl;
	tr.x = br.x;
	Point3D bl = br;
	bl.x = tl.x;
	
	// FIXME: move this logic to imposter_update() instead.
	tl.z += imposter->prototype->z1;
	tr.z += imposter->prototype->z2;
	br.z += imposter->prototype->z3;
	bl.z += imposter->prototype->z4;
	
	#if ENABLE_TEXTURES
	Point2D t1, t2, t3, t4;
	t1.x = 0; t1.y = 0;
	t2.x = 1; t2.y = 0;
	t3.x = 1; t3.y = 1;
	t4.x = 0; t4.y = 1;
	#endif
	
	#if ENABLE_Z_BUFFER
	if ( imposter->header.useZBuffer )
	{
		#if ENABLE_TEXTURES
		if (imposter->prototype->bitmap)
			fillQuad_zt(bitmap, rowstride, &tl, &tr, &br, &bl, imposter->prototype->bitmap, t1, t2, t3, t4
			#if ENABLE_CUSTOM_PATTERNS
			, patterns
			#endif
			#if ENABLE_POLYGON_SCANLINING
			, &imposter->prototype->scanline
			#endif
			#if ENABLE_TEXTURES_GREYSCALE
			// TODO: lighting on imposters
			, 0, 0
			#endif
			#if TEXTURE_PERSPECTIVE_MAPPING
			, 0
			#endif
			);
		else
		#endif
			fillQuad_zbuf(bitmap, rowstride, &tl, &tr, &br, &bl, pattern);
	}
	else
	#endif
	{
		
		#if ENABLE_TEXTURES
		if (imposter->prototype->bitmap)
			fillQuad_t(bitmap, rowstride, &tl, &tr, &br, &bl, imposter->prototype->bitmap, t1, t2, t3, t4
			#if ENABLE_CUSTOM_PATTERNS
			, patterns
			#endif
			#if ENABLE_POLYGON_SCANLINING
			, &imposter->prototype->scanline
			#endif
			#if ENABLE_TEXTURES_GREYSCALE
			// TODO: lighting on imposters
			, 0, 0
			#endif
			#if TEXTURE_PERSPECTIVE_MAPPING
			, 0
			#endif
			);
		else
		#endif
		fillQuad(bitmap, rowstride, &tl, &tr, &br, &bl, pattern);
	}
}

static void drawInstance(Scene3D* scene, InstanceHeader* instance, uint8_t* bitmap, int rowstride)
{
	switch (instance->type)
	{
	case kInstanceTypeShape: {
			ShapeInstance* shape = (ShapeInstance*)instance;
			RenderStyle style = shape->renderStyle;
			
			if ( style & kRenderFilled )
				drawFilledShape(scene, shape, bitmap, rowstride);
			
			if ( style & kRenderWireframe )
				drawWireframe(scene, shape, bitmap, rowstride);
		}
		break;
	case kInstanceTypeImposter: {
			ImposterInstance* imposter = (ImposterInstance*)instance;
			drawImposter(scene, imposter, bitmap, rowstride);
		}
		break;
	}
}

static int compareZ(const void* a, const void* b)
{
	InstanceHeader* shapea = *(InstanceHeader**)a;
	InstanceHeader* shapeb = *(InstanceHeader**)b;
	
	return shapea->center.z < shapeb->center.z;
}

void
Scene3D_drawNode(Scene3D* scene, Scene3DNode* node, uint8_t* bitmap, int rowstride)
{
	#if SORT_3D_INSTANCES_BY_Z
	// recursively get all descendent instances of this node.
	// order shapes by z
	int count = getInstancesAtNode(scene, node, 0);
	
	// and draw back to front
	if ( count > 1 )
	{
		InstanceHeader* tmp;
		
		#define LESS(a, b) scene->instancelist[a]->center.z < scene->instancelist[b]->center.z
		#define SWAP(a, b) tmp = scene->instancelist[a], scene->instancelist[a] = scene->instancelist[b], scene->instancelist[b] = tmp
		
		QSORT(count, LESS, SWAP);
		
		#undef LESS
		#undef SWAP
	}
	
	for ( int i = 0; i < count; ++i )
	{
		drawInstance(scene, scene->instancelist[i], bitmap, rowstride);
	}
	#else
	// we don't really care about the draw order now.
	
	// draw children nodes recursively
	for ( int i = 0; i < node->nChildren; ++i )
	{
		Scene3D_drawNode(scene, node->childNodes[i], bitmap, rowstride);
	}
	
	// draw instances in this node
	for ( InstanceHeader* instance = node->instances; instance; instance = instance->next )
	{
		drawInstance(scene, instance, bitmap, rowstride);
	}
	#endif
}

#if SORT_3D_FACES_BY_Z
static int compareZFace(const void* _a, const void* _b)
{
	SortedFace* a = (SortedFace*)_a;
	SortedFace* b = (SortedFace*)_b;
	
	return a->comparison < b->comparison;
}

static void Scene3D_drawSortedFaces(Scene3D* scene, uint8_t* bitmap, int rowstride)
{
	if (scene->sortedfacelistc > 1)
	{
		SortedFace tmp;
		#define LESS(a, b) scene->sortedfacelist[a].comparison > scene->sortedfacelist[b].comparison
		#define SWAP(a, b) tmp = scene->sortedfacelist[a], scene->sortedfacelist[a] = scene->sortedfacelist[b], scene->sortedfacelist[b] = tmp
		QSORT(scene->sortedfacelistc, LESS, SWAP);
		#undef LESS
		#undef SWAP
		
		#if 0
		pd->system->logToConsole("Faces sorted: %d\n", scene->sortedfacelistc);
		for (int i = 0; i < scene->sortedfacelistc; ++i)
		{
			SortedFace* sf = &scene->sortedfacelist[i];
			pd->system->logToConsole(" t%d.%x:%d z%.1f %x", sf->instance->type, sf->face & ~0x80000000, !!(sf->face & 0x80000000), sf->comparison, rand());
		}
		#endif
	}
	
	// By construction, we know that everything in this list does not fall below CLIP_EPSILON in z,
	// so it is safe to call draw*Face functions directly on each face.
	for ( int i = 0; i < scene->sortedfacelistc; ++i )
	{
		SortedFace* face = &scene->sortedfacelist[i];
		if (face->instance->type == kInstanceTypeShape)
		{
			ShapeInstance* shape = (ShapeInstance*)(void*)face->instance;
			
			RenderStyle style = shape->renderStyle;
			
			if (face->face & 0x80000000)
			{
				// clipped face
				int fidx = face->face & ~0x80000000;
				
				if ( style & kRenderFilled )
					drawClippedFace(scene, shape, &shape->clip[fidx], bitmap, rowstride);
			}
			else
			{
				int fidx = face->face;
				
				if ( style & kRenderFilled )
					drawShapeFace(scene, shape, &shape->faces[fidx], bitmap, rowstride, NULL);
				
				if ( style & kRenderWireframe )
					drawWireframeFace(scene, shape, &shape->faces[fidx], bitmap, rowstride);
			}
		}
		else if (face->instance->type == kInstanceTypeImposter)
		{
			drawImposter(scene, (ImposterInstance*)face->instance, bitmap, rowstride);
		}
	}
}
#endif

void
Scene3D_draw(Scene3D* scene, uint8_t* bitmap, int rowstride)
{
#if ENABLE_Z_BUFFER
	scene->zmin = 1e23;
#endif

#if SORT_3D_FACES_BY_Z
	scene->sortedfacelistc = 0;
#endif

	Scene3D_updateNode(scene, &scene->root, scene->camera, 0, kRenderFilled, 0);
	
#if ENABLE_Z_BUFFER
	resetZBuffer();
#endif
	resetZScale(CLIP_EPSILON);
	
#if SORT_3D_FACES_BY_Z
	Scene3D_drawSortedFaces(scene, bitmap, rowstride);
#else
	Scene3D_drawNode(scene, &scene->root, bitmap, rowstride);
#endif
}