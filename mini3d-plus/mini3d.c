//
//  mini3d.c
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include "mini3d.h"
#include <stdlib.h>

void* (*m3d_realloc)(void* ptr, size_t size);

void*
m3d_malloc(size_t size) { return m3d_realloc(NULL, size); }
void*
m3d_calloc(size_t size, size_t items) {
	void* ptr = m3d_realloc(NULL, size * items);
	memset(ptr, 0, size * items);
	return ptr;
}
void
m3d_free(void* ptr) { m3d_realloc(ptr, 0); }

void mini3d_setRealloc(void* (*realloc)(void* ptr, size_t size))
{
	m3d_realloc = realloc;
}
