//
//  scene.h
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#ifndef scene_h
#define scene_h

#include <stdint.h>
#include "3dmath.h"
#include "shape.h"
#include "imposter.h"

struct FaceInstance
{
	Point3D* p1; // pointers into shape's points array
	Point3D* p2;
	Point3D* p3;
	Point3D* p4;
	Vector3D normal;
	float colorBias; // added to lighting computation
#if ENABLE_TEXTURES
	// index of face in prototype
	// we need this to look up the uv coords of the vertices
	uint16_t org_face;
#endif
#if ENABLE_ORDERING_TABLE
	struct FaceInstance* next;
#endif
	int isDoubleSided : 1;
};
typedef struct FaceInstance FaceInstance;

typedef enum
{
	kRenderInheritStyle		= 0,
	kRenderFilled			= (1<<0),
	kRenderWireframe		= (1<<1),
	kRenderWireframeBack	= (1<<2),
	kRenderWireframeWhite	= (1<<3) // 0 = black, 1 = white
} RenderStyle;

typedef struct
{
	// CONFUSINGLY, these points are misnamed!
	// FIXME: rename these p2->p1, p3->p2, p4->p3, p1->p4.
	Point3D p2;
	Point3D p3;
	Point3D p4;
	Point3D* p1;
	FaceInstance* src;
	#if ENABLE_TEXTURES
	FaceTexture tex;
	#endif
} ClippedFace3D;

#define CLIP_RESIZE 0x10
#define MAXCLIP_CAPACITY 0x60

typedef enum
{
	kInstanceTypeShape,
	kInstanceTypeImposter
} InstanceType;

typedef struct InstanceHeader
{
	InstanceType type;
	Point3D center;
	Matrix3D transform;
	struct InstanceHeader* next;
	#if ENABLE_Z_BUFFER
		int useZBuffer : 1;
	#endif
} InstanceHeader;

struct ShapeInstance
{
	InstanceHeader header; // (superclass -- must be the first member)
	Shape3D* prototype; // pointer to original shape

	// cached values from node tree update
	int nPoints;
	Point3D* points;
	int nFaces;
	FaceInstance* faces;
	// clipped faces are stored separately
	int nClip;
	int clipCapacity;
	ClippedFace3D* clip;
	float colorBias;
	RenderStyle renderStyle;
	int inverted : 1; // transformation flipped it across a plane, so we need to reverse backface check
#if ENABLE_ORDERING_TABLE
	int orderTableSize;
	FaceInstance** orderTable;
#endif
};

typedef struct ShapeInstance ShapeInstance;
typedef struct Scene3DNode Scene3DNode;

typedef struct ImposterInstance
{
	InstanceHeader header; // (superclass -- must be the first member)
	Imposter3D* prototype;
	
	// bounding points
	Point3D tl;
	Point3D br;
} ImposterInstance;

struct Scene3DNode
{
	Matrix3D transform;
	Scene3DNode* parentNode;
	int nChildren;
	Scene3DNode** childNodes;
	int nInstance;
	InstanceHeader* instances;
	float colorBias;
	RenderStyle renderStyle;
	int isVisible:1;
	int needsUpdate:1;
#if ENABLE_Z_BUFFER
	int useZBuffer:1;
	float zmin;
#endif
};

void Scene3DNode_init(Scene3DNode* node);
void Scene3DNode_setTransform(Scene3DNode* node, Matrix3D* xform);
void Scene3DNode_addTransform(Scene3DNode* node, Matrix3D* xform);
void Scene3DNode_addShape(Scene3DNode* node, Shape3D* shape);
void Scene3DNode_addShapeWithOffset(Scene3DNode* node, Shape3D* shape, Vector3D offset);
void Scene3DNode_addShapeWithTransform(Scene3DNode* node, Shape3D* shape, Matrix3D transform);
void Scene3DNode_addImposter(Scene3DNode* node, Imposter3D* imposter);
void Scene3DNode_addImposterWithTransform(Scene3DNode* node, Imposter3D* imposter, Matrix3D transform);
Scene3DNode* Scene3DNode_newChild(Scene3DNode* node);
void Scene3DNode_setColorBias(Scene3DNode* node, float bias);
void Scene3DNode_setRenderStyle(Scene3DNode* node, RenderStyle style);
RenderStyle Scene3DNode_getRenderStyle(Scene3DNode* node);
void Scene3DNode_setVisible(Scene3DNode* node, int visible);
#if ENABLE_Z_BUFFER
void Scene3DNode_setUsesZBuffer(Scene3DNode* node, int flag);
#endif

typedef struct
{
	int hasPerspective : 1;
	
	Matrix3D camera;
	Vector3D light;
	
	// location of the Z vanishing point on the screen. (0,0) is top left corner, (1,1) is bottom right. Defaults to (0.5,0.5)
	float centerx;
	float centery;
	
	// display scaling factor. Default is 120, so that the screen extents are (-1.66,1.66)x(-1,1)
	float scale;
	
	Scene3DNode root;
	
	// all instances from the render tree are added here and z-sorted
	InstanceHeader** instancelist;
	int instancelistsize;

#if ENABLE_Z_BUFFER
	float zmin;
#endif
	
} Scene3D;

void Scene3D_init(Scene3D* scene);
void Scene3D_deinit(Scene3D* scene);
void Scene3D_setCamera(Scene3D* scene, Point3D origin, Point3D lookAt, float scale, Vector3D up);
void Scene3D_setGlobalLight(Scene3D* scene, Vector3D light);
Scene3DNode* Scene3D_getRootNode(Scene3D* scene);
void Scene3D_draw(Scene3D* scene, uint8_t* buffer, int rowstride);
void Scene3D_drawNode(Scene3D* scene, Scene3DNode* node, uint8_t* bitmap, int rowstride);
void Scene3D_setCenter(Scene3D* scene, float x, float y);

#endif /* scene_h */
