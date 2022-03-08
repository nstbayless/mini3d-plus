//
//  shape.c
//  Extension
//
//  Created by Dave Hayden on 10/7/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "mini3d.h"
#include "imposter.h"

void Imposter3D_init(Imposter3D* imposter)
{	
	imposter->retainCount = 0;
	imposter->center.x = 0;
	imposter->center.y = 0;
	imposter->center.z = 0;
	imposter->x1 = -1;
	imposter->x2 = 1;
	imposter->y1 = -1;
	imposter->y2 = 1;
	imposter->bitmap = NULL;
}

Imposter3D* Imposter3D_retain(Imposter3D* imposter)
{
	++imposter->retainCount;
	return imposter;
}

void Imposter3D_release(Imposter3D* imposter)
{
	if ( --imposter->retainCount > 0 )
		return;
	
	m3d_free(imposter);
	
	if (imposter->bitmap)
	{
		pd->graphics->freeBitmap(imposter->bitmap);
	}
}

void Imposter3D_setPosition(Imposter3D* imposter, Point3D* position)
{
	imposter->center = *position;
}

void Imposter3D_setRectangle(Imposter3D* imposter, float x1, float y1, float x2, float y2)
{
	imposter->x1 = x1;
	imposter->y1 = y1;
	imposter->x2 = x2;
	imposter->y2 = y2;
}

void Imposter3D_setBitmap(Imposter3D* imposter, LCDBitmap* bitmap)
{
	if (imposter->bitmap)
	{
		pd->graphics->freeBitmap(imposter->bitmap);
	}
	imposter->bitmap = bitmap;
}