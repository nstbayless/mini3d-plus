//
//  3d-glue.c
//  Extension
//
//  Created by Dave Hayden on 9/19/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include <math.h>
#include "luaglue.h"
#include "render.h"
#include "mini3d.h"
#include "shape.h"
#include "imposter.h"
#include "scene.h"
#include "collision.h"
#include "texture.h"
#include "pattern.h"

#ifndef M_PI
	#define M_PI 3.1415926535f
#endif

PlaydateAPI* pd = NULL;

static const lua_reg lib3DScene[];
static const lua_reg lib3DNode[];
static const lua_reg lib3DShape[];
static const lua_reg lib3DImposter[];
static const lua_reg lib3DPoint[];
static const lua_reg lib3DMatrix[];
static const lua_reg lib3DMath[];
static const lua_reg lib3DRenderer[];

#if ENABLE_TEXTURES
static const lua_reg lib3DTexture[];
#endif

#if ENABLE_CUSTOM_PATTERNS
static const lua_reg lib3DPattern[];
#endif

void register3D(PlaydateAPI* playdate)
{
	pd = playdate;
	
	const char* err;
	
	if ( !pd->lua->registerClass("lib3d.scene", lib3DScene, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->registerClass("lib3d.scenenode", lib3DNode, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->registerClass("lib3d.shape", lib3DShape, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
		
	if ( !pd->lua->registerClass("lib3d.imposter", lib3DImposter, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->registerClass("lib3d.point", lib3DPoint, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->registerClass("lib3d.matrix", lib3DMatrix, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
	
	if ( !pd->lua->registerClass("lib3d.math", lib3DMath, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
		
	if ( !pd->lua->registerClass("lib3d.renderer", lib3DRenderer, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
		
	#if ENABLE_TEXTURES
	if ( !pd->lua->registerClass("lib3d.texture", lib3DTexture, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
	#endif
		
	#if ENABLE_CUSTOM_PATTERNS
	if ( !pd->lua->registerClass("lib3d.pattern", lib3DPattern, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
	#endif
	
	mini3d_setRealloc(pd->system->realloc);
}

static void* get3DObj(int n, char* type)
{
	void* obj = pd->lua->getArgObject(n, type, NULL);
	
	if ( obj == NULL )
		pd->system->error("object of type %s not found at stack position %i", type, n);
	
	return obj;
}

static Scene3D* getScene(int n)			{ return get3DObj(n, "lib3d.scene"); }
static Scene3DNode* getSceneNode(int n)	{ return get3DObj(n, "lib3d.scenenode"); }
static Shape3D* getShape(int n)			{ return get3DObj(n, "lib3d.shape"); }
static Imposter3D* getImposter(int n)	{ return get3DObj(n, "lib3d.imposter"); }
static Point3D* getPoint(int n)			{ return get3DObj(n, "lib3d.point"); }
static Vector3D* getVector(int n)	    { return get3DObj(n, "lib3d.point"); }
static Matrix3D* getMatrix(int n)		{ return get3DObj(n, "lib3d.matrix"); }
#if ENABLE_TEXTURES
static Texture* getTexture(int n)		{ return get3DObj(n, "lib3d.texture"); }
#endif
#if ENABLE_CUSTOM_PATTERNS
static PatternTable* getPatternTable(int n)		{ return get3DObj(n, "lib3d.pattern"); }
#endif

/// Scene

static int scene_new(lua_State* L)
{
	Scene3D* scene = m3d_malloc(sizeof(Scene3D));
	Scene3D_init(scene);
	pd->lua->pushObject(scene, "lib3d.scene", 0);
	return 1;
}

static int scene_gc(lua_State* L)
{
	Scene3D* scene = getScene(1);
	Scene3D_deinit(scene);
	m3d_free(scene);
	return 0;
}

static int scene_getRoot(lua_State* L)
{
	Scene3D* scene = getScene(1);
	pd->lua->pushObject(Scene3D_getRootNode(scene), "lib3d.scenenode", 0);
	return 1;
}

#if ENABLE_INTERLACE
static uint8_t backbuff[LCD_ROWS * LCD_ROWSIZE];

static inline void
clear_backbuff_interlaced(void)
{
	// FIXME: please rewrite this, it's bad
	int step = getInterlaceEnabled() + 1;
	int start = 0;
	int ilace_enabled = getInterlaceEnabled();
	int ilace = getInterlace();
	for (size_t i = 0; i < LCD_ROWS; i ++)
	{
		if (!ilace_enabled || ENABLE_INTERLACE == 2 || (i >> INTERLACE_ROW_LGC) % INTERLACE_INTERVAL == ilace)
		{
			memset(backbuff + (LCD_ROWSIZE * i), 0, LCD_ROWSIZE);
		}
	}
}
#endif

static int _prefetch_zbuf(lua_State* L)
{
	#if ENABLE_Z_BUFFER
	prefetch_zbuf();
	#endif
	return 0;
}

static int scene_draw(lua_State* L)
{
	Scene3D* scene = getScene(1);
	
	#if ENABLE_TEXTURES && TEXTURE_PERSPECTIVE_MAPPING && PRECOMPUTE_PROJECTION
	precomputeProjectionTable();
	#endif
	
	#if !ENABLE_INTERLACE
	Scene3D_draw(scene, pd->graphics->getFrame(), LCD_ROWSIZE);
	pd->graphics->markUpdatedRows(0, LCD_ROWS-1); // XXX
	#else
	if (getInterlaceEnabled())
	{
		#if INTERLACE_INTERVAL <= 2
			setInterlace(!getInterlace());
		#else
			setInterlace((getInterlace()+1)%INTERLACE_INTERVAL);
		#endif
		clear_backbuff_interlaced();
		Scene3D_draw(scene, &backbuff[0], LCD_ROWSIZE);
	}
	else
	{
		clear_backbuff_interlaced();
		Scene3D_draw(scene, &backbuff[0], LCD_ROWSIZE);
	}
	memcpy(pd->graphics->getFrame(), backbuff, LCD_ROWS * LCD_ROWSIZE);
	pd->graphics->markUpdatedRows(0, LCD_ROWS-1);
	#endif
	
	return 0;
}

#if ENABLE_Z_BUFFER
static int draw_zbuff(lua_State* L)
{
	render_zbuff(pd->graphics->getFrame(), LCD_ROWSIZE);
	pd->graphics->markUpdatedRows(0, LCD_ROWS-1); // XXX
	
	return 0;
}
#endif

static int scene_drawNode(lua_State* L)
{
	Scene3D* scene = getScene(1);
	Scene3DNode* node = getSceneNode(2);

	Scene3D_drawNode(scene, node, pd->graphics->getFrame(), LCD_ROWSIZE);
	pd->graphics->markUpdatedRows(0, LCD_ROWS-1); // XXX
	
	return 0;
}

static int scene_setLight(lua_State* L)
{
	Scene3D* scene = getScene(1);
	float x = pd->lua->getArgFloat(2);
	float y = pd->lua->getArgFloat(3);
	float z = pd->lua->getArgFloat(4);
	
	Scene3D_setGlobalLight(scene, Vector3DMake(x, y, z));
	
	return 0;
}

static int scene_setCenter(lua_State* L)
{
	Scene3D* scene = getScene(1);
	float x = pd->lua->getArgFloat(2);
	float y = pd->lua->getArgFloat(3);

	Scene3D_setCenter(scene, x, y);
	
	return 0;
}

static Point3D cameraOrigin = (Point3D){ 0, -1, 1 };
static Point3D cameraLookat = (Point3D){ 0, 0, 0 };
static float cameraScale = 1.0f;
static Vector3D cameraUp = (Vector3D){ 0, 0, 1 };

static int scene_setCameraOrigin(lua_State* L)
{
	Scene3D* scene = getScene(1);
	cameraOrigin.x = pd->lua->getArgFloat(2);
	cameraOrigin.y = pd->lua->getArgFloat(3);
	cameraOrigin.z = pd->lua->getArgFloat(4);

	Scene3D_setCamera(scene, cameraOrigin, cameraLookat, cameraScale, cameraUp);

	return 0;
}

static int scene_getCameraOrigin(lua_State* L)
{
	Scene3D* scene = getScene(1);
	pd->lua->pushFloat(cameraOrigin.x);
	pd->lua->pushFloat(cameraOrigin.y);
	pd->lua->pushFloat(cameraOrigin.z);
	return 3;
}

static int scene_setCameraTarget(lua_State* L)
{
	Scene3D* scene = getScene(1);
	cameraLookat.x = pd->lua->getArgFloat(2);
	cameraLookat.y = pd->lua->getArgFloat(3);
	cameraLookat.z = pd->lua->getArgFloat(4);
	
	Scene3D_setCamera(scene, cameraOrigin, cameraLookat, cameraScale, cameraUp);
	
	return 0;
}

static int scene_getCameraTarget(lua_State* L)
{
	Scene3D* scene = getScene(1);
	pd->lua->pushFloat(cameraLookat.x);
	pd->lua->pushFloat(cameraLookat.y);
	pd->lua->pushFloat(cameraLookat.z);
	return 3;
}

static int scene_setCameraScale(lua_State* L)
{
	Scene3D* scene = getScene(1);
	cameraScale = pd->lua->getArgFloat(2);
	
	Scene3D_setCamera(scene, cameraOrigin, cameraLookat, cameraScale, cameraUp);

	return 0;
}

static int scene_getCameraScale(lua_State* L)
{
	Scene3D* scene = getScene(1);
	pd->lua->pushFloat(cameraScale);
	return 1;
}

static int scene_setCameraUp(lua_State* L)
{
	Scene3D* scene = getScene(1);
	cameraUp.dx = pd->lua->getArgFloat(2);
	cameraUp.dy = pd->lua->getArgFloat(3);
	cameraUp.dz = pd->lua->getArgFloat(4);
	
	Scene3D_setCamera(scene, cameraOrigin, cameraLookat, cameraScale, cameraUp);
	
	return 0;
}

static int scene_getCameraUp(lua_State* L)
{
	Scene3D* scene = getScene(1);
	pd->lua->pushFloat(cameraUp.dx);
	pd->lua->pushFloat(cameraUp.dy);
	pd->lua->pushFloat(cameraUp.dz);
	return 3;
}

static const lua_reg lib3DScene[] =
{
	{ "__gc",			scene_gc },
	{ "new",			scene_new },
	{ "draw",			scene_draw },
#if ENABLE_Z_BUFFER
	{ "drawZBuff",		draw_zbuff },
#endif
	{ "drawNode",		scene_drawNode },
	{ "getRootNode",	scene_getRoot },
	{ "setLight",		scene_setLight },
	{ "setCenter",		scene_setCenter },
	{ "setCameraOrigin",	scene_setCameraOrigin },
	{ "setCameraScale",		scene_setCameraScale },
	{ "setCameraTarget",	scene_setCameraTarget },
	{ "setCameraUp",		scene_setCameraUp },
	{ "getCameraOrigin",	scene_getCameraOrigin },
	{ "getCameraScale",		scene_getCameraScale },
	{ "getCameraTarget",	scene_getCameraTarget },
	{ "getCameraUp",		scene_getCameraUp },
	{ "prefetchZBuff", _prefetch_zbuf},
	{ NULL,				NULL }
};


/// Node

static int node_gc(lua_State* L)
{
	// XXX - release shapes
	return 0;
}

static int node_addShape(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	Shape3D* shape = getShape(2);
	
	Matrix3D* m = pd->lua->getArgObject(3, "lib3d.matrix", NULL);
	
	if ( m != NULL )
		Scene3DNode_addShapeWithTransform(node, shape, *m);

	else if ( pd->lua->getArgCount() > 2 )
	{
		Vector3D v = { 0, 0, 0 };
		v.dx = pd->lua->getArgFloat(3);
		v.dy = pd->lua->getArgFloat(4);
		v.dz = pd->lua->getArgFloat(5);
	
		Scene3DNode_addShapeWithOffset(node, shape, v);
	}
	else
		Scene3DNode_addShape(node, shape);

	//pd->lua->retainObject(shapeobj);
	
	return 0;
}

static int node_addImposter(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	Imposter3D* imposter = getImposter(2);
	Matrix3D* m = pd->lua->getArgObject(3, "lib3d.matrix", NULL);
	if ( m != NULL )
		Scene3DNode_addImposterWithTransform(node, imposter, *m);
	// TODO: Scene3DNode_addImposterWithOffset
	else
		Scene3DNode_addImposter(node, imposter);
		
	return 0;
}

static int node_makeChildNode(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	Scene3DNode* scene = Scene3DNode_newChild(node);

	pd->lua->pushObject(scene, "lib3d.scenenode", 0);

	return 1;
}

static int node_setTransform(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	Matrix3D* xform = getMatrix(2);
	
	Scene3DNode_setTransform(node, xform);
	
	return 0;
}

static int node_addTransform(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	Matrix3D* xform = getMatrix(2);
	
	Scene3DNode_addTransform(node, xform);
	
	return 0;
}

static int node_translateBy(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	
	Matrix3D xform = identityMatrix;
	xform.dx = pd->lua->getArgFloat(2);
	xform.dy = pd->lua->getArgFloat(3);
	xform.dz = pd->lua->getArgFloat(4);
	
	Scene3DNode_addTransform(node, &xform);
	
	return 0;
}

static int node_scaleBy(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	Matrix3D xform = node->transform;
	float sx, sy, sz;
	
	if ( pd->lua->getArgCount() > 2 )
	{
		sx = pd->lua->getArgFloat(2);
		sy = pd->lua->getArgFloat(3);
		sz = pd->lua->getArgFloat(4);
	}
	else
		sx = sy = sz = pd->lua->getArgFloat(2);
	
	xform.isIdentity = 0;
	
	xform.m[0][0] *= sx;
	xform.m[1][0] *= sx;
	xform.m[2][0] *= sx;
	
	xform.m[0][1] *= sy;
	xform.m[1][1] *= sy;
	xform.m[2][1] *= sy;
	
	xform.m[0][2] *= sz;
	xform.m[1][2] *= sz;
	xform.m[2][2] *= sz;
	
	xform.dx *= sx;
	xform.dy *= sy;
	xform.dz *= sz;
	
	xform.inverting = (sx * sy * sz < 0);

	Scene3DNode_setTransform(node, &xform);
	
	return 0;
}

static int node_setColorBias(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	
	Scene3DNode_setColorBias(node, pd->lua->getArgFloat(2));
	
	return 0;
}

static int node_setFilled(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	int flag = pd->lua->getArgBool(2);
	
	RenderStyle style = Scene3DNode_getRenderStyle(node);
	
	if ( flag )
		style |= kRenderFilled;
	else
		style &= ~kRenderFilled;
	
	Scene3DNode_setRenderStyle(node, style);
	
	return 0;
}

static int node_setWireframeMode(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	int mode = pd->lua->getArgInt(2);
	
	RenderStyle style = Scene3DNode_getRenderStyle(node);
	
	if ( mode > 0 )
	{
		style |= kRenderWireframe;

		if ( mode == 1 )
			style &= ~kRenderWireframeBack;
		else
			style |= kRenderWireframeBack;
	}
	else
		style &= ~kRenderWireframe;

	Scene3DNode_setRenderStyle(node, style);
	
	return 0;
}

static int node_setWireframeColor(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);
	int color = pd->lua->getArgInt(2);

	RenderStyle style = Scene3DNode_getRenderStyle(node);

	if ( color )
		style |= kRenderWireframeWhite;
	else
		style &= ~kRenderWireframeWhite;
	
	Scene3DNode_setRenderStyle(node, style);
	
	return 0;
}

static int node_setVisible(lua_State* L)
{
	Scene3DNode* node = getSceneNode(1);

	Scene3DNode_setVisible(node, pd->lua->getArgBool(2));
	
	return 0;
}

static const lua_reg lib3DNode[] =
{
	{ "__gc",			node_gc },
	{ "addChildNode",	node_makeChildNode },
	{ "addShape",		node_addShape },
	{ "addImposter",	node_addImposter },
	{ "addTransform",	node_addTransform },
	{ "setTransform",	node_setTransform },
	{ "translateBy",	node_translateBy },
	{ "scaleBy",		node_scaleBy },
	{ "setColorBias",	node_setColorBias },
	{ "setFilled",		node_setFilled },
	{ "setWireframeMode",	node_setWireframeMode },
	{ "setWireframeColor",	node_setWireframeColor },
	{ "setVisible",		node_setVisible },
	{ NULL,			NULL }
};


/// Shape

static int shape_new(lua_State* L)
{
	Shape3D* shape = m3d_malloc(sizeof(Shape3D));
	Shape3D_init(shape);
	Shape3D_retain(shape);
	pd->lua->pushObject(shape, "lib3d.shape", 0);
	return 1;
}

static int shape_gc(lua_State* L)
{
	Shape3D* shape = getShape(1);
	Shape3D_release(shape);
	return 0;
}

static int shape_addFace(lua_State* L)
{
	Shape3D* shape = getShape(1);
	Point3D* a = getPoint(2);
	Point3D* b = getPoint(3);
	Point3D* c = getPoint(4);

	Point3D* d = NULL;
	const char* type;
	float color = 0;
	
	if ( pd->lua->getArgType(5, &type) == kTypeObject && strcmp(type, "lib3d.point") == 0 )
	{
		d = getPoint(5);
		color = pd->lua->getArgFloat(6);
	}
	else
		color = pd->lua->getArgFloat(5);
	
	int face_idx = Shape3D_addFace(shape, a, b, c, d, color);
	pd->lua->pushInt(face_idx);
	return 1;
}

#if ENABLE_TEXTURES
static int shape_setFaceTextureMap(lua_State* L)
{
	Shape3D* shape = getShape(1);
	int face_idx = pd->lua->getArgInt(2);
	float u1 = pd->lua->getArgFloat(3);
	float v1 = pd->lua->getArgFloat(4);
	float u2 = pd->lua->getArgFloat(5);
	float v2 = pd->lua->getArgFloat(6);
	float u3 = pd->lua->getArgFloat(7);
	float v3 = pd->lua->getArgFloat(8);
	
	// FIXME: use only if these are provided
	float u4 = pd->lua->getArgFloat(9);
	float v4 = pd->lua->getArgFloat(10);
	
	Shape3D_setFaceTextureMap(shape, face_idx,
		(Point2D){u1, v1}, (Point2D){u2, v2}, (Point2D){u3, v3}, (Point2D){u4, v4});
		
	return 0;
}

#if ENABLE_TEXTURES_GREYSCALE
static int shape_setFaceLighting(lua_State* L)
{
	Shape3D* shape = getShape(1);
	int face_idx = pd->lua->getArgInt(2);
	float l = pd->lua->getArgFloat(3);
	
	Shape3D_setFaceLighting(shape, face_idx, l);
	
	return 0;
}
#endif
#endif

#if ENABLE_CUSTOM_PATTERNS
static int shape_setPattern(lua_State* L)
{
	Shape3D* shape = getShape(1);
	PatternTable* pt = NULL;
	if (pd->lua->getArgType(2, NULL) != kTypeNil)
	{
		pt = getPatternTable(2);
	}
	Shape3D_setPattern(shape, pt);
	return 0;
}
#endif

static int shape_setClosed(lua_State* L)
{
	Shape3D_setClosed(getShape(1), pd->lua->getArgBool(2));
	return 0;
}

static int shape_setFaceDoubleSided(lua_State* L)
{
	Shape3D_setFaceDoubleSided(getShape(1), pd->lua->getArgInt(2), pd->lua->getArgBool(3));
	return 0;
}

static int shape_collideSphere(lua_State* L)
{
	Shape3D* shape = getShape(1);
	Point3D* centre = getPoint(2);
	float radius = pd->lua->getArgFloat(3);
	Vector3D o_normal;
	float dist;
	
	float best_collision_distance = radius + 1;
	Vector3D best_normal;
	int best_face_idx;
	int collision_occurred = 0;
	for (int i = 0; i < shape->nFaces; ++i)
	{
		Face3D* face = &shape->faces[i];
		Point3D* p1 = &shape->points[face->p1];
		Point3D* p2 = &shape->points[face->p2];
		Point3D* p3 = &shape->points[face->p3];
		if (test_sphere_triangle(centre, radius, p1, p2, p3, &o_normal, &dist) && dist < best_collision_distance)
		{
			collision_occurred = 1;
			best_collision_distance = dist;
			best_face_idx = i;
			best_normal = o_normal;
		}
		
		if (face->p4 != 0xffff)
		{
			Point3D* p4 = &shape->points[face->p4];
			if (test_sphere_triangle(centre, radius, p1, p3, p4, &o_normal, &dist) && dist < best_collision_distance)
			{
				collision_occurred = 1;
				best_collision_distance = dist;
				best_face_idx = i;
				best_normal = o_normal;
			}
		}
	}
	
	if (collision_occurred)
	{
		pd->lua->pushBool(1);
		Point3D* normal = m3d_malloc(sizeof(Point3D));
		memcpy(normal, &best_normal, sizeof(Vector3D));
		pd->lua->pushObject(normal, "lib3d.point", 0);
		pd->lua->pushFloat(best_collision_distance);
		pd->lua->pushInt(best_face_idx);
		return 4;
	}
	else
	{
		pd->lua->pushBool(0);
		return 1;
	}
}

#if ENABLE_TEXTURES
// gets texture, and refs it too.
// remember to Texture_unref if not null!
static Texture* getArgsTexture(lua_State* L, int primarg, int secarg, const char** outerr)
{
	int greyscale = pd->lua->getArgBool(secarg);
	const char* typename;
	switch (pd->lua->getArgType(primarg, &typename))
	{
	case kTypeNil:
		return NULL;
	case kTypeString:
		return Texture_loadFromPath(pd->lua->getArgString(primarg), greyscale, outerr);
	case kTypeObject:
		if (strcmp(typename, "lib3d.texture") == 0)
		{
			return Texture_ref(getTexture(primarg));
		}
		else 
		{
			LCDBitmap* bitmap = pd->lua->getBitmap(primarg);
			if (!bitmap)
			{
				*outerr = "must be either LCDBitmap or lib3d.texture";
				return NULL;
			}
			
			return Texture_fromLCDBitmap(bitmap);
		}
	default:
		*outerr = "unrecognized texture type";
		return NULL;
	}
}

static int shape_setTexture(lua_State* L)
{
	Shape3D* shape = getShape(1);
	const char* outerr = NULL;
	Texture* t = getArgsTexture(L, 2, 3, &outerr);
	if (outerr != NULL)
	{
		pd->system->error("%s", outerr);
		return 0;
	}
	Shape3D_setTexture(shape, t);
	if (t) Texture_unref(t);
	return 0;
}
#endif

#if ENABLE_POLYGON_SCANLINING
static int shape_setScanlining(lua_State* L)
{
	if (pd->lua->getArgCount() != 3) {
		pd->system->error("Wrong number of arguments to shape:setScanlining; 3 expected");
	}
	Shape3D* shape = getShape(1);
	const char* type = pd->lua->getArgString(2);
	uint32_t scanfill = pd->lua->getArgInt(3);
	ScanlineFill fill;
	fill.fill = scanfill;
	fill.select = kScanlineOdd;
	if (!strcmp(type, "all")) fill.select = kScanlineAll;
	if (!strcmp(type, "even")) fill.select = kScanlineEven;
	if (!strcmp(type, "odd")) fill.select = kScanlineOdd;
	Shape3D_setScanlining(shape, fill);
	return 0;
}
#endif

#if ENABLE_ORDERING_TABLE
static int shape_setOrderTableSize(lua_State* L)
{
	Shape3D_setOrderTableSize(getShape(1), (size_t)MAX(pd->lua->getArgInt(2), 0));
	return 0;
}
#endif

static const lua_reg lib3DShape[] =
{
	{ "new",			shape_new },
	{ "__gc",			shape_gc },
	{ "addFace",		shape_addFace },
	{ "setClosed", 		shape_setClosed },
	{ "collidesSphere", shape_collideSphere },
	{ "setFaceDoubleSided", shape_setFaceDoubleSided },
#if ENABLE_TEXTURES
		{ "setTexture",		shape_setTexture },
		{ "setFaceTextureMap", shape_setFaceTextureMap},
		#if ENABLE_TEXTURES_GREYSCALE
			{ "setFaceLighting", shape_setFaceLighting},
		#endif
#endif
#if ENABLE_POLYGON_SCANLINING
	{ "setScanlining",		shape_setScanlining },
#endif
#if ENABLE_CUSTOM_PATTERNS
	{ "setPattern", shape_setPattern },
#endif
#if ENABLE_ORDERING_TABLE
	{ "setOrderTableSize", shape_setOrderTableSize, },
#endif
	{ NULL,				NULL }
};

/// Imposter

static int imposter_new(lua_State* L)
{
	Imposter3D* imposter = m3d_malloc(sizeof(Imposter3D));
	Imposter3D_init(imposter);
	Imposter3D_retain(imposter);
	pd->lua->pushObject(imposter, "lib3d.imposter", 0);
	return 1;
}

static int imposter_gc(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	Imposter3D_release(imposter);
	return 0;
}

static int imposter_setPosition(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	Point3D* point = getPoint(2);
	Imposter3D_setPosition(imposter, point);
	
	return 0;
}

static int imposter_setRectangle(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	float x1 = pd->lua->getArgFloat(2);
	float y1 = pd->lua->getArgFloat(3);
	float x2 = pd->lua->getArgFloat(4);
	float y2 = pd->lua->getArgFloat(5);
	
	Imposter3D_setRectangle(imposter, x1, y1, x2, y2);
	return 0;
}

static int imposter_setZOffsets(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	float z1 = pd->lua->getArgFloat(2);
	float z2 = pd->lua->getArgFloat(3);
	float z3 = pd->lua->getArgFloat(4);
	float z4 = pd->lua->getArgFloat(5);
	
	Imposter3D_setZOffsets(imposter, z1, z2, z3, z4);
	return 0;
}

#if ENABLE_TEXTURES
static int imposter_setBitmap(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	const char* outerr = NULL;
	Texture* t = getArgsTexture(L, 2, 3, &outerr);
	if (outerr != NULL)
	{
		pd->system->error("%s", outerr);
	}
	Imposter3D_setBitmap(imposter, t);
	if (t) Texture_unref(t);
	return 0;
}

#if ENABLE_TEXTURES_GREYSCALE
static int imposter_setLighting(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	float lighting = pd->lua->getArgFloat(2);
	Imposter3D_setLighting(imposter, lighting);
	return 0;
}
#endif
#endif

#if ENABLE_CUSTOM_PATTERNS
static int imposter_setPattern(lua_State* L)
{
	Imposter3D* imposter = getImposter(1);
	PatternTable* pt = NULL;
	if (pd->lua->getArgType(2, NULL) != kTypeNil)
	{
		pt = getPatternTable(2);
	}
	Imposter3D_setPattern(imposter, pt);
	return 0;
}
#endif

static const lua_reg lib3DImposter[] =
{
	{ "new",			imposter_new },
	{ "__gc",			imposter_gc },
	{ "setPosition",	imposter_setPosition },
	{ "setRectangle", 	imposter_setRectangle },
	{ "setZOffsets", 	imposter_setZOffsets },
#if ENABLE_TEXTURES
	{ "setTexture",		imposter_setBitmap },
	#if ENABLE_TEXTURES_GREYSCALE
		{ "setLighting", imposter_setLighting },
	#endif
#endif
#if ENABLE_CUSTOM_PATTERNS
	{ "setPattern",  imposter_setPattern},
#endif
	{ NULL,				NULL }
};


/// Point

static int point_new(lua_State* L)
{
	Point3D* p = m3d_malloc(sizeof(Point3D));
	p->x = pd->lua->getArgFloat(1);
	p->y = pd->lua->getArgFloat(2);
	p->z = pd->lua->getArgFloat(3);
	pd->lua->pushObject(p, "lib3d.point", 0);
	return 1;
}

static int point_gc(lua_State* L)
{
	Point3D* p = getPoint(1);
	m3d_free(p);
	return 0;
}

static int point_index(lua_State* L)
{
	if (pd->lua->indexMetatable())
	{
		return 1;
	}
	
	Point3D* p = getPoint(1);
	const char* arg = pd->lua->getArgString(2);
	
	if ( strcmp(arg, "x") == 0 )
		pd->lua->pushFloat(p->x);
	else if ( strcmp(arg, "y") == 0 )
		pd->lua->pushFloat(p->y);
	else if ( strcmp(arg, "z") == 0 )
		pd->lua->pushFloat(p->z);
	else
		return 0;
	
	return 1;
}

static int point_newindex(lua_State* L)
{
	Point3D* p = getPoint(1);
	const char* arg = pd->lua->getArgString(2);
	
	if ( strcmp(arg, "x") == 0 )
		p->x = pd->lua->getArgFloat(3);
	else if ( strcmp(arg, "y") == 0 )
		p->y = pd->lua->getArgFloat(3);
	else if ( strcmp(arg, "z") == 0 )
		p->z = pd->lua->getArgFloat(3);
	
	return 0;
}

static int point_mul(lua_State* L)
{
	Point3D* p = getPoint(1);
	if (pd->lua->getArgType(2, NULL) == kTypeFloat)
	{
		float f = pd->lua->getArgFloat(2);
		Point3D* p2 = m3d_malloc(sizeof(Point3D));
		p2->x = p->x * f;
		p2->y = p->y * f;
		p2->z = p->z * f;
		pd->lua->pushObject(p2, "lib3d.point", 0);
	}
	else
	{
		Matrix3D* m = getMatrix(2);

		Point3D* p2 = m3d_malloc(sizeof(Point3D));
		*p2 = Matrix3D_apply(*m, *p);
		pd->lua->pushObject(p2, "lib3d.point", 0);
	}
	return 1;
}

static int point_add(lua_State* L)
{
	Point3D* p = getPoint(1);
	Point3D* p2 = getPoint(2);

	Point3D* p3 = m3d_malloc(sizeof(Point3D));
	p3->x = p->x + p2->x;
	p3->y = p->y + p2->y;
	p3->z = p->z + p2->z;
	pd->lua->pushObject(p3, "lib3d.point", 0);
	return 1;
}

static int point_sub(lua_State* L)
{
	Point3D* p = getPoint(1);
	Point3D* p2 = getPoint(2);

	Point3D* p3 = m3d_malloc(sizeof(Point3D));
	p3->x = p->x - p2->x;
	p3->y = p->y - p2->y;
	p3->z = p->z - p2->z;
	pd->lua->pushObject(p3, "lib3d.point", 0);
	return 1;
}

// in-place normalization
static int vec3d_normalize(lua_State* L)
{
	Vector3D* p1 = getVector(1);
	*p1 = Vector3D_normalize(*p1);
	return 0;
}

static int vec3d_length_squared(lua_State* L)
{
	Vector3D* p1 = getVector(1);
	pd->lua->pushFloat(Vector3D_lengthSquared(p1));
	return 1;
}

static int vec3d_length(lua_State* L)
{
	Vector3D* p1 = getVector(1);
	pd->lua->pushFloat(Vector3D_length(p1));
	return 1;
}

static int vec3d_dot(lua_State* L)
{
	Vector3D* p1 = getVector(1);
	Vector3D* p2 = getVector(2);
	pd->lua->pushFloat(Vector3DDot(*p1, *p2));
	return 1;
}

static int vec3d_cross(lua_State* L)
{
	Vector3D* p1 = getVector(1);
	Vector3D* p2 = getVector(2);
	Vector3D* p = m3d_malloc(sizeof(Point3D));
	*p = Vector3DCross(*p1, *p2);
	pd->lua->pushObject(p, "lib3d.point", 0);
	return 1;
}

static const lua_reg lib3DPoint[] =
{
	{ "new",			point_new },
	{ "__gc", 			point_gc },
	{ "__index",		point_index },
	{ "__newindex",		point_newindex },
	{ "__mul",			point_mul },
	{ "__add",			point_add },
	{ "__sub",			point_sub },
	{ "dot",   			vec3d_dot },
	{ "cross",   		vec3d_cross },
	{ "length",  		vec3d_length },
	{ "lengthSquared",  vec3d_length_squared },
	{ "normalize",  	vec3d_normalize},
	{ NULL,				NULL }
};


/// Matrix

static int matrix_gc(lua_State* L)
{
	Matrix3D* m = getMatrix(1);
	m3d_free(m);
	return 0;
}

static int matrix_mul(lua_State* L)
{
	Matrix3D* l = getMatrix(1);
	Matrix3D* r = getMatrix(2);
	Matrix3D* m = m3d_malloc(sizeof(Matrix3D));

	*m = Matrix3D_multiply(*l, *r);
	
	pd->lua->pushObject(m, "lib3d.matrix", 0);
	return 1;
}

static int matrix_new(lua_State* L)
{
	Matrix3D* p = m3d_malloc(sizeof(Matrix3D));
	
	p->isIdentity = 0;
	p->m[0][0] = pd->lua->getArgFloat(1);
	p->m[0][1] = pd->lua->getArgFloat(2);
	p->m[0][2] = pd->lua->getArgFloat(3);
	p->m[1][0] = pd->lua->getArgFloat(4);
	p->m[1][1] = pd->lua->getArgFloat(5);
	p->m[1][2] = pd->lua->getArgFloat(6);
	p->m[2][0] = pd->lua->getArgFloat(7);
	p->m[2][1] = pd->lua->getArgFloat(8);
	p->m[2][2] = pd->lua->getArgFloat(9);
	p->dx = pd->lua->getArgFloat(10);
	p->dy = pd->lua->getArgFloat(11);
	p->dz = pd->lua->getArgFloat(12);

	p->inverting = Matrix3D_getDeterminant(p) < 0;
	
	pd->lua->pushObject(p, "lib3d.matrix", 0);
	return 1;
}

static int matrix_newRotation(lua_State* L)
{
	Matrix3D* p = m3d_malloc(sizeof(Matrix3D));
	
	float angle = pd->lua->getArgFloat(1);
	float c = cosf(angle * M_PI / 180);
	float s = sinf(angle * M_PI / 180);
	float x = pd->lua->getArgFloat(2);
	float y = pd->lua->getArgFloat(3);
	float z = pd->lua->getArgFloat(4);
	
	float d = fisr(x * x + y * y + z * z);
	
	x *= d;
	y *= d;
	z *= d;

	p->isIdentity = 0;
	p->inverting = 0;
	
	p->m[0][0] = c + x * x * (1-c);
	p->m[0][1] = x * y * (1-c) - z * s;
	p->m[0][2] = x * z * (1-c) + y * s;
	p->m[1][0] = y * x * (1-c) + z * s;
	p->m[1][1] = c + y * y * (1-c);
	p->m[1][2] = y * z * (1-c) - x * s;
	p->m[2][0] = z * x * (1-c) - y * s;
	p->m[2][1] = z * y * (1-c) + x * s;
	p->m[2][2] = c + z * z * (1-c);
	p->dx = p->dy = p->dz = 0;
	
	pd->lua->pushObject(p, "lib3d.matrix", 0);
	return 1;
}

static int matrix_addTranslation(lua_State* L)
{
	Matrix3D* l = getMatrix(1);
	l->dx += pd->lua->getArgFloat(2);
	l->dy += pd->lua->getArgFloat(3);
	l->dz += pd->lua->getArgFloat(4);
	return 0;
}

static const lua_reg lib3DMatrix[] =
{
	{ "__gc", 			matrix_gc },
	{ "__mul", 			matrix_mul },
	{ "new",			matrix_new },
	{ "newRotation",	matrix_newRotation },
	{ "addTranslation",	matrix_addTranslation },
	{ NULL,				NULL }
};

static int sphere_collide_triangle(lua_State* L)
{
	Point3D* centre = getPoint(1);
	float radius = pd->lua->getArgFloat(2);
	Point3D* p1 = getPoint(3);
	Point3D* p2 = getPoint(4);
	Point3D* p3 = getPoint(5);
	
	Vector3D normal;
	int result = test_sphere_triangle(centre, radius, p1, p2, p3, &normal, NULL);
	pd->lua->pushBool(
		result
	);
	
	if (result)
	{
		Point3D* o_normal = m3d_malloc(sizeof(Point3D));
		memcpy(o_normal, &normal, sizeof(Vector3D));
		pd->lua->pushObject(o_normal, "lib3d.point", 0);
		return 2;
	}
	else
	{
		return 1;
	}
}

static const lua_reg lib3DMath[] =
{
	{ "collide_sphere_triangle", sphere_collide_triangle },
	{ NULL,				NULL }
};

#if ENABLE_INTERLACE
static int interlace_enable(lua_State* L)
{
	int enable = (pd->lua->getArgType(1, NULL) == kTypeNil || pd->lua->getArgBool(1));
	setInterlaceEnabled(enable);
	return 0;
}

static int interlace_get_enable(lua_State* L)
{
	pd->lua->pushBool(getInterlaceEnabled());
	return 1;
}
#endif

#if ENABLE_RENDER_DISTANCE_MAX
static int set_render_distance(lua_State* L)
{
	setRenderDistanceMax(pd->lua->getArgFloat(1));
	return 0;
}

static int get_render_distance(lua_State* L)
{
	pd->lua->pushFloat(getRenderDistanceMax());
	return 1;
}
#endif

#if ENABLE_DISTANCE_FOG
static int set_render_fog(lua_State* L)
{
	float color = 0;
	float startz = 0;
	float endz = 1;
	
	size_t argc = pd->lua->getArgCount();
	
	if (argc > 0)
	{
		startz = pd->lua->getArgFloat(1);
	}
	if (argc > 1)
	{
		endz = pd->lua->getArgFloat(2);
	}
	if (argc > 2)
	{
		color = pd->lua->getArgFloat(3);
	}
	
	startz = MAX(startz, 0);
	endz = MAX(endz, 0);
	
	fog_set(
		CLAMP(
			(uint8_t)(color * (LIGHTING_PATTERN_COUNT - 1) + 0.5),
			0, LIGHTING_PATTERN_COUNT - 1),
		startz,
		endz
	);
	
	return 0;	
}
#endif

static const lua_reg lib3DRenderer[] =
{
	#if ENABLE_INTERLACE
	{ "setInterlaceEnabled", interlace_enable },
	{ "getInterlaceEnabled", interlace_get_enable },
	#endif
	#if ENABLE_RENDER_DISTANCE_MAX
	{ "setRenderDistance", set_render_distance },
	{ "getRenderDistance", get_render_distance },
	#endif
	#if ENABLE_DISTANCE_FOG
	{ "setFog", set_render_fog },
	#endif
	{ NULL,				NULL }
};

#if ENABLE_TEXTURES
static int texture_new(lua_State* L)
{
	const char* err;
	Texture* t = getArgsTexture(L, 1, 2, &err);
	if (!t)
	{
		pd->system->error("%s", err);
	}
	pd->lua->pushObject(t, "lib3d.texture", 0);
	return 1;
}

static int texture_gc(lua_State* L)
{
	Texture* t = getTexture(1);
	if (t) Texture_unref(t);
	return 0;
}

static const lua_reg lib3DTexture[] =
{
	{"new", texture_new },
	{"__gc", texture_gc },
	{NULL, NULL}
};
#endif

#if ENABLE_CUSTOM_PATTERNS
static int pattern_new(lua_State* L)
{
	PatternTable* p = Pattern_new();
	pd->lua->pushObject(p, "lib3d.pattern", 0);
	size_t argc = pd->lua->getArgCount() - 1;
	pd->system->logToConsole("%d", argc);
	if (argc == 0)
	{
		memcpy(p, &patterns, sizeof(PatternTable));
		return 1;
	}
	size_t argc8 = ((argc + 7)/8);
	for (size_t i = 0; i < LIGHTING_PATTERN_COUNT; ++i)	
	{
		for (size_t j = 0; j < 8; ++j)
		{
			size_t argidx8 = i % argc8;
			size_t argidx_ = j % (8);
			if (argidx8 == argc8 - 1 && argc % 8 != 0)
			{
				argidx_ = j % (argc % 8);
			}
			size_t argidx = (argidx8 * 8 + argidx_);
			uint8_t v = pd->lua->getArgInt(argidx + 1);
			(*p)[i][j] = v;
		}
	}
	return 1;
}

static int pattern_gc(lua_State* L)
{
	PatternTable* pt = getPatternTable(1);
	if (pt) Pattern_unref(pt);
	return 0;
}

static const lua_reg lib3DPattern[] =
{
	{"new", pattern_new },
	{"__gc", pattern_gc },
	{NULL, NULL}
};
#endif