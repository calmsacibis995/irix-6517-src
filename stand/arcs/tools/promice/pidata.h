/*	pidata.h - Edit 6

	LoadICE Version 3
	Copyright (C) 1990-95 Grammar Engine, Inc.
	All rights reserved
	
	NOTICE:  This software source is a licensed copy of Grammar Engine's
	property.  It is supplied to you as part of support and maintenance
	of some Grammar Engine products that you may have purchased.  Use of
	this software is strictly limited to use with such products.  Any
	other use constitutes a violation of this license to you.
*/

/*	 - This file contains valid data values for the parser
*/

/* valid values for the romsize */

#ifdef PDGLOB
long pxromx[] = {
	2716, 2048, 2, 2048,
	2732, 4096, 4, 4096,
	2764, 8192, 8, 8192,
	27128, 16384, 16, 16384,
	27256, 32768, 32, 32768,
	27512, 65536, 64, 65536,
	27010, 131072, 128, 131072,
	27020, 262144, 256, 262144,
	27040, 524288, 512, 524288,
	27080, 1048576, 1024, 1048576,
	27160, 2097152, 2048, 2097152,
	0
	};
long pxroms[] = {
	0, 2048, 4096, 8192, 16384, 32768, 65536,
	131072, 262144, 524288, 1048576, 2097152,
	0,0,0,0,0
	};

/* valid values for the baud rate */

long pxbauds[] = {
	1200,0, 2400,1, 4800,2, 9600,4, 19200,6, 57600,8, 0,0
	};
#else
extern long pxromx[];
extern long pxroms[];
extern long pxbauds[];
#endif
