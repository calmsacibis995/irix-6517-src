/*	PiMap.c - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - Handle File Data Maps
*/

#include <stdio.h>
#include "piconfig.h"
#include "pistruct.h"

#ifdef ANSI
void pimap(unsigned long start, unsigned long end)
#else
void pimap(start, end)
unsigned long start, end;
#endif
	{
	if (pxflags&POMP)
		printf("\nData %08lX-%08lX",start,end);
	}
