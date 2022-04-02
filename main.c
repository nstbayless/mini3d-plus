//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdlib.h>

#include <pd_api.h>
#include "luaglue.h"

#ifdef _WINDLL
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif


#ifdef MINI3D_AS_LIBRARY
	int
	mini3D_eventHandler
#else
	DllExport int
	eventHandler
#endif
(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg)
{
	if ( event == kEventInitLua )
		register3D(playdate);
	
	return 0;
}
