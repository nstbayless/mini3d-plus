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
}