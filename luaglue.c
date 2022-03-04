//
//  3d-glue.c
//  Extension
//
//  Created by Dave Hayden on 9/19/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include <math.h>
#include "luaglue.h"
#include "mini3d.h"
#include "shape.h"
#include "scene.h"

static PlaydateAPI* pd = NULL;

static const lua_reg lib3DScene[];
static const lua_reg lib3DNode[];
static const lua_reg lib3DShape[];
static const lua_reg lib3DPoint[];
static const lua_reg lib3DMatrix[];

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

	if ( !pd->lua->registerClass("lib3d.point", lib3DPoint, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->registerClass("lib3d.matrix", lib3DMatrix, NULL, 0, &err) )
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
	
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
static Point3D* getPoint(int n)			{ return get3DObj(n, "lib3d.point"); }
static Matrix3D* getMatrix(int n)		{ return get3DObj(n, "lib3d.matrix"); }

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

static int scene_draw(lua_State* L)
{
	Scene3D* scene = getScene(1);
	
	Scene3D_draw(scene, pd->graphics->getFrame(), LCD_ROWSIZE);
	pd->graphics->markUpdatedRows(0, LCD_ROWS-1); // XXX
	
	return 0;
}

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
static float cameraScale = 1.0;
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

static int scene_setCameraTarget(lua_State* L)
{
	Scene3D* scene = getScene(1);
	cameraLookat.x = pd->lua->getArgFloat(2);
	cameraLookat.y = pd->lua->getArgFloat(3);
	cameraLookat.z = pd->lua->getArgFloat(4);
	
	Scene3D_setCamera(scene, cameraOrigin, cameraLookat, cameraScale, cameraUp);
	
	return 0;
}

static int scene_setCameraScale(lua_State* L)
{
	Scene3D* scene = getScene(1);
	cameraScale = pd->lua->getArgFloat(2);
	
	Scene3D_setCamera(scene, cameraOrigin, cameraLookat, cameraScale, cameraUp);

	return 0;
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

static const lua_reg lib3DScene[] =
{
	{ "__gc",			scene_gc },
	{ "new",			scene_new },
	{ "draw",			scene_draw },
	{ "drawNode",		scene_drawNode },
	{ "getRootNode",	scene_getRoot },
	{ "setLight",		scene_setLight },
	{ "setCenter",		scene_setCenter },
	{ "setCameraOrigin",	scene_setCameraOrigin },
	{ "setCameraScale",		scene_setCameraScale },
	{ "setCameraTarget",	scene_setCameraTarget },
	{ "setCameraUp",		scene_setCameraUp },
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
	
	Shape3D_addFace(shape, a, b, c, d, color);

	return 0;
}

static int shape_setClosed(lua_State* L)
{
	Shape3D_setClosed(getShape(1), pd->lua->getArgBool(2));
	return 0;
}

#if ENABLE_ORDERING_TABLE
static int shape_setOrderTableSize(lua_State* L)
{
	Shape3D_setOrderTableSize(getShape(1), pd->lua->getArgInt(2));
	return 0;
}
#endif

static const lua_reg lib3DShape[] =
{
	{ "new",			shape_new },
	{ "__gc",			shape_gc },
	{ "addFace",		shape_addFace },
	{ "setClosed", 		shape_setClosed },
#if ENABLE_ORDERING_TABLE
	{ "setOrderTableSize", shape_setOrderTableSize, },
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
	Point3D* p = getPoint(1);
	const char* arg = pd->lua->getArgString(2);
	
	if ( strcmp(arg, "x") == 0 )
		pd->lua->pushFloat(p->x);
	else if ( strcmp(arg, "y") == 0 )
		pd->lua->pushFloat(p->y);
	else if ( strcmp(arg, "z") == 0 )
		pd->lua->pushFloat(p->z);
	else
		pd->lua->pushNil();
	
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
	Matrix3D* m = getMatrix(2);

	Point3D* p2 = m3d_malloc(sizeof(Point3D));
	*p2 = Matrix3D_apply(*m, *p);
	pd->lua->pushObject(p2, "lib3d.point", 0);
	return 1;
}

static const lua_reg lib3DPoint[] =
{
	{ "new",			point_new },
	{ "__gc", 			point_gc },
	{ "__index",		point_index },
	{ "__newindex",		point_newindex },
	{ "__mul",			point_mul },
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
	float c = cos(angle * M_PI / 180);
	float s = sin(angle * M_PI / 180);
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


