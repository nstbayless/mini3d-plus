//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include <pd_api.h>
#include "luaglue.h"

#ifdef _WINDLL
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif


DllExport int
eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg)
{
	if ( event == kEventInitLua )
		register3D(playdate);
	
	return 0;
}
