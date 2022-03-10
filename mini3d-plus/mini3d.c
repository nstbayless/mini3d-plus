//
//  mini3d.c
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "mini3d.h"

void* (*m3d_realloc)(void* ptr, size_t size);

void mini3d_setRealloc(void* (*realloc)(void* ptr, size_t size))
{
	m3d_realloc = realloc;
}
